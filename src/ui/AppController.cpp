#include "AppController.h"

#include "BeyGramVersion.h"
#include "core/AppPaths.h"
#include "core/Logging.h"
#include "media/BgMediaStatus.h"
#include "telegram/TelegramConfig.h"
#include "telegram/TelegramSession.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QLocale>
#include <QSysInfo>
#include <QUrl>

namespace beygram::ui {
namespace {

telegram::TelegramConfig makeConfig()
{
    telegram::TelegramConfig config;
    config.credentials = telegram::TelegramCredentials::fromEnvironment();
    config.databaseDirectory =
        QDir(AppPaths::tdlibAccountDirectory(QStringLiteral("primary")))
            .filePath(QStringLiteral("database"));
    config.filesDirectory =
        QDir(AppPaths::tdlibAccountDirectory(QStringLiteral("primary")))
            .filePath(QStringLiteral("files"));
    config.systemLanguageCode = QLocale::system().name().section(QLatin1Char('_'), 0, 0);
    if (config.systemLanguageCode.isEmpty())
        config.systemLanguageCode = QStringLiteral("en");
    config.deviceModel = QStringLiteral("BeyGram Windows desktop");
    config.systemVersion =
        QStringLiteral("%1 %2").arg(QSysInfo::productType(), QSysInfo::productVersion());
    config.applicationVersion = QStringLiteral(BEYGRAM_VERSION_STRING);

    QString error;
    if (!AppPaths::ensureDirectory(config.databaseDirectory, &error))
        qWarning("%s", qUtf8Printable(error));
    if (!AppPaths::ensureDirectory(config.filesDirectory, &error))
        qWarning("%s", qUtf8Printable(error));
    return config;
}

QString compilerLabel()
{
#if defined(_MSC_VER)
    return QStringLiteral("MSVC %1.%2").arg(_MSC_VER / 100).arg(_MSC_VER % 100);
#elif defined(__clang__)
    return QStringLiteral("Clang %1").arg(QString::fromLatin1(__clang_version__));
#elif defined(__GNUC__)
    return QStringLiteral("GCC %1.%2").arg(__GNUC__).arg(__GNUC_MINOR__);
#else
    return QStringLiteral("unknown compiler");
#endif
}

} // namespace

AppController::AppController(QObject* parent) : QObject(parent)
{
    buildConfig();

    m_session = std::make_unique<telegram::TelegramSession>(makeConfig());

    m_credentialsConfigured = m_session->config().credentials.isValid();
    m_tdlibAvailable = m_session->probeTdlib(&m_tdlibDetail);

    connectSession();

    if (canStart()) {
        m_session->start();
    }
    notifyChanged();
}

AppController::~AppController() = default;

void AppController::buildConfig()
{
    QString error;
    if (!AppPaths::ensureDirectory(AppPaths::dataDirectory(), &error))
        qWarning("%s", qUtf8Printable(error));
    if (!AppPaths::ensureDirectory(AppPaths::logDirectory(), &error))
        qWarning("%s", qUtf8Printable(error));
}

void AppController::connectSession()
{
    connect(m_session.get(), &telegram::TelegramSession::stageChanged, this,
            [this](telegram::AuthStage) { notifyChanged(); });
    connect(m_session.get(), &telegram::TelegramSession::connectionStateChanged, this,
            [this] { notifyChanged(); });
    connect(m_session.get(), &telegram::TelegramSession::errorOccurred, this,
            [this](const QString&) { notifyChanged(); });
}

void AppController::notifyChanged()
{
    emit stateChanged();
}

QString AppController::appVersion() const
{
    return QStringLiteral(BEYGRAM_VERSION_STRING);
}

QString AppController::qtVersion() const
{
    return QString::fromLatin1(qVersion());
}

QString AppController::buildInfo() const
{
    return QStringLiteral("%1 %2 · Qt %3 · %4")
        .arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture(), qtVersion(),
             compilerLabel());
}

