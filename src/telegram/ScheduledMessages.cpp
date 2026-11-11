#include "ScheduledMessages.h"

#include "TelegramRequests.h"

#include <QJsonArray>
#include <QSet>
#include <QString>

namespace beygram::telegram {
namespace {

/// TDLib rejects non-positive message ids and duplicates, so drop them before
/// the request reaches the transport.
QVector<qint64> usableMessageIds(const QVector<qint64>& messageIds)
{
    QVector<qint64> ids;
    ids.reserve(messageIds.size());
    QSet<qint64> seen;
    for (const qint64 messageId : messageIds) {
        if (messageId <= 0 || seen.contains(messageId))
            continue;
        seen.insert(messageId);
        ids.append(messageId);
    }
    return ids;
}

} // namespace

QJsonObject scheduledSendState(qint64 sendAt)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageSchedulingStateSendAtDate")},
        {QStringLiteral("send_date"), sendAt},
        {QStringLiteral("repeat_period"), 0},
    };
}

QJsonObject onlineSendState()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageSchedulingStateSendWhenOnline")},
    };
}

QJsonObject getScheduledMessagesRequest(qint64 chatId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getChatScheduledMessages")},
        {QStringLiteral("chat_id"), chatId},
    };
}

QJsonObject deleteScheduledMessagesRequest(qint64 chatId, const QVector<qint64>& ids)
{
    QJsonArray messageIds;
    for (const qint64 id : usableMessageIds(ids))
        messageIds.append(id);

    // TDLib 1.8.69 cancels scheduled messages with deleteMessages(revoke=false).
    return deleteMessagesRequest(chatId, messageIds, false);
}

} // namespace beygram::telegram
