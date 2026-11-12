#include "CallHistory.h"

#include "TelegramTypes.h"

#include <QJsonValue>

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int kMinCallSearchLimit = 1;
constexpr int kMaxCallSearchLimit = 100;

// TDLib rejects limits outside 1..100.
int clampedLimit(int limit)
{
    return std::clamp(limit, kMinCallSearchLimit, kMaxCallSearchLimit);
}

QString typeOf(const QJsonObject& object)
{
    return object.value(QStringLiteral("@type")).toString();
}

/// Maps one callDiscardReason* object to a CallRecord::status token. TDLib uses
/// the same reason for a cancelled outgoing call and a missed incoming one, so
/// the direction decides how it is reported.
QString statusFromDiscardReason(const QJsonValue& value, bool outgoing)
{
    if (!value.isObject())
        return QStringLiteral("unknown");

    const QString reason = typeOf(value.toObject());
    if (reason == QLatin1String("callDiscardReasonEmpty")
        || reason == QLatin1String("callDiscardReasonHungUp")) {
        return QStringLiteral("ended");
    }
    if (reason == QLatin1String("callDiscardReasonMissed"))
        return outgoing ? QStringLiteral("cancelled") : QStringLiteral("missed");
    if (reason == QLatin1String("callDiscardReasonDeclined"))
        return QStringLiteral("declined");
    if (reason == QLatin1String("callDiscardReasonDisconnected"))
        return QStringLiteral("disconnected");
    if (reason == QLatin1String("callDiscardReasonUpgradeToGroupCall"))
        return QStringLiteral("upgraded");
    return QStringLiteral("unknown");
}

} // namespace

CallRecord callFromMessage(const QJsonObject& message)
{
    CallRecord record;

    const QJsonValue contentValue = message.value(QStringLiteral("content"));
    if (!contentValue.isObject())
        return record;

    const QJsonObject content = contentValue.toObject();
    if (typeOf(content) != QLatin1String("messageCall"))
        return record;

    record.messageId = jsonInt64(message.value(QStringLiteral("id")));
    record.chatId = jsonInt64(message.value(QStringLiteral("chat_id")));
    record.date = jsonInt64(message.value(QStringLiteral("date")));

    // Only older revisions nest the call object inside messageCall; it is read
    // as a fallback and stays empty for the pinned 1.8.69 shape.
    const QJsonObject call = content.value(QStringLiteral("call")).toObject();

    record.outgoing = message.value(QStringLiteral("is_outgoing"))
                          .toBool(call.value(QStringLiteral("is_outgoing")).toBool(false));
    record.video = content.value(QStringLiteral("is_video"))
                       .toBool(call.value(QStringLiteral("is_video")).toBool(false));

    const int duration = content.value(QStringLiteral("duration")).toInt(0);
    record.duration = duration > 0 ? duration : 0;

    const QJsonValue reasonValue = content.value(QStringLiteral("discard_reason"));
    if (reasonValue.isObject()) {
        record.status = statusFromDiscardReason(reasonValue, record.outgoing);
        return record;
    }

    const QJsonObject state = call.value(QStringLiteral("state")).toObject();
    const QJsonValue stateReason = state.value(QStringLiteral("reason"));
    if (stateReason.isObject())
        record.status = statusFromDiscardReason(stateReason, record.outgoing);
    else if (typeOf(state) == QLatin1String("callStateError"))
        record.status = QStringLiteral("failed");
    else
        record.status = QStringLiteral("unknown");

    return record;
}

QJsonObject searchCallMessagesRequest(qint64 /*fromMessageId*/, int limit)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("searchCallMessages")},
        {QStringLiteral("offset"), QString()},
        {QStringLiteral("limit"), clampedLimit(limit)},
        {QStringLiteral("only_missed"), false},
    };
}

} // namespace beygram::telegram
