#include "TelegramSession.h"

#include "core/Logging.h"
#include "core/Redaction.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>

namespace beygram::telegram {
namespace {
Q_LOGGING_CATEGORY(lcSession, "beygram.telegram.session")
} // namespace

TelegramSession::TelegramSession(TelegramConfig config, QObject* parent)
    : QObject(parent), m_config(std::move(config))
{
    qRegisterMetaType<AuthStage>("beygram::telegram::AuthStage");

    connect(&m_transport, &TdJsonTransport::updateReceived, this,
            &TelegramSession::handleUpdate);
    connect(&m_transport, &TdJsonTransport::transportError, this,
            [this](const QString& message) { reportError(message); });
}

TelegramSession::~TelegramSession()
{
    stop();
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
        *detail = available ? QStringLiteral("tdjson loaded successfully") : error;
    return available;
}

void TelegramSession::start()
{
    if (m_transport.isRunning())
        return;

    m_lastError.clear();
    m_connectionState = QStringLiteral("unknown");

    QString loadError;
    if (!m_transport.load(&loadError)) {
        setStage(AuthStage::Unavailable, loadError);
        return;
    }

    if (!m_config.credentials.isValid()) {
        setStage(AuthStage::NotConfigured,
                 QStringLiteral(
                     "Set BEYGRAM_TELEGRAM_API_ID and BEYGRAM_TELEGRAM_API_HASH. "
                     "Credentials are read from the process environment in Phase 0."));
        return;
    }

    QString startError;
    if (!m_transport.start(&startError)) {
        setStage(AuthStage::Error, startError);
        return;
    }

    // TDLib's own file log is disabled; the app log stays the single sink.
    m_transport.send(QJsonObject{{QStringLiteral("@type"), QStringLiteral("setLogVerbosityLevel")},
                                 {QStringLiteral("new_verbosity_level"), 1}});

    setStage(AuthStage::Loading, QStringLiteral("TDLib client created; waiting for its state"));
}

void TelegramSession::stop()
{
    if (m_transport.isRunning())
        qCInfo(lcSession) << "Stopping TDLib session";
    m_transport.stop();
    m_qrLink.clear();
    m_connectionState = QStringLiteral("unknown");
    setStage(AuthStage::Idle, QStringLiteral("Telegram core is stopped"));
}

void TelegramSession::requestClose()
{
    if (!m_transport.isRunning())
        return;
    setStage(AuthStage::Closing, QStringLiteral("Close requested; waiting for TDLib"));
    m_transport.send(QJsonObject{{QStringLiteral("@type"), QStringLiteral("close")}});
}

void TelegramSession::submitPhoneNumber(const QString& phoneNumber)
{
    if (phoneNumber.trimmed().isEmpty()) {
        reportError(QStringLiteral("Phone number is empty"));
        return;
    }
    m_transport.send(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setAuthenticationPhoneNumber")},
        {QStringLiteral("phone_number"), phoneNumber.trimmed()},
        {QStringLiteral("settings"),
         QJsonObject{{QStringLiteral("@type"), QStringLiteral("phoneNumberAuthenticationSettings")},
                     {QStringLiteral("allow_flash_call"), false},
                     {QStringLiteral("is_current_phone_number"), false},
                     {QStringLiteral("allow_sms_retriever_api"), false}}},
    });
}

void TelegramSession::submitCode(const QString& code)
{
    if (code.trimmed().isEmpty()) {
        reportError(QStringLiteral("Login code is empty"));
        return;
    }
    m_transport.send(QJsonObject{{QStringLiteral("@type"), QStringLiteral("checkAuthenticationCode")},
                                 {QStringLiteral("code"), code.trimmed()}});
}

void TelegramSession::submitPassword(const QString& password)
{
    if (password.isEmpty()) {
        reportError(QStringLiteral("Password is empty"));
        return;
    }
    m_transport.send(QJsonObject{{QStringLiteral("@type"), QStringLiteral("checkAuthenticationPassword")},
                                 {QStringLiteral("password"), password}});
}

void TelegramSession::requestQrCodeLogin()
{
    m_transport.send(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("requestQrCodeAuthentication")},
        {QStringLiteral("other_user_ids"), QJsonArray()},
    });
}

void TelegramSession::resetError()
{
    m_lastError.clear();
    if (m_stage == AuthStage::Error)
        setStage(AuthStage::Idle, QStringLiteral("Telegram core is stopped"));
    emit errorOccurred(QString());
}

void TelegramSession::sendRequest(const QJsonObject& request)
{
    m_transport.send(request);
}

void TelegramSession::handleUpdate(const QJsonObject& update)
{
    emit updateReceived(update);

    const QString type = update.value(QStringLiteral("@type")).toString();
    if (type == QLatin1String("updateAuthorizationState")) {
        const QJsonObject state = update.value(QStringLiteral("authorization_state")).toObject();
        handleAuthorizationState(state);
    } else if (type == QLatin1String("updateConnectionState")) {
        const QJsonObject state = update.value(QStringLiteral("state")).toObject();
        const QString name = state.value(QStringLiteral("@type")).toString();
        if (!name.isEmpty() && name != m_connectionState) {
            m_connectionState = name;
            emit connectionStateChanged();
        }
    } else if (type == QLatin1String("error")) {
        const int code = update.value(QStringLiteral("code")).toInt();
        const QString message = update.value(QStringLiteral("message")).toString();
        reportError(QStringLiteral("TDLib error %1: %2").arg(code).arg(message));
    }
}

void TelegramSession::handleAuthorizationState(const QJsonObject& state)
{
    const QString name = state.value(QStringLiteral("@type")).toString();
    const AuthStage next = authStageFromTdlib(name);

    switch (next) {
    case AuthStage::WaitTdlibParameters:
        setStage(AuthStage::WaitTdlibParameters,
                 QStringLiteral("Sending database and API parameters to TDLib"));
        sendTdlibParameters();
        break;
    case AuthStage::WaitPhoneNumber:
        setStage(next, QStringLiteral("TDLib can accept a phone number"));
        break;
    case AuthStage::WaitCode:
        setStage(next, QStringLiteral("Enter the code Telegram sent"));
        break;
    case AuthStage::WaitPassword:
        setStage(next, QStringLiteral("Two-step verification is enabled for this account"));
        break;
    case AuthStage::WaitRegistration:
        setStage(next, QStringLiteral("This phone number has no Telegram account yet"));
        break;
    case AuthStage::WaitQrCode:
        m_qrLink = state.value(QStringLiteral("link")).toString();
        setStage(next, QStringLiteral("Use an authorized Telegram app to confirm the QR login"));
        break;
    case AuthStage::Ready:
        m_qrLink.clear();
        setStage(next, QStringLiteral("The account is authorized; chat repositories arrive in Phase 2"));
        break;
    case AuthStage::Closing:
        setStage(next, QStringLiteral("The session is closing"));
        break;
    case AuthStage::Closed:
        m_qrLink.clear();
        setStage(next, QStringLiteral("The session is closed"));
        break;
    default:
        setStage(AuthStage::Loading, QStringLiteral("TDLib reported an unknown authorization state"));
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

void TelegramSession::sendTdlibParameters()
{
    // The request contains api_hash. It is handed to TDLib directly and never
    // logged or exposed through signals.
    m_transport.send(m_config.setTdlibParametersRequest());
}

void TelegramSession::reportError(const QString& message)
{
    m_lastError = logging::redact(message);
    qCWarning(lcSession).noquote() << m_lastError;
    emit errorOccurred(m_lastError);
}

} // namespace beygram::telegram
