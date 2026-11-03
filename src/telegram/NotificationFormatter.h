#pragma once

#include <QJsonObject>
#include <QString>

namespace beygram::telegram {

/// Everything a desktop notification needs for one incoming TDLib message.
/// When `shouldNotify` is false, `title` and `body` stay empty.
struct NotificationPayload {
    bool shouldNotify = false;
    qint64 chatId = 0;
    qint64 messageId = 0;
    QString title;
    QString body;
};

/// Decides whether a TDLib message should raise a desktop notification and
/// builds the text. `currentChatId` is the chat the user is looking at.
[[nodiscard]] NotificationPayload notificationForMessage(
    const QJsonObject& message,
    const QString& chatTitle,
    bool isPrivateChat,
    qint64 currentChatId,
    bool chatMuted);

} // namespace beygram::telegram
