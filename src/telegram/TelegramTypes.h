#pragma once

#include <QJsonObject>
#include <QString>

namespace beygram::telegram {

struct ChatData {
    qint64 id = 0;
    QString title;
    QString type;
    QString preview;
    qint64 order = 0;
    qint64 lastMessageDate = 0;
    qint64 lastMessageId = 0;
    int unreadCount = 0;
    bool pinned = false;
    bool muted = false;
    bool inMainList = false;
    bool isChannel = false;
    int photoFileId = 0;
    bool photoReady = false;
    int photoRevision = 0;
    QString initials;
    int accentIndex = 0;
};

struct MessageData {
    qint64 id = 0;
    qint64 chatId = 0;
    qint64 date = 0;
    qint64 senderUserId = 0;
    qint64 replyToMessageId = 0;
    qint64 replyChatId = 0;
    QString text;
    QString textHtml;
    QString replyPreview;
    QString senderName;
    QString contentType;
    bool outgoing = false;
    bool edited = false;
    bool failed = false;
    bool pending = false;
    int mediaFileId = 0;
    int mediaFullFileId = 0;
    int mediaWidth = 0;
    int mediaHeight = 0;
    bool mediaReady = false;
    int mediaRevision = 0;
};

[[nodiscard]] qint64 jsonInt64(const QJsonValue& value);
[[nodiscard]] int photoThumbnailFileId(const QJsonObject& photo, int targetSize,
                                       int* width = nullptr, int* height = nullptr);
[[nodiscard]] int photoLargestFileId(const QJsonObject& photo);
[[nodiscard]] int messageThumbnailFileId(const QJsonObject& content,
                                          int* width = nullptr, int* height = nullptr);
[[nodiscard]] int messageFullFileId(const QJsonObject& content);
[[nodiscard]] QString messageContentText(const QJsonObject& content);
[[nodiscard]] QString messageContentLabel(const QJsonObject& content);
[[nodiscard]] QString chatPreview(const QJsonObject& lastMessage);
[[nodiscard]] ChatData chatFromJson(const QJsonObject& chat);
[[nodiscard]] MessageData messageFromJson(const QJsonObject& message);
void applyChatPosition(ChatData& chat, const QJsonObject& position);
[[nodiscard]] QString initialsForTitle(const QString& title);
[[nodiscard]] int accentIndexForId(qint64 id);

} // namespace beygram::telegram
