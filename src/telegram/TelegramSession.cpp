#include "TelegramSession.h"

#include "AuthRequests.h"
#include "core/Logging.h"
#include "core/Redaction.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QThread>
#include <QTimer>

namespace beygram::telegram {
namespace {
Q_LOGGING_CATEGORY(lcSession, "beygram.telegram.session")

constexpr int CloseTimeoutMs = 15000;

} // namespace

TelegramSession::TelegramSession(TelegramConfig config, QObject* parent)
    : QObject(parent),
      m_config(std::move(config)),
      m_lock(QDir(m_config.accountDirectory).filePath(QStringLiteral("session.lock")))
{
    qRegisterMetaType<AuthStage>("beygram::telegram::AuthStage");

    m_closeTimer = new QTimer(this);
    m_closeTimer->setSingleShot(true);
    connect(m_closeTimer, &QTimer::timeout, this, &TelegramSession::handleCloseTimeout);

    connect(&m_transport, &TdJsonTransport::updateReceived, this,
            &TelegramSession::handleUpdate);
    connect(&m_transport, &TdJsonTransport::transportError, this,
            [this](const QString& message) { reportError(message); });
}

TelegramSession::~TelegramSession()
{
    // Destructor must never block on a network handshake. AppController calls
    // shutdown() first; if that did not happen, stop without a handshake.
    m_transport.stop();
    releaseLock();
}

QString TelegramSession::connectionStateTitle() const
{
    if (m_connectionState == QLatin1String("connectionStateWaitingForNetwork"))
        return QStringLiteral("Waiting for the network");
    if (m_connectionState == QLatin1String("connectionStateConnecting"))
        return QStringLiteral("Connecting to Telegram");
    if (m_connectionState == QLatin1String("connectionStateConnectingToProxy"))
        return QStringLiteral("Connecting through a proxy");
    if (m_connectionState == QLatin1String("connectionStateUpdating"))
        return QStringLiteral("Downloading updates");
    if (m_connectionState == QLatin1String("connectionStateReady"))
        return QStringLiteral("Connected");
    return QStringLiteral("Not connected");
}

bool TelegramSession::isRunning() const
{
    return m_transport.isRunning();
}

bool TelegramSession::probeTdlib(QString* detail)
{
    QString error;
    const bool available = m_transport.load(&error);
    if (detail)
        *detail = available ? QStringLiteral("tdjson loaded from %1").arg(m_transport.loadedPath())
                            : error;
    return available;
}

QString TelegramSession::tdlibPath() const
{
    return m_transport.loadedPath();
}

void TelegramSession::start()
{
    if (m_transport.isRunning())
        return;

    m_lastError.clear();
    m_lastErrorCode = 0;
    m_connectionState = QStringLiteral("unknown");
    m_loggingOut = false;
    m_proxySetupStarted = false;
    m_tdlibParametersSent = false;

    QString loadError;
    if (!m_transport.load(&loadError)) {
        setStage(AuthStage::Unavailable, loadError);
        return;
    }

    if (!m_config.credentials.isValid()) {
        setStage(AuthStage::NotConfigured,
                 QStringLiteral(
                     "Set the Telegram API id and hash in Settings, or provide "
                     "BEYGRAM_TELEGRAM_API_ID and BEYGRAM_TELEGRAM_API_HASH."));
        return;
    }
    if (!m_config.hasDatabaseKey()) {
        setStage(AuthStage::NotConfigured,
                 QStringLiteral("The TDLib database key is unavailable; refusing to open an "
                                "unencrypted database."));
        return;
    }

    QString directoryError;
    if (!QDir().mkpath(m_config.databaseDirectory)
        || !QDir().mkpath(m_config.filesDirectory)) {
        setStage(AuthStage::Error,
                 QStringLiteral("Could not create the TDLib data directories"));
        return;
    }

    const Status locked = m_lock.acquire();
    if (!locked.ok()) {
        setStage(AuthStage::Locked, locked.error().message);
        return;
    }

    QString startError;
    if (!m_transport.start(&startError)) {
        releaseLock();
        setStage(AuthStage::Error, startError);
        return;
    }

    m_transport.send(QJsonObject{{QStringLiteral("@type"), QStringLiteral("setLogVerbosityLevel")},
                                 {QStringLiteral("new_verbosity_level"), 0}});

    setStage(AuthStage::Loading,
             QStringLiteral("TDLib client created; waiting for the authorization state"));
    if (m_config.hasProxy())
        qCInfo(lcSession) << "Using proxy at" << m_config.proxy.summary();
}

void TelegramSession::requestClose()
{
    if (!m_transport.isRunning())
        return;
    m_loggingOut = false;
    setStage(AuthStage::Closing, QStringLiteral("Closing the Telegram session"));
    m_closeTimer->start(CloseTimeoutMs);
    sendRequest(QJsonObject{{QStringLiteral("@type"), QStringLiteral("close")}},
                [this](const QJsonObject& response) {
                    if (response.value(QStringLiteral("@type")).toString()
                        == QLatin1String("error")) {
                        m_closeTimer->stop();
                        int code = 0;
                        const QString message = friendlyAuthError(response, &code);
                        reportError(message, code);
                        setStage(AuthStage::Error,
                                 QStringLiteral("Close failed: %1").arg(message));
                    }
                },
                0);
}

void TelegramSession::requestLogout()
{
    if (!m_transport.isRunning())
        return;
    m_loggingOut = true;
    setStage(AuthStage::Closing, QStringLiteral("Logging out and clearing local Telegram data"));
    m_closeTimer->start(CloseTimeoutMs);
    sendRequest(QJsonObject{{QStringLiteral("@type"), QStringLiteral("logOut")}},
                [this](const QJsonObject& response) {
                    if (response.value(QStringLiteral("@type")).toString()
                        == QLatin1String("error")) {
                        m_loggingOut = false;
                        m_closeTimer->stop();
                        int code = 0;
                        const QString message = friendlyAuthError(response, &code);
                        reportError(message, code);
                        if (m_transport.isRunning())
                            setStage(AuthStage::Ready,
                                     QStringLiteral("Logout failed; the account is still "
                                                    "authorized. %1")
                                         .arg(message));
                    }
                },
                120000);
}

void TelegramSession::restartSession()
{
    m_closeTimer->stop();
    m_transport.stop();
    releaseLock();
    resetAuthDetails();
    m_lastError.clear();
    m_lastErrorCode = 0;
    setStage(AuthStage::Idle, QStringLiteral("Restarting the Telegram session"));
    QTimer::singleShot(0, this, &TelegramSession::start);
}

bool TelegramSession::shutdown(int timeoutMs)
{
    m_closeTimer->stop();

    if (!m_transport.isRunning()) {
        releaseLock();
        return true;
    }

    if (m_stage != AuthStage::Closed) {
        sendRequest(QJsonObject{{QStringLiteral("@type"), QStringLiteral("close")}},
                    [](const QJsonObject&) {}, 0);

        QElapsedTimer timer;
        timer.start();
        while (m_stage != AuthStage::Closed && timer.elapsed() < timeoutMs) {
            QCoreApplication::processEvents(
                QEventLoop::ExcludeUserInputEvents | QEventLoop::AllEvents, 25);
            QThread::msleep(5);
        }
    }

    const bool closed = m_stage == AuthStage::Closed;
    m_transport.stop();
    releaseLock();
    m_loggingOut = false;

    if (!closed) {
        setStage(AuthStage::Error,
                 QStringLiteral("TDLib did not confirm the close handshake; the client was "
                                "stopped without it."));
    }
    return closed;
}

void TelegramSession::abort()
{
    m_closeTimer->stop();
    m_transport.stop();
    releaseLock();
    resetAuthDetails();
    m_loggingOut = false;
    m_connectionState = QStringLiteral("unknown");
    setStage(AuthStage::Idle, QStringLiteral("Telegram session stopped"));
}

void TelegramSession::submitPhoneNumber(const QString& phoneNumber)
{
    const QString normalized = auth::normalizePhoneNumber(phoneNumber);
    if (normalized.isEmpty()) {
        reportError(QStringLiteral(
            "Enter a phone number in international format starting with + and the "
            "country code."));
        return;
    }

    m_lastError.clear();
    m_lastErrorCode = 0;
    sendAuthRequest(auth::setPhoneNumberRequest(normalized),
                    QStringLiteral("Could not submit the phone number"));
}

void TelegramSession::submitCode(const QString& code)
{
    const QString value = auth::normalizeCode(code);
    if (value.isEmpty()) {
        reportError(QStringLiteral("Enter the Telegram login code."));
        return;
    }
    m_lastError.clear();
    m_lastErrorCode = 0;
    sendAuthRequest(auth::checkCodeRequest(value),
                    QStringLiteral("Could not check the login code"));
}

void TelegramSession::submitPassword(const QString& password)
{
    if (password.isEmpty()) {
        reportError(QStringLiteral("Enter your two-step verification password."));
        return;
    }
    m_lastError.clear();
    m_lastErrorCode = 0;
    sendAuthRequest(auth::checkPasswordRequest(password),
                    QStringLiteral("Could not check the two-step password"));
}

void TelegramSession::submitEmailAddress(const QString& emailAddress)
{
    const QString value = emailAddress.trimmed();
    if (value.isEmpty() || !value.contains(QLatin1Char('@'))) {
        reportError(QStringLiteral("Enter a valid email address."));
        return;
    }
    m_lastError.clear();
    m_lastErrorCode = 0;
    sendAuthRequest(auth::setEmailAddressRequest(value),
                    QStringLiteral("Could not submit the email address"));
}

void TelegramSession::submitEmailCode(const QString& code)
{
    const QString value = auth::normalizeCode(code);
    if (value.isEmpty()) {
        reportError(QStringLiteral("Enter the code from the email."));
        return;
    }
    m_lastError.clear();
    m_lastErrorCode = 0;
    sendAuthRequest(auth::checkEmailCodeRequest(value),
                    QStringLiteral("Could not check the email code"));
}

void TelegramSession::submitRegistration(const QString& firstName, const QString& lastName)
{
    if (firstName.trimmed().isEmpty()) {
        reportError(QStringLiteral("Enter your first name."));
        return;
    }
    m_lastError.clear();
    m_lastErrorCode = 0;
    sendAuthRequest(auth::registerUserRequest(firstName, lastName),
                    QStringLiteral("Could not complete registration"));
}

void TelegramSession::resendCode()
{
    m_lastError.clear();
    m_lastErrorCode = 0;
    sendAuthRequest(auth::resendCodeRequest(), QStringLiteral("Could not resend the code"));
}

void TelegramSession::requestQrCodeLogin()
{
    m_lastError.clear();
    m_lastErrorCode = 0;
    sendAuthRequest(auth::requestQrCodeRequest(), QStringLiteral("Could not request QR login"));
}

void TelegramSession::clearError()
{
    const bool hadError = !m_lastError.isEmpty();
    m_lastError.clear();
    m_lastErrorCode = 0;
    if (hadError)
        emit errorOccurred(QString());
}

TdJsonTransport::RequestId TelegramSession::sendRequest(const QJsonObject& request,
                                                       TdJsonTransport::ResponseHandler handler,
                                                       int timeoutMs)
{
    return m_transport.sendRequest(request, std::move(handler), timeoutMs);
}

void TelegramSession::handleUpdate(const QJsonObject& update)
{
    emit updateReceived(update);

    const QString type = update.value(QStringLiteral("@type")).toString();
    if (type == QLatin1String("updateAuthorizationState")) {
        handleAuthorizationState(update.value(QStringLiteral("authorization_state")).toObject());
    } else if (type == QLatin1String("updateConnectionState")) {
        const QJsonObject state = update.value(QStringLiteral("state")).toObject();
        const QString name = state.value(QStringLiteral("@type")).toString();
        if (!name.isEmpty() && name != m_connectionState) {
            m_connectionState = name;
            qCInfo(lcSession) << "Connection state:" << name;
            emit connectionStateChanged();
        }
    } else if (type == QLatin1String("error")) {
        int code = 0;
        reportError(friendlyAuthError(update, &code), code);
    }
}

void TelegramSession::handleAuthorizationState(const QJsonObject& state)
{
    const QString name = state.value(QStringLiteral("@type")).toString();
    const AuthStage next = authStageFromTdlib(name);

    m_lastError.clear();
    m_lastErrorCode = 0;

    switch (next) {
    case AuthStage::WaitTdlibParameters:
        setStage(next, QStringLiteral("Sending database and API parameters to TDLib"));
        ensureTdlibParameters();
        break;
    case AuthStage::WaitPhoneNumber:
        resetAuthDetails();
        setStage(next, QStringLiteral("Enter the phone number of your Telegram account"));
        break;
    case AuthStage::WaitCode:
        applyCodeInfo(state.value(QStringLiteral("code_info")).toObject());
        setStage(next, m_codePhoneNumber.isEmpty()
                           ? QStringLiteral("Enter the code Telegram sent")
                           : QStringLiteral("Enter the code Telegram sent to %1")
                                 .arg(m_codePhoneNumber));
        break;
    case AuthStage::WaitPassword:
        m_passwordHint = state.value(QStringLiteral("password_hint")).toString();
        m_passwordHasRecoveryEmail =
            state.value(QStringLiteral("has_recovery_email_address")).toBool();
        setStage(next, m_passwordHint.isEmpty()
                           ? QStringLiteral("Two-step verification is enabled")
                           : QStringLiteral("Hint: %1").arg(m_passwordHint));
        break;
    case AuthStage::WaitRegistration: {
        const QJsonObject terms = state.value(QStringLiteral("terms_of_service")).toObject();
        m_registrationTerms = terms.value(QStringLiteral("text")).toString();
        m_registrationMinAge = terms.value(QStringLiteral("min_user_age")).toInt(16);
        setStage(next, QStringLiteral("This phone number has no Telegram account yet"));
        break;
    }
    case AuthStage::WaitEmailAddress:
        resetAuthDetails();
        setStage(next, QStringLiteral("Telegram asks for an email address for this account"));
        break;
    case AuthStage::WaitEmailCode: {
        const QJsonObject info = state.value(QStringLiteral("code_info")).toObject();
        m_emailCodeLength = info.value(QStringLiteral("length")).toInt();
        m_emailPattern = info.value(QStringLiteral("email_address_pattern")).toString();
        setStage(next, m_emailPattern.isEmpty()
                           ? QStringLiteral("Enter the code from the email")
                           : QStringLiteral("Enter the code sent to %1").arg(m_emailPattern));
        break;
    }
    case AuthStage::WaitQrCode: {
        const QString link = state.value(QStringLiteral("link")).toString();
        if (!link.isEmpty() && link != m_qrLink) {
            m_qrLink = link;
            ++m_qrRevision;
            emit qrCodeChanged();
        }
        setStage(next, QStringLiteral("Scan the QR code with an authorized Telegram app"));
        break;
    }
    case AuthStage::Ready:
        m_closeTimer->stop();
        m_loggingOut = false;
        resetAuthDetails();
        setStage(next, QStringLiteral("The account is authorized; chats, history and "
                                      "media are available"));
        break;
    case AuthStage::Closing:
        setStage(next, QStringLiteral("The session is closing"));
        break;
    case AuthStage::Closed:
        m_closeTimer->stop();
        m_loggingOut = false;
        resetAuthDetails();
        setStage(next, QStringLiteral("The session is closed"));
        m_transport.stop();
        releaseLock();
        break;
    default:
        setStage(AuthStage::Loading,
                 QStringLiteral("TDLib reported an unsupported authorization state: %1").arg(name));
        break;
    }
}

void TelegramSession::setStage(AuthStage stage, const QString& detail)
{
    const bool changed = m_stage != stage || m_stageDetail != detail;
    m_stage = stage;
    m_stageDetail = detail;
    if (changed) {
        qCInfo(lcSession) << "Auth stage:" << authStageName(stage);
        emit stageChanged(stage);
    }
}

void TelegramSession::ensureTdlibParameters()
{
    if (m_config.hasProxy() && !m_proxySetupStarted) {
        m_proxySetupStarted = true;
        setStage(AuthStage::WaitTdlibParameters,
                 QStringLiteral("Configuring proxy %1").arg(m_config.proxy.summary()));

        QJsonObject proxy{
            {QStringLiteral("@type"), QStringLiteral("proxy")},
            {QStringLiteral("server"), m_config.proxy.server},
            {QStringLiteral("port"), m_config.proxy.port},
            {QStringLiteral("type"), proxyTypeObject()},
        };

        sendRequest(
            QJsonObject{{QStringLiteral("@type"), QStringLiteral("addProxy")},
                        {QStringLiteral("proxy"), proxy},
                        {QStringLiteral("enable"), true},
                        {QStringLiteral("comment"), QStringLiteral("BeyGram")}},
            [this](const QJsonObject& response) {
                if (response.value(QStringLiteral("@type")).toString()
                    == QLatin1String("error")) {
                    int code = 0;
                    reportError(QStringLiteral("Proxy setup failed: %1")
                                    .arg(friendlyAuthError(response, &code)),
                                code);
                }
            },
            0);

        // TDLib may delay the addProxy response until it has attempted a
        // connection through the proxy. The request is processed in order, so
        // parameters can be sent immediately instead of stalling startup.
        sendTdlibParameters();
        return;
    }

    sendTdlibParameters();
}

void TelegramSession::applyProxy(const TelegramProxySettings& settings)
{
    if (!settings.isValid()) {
        reportError(QStringLiteral("Proxy settings are incomplete"));
        return;
    }

    m_config.proxy = settings;
    if (!m_transport.isRunning()) {
        // The client is not up yet; start() will apply m_config.proxy.
        return;
    }

    QJsonObject proxy{
        {QStringLiteral("@type"), QStringLiteral("proxy")},
        {QStringLiteral("server"), settings.server},
        {QStringLiteral("port"), settings.port},
        {QStringLiteral("type"), proxyTypeObject()},
    };

    sendRequest(
        QJsonObject{{QStringLiteral("@type"), QStringLiteral("addProxy")},
                    {QStringLiteral("proxy"), proxy},
                    {QStringLiteral("enable"), true},
                    {QStringLiteral("comment"), QStringLiteral("BeyGram")}},
        [this](const QJsonObject& response) {
            if (response.value(QStringLiteral("@type")).toString() == QLatin1String("error")) {
                int code = 0;
                reportError(QStringLiteral("Proxy setup failed: %1")
                                .arg(friendlyAuthError(response, &code)),
                            code);
            }
        },
        0);
}

QJsonObject TelegramSession::proxyTypeObject() const
{
    if (m_config.proxy.type == QLatin1String("http")) {
        return QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("proxyTypeHttp")},
            {QStringLiteral("username"), m_config.proxy.username},
            {QStringLiteral("password"), m_config.proxy.password},
            {QStringLiteral("http_only"), false},
        };
    }
    if (m_config.proxy.type == QLatin1String("mtproto")) {
        return QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("proxyTypeMtproto")},
            {QStringLiteral("secret"), m_config.proxy.password},
        };
    }
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("proxyTypeSocks5")},
        {QStringLiteral("username"), m_config.proxy.username},
        {QStringLiteral("password"), m_config.proxy.password},
    };
}

