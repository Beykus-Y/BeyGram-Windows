#pragma once

#include "core/Result.h"
#include "telegram/AccountRegistry.h"
#include "telegram/ChatFilterModel.h"
#include "telegram/ChatListModel.h"
#include "telegram/CredentialStore.h"
#include "telegram/DraftStore.h"
#include "telegram/MediaFileStore.h"
#include "telegram/MediaSend.h"
#include "telegram/MessageListModel.h"
#include "telegram/UserProfile.h"
#include "ui/MediaViewerController.h"
#include "ui/MediaPlaybackController.h"
#include "ui/NotificationSettingsStore.h"
#include "telegram/TelegramConfig.h"

#include <QAbstractItemModel>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <memory>

class QTimer;
class QSystemTrayIcon;

namespace beygram::telegram {
class TelegramRepository;
class TelegramSession;
class MediaFileStore;
}

namespace beygram::ui {

/// UI-facing application state. It owns the account registry, credential store
/// and the current Telegram session, and exposes only derived, non-secret
/// facts. No QML object ever sends TDLib requests.
class AppController final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(QString buildInfo READ buildInfo CONSTANT)
    Q_PROPERTY(QString dataDirectory READ dataDirectory CONSTANT)
    Q_PROPERTY(QString logDirectory READ logDirectory CONSTANT)

    Q_PROPERTY(bool tdlibAvailable READ tdlibAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString tdlibPath READ tdlibPath NOTIFY stateChanged)
    Q_PROPERTY(QString tdlibDetail READ tdlibDetail NOTIFY stateChanged)
    Q_PROPERTY(QString startupError READ startupError NOTIFY stateChanged)
    Q_PROPERTY(QString proxySummary READ proxySummary NOTIFY stateChanged)

