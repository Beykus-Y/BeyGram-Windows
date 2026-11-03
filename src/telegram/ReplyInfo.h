#pragma once

#include <QJsonObject>
#include <QString>

namespace beygram::telegram {

/// Everything needed to show and send a reply. Parsed from TDLib without
/// touching the UI, so the model layer can hold it directly.
struct ReplyTarget {
    enum class Kind { None, Message, ExternalMessage, Story };

    Kind kind = Kind::None;
    qint64 chatId = 0;        // for ExternalMessage
    qint64 messageId = 0;     // for Message/ExternalMessage
    qint64 storyPosterChatId = 0;
    int storyId = 0;
    QString quoteText;        // plain quote text, may be empty
    [[nodiscard]] bool isValid() const;
};

/// Parses message["reply_to"] from a TDLib message object.
[[nodiscard]] ReplyTarget replyTargetFromMessage(const QJsonObject& message);

/// Builds TDLib inputMessageReplyToMessage for sendMessage.
[[nodiscard]] QJsonObject inputReplyToMessageRequest(qint64 messageId);

} // namespace beygram::telegram
