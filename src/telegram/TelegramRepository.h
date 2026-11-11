#pragma once

#include "ChatListModel.h"
#include "MessageListModel.h"
#include "SearchResultModel.h"
#include "TelegramTypes.h"
#include "UserProfile.h"
#include "TelegramUpdateScheduler.h"

#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QSet>
#include <QVector>

#include <functional>
#include <memory>

class QThread;
class QTimer;

namespace beygram::telegram {

class TelegramSession;
class MediaFileStore;
class TelegramParseWorker;

/// Phase 2 repository slice: main chat list, text history, sending, editing,
/// deleting and read receipts. Media thumbnails are downloaded for avatars and
/// bubbles; full media playback/viewing arrives later.
class TelegramRepository final : public QObject {
    Q_OBJECT

public:
    TelegramRepository(TelegramSession* session, ChatListModel* chatModel,
                       MessageListModel* messageModel, MediaFileStore* mediaStore,
                       QObject* parent = nullptr);
    ~TelegramRepository() override;

    void start();
    void openChat(qint64 chatId);
    void closeChat();
    void loadOlder();
    void sendText(const QString& text, qint64 replyToMessageId = 0);
    void editMessage(qint64 messageId, const QString& text);
    void deleteMessage(qint64 messageId);
    void markCurrentChatRead();
    void searchInChat(const QString& query);
    void clearSearch();
    void requestProfile(qint64 chatId);

    [[nodiscard]] qint64 currentChatId() const { return m_currentChatId; }
    [[nodiscard]] QString currentChatTitle() const { return m_currentChatTitle; }
    [[nodiscard]] QString currentChatSubtitle() const { return m_currentChatSubtitle; }
    [[nodiscard]] bool historyLoading() const { return m_historyLoading; }
    [[nodiscard]] bool hasMoreHistory() const { return m_hasMoreHistory; }
    [[nodiscard]] bool loadingChats() const { return m_loadingChats; }
    [[nodiscard]] QString error() const { return m_error; }
    [[nodiscard]] int currentChatPhotoFileId() const { return m_currentChatPhotoFileId; }
    [[nodiscard]] bool currentChatPhotoReady() const { return m_currentChatPhotoReady; }
    [[nodiscard]] int currentChatPhotoRevision() const { return m_currentChatPhotoRevision; }
    [[nodiscard]] SearchResultModel* searchModel() { return &m_searchModel; }
    [[nodiscard]] bool searchLoading() const { return m_searchLoading; }

signals:
    void currentChatChanged();
    void stateChanged();
    void errorOccurred(const QString& message);
    void initialHistoryLoaded();
    void notificationRequested(const QString& title, const QString& body);
    void profileLoaded(const UserProfile& profile);

    // Internal request signals: they carry plain JSON to the parse worker.
    void parseChatRequested(quint64 jobId, qint64 chatId, const QJsonObject& chat);
    void parseHistoryRequested(quint64 jobId, qint64 chatId, bool older,
                               const QJsonArray& messages);

private:
    void handleUpdate(const QJsonObject& update);
    void processUpdate(const QJsonObject& update);
    void processUpdates(const QVector<QJsonObject>& updates);
    void requestChats();
    void rebuildChatList();
    void rebuildChatListNow();
    void requestHistory(qint64 chatId, qint64 fromMessageId, bool older);
    void handleHistory(const QJsonObject& response, qint64 chatId, bool older,
                       quint64 jobId);
    void applyHistory(quint64 jobId, qint64 chatId, bool older,
                      const QVector<MessageData>& parsed);
    void applyParsedChat(quint64 jobId, qint64 chatId, const ChatData& chat);
    void enqueueChatParse(qint64 chatId, const QJsonObject& chat);
    void updateChatFromMessage(qint64 chatId, const QJsonObject& message);
    void trackMessageMedia(MessageData& message);
    void resolveSender(MessageData& message);
    void requestUser(qint64 userId);
    void setError(const QString& message);
    void sendRequest(const QJsonObject& request,
                     std::function<void(const QJsonObject&)> handler,
                     const QString& description, int timeoutMs = 30000);

    TelegramSession* m_session = nullptr;
    ChatListModel* m_chatModel = nullptr;
    MessageListModel* m_messageModel = nullptr;
    MediaFileStore* m_mediaStore = nullptr;
    SearchResultModel m_searchModel;

    QHash<qint64, ChatData> m_chats;
    QSet<qint64> m_requestedChats;
    QHash<qint64, QString> m_userNames;
    QSet<qint64> m_requestedUsers;

    qint64 m_currentChatId = 0;
    QString m_currentChatTitle;
    QString m_currentChatSubtitle;
    QString m_currentChatType;
    qint64 m_oldestMessageId = 0;
    qint64 m_lastReadMessageId = 0;
    int m_currentChatPhotoFileId = 0;
    bool m_currentChatPhotoReady = false;
    int m_currentChatPhotoRevision = 0;
    bool m_historyLoading = false;
    bool m_searchLoading = false;
    bool m_hasMoreHistory = true;
    bool m_loadingChats = false;
    bool m_started = false;
    bool m_chatListRebuildPending = false;
    QString m_error;
    QString m_searchQuery;

    TelegramUpdateScheduler m_scheduler;
    QThread* m_parseThread = nullptr;
    TelegramParseWorker* m_parseWorker = nullptr;
    QTimer* m_chatListRebuildTimer = nullptr;
    quint64 m_nextParseJob = 0;
    quint64 m_historyJob = 0;
};

} // namespace beygram::telegram