    Q_PROPERTY(bool credentialsConfigured READ credentialsConfigured NOTIFY stateChanged)
    Q_PROPERTY(bool credentialsFromEnvironment READ credentialsFromEnvironment NOTIFY stateChanged)
    Q_PROPERTY(bool credentialsFromStore READ credentialsFromStore NOTIFY stateChanged)
    Q_PROPERTY(bool credentialsFromBuild READ credentialsFromBuild NOTIFY stateChanged)
    Q_PROPERTY(QString credentialsSource READ credentialsSource NOTIFY stateChanged)
    Q_PROPERTY(QString credentialsError READ credentialsError NOTIFY stateChanged)
    Q_PROPERTY(QString credentialsHint READ credentialsHint NOTIFY stateChanged)

    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool canStart READ canStart NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool loggingOut READ loggingOut NOTIFY stateChanged)

    Q_PROPERTY(QString stageName READ stageName NOTIFY stateChanged)
    Q_PROPERTY(QString stageTitle READ stageTitle NOTIFY stateChanged)
    Q_PROPERTY(QString stageDetail READ stageDetail NOTIFY stateChanged)
    Q_PROPERTY(QString connectionTitle READ connectionTitle NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)
    Q_PROPERTY(QString authError READ authError NOTIFY stateChanged)
    Q_PROPERTY(int authErrorCode READ authErrorCode NOTIFY stateChanged)

    Q_PROPERTY(int codeLength READ codeLength NOTIFY stateChanged)
    Q_PROPERTY(QString codePhoneNumber READ codePhoneNumber NOTIFY stateChanged)
    Q_PROPERTY(QString codeTypeName READ codeTypeName NOTIFY stateChanged)
    Q_PROPERTY(int emailCodeLength READ emailCodeLength NOTIFY stateChanged)
    Q_PROPERTY(QString emailPattern READ emailPattern NOTIFY stateChanged)
    Q_PROPERTY(QString passwordHint READ passwordHint NOTIFY stateChanged)
    Q_PROPERTY(bool passwordHasRecoveryEmail READ passwordHasRecoveryEmail NOTIFY stateChanged)
    Q_PROPERTY(QString registrationTerms READ registrationTerms NOTIFY stateChanged)
    Q_PROPERTY(int registrationMinUserAge READ registrationMinUserAge NOTIFY stateChanged)
    Q_PROPERTY(int qrRevision READ qrRevision NOTIFY qrChanged)

    Q_PROPERTY(QString accountDisplayName READ accountDisplayName NOTIFY stateChanged)
    Q_PROPERTY(QString accountUsername READ accountUsername NOTIFY stateChanged)

    Q_PROPERTY(QAbstractItemModel* chatList READ chatList CONSTANT)
    Q_PROPERTY(QAbstractItemModel* messages READ messages CONSTANT)
    Q_PROPERTY(QAbstractItemModel* searchResults READ searchResults NOTIFY chatStateChanged)
    Q_PROPERTY(bool searchLoading READ searchLoading NOTIFY chatStateChanged)
    Q_PROPERTY(QAbstractItemModel* chatFolders READ chatFolders NOTIFY chatStateChanged)
    Q_PROPERTY(qint64 currentFolderId READ currentFolderId NOTIFY chatStateChanged)
    Q_PROPERTY(QAbstractItemModel* globalSearchResults READ globalSearchResults NOTIFY chatStateChanged)
    Q_PROPERTY(bool globalSearchLoading READ globalSearchLoading NOTIFY chatStateChanged)
    Q_PROPERTY(qint64 currentChatId READ currentChatId NOTIFY chatStateChanged)
    Q_PROPERTY(QString currentChatTitle READ currentChatTitle NOTIFY chatStateChanged)
    Q_PROPERTY(QString currentChatSubtitle READ currentChatSubtitle NOTIFY chatStateChanged)
    Q_PROPERTY(int currentChatPhotoFileId READ currentChatPhotoFileId NOTIFY chatStateChanged)
    Q_PROPERTY(bool currentChatPhotoReady READ currentChatPhotoReady NOTIFY chatStateChanged)
    Q_PROPERTY(int currentChatPhotoRevision READ currentChatPhotoRevision NOTIFY chatStateChanged)
    Q_PROPERTY(bool chatLoading READ chatLoading NOTIFY chatStateChanged)
    Q_PROPERTY(bool historyLoading READ historyLoading NOTIFY chatStateChanged)
    Q_PROPERTY(bool hasMoreHistory READ hasMoreHistory NOTIFY chatStateChanged)
    Q_PROPERTY(QString chatError READ chatError NOTIFY chatStateChanged)
    Q_PROPERTY(QString chatFilter READ chatFilter WRITE setChatFilter NOTIFY chatFilterChanged)
    Q_PROPERTY(qint64 replyToMessageId READ replyToMessageId NOTIFY replyStateChanged)
    Q_PROPERTY(QString replyPreview READ replyPreview NOTIFY replyStateChanged)

    Q_PROPERTY(bool profileVisible READ profileVisible NOTIFY profileChanged)
    Q_PROPERTY(bool profileLoading READ profileLoading NOTIFY profileChanged)
    Q_PROPERTY(QString profileName READ profileName NOTIFY profileChanged)
    Q_PROPERTY(QString profileUsername READ profileUsername NOTIFY profileChanged)
    Q_PROPERTY(QString profileBio READ profileBio NOTIFY profileChanged)
    Q_PROPERTY(QString profilePhone READ profilePhone NOTIFY profileChanged)
    Q_PROPERTY(int profilePhotoFileId READ profilePhotoFileId NOTIFY profileChanged)
    Q_PROPERTY(bool profileIsGroup READ profileIsGroup NOTIFY profileChanged)
    Q_PROPERTY(bool profileIsChannel READ profileIsChannel NOTIFY profileChanged)
    Q_PROPERTY(int profileMemberCount READ profileMemberCount NOTIFY profileChanged)
    Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled WRITE setNotificationsEnabled NOTIFY notificationSettingsChanged)
    Q_PROPERTY(bool notificationPreview READ notificationPreview WRITE setNotificationPreview NOTIFY notificationSettingsChanged)
    Q_PROPERTY(qint64 pinnedMessageId READ pinnedMessageId NOTIFY pinnedChanged)
    Q_PROPERTY(QString pinnedPreview READ pinnedPreview NOTIFY pinnedChanged)
    Q_PROPERTY(bool pinnedVisible READ pinnedVisible NOTIFY pinnedChanged)
    Q_PROPERTY(QString typingText READ typingText NOTIFY typingChanged)

    Q_PROPERTY(QString bgMediaTitle READ bgMediaTitle CONSTANT)
    Q_PROPERTY(QString bgMediaDetail READ bgMediaDetail CONSTANT)

