#include "PinnedMessages.h"

#include "TelegramTypes.h"

#include <QJsonValue>
#include <QString>

namespace beygram::telegram {
namespace {

/// TDLib message ids are positive; anything else means "no pinned message".
qint64 positiveIdOrZero(qint64 id)
{
    return id > 0 ? id : 0;
}

} // namespace

QJsonObject getChatPinnedMessageRequest(qint64 chatId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getChatPinnedMessage")},
        {QStringLiteral("chat_id"), chatId},
    };
}

QJsonObject pinChatMessageRequest(qint64 chatId, qint64 messageId, bool disableNotification,
                                  bool onlyForSelf)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("pinChatMessage")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
        {QStringLiteral("disable_notification"), disableNotification},
        {QStringLiteral("only_for_self"), onlyForSelf},
    };
}

QJsonObject unpinChatMessageRequest(qint64 chatId, qint64 messageId)
{
    if (messageId <= 0) {
        return QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("unpinAllChatMessages")},
            {QStringLiteral("chat_id"), chatId},
        };
    }

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("unpinChatMessage")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
    };
}

qint64 pinnedMessageIdFromUpdate(const QJsonObject& update)
{
    const QString type = update.value(QStringLiteral("@type")).toString();

    if (type == QStringLiteral("updateChatPinnedMessage")) {
        return positiveIdOrZero(jsonInt64(update.value(QStringLiteral("pinned_message_id"))));
    }

    // TDLib 1.7.0 removed updateChatPinnedMessage in favour of this per-message
    // update; an unpin carries is_pinned = false and no pinned id.
    if (type == QStringLiteral("updateMessageIsPinned")) {
        if (!update.value(QStringLiteral("is_pinned")).toBool())
            return 0;
        return positiveIdOrZero(jsonInt64(update.value(QStringLiteral("message_id"))));
    }

    return 0;
}

} // namespace beygram::telegram
