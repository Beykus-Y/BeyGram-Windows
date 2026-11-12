#include "GiftRequests.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>
#include <limits>

namespace beygram::telegram {
namespace {

constexpr int kMinReceivedGiftsLimit = 1;
constexpr int kMaxReceivedGiftsLimit = 100;

// TDLib rejects limits outside 1..100 for getReceivedGifts.
int clampedLimit(int limit)
{
    return std::clamp(limit, kMinReceivedGiftsLimit, kMaxReceivedGiftsLimit);
}

// star_count is int53 in the schema, but GiftInfo keeps an int.
int clampedStarCount(qint64 starCount)
{
    if (starCount <= 0)
        return 0;
    return static_cast<int>(
        std::min<qint64>(starCount, std::numeric_limits<int>::max()));
}

// File ids are TDLib int32 values; anything outside that range is treated as
// "no file" instead of being silently truncated.
int fileIdFromJson(const QJsonObject& file)
{
    const qint64 id = jsonInt64(file.value(QStringLiteral("id")));
    if (id <= 0 || id > std::numeric_limits<int>::max())
        return 0;
    return static_cast<int>(id);
}

int giftFileId(const QJsonObject& gift)
{
    const QJsonObject sticker = gift.value(QStringLiteral("sticker")).toObject();
    int fileId = fileIdFromJson(sticker.value(QStringLiteral("sticker")).toObject());
    if (fileId == 0) {
        fileId = fileIdFromJson(
            sticker.value(QStringLiteral("thumbnail"))
                .toObject()
                .value(QStringLiteral("file"))
                .toObject());
    }
    return fileId;
}

// A gift owner is a user or a channel chat. This module only receives a private
// chat id, which TDLib keeps equal to the user id, so messageSenderUser is the
// one variant built here; groups are not gift targets.
QJsonObject giftOwner(qint64 chatId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageSenderUser")},
        {QStringLiteral("user_id"), chatId},
    };
}

// sendGift.text is a formattedText, not a plain string. No entities are added,
// so the caller's text is shown as written.
QJsonObject plainFormattedText(const QString& text)
{
    return QJsonObject{
        {QStringLiteral("text"), text},
        {QStringLiteral("entities"), QJsonArray()},
    };
}

QJsonObject receivedGiftsRequest(qint64 chatId, const QString& offset, int limit)
{
    // Every filter is false, so the answer is the owner's complete gift list
    // (saved and unsaved, upgraded and regular), sorted by send date.
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getReceivedGifts")},
        {QStringLiteral("business_connection_id"), QString()},
        {QStringLiteral("owner_id"), giftOwner(chatId)},
        {QStringLiteral("collection_id"), 0},
        {QStringLiteral("exclude_unsaved"), false},
        {QStringLiteral("exclude_saved"), false},
        {QStringLiteral("exclude_unlimited"), false},
        {QStringLiteral("exclude_upgradable"), false},
        {QStringLiteral("exclude_non_upgradable"), false},
        {QStringLiteral("exclude_upgraded"), false},
        {QStringLiteral("exclude_without_colors"), false},
        {QStringLiteral("exclude_hosted"), false},
        {QStringLiteral("sort_by_price"), false},
        {QStringLiteral("offset"), offset},
        {QStringLiteral("limit"), clampedLimit(limit)},
    };
}

} // namespace

QJsonObject getAvailableGiftsRequest()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getAvailableGifts")},
    };
}

QJsonObject sendGiftRequest(qint64 giftId, qint64 chatId, const QString& text)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("sendGift")},
        {QStringLiteral("gift_id"), giftId},
        {QStringLiteral("owner_id"), giftOwner(chatId)},
        {QStringLiteral("text"), plainFormattedText(text)},
        {QStringLiteral("is_private"), false},
        {QStringLiteral("pay_for_upgrade"), false},
    };
}

QJsonObject getReceivedGiftsRequest(qint64 chatId, int offset, int limit)
{
    // The cursor is opaque, so only 0 (first page) has a schema meaning here.
    const QString cursor = offset <= 0 ? QString() : QString::number(offset);
    return receivedGiftsRequest(chatId, cursor, limit);
}

QJsonObject getReceivedGiftsRequest(qint64 chatId, const QString& offset, int limit)
{
    return receivedGiftsRequest(chatId, offset, limit);
}

QVector<GiftInfo> giftsFromResponse(const QJsonObject& response)
{
    QVector<GiftInfo> gifts;

    const QJsonValue giftsValue = response.value(QStringLiteral("gifts"));
    if (!giftsValue.isArray())
        return gifts;

    const QJsonArray entries = giftsValue.toArray();
    gifts.reserve(entries.size());
    for (const QJsonValue& entryValue : entries) {
        if (!entryValue.isObject())
            continue;

        const QJsonObject entry = entryValue.toObject();
        // Catalog entries wrap the gift in availableGift. A bare gift object is
        // accepted too, so a caller that already unwrapped the array still
        // parses; an entry with a null gift falls back to itself and is then
        // dropped for having no id.
        const QJsonValue giftValue = entry.value(QStringLiteral("gift"));
        const QJsonObject gift = giftValue.isObject() ? giftValue.toObject() : entry;

        const qint64 id = jsonInt64(gift.value(QStringLiteral("id")));
        if (id <= 0)
            continue;

        GiftInfo info;
        info.id = id;
        // availableGift.title is the only schema title; a bare upgraded gift
        // carries its title on the gift itself.
        info.title = entry.value(QStringLiteral("title")).toString();
        if (info.title.isEmpty())
            info.title = gift.value(QStringLiteral("title")).toString();
        info.starCount =
            clampedStarCount(jsonInt64(gift.value(QStringLiteral("star_count"))));
        info.fileId = giftFileId(gift);
        gifts.append(info);
    }

    return gifts;
}

} // namespace beygram::telegram
