#include "AppController.h"

#include "BeyGramVersion.h"
#include "core/AppPaths.h"
#include "core/Logging.h"
#include "media/BgMediaStatus.h"
#include "platform/windows/DatabaseKeyStore.h"
#include "telegram/TelegramRepository.h"
#include "telegram/TelegramSession.h"

#ifdef BEYGRAM_EMBEDDED_CREDENTIALS
#include "BeyGramBuildSecrets.h"
#endif

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocale>
#include <QStyle>
#include <QSysInfo>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>

namespace beygram::ui {
namespace {

telegram::TelegramCredentials environmentCredentials()
{
    const telegram::TelegramCredentials credentials = telegram::TelegramCredentials::fromEnvironment();
    if (credentials.isValid()
        && telegram::CredentialStore::isValidApiHash(credentials.apiHash)) {
        return credentials;
    }
    return {};
}

telegram::TelegramCredentials buildCredentials()
{
#ifdef BEYGRAM_EMBEDDED_CREDENTIALS
    telegram::TelegramCredentials credentials;
    credentials.apiId = BEYGRAM_BUILD_API_ID;
    credentials.apiHash = QStringLiteral(BEYGRAM_BUILD_API_HASH);
    if (credentials.isValid()
        && telegram::CredentialStore::isValidApiHash(credentials.apiHash)) {
        return credentials;
    }
#endif
    return {};
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

QString fullName(const QJsonObject& user)
{
    const QString first = user.value(QStringLiteral("first_name")).toString().trimmed();
    const QString last = user.value(QStringLiteral("last_name")).toString().trimmed();
    if (first.isEmpty())
        return last;
    if (last.isEmpty())
        return first;
    return first + QLatin1Char(' ') + last;
}

QString username(const QJsonObject& user)
{
    const QJsonObject usernames = user.value(QStringLiteral("usernames")).toObject();
    const QJsonArray active = usernames.value(QStringLiteral("active_usernames")).toArray();
    if (!active.isEmpty())
        return active.first().toString();
    if (!usernames.value(QStringLiteral("editable_username")).toString().isEmpty())
        return usernames.value(QStringLiteral("editable_username")).toString();
    return user.value(QStringLiteral("username")).toString();
}

} // namespace

AppController::AppController(QObject* parent)
    : QObject(parent),
      m_credentials(QDir(AppPaths::dataDirectory()).filePath(QStringLiteral("secrets"))),
      m_registry(AppPaths::dataDirectory()),
      m_mediaViewer(&m_mediaStore),
      m_notificationSettings(
          QDir(AppPaths::dataDirectory())
              .filePath(QStringLiteral("state/notification-settings.json"))),
      m_drafts(QDir(AppPaths::dataDirectory()).filePath(QStringLiteral("state/drafts.json")))
{
    buildConfig();
    m_drafts.load();
    m_notificationSettings.load();

    m_chatFilterModel.setSourceModel(&m_chatListModel);

    m_busyTimer = new QTimer(this);
    m_busyTimer->setSingleShot(true);
    connect(m_busyTimer, &QTimer::timeout, this, [this] { setBusy(false); });

    rebuildSession(true);
}

AppController::~AppController() = default;

void AppController::buildConfig()
{
    for (const QString& path :
         {AppPaths::dataDirectory(), AppPaths::logDirectory(),
          QDir(AppPaths::dataDirectory()).filePath(QStringLiteral("secrets"))}) {
        QString error;
        if (!AppPaths::ensureDirectory(path, &error))
            qWarning("%s", qUtf8Printable(error));
    }
}

Result<telegram::TelegramConfig> AppController::prepareConfig()
{
    m_credentialsError.clear();

    Status registryStatus = m_registry.load();
    if (!registryStatus.ok())
        return Result<telegram::TelegramConfig>::fail(registryStatus.error().code,
                                            registryStatus.error().message);

    registryStatus = m_registry.ensurePrimary();
    if (!registryStatus.ok())
        return Result<telegram::TelegramConfig>::fail(registryStatus.error().code,
                                            registryStatus.error().message);

    const auto account = m_registry.active();
    if (!account.hasValue())
        return Result<telegram::TelegramConfig>::fail(account.error().code, account.error().message);

    m_accountId = account.value().id;
    const QString accountDirectory = m_registry.accountDirectory(m_accountId);

    QString pathError;
    if (!AppPaths::ensureDirectory(accountDirectory, &pathError))
        return Result<telegram::TelegramConfig>::fail(ErrorCode::Io, pathError);

    platform::DatabaseKeyStore keyStore(accountDirectory);
    const auto key = keyStore.loadOrCreate();
    if (!key.hasValue())
        return Result<telegram::TelegramConfig>::fail(key.error().code, key.error().message);

    telegram::TelegramCredentials credentials = environmentCredentials();
    m_credentialsSource.clear();
    if (credentials.isValid()) {
        m_credentialsSource = QStringLiteral("environment");
    } else {
        const auto stored = m_credentials.load();
        if (stored.hasValue()) {
            credentials = stored.value();
            m_credentialsSource = QStringLiteral("DPAPI");
        } else {
            if (stored.error().code != ErrorCode::NotFound)
                m_credentialsError = stored.error().message;

            const telegram::TelegramCredentials embedded = buildCredentials();
            if (embedded.isValid()) {
                credentials = embedded;
                m_credentialsSource = QStringLiteral("build");
            }
        }
    }
    m_credentialsConfigured = credentials.isValid();

    telegram::TelegramConfig config;
    config.credentials = credentials;
    config.accountId = m_accountId;
    config.accountDirectory = accountDirectory;
    config.databaseDirectory = QDir(accountDirectory).filePath(QStringLiteral("database"));
    config.filesDirectory = QDir(accountDirectory).filePath(QStringLiteral("files"));
    config.databaseEncryptionKey = key.value();
    config.proxy = telegram::TelegramProxySettings::fromEnvironment();
    config.systemLanguageCode = QLocale::system().name().section(QLatin1Char('_'), 0, 0);
    if (config.systemLanguageCode.isEmpty())
        config.systemLanguageCode = QStringLiteral("en");
    config.deviceModel = QStringLiteral("BeyGram Windows desktop");
    config.systemVersion =
        QStringLiteral("%1 %2").arg(QSysInfo::productType(), QSysInfo::productVersion());
    config.applicationVersion = QStringLiteral(BEYGRAM_VERSION_STRING);

    if (!AppPaths::ensureDirectory(config.databaseDirectory, &pathError)
        || !AppPaths::ensureDirectory(config.filesDirectory, &pathError)) {
        return Result<telegram::TelegramConfig>::fail(ErrorCode::Io, pathError);
    }

    return Result<telegram::TelegramConfig>::ok(config);
}

void AppController::rebuildSession(bool autoStart)
{
    if (m_rebuilding)
        return;
    m_rebuilding = true;

    if (m_session) {
        resetRepository();
        m_session->shutdown(3000);
        m_session.reset();
    }

    m_startupError.clear();
    m_authError.clear();
    m_authErrorCode = 0;
    m_accountDisplayName.clear();
    m_accountUsername.clear();
    setBusy(false);

    auto prepared = prepareConfig();
    if (!prepared.hasValue()) {
        m_startupError = prepared.error().message;
        m_tdlibAvailable = false;
        m_tdlibPath.clear();
        m_tdlibDetail = m_startupError;
        m_rebuilding = false;
        notifyChanged();
        return;
    }

    m_session = std::make_unique<telegram::TelegramSession>(prepared.take());
    connectSession();

    QString detail;
    m_tdlibAvailable = m_session->probeTdlib(&detail);
    m_tdlibDetail = detail;
    m_tdlibPath = m_session->tdlibPath();

    if (autoStart)
        m_session->start();

    m_rebuilding = false;
    notifyChanged();
}

void AppController::connectSession()
{
    if (!m_session)
        return;

    connect(m_session.get(), &telegram::TelegramSession::stageChanged, this,
            [this](telegram::AuthStage) { onSessionStageChanged(); });
    connect(m_session.get(), &telegram::TelegramSession::connectionStateChanged, this,
            [this] { if (!m_rebuilding) notifyChanged(); });
    connect(m_session.get(), &telegram::TelegramSession::errorOccurred, this,
            [this](const QString& message) {
                if (m_rebuilding)
                    return;
                m_authError = message;
                m_authErrorCode = m_session ? m_session->lastErrorCode() : 0;
                setBusy(false);
                if (m_logoutRequested && m_session
                    && m_session->stage() != telegram::AuthStage::Closed) {
                    m_logoutRequested = false;
                }
                notifyChanged();
            });
    connect(m_session.get(), &telegram::TelegramSession::qrCodeChanged, this, [this] {
        if (m_rebuilding)
            return;
        emit qrCodeChanged();
        emit qrChanged();
        notifyChanged();
    });
    connect(m_session.get(), &telegram::TelegramSession::authRequestFinished, this, [this] {
        if (!m_rebuilding)
            setBusy(false);
    });
}

void AppController::onSessionStageChanged()
{
    if (m_rebuilding || !m_session)
        return;

    setBusy(false);
    m_authError.clear();
    m_authErrorCode = 0;

    if (m_session->stage() == telegram::AuthStage::Ready) {
        ensureRepository();
        requestAccountProfile();
        (void)m_registry.touch(m_accountId);
    } else if (m_session->stage() == telegram::AuthStage::Closed) {
        const bool loggedOut = m_logoutRequested;
        m_logoutRequested = false;
        resetRepository();
        if (loggedOut) {
            QTimer::singleShot(0, this, [this] { rebuildSession(true); });
            return;
        }
    } else if (m_session->stage() == telegram::AuthStage::Idle) {
        resetRepository();
    }

    notifyChanged();
}

void AppController::notifyChanged()
{
    emit stateChanged();
}

void AppController::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    if (m_busyTimer) {
        if (busy)
            m_busyTimer->start(30000);
        else
            m_busyTimer->stop();
    }
    emit stateChanged();
}

void AppController::clearAuthErrorState()
{
    m_authError.clear();
    m_authErrorCode = 0;
    if (m_session)
        m_session->clearError();
}

void AppController::requestAccountProfile()
{
    if (!m_session)
        return;
    m_session->sendRequest(
        QJsonObject{{QStringLiteral("@type"), QStringLiteral("getMe")}},
        [this](const QJsonObject& response) {
            if (response.value(QStringLiteral("@type")).toString()
                != QLatin1String("user")) {
                return;
            }
            m_accountDisplayName = fullName(response);
            m_accountUsername = username(response);
            if (m_accountDisplayName.isEmpty())
                m_accountDisplayName = QStringLiteral("Telegram account");
            notifyChanged();
        },
        15000);
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

QString AppController::tdlibPath() const
{
    return m_tdlibPath;
}

QString AppController::tdlibDetail() const
{
    return m_tdlibDetail;
}

QString AppController::startupError() const
{
    return m_startupError;
}

QString AppController::proxySummary() const
{
    if (!m_session)
        return QStringLiteral("Direct connection");
    return m_session->config().hasProxy() ? m_session->config().proxy.summary()
                                          : QStringLiteral("Direct connection");
}

bool AppController::credentialsConfigured() const
{
    return m_credentialsConfigured;
}

bool AppController::credentialsFromEnvironment() const
{
    return m_credentialsSource == QLatin1String("environment");
}

bool AppController::credentialsFromStore() const
{
    return m_credentialsSource == QLatin1String("DPAPI");
}

bool AppController::credentialsFromBuild() const
{
    return m_credentialsSource == QLatin1String("build");
}

QString AppController::credentialsSource() const
{
    return m_credentialsSource;
}

QString AppController::credentialsError() const
{
    return m_credentialsError;
}

QString AppController::credentialsHint() const
{
    if (m_credentialsSource == QLatin1String("environment")) {
        return QStringLiteral(
            "API credentials come from BEYGRAM_TELEGRAM_API_ID and "
            "BEYGRAM_TELEGRAM_API_HASH. Environment variables take precedence over the "
            "DPAPI store and build-time defaults.");
    }
    if (m_credentialsSource == QLatin1String("build")) {
        return QStringLiteral(
            "This local test build embeds api_id/api_hash from secrets.properties. "
            "They are not written to logs or the repository, but they can be extracted "
            "from this executable: do not distribute it.");
    }
    if (m_credentialsSource == QLatin1String("DPAPI")) {
        return QStringLiteral(
            "API credentials are stored with Windows DPAPI for this Windows user. The hash "
            "is never shown or logged.");
    }
    return QStringLiteral(
        "Telegram requires an api_id and api_hash from my.telegram.org. Phase 1 stores them "
        "with Windows DPAPI; they are never written to the log or the repository.");
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
    return m_session && !m_session->isRunning() && m_tdlibAvailable;
}

bool AppController::busy() const
{
    return m_busy;
}

bool AppController::loggingOut() const
{
    return m_session && m_session->isLoggingOut();
}

QString AppController::stageName() const
{
    if (!m_startupError.isEmpty())
        return QStringLiteral("startup_error");
    return m_session ? telegram::authStageName(m_session->stage()) : QStringLiteral("idle");
}

QString AppController::stageTitle() const
{
    if (!m_startupError.isEmpty())
        return QStringLiteral("The local account could not be prepared");
    return m_session ? telegram::authStageTitle(m_session->stage())
                     : QStringLiteral("Telegram core is stopped");
}

QString AppController::stageDetail() const
{
    if (!m_startupError.isEmpty())
        return m_startupError;
    return m_session ? m_session->stageDetail() : QString();
}

QString AppController::connectionTitle() const
{
    return m_session ? m_session->connectionStateTitle() : QStringLiteral("Not connected");
}

QString AppController::lastError() const
{
    if (!m_startupError.isEmpty())
        return m_startupError;
    return m_session ? m_session->lastError() : QString();
}

QString AppController::authError() const
{
    return m_authError;
}

int AppController::authErrorCode() const
{
    return m_authErrorCode;
}

int AppController::codeLength() const
{
    return m_session ? m_session->codeLength() : 0;
}

QString AppController::codePhoneNumber() const
{
    return m_session ? m_session->codePhoneNumber() : QString();
}

QString AppController::codeTypeName() const
{
    return m_session ? m_session->codeTypeName() : QString();
}

int AppController::emailCodeLength() const
{
    return m_session ? m_session->emailCodeLength() : 0;
}

QString AppController::emailPattern() const
{
    return m_session ? m_session->emailPattern() : QString();
}

QString AppController::passwordHint() const
{
    return m_session ? m_session->passwordHint() : QString();
}

bool AppController::passwordHasRecoveryEmail() const
{
    return m_session && m_session->passwordHasRecoveryEmail();
}

QString AppController::registrationTerms() const
{
    return m_session ? m_session->registrationTerms() : QString();
}

int AppController::registrationMinUserAge() const
{
    return m_session ? m_session->registrationMinUserAge() : 16;
}

int AppController::qrRevision() const
{
    return m_session ? m_session->qrRevision() : 0;
}

QString AppController::accountDisplayName() const
{
    return m_accountDisplayName;
}

QString AppController::accountUsername() const
{
    return m_accountUsername;
}

void AppController::ensureRepository()
{
    if (m_repository || !m_session)
        return;

    m_repository = std::make_unique<telegram::TelegramRepository>(
        m_session.get(), &m_chatListModel, &m_messageModel, &m_mediaStore, this);
    m_mediaStore.setSession(m_session.get());
    connect(m_repository.get(), &telegram::TelegramRepository::stateChanged, this,
            [this] { emit chatStateChanged(); });
    connect(m_repository.get(), &telegram::TelegramRepository::currentChatChanged, this,
            [this] { emit chatStateChanged(); });
    connect(m_repository.get(), &telegram::TelegramRepository::errorOccurred, this,
            [this](const QString& message) {
                m_chatError = message;
                emit chatStateChanged();
            });
    connect(m_repository.get(), &telegram::TelegramRepository::initialHistoryLoaded, this,
            [this] { emit initialHistoryLoaded(); });
    connect(m_repository.get(), &telegram::TelegramRepository::notificationRequested, this,
            &AppController::showNotification);
    connect(m_repository.get(), &telegram::TelegramRepository::profileLoaded, this,
            [this](const telegram::UserProfile& profile) {
                m_profile = profile;
                m_profileLoading = false;
                m_profileVisible = true;
                emit profileChanged();
            });
    connect(m_repository.get(), &telegram::TelegramRepository::pinnedMessageChanged, this,
            [this](qint64 messageId, const QString& preview) {
                m_pinnedMessageId = messageId;
                m_pinnedPreview = preview;
                emit pinnedChanged();
            });
    connect(m_repository.get(), &telegram::TelegramRepository::typingChanged, this,
            [this](qint64 chatId, const QString& label) {
                if (chatId != currentChatId())
                    return;
                m_typingText = label;
                emit typingChanged();
            });
    m_repository->start();
}

void AppController::showNotification(const QString& title, const QString& body)
{
    const NotificationSettings settings = m_notificationSettings.settings();
    if (!settings.enabled || title.isEmpty() || body.isEmpty()
        || !QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }
    if (!m_tray) {
        m_tray = new QSystemTrayIcon(this);
        m_tray->setIcon(QApplication::style()->standardIcon(
            QStyle::SP_MessageBoxInformation));
        m_tray->setToolTip(QStringLiteral("BeyGram Windows"));
        m_tray->show();
    }
    m_tray->showMessage(title, settings.showPreview ? body : QStringLiteral("New message"),
                        QSystemTrayIcon::Information, 6000);
}

void AppController::resetRepository()
{
    m_mediaStore.setSession(nullptr);
    m_repository.reset();
    m_chatListModel.clear();
    m_messageModel.clear();
    m_chatError.clear();
    m_replyToMessageId = 0;
    m_replyChatId = 0;
    m_replyPreview.clear();
    emit replyStateChanged();
    m_profile = {};
    m_profileVisible = false;
    m_profileLoading = false;
    emit profileChanged();
    emit chatStateChanged();
}

QAbstractItemModel* AppController::chatList()
{
    return &m_chatFilterModel;
}

QAbstractItemModel* AppController::messages()
{
    return &m_messageModel;
}

QAbstractItemModel* AppController::searchResults()
{
    return m_repository ? m_repository->searchModel() : nullptr;
}

bool AppController::searchLoading() const
{
    return m_repository && m_repository->searchLoading();
}

QAbstractItemModel* AppController::chatFolders()
{
    return m_repository ? m_repository->folderModel() : nullptr;
}

qint64 AppController::currentFolderId() const
{
    return m_repository ? m_repository->currentFolderId() : 0;
}

QAbstractItemModel* AppController::globalSearchResults()
{
    return m_repository ? m_repository->globalSearchModel() : nullptr;
}

bool AppController::globalSearchLoading() const
{
    return m_repository && m_repository->globalSearchLoading();
}

void AppController::searchGlobal(const QString& query)
{
    if (m_repository)
        m_repository->searchGlobal(query);
}

void AppController::loadMoreGlobalSearch()
{
    if (m_repository)
        m_repository->loadMoreGlobal();
}

void AppController::clearGlobalSearch()
{
    if (m_repository)
        m_repository->searchGlobal(QString());
}

qint64 AppController::currentChatId() const
{
    return m_repository ? m_repository->currentChatId() : 0;
}

QString AppController::currentChatTitle() const
{
    return m_repository ? m_repository->currentChatTitle() : QString();
}

QString AppController::currentChatSubtitle() const
{
    return m_repository ? m_repository->currentChatSubtitle() : QString();
}

int AppController::currentChatPhotoFileId() const
{
    return m_repository ? m_repository->currentChatPhotoFileId() : 0;
}

bool AppController::currentChatPhotoReady() const
{
    return m_repository && m_repository->currentChatPhotoReady();
}

int AppController::currentChatPhotoRevision() const
{
    return m_repository ? m_repository->currentChatPhotoRevision() : 0;
}

telegram::MediaFileStore* AppController::mediaStore()
{
    return &m_mediaStore;
}

MediaViewerController* AppController::mediaViewer()
{
    return &m_mediaViewer;
}

MediaPlaybackController* AppController::playback()
{
    return &m_playback;
}

void AppController::openMediaPlayback(int fileId, const QString& title)
{
    if (fileId <= 0)
        return;

    m_mediaStore.trackFileId(fileId, 32);
    const QString path = m_mediaStore.path(fileId);
    if (!path.isEmpty()) {
        m_playback.open(QUrl::fromLocalFile(path), title);
        return;
    }

    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(
        &m_mediaStore, &telegram::MediaFileStore::fileReady, this,
        [this, fileId, title, connection](int readyId) {
            if (readyId != fileId)
                return;
            QObject::disconnect(*connection);
            const QString readyPath = m_mediaStore.path(fileId);
            if (!readyPath.isEmpty())
                m_playback.open(QUrl::fromLocalFile(readyPath), title);
        });
}

bool AppController::saveOpenMediaAs(const QUrl& target)
{
    return m_mediaViewer.saveAs(target.toLocalFile());
}

bool AppController::chatLoading() const
{
    return m_repository && m_repository->loadingChats();
}

bool AppController::historyLoading() const
{
    return m_repository && m_repository->historyLoading();
}

bool AppController::hasMoreHistory() const
{
    return m_repository && m_repository->hasMoreHistory();
}

QString AppController::chatError() const
{
    return m_chatError;
}

QString AppController::chatFilter() const
{
    return m_chatFilterModel.filterText();
}

void AppController::setChatFilter(const QString& filter)
{
    m_chatFilterModel.setFilterText(filter);
    emit chatFilterChanged();
}

qint64 AppController::replyToMessageId() const
{
    return m_replyToMessageId;
}

QString AppController::replyPreview() const
{
    return m_replyPreview;
}

bool AppController::profileVisible() const
{
    return m_profileVisible;
}

bool AppController::profileLoading() const
{
    return m_profileLoading;
}

QString AppController::profileName() const
{
    return m_profile.displayName;
}

QString AppController::profileUsername() const
{
    return m_profile.username;
}

QString AppController::profileBio() const
{
    return m_profile.bio;
}

QString AppController::profilePhone() const
{
    return m_profile.phoneNumber;
}

int AppController::profilePhotoFileId() const
{
    return m_profile.photoFileId;
}

bool AppController::profileIsGroup() const
{
    return m_profile.isGroup;
}

bool AppController::profileIsChannel() const
{
    return m_profile.isChannel;
}

int AppController::profileMemberCount() const
{
    return m_profile.memberCount;
}

bool AppController::notificationsEnabled() const
{
    return m_notificationSettings.settings().enabled;
}

bool AppController::notificationPreview() const
{
    return m_notificationSettings.settings().showPreview;
}

qint64 AppController::pinnedMessageId() const
{
    return m_pinnedMessageId;
}

QString AppController::pinnedPreview() const
{
    return m_pinnedPreview;
}

bool AppController::pinnedVisible() const
{
    return m_pinnedMessageId != 0;
}

QString AppController::typingText() const
{
    return m_typingText;
}

void AppController::notifyTyping()
{
    if (m_repository)
        m_repository->sendTyping();
}

void AppController::unpinPinnedMessage()
{
    if (m_repository)
        m_repository->unpinPinnedMessage();
}

void AppController::setNotificationsEnabled(bool enabled)
{
    m_notificationSettings.setEnabled(enabled);
    emit notificationSettingsChanged();
}

void AppController::setNotificationPreview(bool enabled)
{
    m_notificationSettings.setShowPreview(enabled);
    emit notificationSettingsChanged();
}

void AppController::openChat(qint64 chatId)
{
    m_chatError.clear();
    if (m_profileVisible) {
        m_profileVisible = false;
        m_profileLoading = false;
        emit profileChanged();
    }
    if (!m_typingText.isEmpty()) {
        m_typingText.clear();
        emit typingChanged();
    }
    if (chatId != m_replyChatId) {
        m_replyToMessageId = 0;
        m_replyPreview.clear();
        m_replyChatId = chatId;
        emit replyStateChanged();
    }
    if (m_repository)
        m_repository->openChat(chatId);
    emit chatStateChanged();
}

void AppController::closeChat()
{
    if (m_replyToMessageId != 0 || !m_replyPreview.isEmpty()) {
        m_replyToMessageId = 0;
        m_replyPreview.clear();
        emit replyStateChanged();
    }
    m_replyChatId = 0;
    if (m_repository)
        m_repository->closeChat();
    if (!m_typingText.isEmpty()) {
        m_typingText.clear();
        emit typingChanged();
    }
    emit chatStateChanged();
}

void AppController::loadOlderMessages()
{
    if (m_repository)
        m_repository->loadOlder();
}

void AppController::searchInChat(const QString& query)
{
    if (m_repository)
        m_repository->searchInChat(query);
}

void AppController::clearSearch()
{
    if (m_repository)
        m_repository->clearSearch();
}

void AppController::selectFolder(qint64 folderId)
{
    if (m_repository)
        m_repository->selectFolder(folderId);
    emit chatStateChanged();
}

QString AppController::draftText(qint64 chatId) const
{
    return m_drafts.draft(chatId).text;
}

qint64 AppController::draftReplyToMessageId(qint64 chatId) const
{
    return m_drafts.draft(chatId).replyToMessageId;
}

void AppController::saveDraft(qint64 chatId, const QString& text, qint64 replyToMessageId)
{
    if (chatId == 0)
        return;
    m_drafts.setDraft(chatId, text, replyToMessageId);
}

void AppController::clearDraft(qint64 chatId)
{
    if (chatId == 0)
        return;
    m_drafts.clearDraft(chatId);
}

void AppController::sendChatMessage(const QString& text)
{
    m_chatError.clear();
    const qint64 replyTo = m_replyToMessageId;
    if (m_repository)
        m_repository->sendText(text, replyTo);
    if (replyTo != 0 || !m_replyPreview.isEmpty()) {
        m_replyToMessageId = 0;
        m_replyPreview.clear();
        emit replyStateChanged();
    }
    emit chatStateChanged();
}

bool AppController::sendChatMedia(const QUrl& file, const QString& caption)
{
    m_chatError.clear();
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (path.isEmpty() || !m_repository) {
        m_chatError = QStringLiteral("Choose a local file to send");
        emit chatStateChanged();
        return false;
    }

    telegram::OutgoingMediaKind kind;
    if (telegram::isSupportedMediaPath(path, telegram::OutgoingMediaKind::Photo))
        kind = telegram::OutgoingMediaKind::Photo;
    else if (telegram::isSupportedMediaPath(path, telegram::OutgoingMediaKind::Video))
        kind = telegram::OutgoingMediaKind::Video;
    else if (telegram::isSupportedMediaPath(path, telegram::OutgoingMediaKind::Document))
        kind = telegram::OutgoingMediaKind::Document;
    else {
        m_chatError = QStringLiteral("Unsupported or missing media file");
        emit chatStateChanged();
        return false;
    }

    const qint64 replyTo = m_replyToMessageId;
    m_repository->sendMedia(kind, path, caption, replyTo);
    if (replyTo != 0 || !m_replyPreview.isEmpty()) {
        m_replyToMessageId = 0;
        m_replyPreview.clear();
        emit replyStateChanged();
    }
    emit chatStateChanged();
    return true;
}

void AppController::beginReply(qint64 messageId, const QString& preview)
{
    if (messageId == 0)
        return;
    m_replyToMessageId = messageId;
    m_replyChatId = m_repository ? m_repository->currentChatId() : m_replyChatId;
    m_replyPreview = preview;
    emit replyStateChanged();
}

void AppController::cancelReply()
{
    if (m_replyToMessageId == 0 && m_replyPreview.isEmpty())
        return;
    m_replyToMessageId = 0;
    m_replyPreview.clear();
    emit replyStateChanged();
}

void AppController::requestProfile(qint64 chatId)
{
    if (chatId == 0)
        return;
    m_profileLoading = true;
    m_profileVisible = true;
    emit profileChanged();
    if (m_repository)
        m_repository->requestProfile(chatId);
}

void AppController::closeProfile()
{
    if (!m_profileVisible)
        return;
    m_profileVisible = false;
    m_profileLoading = false;
    emit profileChanged();
}

void AppController::editChatMessage(qint64 messageId, const QString& text)
{
    m_chatError.clear();
    if (m_repository)
        m_repository->editMessage(messageId, text);
    emit chatStateChanged();
}

void AppController::deleteChatMessage(qint64 messageId)
{
    m_chatError.clear();
    if (m_repository)
        m_repository->deleteMessage(messageId);
    emit chatStateChanged();
}

void AppController::sendReaction(qint64 chatId, qint64 messageId, const QString& emoji)
{
    m_chatError.clear();
    if (m_repository)
        m_repository->sendReaction(chatId, messageId, emoji);
    emit chatStateChanged();
}

void AppController::forwardMessage(qint64 fromChatId, qint64 messageId, qint64 toChatId)
{
    m_chatError.clear();
    if (m_repository)
        m_repository->forwardMessages(toChatId, fromChatId, {messageId});
    emit chatStateChanged();
}

void AppController::copyText(const QString& text)
{
    if (QGuiApplication::instance() != nullptr
        && !text.isEmpty()) {
        QGuiApplication::clipboard()->setText(text);
    }
}

void AppController::clearChatError()
{
    m_chatError.clear();
    emit chatStateChanged();
}

void AppController::requestChatPhoto(int fileId)
{
    m_mediaStore.trackFileId(fileId, 8);
}

void AppController::openLink(const QString& url)
{
    const QUrl target(url);
    const QString scheme = target.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https")
        && scheme != QLatin1String("tg") && scheme != QLatin1String("mailto")) {
        return;
    }
    if (target.isValid())
        QDesktopServices::openUrl(target);
}

QString AppController::bgMediaTitle() const
{
    return media::bgMediaStatus().title;
}

QString AppController::bgMediaDetail() const
{
    return media::bgMediaStatus().detail;
}

QString AppController::qrLoginLink() const
{
    return m_session ? m_session->qrLoginLink() : QString();
}

void AppController::refresh()
{
    if (m_session) {
        QString detail;
        m_tdlibAvailable = m_session->probeTdlib(&detail);
        m_tdlibDetail = detail;
        m_tdlibPath = m_session->tdlibPath();
    }
    notifyChanged();
}

void AppController::startTelegram()
{
    if (!m_session) {
        rebuildSession(true);
        return;
    }
    if (!m_session->isRunning())
        m_session->start();
    notifyChanged();
}

void AppController::retrySession()
{
    clearAuthErrorState();
    if (!m_session || !m_session->isRunning()) {
        startTelegram();
        return;
    }
    m_session->restartSession();
    notifyChanged();
}

void AppController::stopTelegram()
{
    if (m_session)
        m_session->requestClose();
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
          << QStringLiteral("TDLib path: %1").arg(tdlibPath())
          << QStringLiteral("TDLib detail: %1").arg(tdlibDetail())
          << QStringLiteral("Credentials configured: %1")
                 .arg(credentialsConfigured() ? "yes" : "no")
          << QStringLiteral("Credentials source: %1")
                 .arg(m_credentialsSource.isEmpty() ? QStringLiteral("none")
                                                    : m_credentialsSource)
          << QStringLiteral("Proxy: %1")
                 .arg(m_session && m_session->config().hasProxy()
                          ? m_session->config().proxy.summary()
                          : QStringLiteral("direct"))
          << QStringLiteral("Account id: %1").arg(m_accountId)
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

void AppController::submitPhone(const QString& phoneNumber)
{
    if (!m_session)
        return;
    clearAuthErrorState();
    setBusy(true);
    m_session->submitPhoneNumber(phoneNumber);
}

void AppController::submitCode(const QString& code)
{
    if (!m_session)
        return;
    clearAuthErrorState();
    setBusy(true);
    m_session->submitCode(code);
}

void AppController::submitPassword(const QString& password)
{
    if (!m_session)
        return;
    clearAuthErrorState();
    setBusy(true);
    m_session->submitPassword(password);
}

void AppController::submitEmailAddress(const QString& emailAddress)
{
    if (!m_session)
        return;
    clearAuthErrorState();
    setBusy(true);
    m_session->submitEmailAddress(emailAddress);
}

void AppController::submitEmailCode(const QString& code)
{
    if (!m_session)
        return;
    clearAuthErrorState();
    setBusy(true);
    m_session->submitEmailCode(code);
}

void AppController::submitRegistration(const QString& firstName, const QString& lastName)
{
    if (!m_session)
        return;
    clearAuthErrorState();
    setBusy(true);
    m_session->submitRegistration(firstName, lastName);
}

void AppController::resendCode()
{
    if (!m_session)
        return;
    clearAuthErrorState();
    setBusy(true);
    m_session->resendCode();
}

void AppController::requestQrLogin()
{
    if (!m_session)
        return;
    clearAuthErrorState();
    setBusy(true);
    m_session->requestQrCodeLogin();
}

void AppController::openQrLoginInTelegram()
{
    if (!m_session)
        return;
    const QString link = m_session->qrLoginLink();
    if (link.startsWith(QLatin1String("tg://")) || link.startsWith(QLatin1String("https://")))
        QDesktopServices::openUrl(QUrl(link));
}

void AppController::usePhoneInstead()
{
    clearAuthErrorState();
    if (m_session)
        m_session->restartSession();
    notifyChanged();
}

void AppController::clearAuthError()
{
    clearAuthErrorState();
    notifyChanged();
}

void AppController::logout()
{
    if (!m_session || !m_session->isReady())
        return;
    m_logoutRequested = true;
    m_session->requestLogout();
    notifyChanged();
}

bool AppController::saveCredentials(const QString& apiId, const QString& apiHash)
{
    bool numberOk = false;
    const int id = apiId.trimmed().toInt(&numberOk);
    const QString hash = apiHash.trimmed();

    if (!numberOk || id <= 0) {
        m_credentialsError = QStringLiteral("API id must be a positive integer.");
        notifyChanged();
        return false;
    }
    if (!telegram::CredentialStore::isValidApiHash(hash)) {
        m_credentialsError = QStringLiteral("API hash must be exactly 32 hexadecimal characters.");
        notifyChanged();
        return false;
    }

    telegram::TelegramCredentials credentials;
    credentials.apiId = id;
    credentials.apiHash = hash;
    const Status saved = m_credentials.save(credentials);
    if (!saved.ok()) {
        m_credentialsError = saved.error().message;
        notifyChanged();
        return false;
    }

    m_credentialsError.clear();
    rebuildSession(true);
    return true;
}

void AppController::clearStoredCredentials()
{
    (void)m_credentials.clear();
    rebuildSession(true);
}

void AppController::shutdown()
{
    if (m_session)
        m_session->shutdown(5000);
    resetRepository();
}

} // namespace beygram::ui
