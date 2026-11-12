#include "DraftSync.h"

#include "ReplyInfo.h"
#include "TelegramTypes.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonValue>

namespace beygram::telegram {
namespace {

QJsonObject formattedText(const QString& text)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("formattedText")},
        {QStringLiteral("text"), text},
        {QStringLiteral("entities"), QJsonArray()},
    };
}

/// Reads the plain text of a TDLib formattedText object. A bare string is
/// accepted too because a few older revisions sent plain text.
QString formattedTextPlain(const QJsonValue& value)
{
    if (value.isString())
        return value.toString();
    return value.toObject().value(QStringLiteral("text")).toString();
}

QJsonObject draftContent(const QString& text)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("draftMessageContentText")},
        {QStringLiteral("text"), formattedText(text)},
        // Null keeps TDLib's default link-preview behaviour; the composer has
        // no preview options of its own yet.
        {QStringLiteral("link_preview_options"), QJsonValue::Null},
    };
}

/// Draft replies are InputMessageReplyTo. Only a reply to a regular message is
/// representable in ChatDraft; stories and other targets collapse to "no reply".
qint64 replyToMessageIdFrom(const QJsonObject& draft)
{
    const QJsonObject replyTo =
        draft.value(QStringLiteral("reply_to")).toObject();
    if (replyTo.value(QStringLiteral("@type")).toString()
        != QLatin1String("inputMessageReplyToMessage")) {
        return 0;
    }

    const qint64 messageId = jsonInt64(replyTo.value(QStringLiteral("message_id")));
    return messageId > 0 ? messageId : 0;
}

QString textFromDraft(const QJsonObject& draft)
{
    const QJsonObject content = draft.value(QStringLiteral("content")).toObject();
    if (content.value(QStringLiteral("@type")).toString()
        == QLatin1String("draftMessageContentText")) {
        return formattedTextPlain(content.value(QStringLiteral("text")));
    }

    // TDLib revisions before DraftMessageContent stored the same text as an
    // inputMessageText in input_message_text.
    const QJsonObject inputMessageText =
        draft.value(QStringLiteral("input_message_text")).toObject();
    if (inputMessageText.value(QStringLiteral("@type")).toString()
        == QLatin1String("inputMessageText")) {
        return formattedTextPlain(inputMessageText.value(QStringLiteral("text")));
    }

    return QString();
}

} // namespace

QJsonObject setChatDraftMessageRequest(qint64 chatId, const QString& text,
                                       qint64 replyToMessageId)
{
    const QJsonValue reply = replyToMessageId > 0
        ? QJsonValue(inputReplyToMessageRequest(replyToMessageId))
        : QJsonValue(QJsonValue::Null);

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setChatDraftMessage")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("topic_id"), QJsonValue::Null},
        {QStringLiteral("draft_message"),
         QJsonObject{
             {QStringLiteral("@type"), QStringLiteral("draftMessage")},
             {QStringLiteral("reply_to"), reply},
             // TDLib uses the date to decide which side of a conflict is newer.
             {QStringLiteral("date"), QDateTime::currentSecsSinceEpoch()},
             {QStringLiteral("content"), draftContent(text)},
             {QStringLiteral("effect_id"), 0},
             {QStringLiteral("suggested_post_info"), QJsonValue::Null},
         }},
    };
}

ChatDraft draftFromUpdate(const QJsonObject& update)
{
    ChatDraft draft;

    if (update.value(QStringLiteral("@type")).toString()
        != QLatin1String("updateChatDraftMessage")) {
        return draft;
    }

    draft.chatId = jsonInt64(update.value(QStringLiteral("chat_id")));

    const QJsonObject message =
        update.value(QStringLiteral("draft_message")).toObject();
    if (message.isEmpty())
        return draft;

    draft.replyToMessageId = replyToMessageIdFrom(message);
    draft.text = textFromDraft(message);
    return draft;
}

QJsonObject clearChatDraftRequest(qint64 chatId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setChatDraftMessage")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("topic_id"), QJsonValue::Null},
        {QStringLiteral("draft_message"), QJsonValue::Null},
    };
}

} // namespace beygram::telegram
