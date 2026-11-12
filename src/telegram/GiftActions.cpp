#include "GiftActions.h"

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int kMinGiftListLimit = 1;
constexpr int kMaxGiftListLimit = 100;

// TDLib rejects limits outside 1..100 for both gift lists.
int clampedLimit(int limit)
{
    return std::clamp(limit, kMinGiftListLimit, kMaxGiftListLimit);
}

// transferGift.new_owner_id is a MessageSender. Only a private chat/user id is
// available here, and TDLib keeps a private chat id equal to its user id.
QJsonObject giftOwner(qint64 ownerId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageSenderUser")},
        {QStringLiteral("user_id"), ownerId},
    };
}

} // namespace

QJsonObject getGiftUpgradePreviewRequest(qint64 giftId)
{
    if (giftId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getGiftUpgradePreview")},
        {QStringLiteral("gift_id"), giftId},
    };
}

QJsonObject upgradeGiftRequest(qint64 giftId, bool keepOriginalDetails)
{
    if (giftId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("upgradeGift")},
        {QStringLiteral("gift_id"), giftId},
        {QStringLiteral("keep_original_details"), keepOriginalDetails},
    };
}

QJsonObject transferGiftRequest(qint64 giftId, qint64 newOwnerId)
{
    if (giftId <= 0 || newOwnerId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("transferGift")},
        {QStringLiteral("gift_id"), giftId},
        {QStringLiteral("new_owner_id"), giftOwner(newOwnerId)},
    };
}

QJsonObject convertGiftToStarsRequest(qint64 giftId)
{
    if (giftId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("convertGiftToStars")},
        {QStringLiteral("gift_id"), giftId},
    };
}

QJsonObject getReceivedGiftsRequest(qint64 chatId, int limit)
{
    // A negative chat id names no receiver. Zero is the account-wide form and
    // is handled below; positive ids are exactly the GiftRequests builder.
    if (chatId < 0)
        return {};

    QJsonObject request = beygram::telegram::getReceivedGiftsRequest(chatId, 0, limit);
    if (chatId == 0) {
        // owner_id names a single receiver in the schema; dropping it is the
        // only "all received gifts" shape this signature can express.
        request.remove(QStringLiteral("owner_id"));
    }
    return request;
}

QJsonObject getSentGiftsRequest(int limit)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getSentGifts")},
        // The empty cursor asks for the first chunk.
        {QStringLiteral("offset"), QString()},
        {QStringLiteral("limit"), clampedLimit(limit)},
    };
}

QJsonObject toggleGiftIsSavedRequest(qint64 giftId, bool saved)
{
    if (giftId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("toggleGiftIsSaved")},
        {QStringLiteral("gift_id"), giftId},
        {QStringLiteral("is_saved"), saved},
    };
}

} // namespace beygram::telegram