void TelegramSession::sendTdlibParameters()
{
    if (m_tdlibParametersSent)
        return;
    m_tdlibParametersSent = true;

    // The request contains api_hash and the database key. It is handed to
    // TDLib directly and never logged or exposed through signals.
    sendRequest(m_config.setTdlibParametersRequest(),
                [this](const QJsonObject& response) {
                    if (response.value(QStringLiteral("@type")).toString()
                        == QLatin1String("error")) {
                        int code = 0;
                        const QString message = friendlyAuthError(response, &code);
                        reportError(message, code);
                        m_transport.stop();
                        releaseLock();
                        setStage(AuthStage::Error, message);
                    }
                },
                60000);
}

void TelegramSession::sendAuthRequest(const QJsonObject& request,
                                      const QString& failureContext,
                                      int timeoutMs)
{
    sendRequest(request,
                [this, failureContext](const QJsonObject& response) {
                    const QString type = response.value(QStringLiteral("@type")).toString();
                    if (type == QLatin1String("error")) {
                        int code = 0;
                        const QString message = friendlyAuthError(response, &code);
                        reportError(QStringLiteral("%1: %2").arg(failureContext, message), code);
                    }
                    emit authRequestFinished();
                },
                timeoutMs);
}

void TelegramSession::reportError(const QString& message, int code)
{
    m_lastError = logging::redact(message);
    m_lastErrorCode = code;
    qCWarning(lcSession).noquote() << "Auth error" << code << ":" << m_lastError;
    emit errorOccurred(m_lastError);
}

