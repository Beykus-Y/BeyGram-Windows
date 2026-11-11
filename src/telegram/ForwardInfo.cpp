#include "ForwardInfo.h"

#include "TelegramTypes.h"

#include <QJsonValue>

namespace beygram::telegram {
namespace {

ForwardInfo::Origin originFromType(const QString& type)
{
    if (type == QLatin1String("messageForwardOriginUser"))
        return ForwardInfo::Origin::User;
    if (type == QLatin1String("messageForwardOriginHiddenUser"))
        return ForwardInfo::Origin::HiddenUser;
    if (type == QLatin1String("messageForwardOriginChat"))
        return ForwardInfo::Origin::Chat;
    if (type == QLatin1String("messageForwardOriginChannel"))
        return ForwardInfo::Origin::Channel;
    if (type == QLatin1String("messageForwardOriginAnonymous"))
        return ForwardInfo::Origin::Anonymous;
    return ForwardInfo::Origin::None;
}

} // namespace

ForwardInfo forwardInfoFromMessage(const QJsonObject& message)
{
    ForwardInfo info;

    const QJsonValue forwardValue = message.value(QStringLiteral("forward_info"));
    if (!forwardValue.isObject())
        return info;

    const QJsonObject forward = forwardValue.toObject();
    info.date = jsonInt64(forward.value(QStringLiteral("date")));

    const QJsonValue originValue = forward.value(QStringLiteral("origin"));
    if (!originValue.isObject())
        return info;

    const QJsonObject origin = originValue.toObject();
    info.origin = originFromType(origin.value(QStringLiteral("@type")).toString());

    switch (info.origin) {
    case ForwardInfo::Origin::User:
        info.senderUserId = jsonInt64(origin.value(QStringLiteral("sender_user_id")));
        break;
    case ForwardInfo::Origin::HiddenUser:
        info.senderName = origin.value(QStringLiteral("sender_name")).toString();
        break;
    case ForwardInfo::Origin::Chat:
        info.sourceChatId = jsonInt64(origin.value(QStringLiteral("sender_chat_id")));
        info.authorSignature = origin.value(QStringLiteral("author_signature")).toString();
        break;
    case ForwardInfo::Origin::Channel:
        info.sourceChatId = jsonInt64(origin.value(QStringLiteral("chat_id")));
        info.sourceMessageId = jsonInt64(origin.value(QStringLiteral("message_id")));
        info.authorSignature = origin.value(QStringLiteral("author_signature")).toString();
        break;
    case ForwardInfo::Origin::Anonymous:
    case ForwardInfo::Origin::None:
        break;
    }

    // messageSource only adds identifiers the origin did not already provide.
    const QJsonObject source = forward.value(QStringLiteral("source")).toObject();
    if (info.sourceChatId == 0)
        info.sourceChatId = jsonInt64(source.value(QStringLiteral("chat_id")));
    if (info.sourceMessageId == 0)
        info.sourceMessageId = jsonInt64(source.value(QStringLiteral("message_id")));

    return info;
}

} // namespace beygram::telegram
