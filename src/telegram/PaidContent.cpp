#include "PaidContent.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QString>

#include <algorithm>
#include <limits>

namespace beygram::telegram {
namespace {

// A getPaidMedia request carries at most 100 message ids.
constexpr int kMaxPaidMediaMessageIds = 100;

// Keeps only positive, unique ids in their original order and stops after the
// request's maximum. getPaidMediaRequest() applies the same rules to the ids a
// caller passes in.
QVector<qint64> normalizedMessageIds(const QJsonArray& values)
{
    QVector<qint64> ids;
    ids.reserve(std::min<qsizetype>(values.size(), kMaxPaidMediaMessageIds));

    for (const QJsonValue& value : values) {
        const qint64 id = jsonInt64(value);
        if (id <= 0)
            continue;
        if (std::find(ids.cbegin(), ids.cend(), id) != ids.cend())
            continue;

        ids.append(id);
        if (ids.size() >= kMaxPaidMediaMessageIds)
            break;
    }

    return ids;
}

} // namespace

PaidMediaInfo paidMediaFromContent(const QJsonObject& content)
{
    // An untagged object is accepted so a caller that already unwrapped the
    // content still parses; any other content type is definitely not paid media.
    const QString type = content.value(QStringLiteral("@type")).toString();
    if (!type.isEmpty() && type != QLatin1String("messagePaidMedia"))
        return {};

    const qint64 starCount = jsonInt64(content.value(QStringLiteral("star_count")));
    if (starCount <= 0)
        return {};

    const QJsonValue mediaValue = content.value(QStringLiteral("media"));
    if (!mediaValue.isArray())
        return {};

    const QJsonArray media = mediaValue.toArray();
    if (media.isEmpty())
        return {};

    // A paid-media part is always a PaidMedia object. A null or scalar entry
    // means the content was truncated or assembled by hand, so the whole bundle
    // is refused instead of being reported with a misleading part count.
    for (const QJsonValue& part : media) {
        if (!part.isObject())
            return {};
    }

    PaidMediaInfo info;
    info.starCount = starCount;
    info.partCount = media.size() > std::numeric_limits<int>::max()
        ? std::numeric_limits<int>::max()
        : static_cast<int>(media.size());

    const QJsonValue idsValue = content.value(QStringLiteral("message_ids"));
    if (idsValue.isArray())
        info.messageIds = normalizedMessageIds(idsValue.toArray());

    return info;
}

QJsonObject getPaidMediaRequest(qint64 chatId, const QVector<qint64>& messageIds,
                                bool onlyPreviews)
{
    if (chatId <= 0)
        return {};

    QJsonArray ids;
    QVector<qint64> seen;
    seen.reserve(std::min<qsizetype>(messageIds.size(), kMaxPaidMediaMessageIds));

    for (const qint64 id : messageIds) {
        if (id <= 0)
            continue;
        if (std::find(seen.cbegin(), seen.cend(), id) != seen.cend())
            continue;

        seen.append(id);
        ids.append(id);
        if (ids.size() >= kMaxPaidMediaMessageIds)
            break;
    }

    // No usable id means there is no bundle to request.
    if (ids.isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getPaidMedia")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_ids"), ids},
        // The flag is singular even though it applies to the whole list.
        {QStringLiteral("only_preview"), onlyPreviews},
    };
}

QJsonObject openPaidReactionPrivacyRequest()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("openPaidReactionPrivacy")},
    };
}

} // namespace beygram::telegram