void TelegramSession::handleCloseTimeout()
{
    if (m_stage != AuthStage::Closing)
        return;

    m_transport.stop();
    releaseLock();
    m_loggingOut = false;
    const QString message =
        QStringLiteral("TDLib did not confirm the close handshake within %1 seconds.")
            .arg(CloseTimeoutMs / 1000);
    reportError(message);
    setStage(AuthStage::Error, message);
}

void TelegramSession::releaseLock()
{
    if (m_lock.isLocked())
        m_lock.release();
}

void TelegramSession::resetAuthDetails()
{
    m_codeLength = 0;
    m_codePhoneNumber.clear();
    m_codeTypeName.clear();
    m_emailCodeLength = 0;
    m_emailPattern.clear();
    m_passwordHint.clear();
    m_passwordHasRecoveryEmail = false;
    m_registrationTerms.clear();
    m_registrationMinAge = 16;
    if (!m_qrLink.isEmpty()) {
        m_qrLink.clear();
        ++m_qrRevision;
        emit qrCodeChanged();
    }
}

void TelegramSession::applyCodeInfo(const QJsonObject& codeInfo)
{
    m_codePhoneNumber = codeInfo.value(QStringLiteral("phone_number")).toString();
    const QJsonObject type = codeInfo.value(QStringLiteral("type")).toObject();
    m_codeTypeName = type.value(QStringLiteral("@type")).toString();
    m_codeLength = type.value(QStringLiteral("length")).toInt();
}

QString TelegramSession::friendlyAuthError(const QJsonObject& error, int* code) const
{
    const int errorCode = error.value(QStringLiteral("code")).toInt();
    if (code)
        *code = errorCode;
    return auth::describeError(errorCode,
                               error.value(QStringLiteral("message")).toString());
}

} // namespace beygram::telegram
