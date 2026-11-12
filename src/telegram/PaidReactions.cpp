#include "PaidReactions.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QString>

#include <algorithm>

namespace beygram::telegram {
namespace {

// A single paid reaction may carry 1..10000 Stars. The parser clamps display
// counts to the same ceiling so a corrupt update cannot make a row claim an
// impossible amount.
constexpr qint64 kMinPaidReactionStarCount = 1;
constexpr qint64 kMaxPaidReactionStarCount = 10000;

qint64 positiveId(const QJsonValue& value)
{
    const qint64 id = jsonInt64(value);
    return id > 0 ? id : 0;
}

// A paidReaction names its sender through MessageSender. The flattened
// sender_user_id/user_id forms are accepted too, so an update that has already
// been reduced to plain JSON still parses. A messageSenderChat sender has no
// user id and is not representable in PaidReactionRow, so it is treated as
// malformed.
qint64 senderUserIdFrom(const QJsonObject& row)
{
    for (const QString& key :
         {QStringLiteral("sender_user_id"), QStringLiteral("user_id")}) {
        const QJsonValue value = row.value(key);
        if (!value.isDouble() && !value.isString())
            continue;
        const qint64 id = positiveId(value);
        if (id > 0)
            return id;
    }

    const QJsonValue senderValue = row.value(QStringLiteral("sender_id"));
    if (senderValue.isObject()) {
        const QJsonObject sender = senderValue.toObject();
        const QString type = sender.value(QStringLiteral("@type")).toString();
        if (!type.isEmpty() && type != QLatin1String("messageSenderUser"))
            return 0;
        return positiveId(sender.value(QStringLiteral("user_id")));
    }
    if (senderValue.isDouble() || senderValue.isString())
        return positiveId(senderValue);

    return 0;
}

qint64 clampedStarCount(qint64 value)
{
    if (value <= 0)
        return 0;
    return std::min(value, kMaxPaidReactionStarCount);
}

} // namespace

QJsonObject sendPaidReactionRequest(qint64 chatId, qint64 messageId, qint64 starCount,
                                    bool anonymous)
{
    if (chatId <= 0 || messageId <= 0)
        return {};

    // Refuse an amount outside the allowed range rather than silently changing
    // how many Stars the user would spend.
    if (starCount < kMinPaidReactionStarCount || starCount > kMaxPaidReactionStarCount)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("sendPaidReaction")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
        {QStringLiteral("star_count"), starCount},
        {QStringLiteral("is_anonymous"), anonymous},
    };
}

QJsonObject getPaidReactionPrivacyRequest()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getPaidReactionPrivacy")},
    };
}

QVector<PaidReactionRow> paidReactionsFromUpdate(const QJsonObject& update)
{
    QVector<PaidReactionRow> rows;

    const QJsonValue listValue = update.value(QStringLiteral("paid_reactions"));
    if (!listValue.isArray())
        return rows;

    const QJsonArray entries = listValue.toArray();
    rows.reserve(entries.size());

    for (const QJsonValue& entryValue : entries) {
        if (!entryValue.isObject())
            continue;

        const QJsonObject entry = entryValue.toObject();
        const qint64 senderUserId = senderUserIdFrom(entry);
        if (senderUserId <= 0)
            continue;

        PaidReactionRow row;
        row.senderUserId = senderUserId;
        row.starCount = clampedStarCount(jsonInt64(entry.value(QStringLiteral("star_count"))));
        // TDLib spells the flag is_anonymous; the flattened anonymous form is
        // accepted for a row that was already reduced to plain JSON.
        row.anonymous = entry.contains(QStringLiteral("is_anonymous"))
            ? entry.value(QStringLiteral("is_anonymous")).toBool(false)
            : entry.value(QStringLiteral("anonymous")).toBool(false);
        rows.append(row);
    }

    return rows;
}

} // namespace beygram::telegram
