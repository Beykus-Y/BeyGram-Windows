#include "ReplyInfo.h"

#include "TelegramTypes.h"

#include <QJsonValue>

#include <limits>

namespace beygram::telegram {
namespace {

QString quoteTextFrom(const QJsonObject& replyTo)
{
    return replyTo.value(QStringLiteral("quote"))
        .toObject()
        .value(QStringLiteral("text"))
        .toObject()
        .value(QStringLiteral("text"))
        .toString();
}

} // namespace

bool ReplyTarget::isValid() const
{
    switch (kind) {
    case Kind::Message:
    case Kind::ExternalMessage:
        return messageId > 0;
    case Kind::Story:
        return storyPosterChatId != 0 && storyId > 0;
    case Kind::None:
        break;
    }
    return false;
}

ReplyTarget replyTargetFromMessage(const QJsonObject& message)
{
    ReplyTarget target;

    const QJsonValue replyToValue = message.value(QStringLiteral("reply_to"));
    if (!replyToValue.isObject())
        return target;

    const QJsonObject replyTo = replyToValue.toObject();
    const QString type = replyTo.value(QStringLiteral("@type")).toString();

    if (type == QLatin1String("messageReplyToMessage")) {
        target.kind = ReplyTarget::Kind::Message;
        target.messageId = jsonInt64(replyTo.value(QStringLiteral("message_id")));
        target.quoteText = quoteTextFrom(replyTo);
    } else if (type == QLatin1String("messageReplyToExternalMessage")) {
        target.kind = ReplyTarget::Kind::ExternalMessage;
        target.chatId = jsonInt64(replyTo.value(QStringLiteral("chat_id")));
        target.messageId = jsonInt64(replyTo.value(QStringLiteral("message_id")));
        target.quoteText = quoteTextFrom(replyTo);
    } else if (type == QLatin1String("messageReplyToStory")) {
        target.kind = ReplyTarget::Kind::Story;
        target.storyPosterChatId =
            jsonInt64(replyTo.value(QStringLiteral("story_poster_chat_id")));
        const qint64 storyId = jsonInt64(replyTo.value(QStringLiteral("story_id")));
        target.storyId = storyId > 0 && storyId <= std::numeric_limits<int>::max()
            ? static_cast<int>(storyId)
            : 0;
    }

    return target;
}

QJsonObject inputReplyToMessageRequest(qint64 messageId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("inputMessageReplyToMessage")},
        {QStringLiteral("message_id"), messageId},
        {QStringLiteral("quote"), QJsonValue::Null},
        {QStringLiteral("checklist_task_id"), 0},
        {QStringLiteral("poll_option_id"), QString()},
    };
}

} // namespace beygram::telegram