public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    [[nodiscard]] QString appVersion() const;
    [[nodiscard]] QString qtVersion() const;
    [[nodiscard]] QString buildInfo() const;
    [[nodiscard]] QString dataDirectory() const;
    [[nodiscard]] QString logDirectory() const;

    [[nodiscard]] bool tdlibAvailable() const;
    [[nodiscard]] QString tdlibPath() const;
    [[nodiscard]] QString tdlibDetail() const;
    [[nodiscard]] QString startupError() const;
    [[nodiscard]] QString proxySummary() const;

    [[nodiscard]] bool credentialsConfigured() const;
    [[nodiscard]] bool credentialsFromEnvironment() const;
    [[nodiscard]] bool credentialsFromStore() const;
    [[nodiscard]] bool credentialsFromBuild() const;
    [[nodiscard]] QString credentialsSource() const;
    [[nodiscard]] QString credentialsError() const;
    [[nodiscard]] QString credentialsHint() const;

    [[nodiscard]] bool running() const;
    [[nodiscard]] bool ready() const;
    [[nodiscard]] bool canStart() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] bool loggingOut() const;

    [[nodiscard]] QString stageName() const;
    [[nodiscard]] QString stageTitle() const;
    [[nodiscard]] QString stageDetail() const;
    [[nodiscard]] QString connectionTitle() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] QString authError() const;
    [[nodiscard]] int authErrorCode() const;

    [[nodiscard]] int codeLength() const;
    [[nodiscard]] QString codePhoneNumber() const;
    [[nodiscard]] QString codeTypeName() const;
    [[nodiscard]] int emailCodeLength() const;
    [[nodiscard]] QString emailPattern() const;
    [[nodiscard]] QString passwordHint() const;
    [[nodiscard]] bool passwordHasRecoveryEmail() const;
    [[nodiscard]] QString registrationTerms() const;
    [[nodiscard]] int registrationMinUserAge() const;
    [[nodiscard]] int qrRevision() const;

    [[nodiscard]] QString accountDisplayName() const;
    [[nodiscard]] QString accountUsername() const;

    [[nodiscard]] QAbstractItemModel* chatList();
    [[nodiscard]] QAbstractItemModel* messages();
    [[nodiscard]] QAbstractItemModel* searchResults();
    [[nodiscard]] bool searchLoading() const;
    [[nodiscard]] QAbstractItemModel* chatFolders();
    [[nodiscard]] qint64 currentFolderId() const;
    [[nodiscard]] QAbstractItemModel* globalSearchResults();
    [[nodiscard]] bool globalSearchLoading() const;
    [[nodiscard]] qint64 currentChatId() const;
    [[nodiscard]] QString currentChatTitle() const;
    [[nodiscard]] QString currentChatSubtitle() const;
    [[nodiscard]] int currentChatPhotoFileId() const;
    [[nodiscard]] bool currentChatPhotoReady() const;
    [[nodiscard]] int currentChatPhotoRevision() const;
    [[nodiscard]] bool chatLoading() const;
    [[nodiscard]] bool historyLoading() const;
    [[nodiscard]] bool hasMoreHistory() const;
    [[nodiscard]] QString chatError() const;
    [[nodiscard]] QString chatFilter() const;
    void setChatFilter(const QString& filter);
    [[nodiscard]] qint64 replyToMessageId() const;
    [[nodiscard]] QString replyPreview() const;
    [[nodiscard]] bool profileVisible() const;
    [[nodiscard]] bool profileLoading() const;
    [[nodiscard]] QString profileName() const;
    [[nodiscard]] QString profileUsername() const;
    [[nodiscard]] QString profileBio() const;
    [[nodiscard]] QString profilePhone() const;
    [[nodiscard]] int profilePhotoFileId() const;
    [[nodiscard]] bool profileIsGroup() const;
    [[nodiscard]] bool profileIsChannel() const;
    [[nodiscard]] int profileMemberCount() const;
    [[nodiscard]] bool notificationsEnabled() const;
    [[nodiscard]] bool notificationPreview() const;
    [[nodiscard]] qint64 pinnedMessageId() const;
    [[nodiscard]] QString pinnedPreview() const;
    [[nodiscard]] bool pinnedVisible() const;
    [[nodiscard]] QString typingText() const;
    Q_INVOKABLE void unpinPinnedMessage();
    Q_INVOKABLE void notifyTyping();
    void setNotificationsEnabled(bool enabled);
    void setNotificationPreview(bool enabled);

    [[nodiscard]] QString bgMediaTitle() const;
    [[nodiscard]] QString bgMediaDetail() const;

    /// Secret QR link for the image provider only. Never expose this as a QML
    /// property and never log it.
    [[nodiscard]] QString qrLoginLink() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void startTelegram();
    Q_INVOKABLE void retrySession();
    Q_INVOKABLE void stopTelegram();
    Q_INVOKABLE void openDataFolder();
    Q_INVOKABLE void openLogFolder();
    Q_INVOKABLE QStringList recentLogs() const;
    Q_INVOKABLE QString diagnosticsText() const;
    Q_INVOKABLE void copyDiagnostics();

    // Authorization actions.
    Q_INVOKABLE void submitPhone(const QString& phoneNumber);
    Q_INVOKABLE void submitCode(const QString& code);
    Q_INVOKABLE void submitPassword(const QString& password);
    Q_INVOKABLE void submitEmailAddress(const QString& emailAddress);
    Q_INVOKABLE void submitEmailCode(const QString& code);
    Q_INVOKABLE void submitRegistration(const QString& firstName, const QString& lastName);
    Q_INVOKABLE void resendCode();
    Q_INVOKABLE void requestQrLogin();
    Q_INVOKABLE void openQrLoginInTelegram();
    Q_INVOKABLE void usePhoneInstead();
    Q_INVOKABLE void clearAuthError();
    Q_INVOKABLE void logout();

    // Phase 2 chat actions.
    Q_INVOKABLE void openChat(qint64 chatId);
    Q_INVOKABLE void closeChat();
    Q_INVOKABLE void loadOlderMessages();
    Q_INVOKABLE void searchInChat(const QString& query);
    Q_INVOKABLE void clearSearch();
    Q_INVOKABLE void selectFolder(qint64 folderId);
    Q_INVOKABLE void searchGlobal(const QString& query);
    Q_INVOKABLE void loadMoreGlobalSearch();
    Q_INVOKABLE void clearGlobalSearch();
    Q_INVOKABLE void sendChatMessage(const QString& text);
    Q_INVOKABLE bool sendChatMedia(const QUrl& file, const QString& caption = {});
    Q_INVOKABLE QString draftText(qint64 chatId) const;
    Q_INVOKABLE qint64 draftReplyToMessageId(qint64 chatId) const;
    Q_INVOKABLE void saveDraft(qint64 chatId, const QString& text,
                               qint64 replyToMessageId = 0);
    Q_INVOKABLE void clearDraft(qint64 chatId);
    Q_INVOKABLE void beginReply(qint64 messageId, const QString& preview);
    Q_INVOKABLE void cancelReply();
    Q_INVOKABLE void requestProfile(qint64 chatId);
    Q_INVOKABLE void closeProfile();
    Q_INVOKABLE void editChatMessage(qint64 messageId, const QString& text);
    Q_INVOKABLE void deleteChatMessage(qint64 messageId);
    Q_INVOKABLE void sendReaction(qint64 chatId, qint64 messageId, const QString& emoji);
    Q_INVOKABLE void forwardMessage(qint64 fromChatId, qint64 messageId, qint64 toChatId);
    Q_INVOKABLE void votePoll(qint64 chatId, qint64 messageId, int optionIndex);
    Q_INVOKABLE void copyText(const QString& text);
    Q_INVOKABLE void clearChatError();
    Q_INVOKABLE void requestChatPhoto(int fileId);
    Q_INVOKABLE void openLink(const QString& url);

    /// C++-only access for the QML image provider. Not exposed to QML.
    [[nodiscard]] telegram::MediaFileStore* mediaStore();
    [[nodiscard]] MediaViewerController* mediaViewer();
    [[nodiscard]] MediaPlaybackController* playback();
    Q_INVOKABLE bool saveOpenMediaAs(const QUrl& target);
    Q_INVOKABLE void openMediaPlayback(int fileId, const QString& title);

    // Credential provisioning (developer API id/hash).
    Q_INVOKABLE bool saveCredentials(const QString& apiId, const QString& apiHash);
    Q_INVOKABLE void clearStoredCredentials();

    /// Stops the TDLib receiver before Qt tears down the QML engine.
    void shutdown();

