#pragma once

#include "ChatListModel.h"
#include "MessageListModel.h"
#include "TelegramTypes.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QVector>

#include <functional>

namespace beygram::telegram {

class TelegramSession;
class MediaFileStore;

/// Phase 2 repository slice: main chat list, text history, sending, editing,
/// deleting and read receipts. Media thumbnails are downloaded for avatars and
/// bubbles; full media playback/viewing arrives later.
class TelegramRepository final : public QObject {
    Q_OBJECT

public:
    TelegramRepository(TelegramSession* session, ChatListModel* chatModel,
                       MessageListModel* messageModel, MediaFileStore* mediaStore,
                       QObject* parent = nullptr);

    void start();
    void openChat(qint64 chatId);
    void closeChat();
    void loadOlder();
    void sendText(const QString& text, qint64 replyToMessageId = 0);
    void editMessage(qint64 messageId, const QString& text);
    void deleteMessage(qint64 messageId);
    void markCurrentChatRead();

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

signals:
    void currentChatChanged();
    void stateChanged();
    void errorOccurred(const QString& message);
    void initialHistoryLoaded();
    void notificationRequested(const QString& title, const QString& body);

private:
    void handleUpdate(const QJsonObject& update);
    void requestChats();
    void rebuildChatList();
    void requestHistory(qint64 chatId, qint64 fromMessageId, bool older);
    void handleHistory(const QJsonObject& response, qint64 chatId, bool older);
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
    bool m_hasMoreHistory = true;
    bool m_loadingChats = false;
    bool m_started = false;
    QString m_error;
};

} // namespace beygram::telegram