QString AppController::dataDirectory() const
{
    return AppPaths::dataDirectory();
}

QString AppController::logDirectory() const
{
    return AppPaths::logDirectory();
}

bool AppController::tdlibAvailable() const
{
    return m_tdlibAvailable;
}

QString AppController::tdlibDetail() const
{
    return m_tdlibDetail;
}

bool AppController::credentialsConfigured() const
{
    return m_credentialsConfigured;
}

bool AppController::running() const
{
    return m_session && m_session->isRunning();
}

bool AppController::ready() const
{
    return m_session && m_session->isReady();
}

bool AppController::canStart() const
{
    return m_tdlibAvailable && m_credentialsConfigured && m_session && !m_session->isRunning();
}

QString AppController::stageName() const
{
    return m_session ? telegram::authStageName(m_session->stage())
                     : QStringLiteral("idle");
}

QString AppController::stageTitle() const
{
    return m_session ? telegram::authStageTitle(m_session->stage())
                     : QStringLiteral("Telegram core is stopped");
}

QString AppController::stageDetail() const
{
    return m_session ? m_session->stageDetail() : QString();
}

QString AppController::connectionTitle() const
{
    return m_session ? m_session->connectionStateTitle() : QStringLiteral("Not connected");
}

QString AppController::lastError() const
{
    return m_session ? m_session->lastError() : QString();
}

QString AppController::credentialsHint() const
{
    return QStringLiteral(
        "Set BEYGRAM_TELEGRAM_API_ID and BEYGRAM_TELEGRAM_API_HASH in the environment "
        "before starting BeyGram. Credentials are never written to the log.");
}

QString AppController::bgMediaTitle() const
{
    return media::bgMediaStatus().title;
}

QString AppController::bgMediaDetail() const
{
    return media::bgMediaStatus().detail;
}

void AppController::refresh()
{
    QString detail;
    m_tdlibAvailable = m_session && m_session->probeTdlib(&detail);
    m_tdlibDetail = detail;
    m_credentialsConfigured = m_session && m_session->config().credentials.isValid();
    notifyChanged();
}

void AppController::startTelegram()
{
    if (!m_session || !canStart())
        return;
    m_session->start();
    notifyChanged();
}

void AppController::stopTelegram()
{
    if (!m_session)
        return;
    m_session->stop();
    notifyChanged();
}

void AppController::openDataFolder()
{
    const QString path = dataDirectory();
    if (QDir(path).exists())
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void AppController::openLogFolder()
{
    const QString path = logDirectory();
    if (QDir(path).exists())
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QStringList AppController::recentLogs() const
{
    return logging::recentMessages();
}

QString AppController::diagnosticsText() const
{
    QStringList lines;
    lines << QStringLiteral("BeyGram Windows %1").arg(appVersion())
          << QStringLiteral("Qt: %1").arg(qtVersion())
          << QStringLiteral("Build: %1").arg(buildInfo())
          << QStringLiteral("TDLib available: %1").arg(tdlibAvailable() ? "yes" : "no")
          << QStringLiteral("TDLib detail: %1").arg(tdlibDetail())
          << QStringLiteral("Credentials configured: %1")
                 .arg(credentialsConfigured() ? "yes" : "no")
          << QStringLiteral("Stage: %1").arg(stageName())
          << QStringLiteral("Connection: %1").arg(connectionTitle())
          << QStringLiteral("Data directory: %1").arg(dataDirectory())
          << QStringLiteral("Log file: %1").arg(logging::logFilePath())
          << QString()
          << QStringLiteral("— Recent log (redacted) —");
    lines.append(recentLogs());
    return lines.join(QLatin1Char('\n'));
}

void AppController::copyDiagnostics()
{
    if (QGuiApplication::instance() != nullptr) {
        QGuiApplication::clipboard()->setText(diagnosticsText());
        qInfo("Diagnostics copied to clipboard");
    }
}

void AppController::shutdown()
{
    if (m_session)
        m_session->stop();
}

} // namespace beygram::ui
