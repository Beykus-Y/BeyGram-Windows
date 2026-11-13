#include "ForwardBatch.h"

#include <QJsonArray>
#include <QSet>

namespace beygram::telegram {
namespace {

constexpr qsizetype kMaxMessages = 100;
constexpr qsizetype kMaxTargets = 20;

/// TDLib rejects non-positive ids and duplicates, so the limits and the request
/// bodies are computed from this filtered view. First occurrence wins.
QVector<qint64> uniquePositive(const QVector<qint64>& ids)
{
    QVector<qint64> usable;
    usable.reserve(ids.size());
    QSet<qint64> seen;
    for (const qint64 id : ids) {
        if (id <= 0 || seen.contains(id))
            continue;
        seen.insert(id);
        usable.append(id);
    }
    return usable;
}

/// Schema-complete messageSendOptions. The batch has no notification or
/// scheduling switches, so every field carries its neutral value.
QJsonObject messageSendOptions()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageSendOptions")},
        {QStringLiteral("suggested_post_info"), QJsonValue::Null},
        {QStringLiteral("disable_notification"), false},
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

QJsonObject forwardRequest(qint64 targetChatId, qint64 fromChatId,
                           const QVector<qint64>& messageIds, bool dropAuthor,
                           bool dropCaptions)
{
    QJsonArray ids;
    for (const qint64 messageId : messageIds)
        ids.append(messageId);

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("forwardMessages")},
        {QStringLiteral("chat_id"), targetChatId},
        {QStringLiteral("topic_id"), QJsonValue::Null},
        {QStringLiteral("from_chat_id"), fromChatId},
        {QStringLiteral("message_ids"), ids},
        {QStringLiteral("options"), messageSendOptions()},
        {QStringLiteral("send_copy"), dropAuthor},
        {QStringLiteral("remove_caption"), dropCaptions},
    };
}

} // namespace

QJsonObject ForwardBatchPlanner::build(const ForwardBatchPlan& plan, bool dropAuthor,
                                       bool dropCaptions)
{
    const QVector<QJsonObject> requests = buildAll(plan, dropAuthor, dropCaptions);
    return requests.isEmpty() ? QJsonObject{} : requests.first();
}

QVector<QJsonObject> ForwardBatchPlanner::buildAll(const ForwardBatchPlan& plan, bool dropAuthor,
                                                   bool dropCaptions)
{
    QVector<QJsonObject> requests;
    if (!validate(plan).isEmpty())
        return requests;

    const QVector<qint64> messageIds = uniquePositive(plan.messageIds);
    const QVector<qint64> targetChatIds = uniquePositive(plan.targetChatIds);
    requests.reserve(targetChatIds.size());
    for (const qint64 targetChatId : targetChatIds) {
        requests.append(
            forwardRequest(targetChatId, plan.fromChatId, messageIds, dropAuthor, dropCaptions));
    }
    return requests;
}

QString ForwardBatchPlanner::validate(const ForwardBatchPlan& plan)
{
    if (plan.fromChatId == 0)
        return QStringLiteral("no-source");

    const QVector<qint64> messageIds = uniquePositive(plan.messageIds);
    if (messageIds.isEmpty())
        return QStringLiteral("no-messages");

    const QVector<qint64> targetChatIds = uniquePositive(plan.targetChatIds);
    if (targetChatIds.isEmpty())
        return QStringLiteral("no-targets");

    if (messageIds.size() > kMaxMessages)
        return QStringLiteral("too-many-messages");

    if (targetChatIds.size() > kMaxTargets)
        return QStringLiteral("too-many-targets");

    return {};
}

} // namespace beygram::telegram
