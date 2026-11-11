#include "ForwardMessages.h"

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

QJsonObject messageSendOptions(bool silently)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageSendOptions")},
        {QStringLiteral("suggested_post_info"), QJsonValue::Null},
        {QStringLiteral("disable_notification"), silently},
        {QStringLiteral("from_background"), false},
        {QStringLiteral("protect_content"), false},
        {QStringLiteral("allow_paid_broadcast"), false},
        {QStringLiteral("paid_message_star_count"), qint64(0)},
        {QStringLiteral("update_order_of_installed_sticker_sets"), false},
        {QStringLiteral("scheduling_state"), QJsonValue::Null},
        {QStringLiteral("effect_id"), qint64(0)},
        {QStringLiteral("sending_id"), 0},
        {QStringLiteral("only_preview"), false},
    };
}

} // namespace

QJsonObject forwardMessagesRequest(qint64 toChatId, qint64 fromChatId,
                                   const QVector<qint64>& messageIds, bool silently)
{
    QJsonArray ids;
    for (const qint64 messageId : usableMessageIds(messageIds))
        ids.append(messageId);

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("forwardMessages")},
        {QStringLiteral("chat_id"), toChatId},
        {QStringLiteral("topic_id"), QJsonValue::Null},
        {QStringLiteral("from_chat_id"), fromChatId},
        {QStringLiteral("message_ids"), ids},
        {QStringLiteral("options"), messageSendOptions(silently)},
        {QStringLiteral("send_copy"), false},
        {QStringLiteral("remove_caption"), false},
    };
}

bool canForward(const QVector<qint64>& messageIds)
{
    return !usableMessageIds(messageIds).isEmpty();
}

} // namespace beygram::telegram