signals:
    void stateChanged();
    void qrChanged();
    void qrCodeChanged();
    void chatStateChanged();
    void chatFilterChanged();
    void replyStateChanged();
    void profileChanged();
    void notificationSettingsChanged();
    void pinnedChanged();
    void typingChanged();
    void initialHistoryLoaded();

private:
    void buildConfig();
    [[nodiscard]] Result<telegram::TelegramConfig> prepareConfig();
    void rebuildSession(bool autoStart);
    void connectSession();
    void notifyChanged();
    void setBusy(bool busy);
    void clearAuthErrorState();
    void requestAccountProfile();
    void onSessionStageChanged();
    void ensureRepository();
    void resetRepository();
    void showNotification(const QString& title, const QString& body);

    std::unique_ptr<telegram::TelegramSession> m_session;
    std::unique_ptr<telegram::TelegramRepository> m_repository;
    telegram::AccountRegistry m_registry;
    telegram::CredentialStore m_credentials;
    telegram::ChatListModel m_chatListModel;
    telegram::MessageListModel m_messageModel;
    telegram::ChatFilterModel m_chatFilterModel;
    telegram::MediaFileStore m_mediaStore;
    MediaViewerController m_mediaViewer;
    MediaPlaybackController m_playback;
    NotificationSettingsStore m_notificationSettings;
    telegram::DraftStore m_drafts;
    QTimer* m_busyTimer = nullptr;
    QSystemTrayIcon* m_tray = nullptr;

    QString m_accountId;
    QString m_tdlibDetail;
    QString m_tdlibPath;
    QString m_startupError;
    QString m_credentialsError;
    QString m_credentialsSource;
    QString m_authError;
    QString m_accountDisplayName;
    QString m_accountUsername;
    QString m_chatError;
    qint64 m_replyToMessageId = 0;
    qint64 m_replyChatId = 0;
    QString m_replyPreview;
    telegram::UserProfile m_profile;
    qint64 m_pinnedMessageId = 0;
    QString m_pinnedPreview;
    QString m_typingText;
    bool m_profileVisible = false;
    bool m_profileLoading = false;
    int m_authErrorCode = 0;
    bool m_tdlibAvailable = false;
    bool m_credentialsConfigured = false;
    bool m_busy = false;
    bool m_logoutRequested = false;
    bool m_rebuilding = false;
};

} // namespace beygram::ui
