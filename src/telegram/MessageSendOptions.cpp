#include "MessageSendOptions.h"

#include "ScheduledMessages.h"

#include <QJsonValue>

namespace beygram::telegram {
namespace {

QJsonValue schedulingState(const SendOptions& options)
{
    // A concrete date and "send when online" are alternative states, never
    // combined; a date chosen by the user is the more specific instruction.
    if (options.scheduleDate > 0)
        return scheduledSendState(options.scheduleDate);
    if (options.sendWhenOnline)
        return onlineSendState();
    return QJsonValue(QJsonValue::Null);
}

} // namespace

QJsonObject messageSendOptionsObject(const SendOptions& options)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageSendOptions")},
        {QStringLiteral("suggested_post_info"), QJsonValue::Null},
        {QStringLiteral("disable_notification"), options.silent},
        {QStringLiteral("from_background"), options.background},
        {QStringLiteral("protect_content"), options.protectContent},
        {QStringLiteral("allow_paid_broadcast"), false},
        {QStringLiteral("paid_message_star_count"), qint64(0)},
        {QStringLiteral("update_order_of_installed_sticker_sets"), false},
        {QStringLiteral("scheduling_state"), schedulingState(options)},
        {QStringLiteral("effect_id"), qint64(0)},
        {QStringLiteral("sending_id"), 0},
        {QStringLiteral("only_preview"), false},
    };
}

QJsonObject withSendOptions(QJsonObject request, const SendOptions& options)
{
    if (request.isEmpty())
        return request;
    request.insert(QStringLiteral("options"), messageSendOptionsObject(options));
    return request;
}

} // namespace beygram::telegram
