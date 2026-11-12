#pragma once

#include "GiftRequests.h"

#include <QJsonObject>
#include <QtGlobal>

namespace beygram::telegram {

/// Builders for the gift actions that follow a gift's delivery: previewing an
/// upgrade, upgrading, transferring, converting back to Stars, listing received
/// and sent gifts, and toggling whether a gift is shown on a profile.
///
/// Sending stays in GiftRequests.h; this module only adds the actions that were
/// missing there and reuses getReceivedGiftsRequest() for the received list.
///
/// Shapes follow the gift-action methods used by the client:
///   getGiftUpgradePreview gift_id:int64 = GiftUpgradePreview;
///   upgradeGift gift_id:int64 keep_original_details:Bool = UpgradedGift;
///   transferGift gift_id:int64 new_owner_id:MessageSender = Ok;
///   convertGiftToStars gift_id:int64 = Ok;
///   getSentGifts offset:string limit:int32 = SentGifts;
///   toggleGiftIsSaved gift_id:int64 is_saved:Bool = Ok;
///
/// Every builder is pure: it adds no @extra, logs nothing and performs no
/// transport. A non-positive gift or owner id cannot name a gift action, so the
/// builder returns an empty object, which the caller must not send.

/// Builds getGiftUpgradePreview for one regular gift. The response describes
/// the variants an upgrade can produce; nothing is purchased here.
[[nodiscard]] QJsonObject getGiftUpgradePreviewRequest(qint64 giftId);

/// Builds upgradeGift for a received regular gift. keepOriginalDetails keeps
/// the original sender, receiver and message on the upgraded gift; false lets
/// the upgrade replace them with the current owner.
[[nodiscard]] QJsonObject upgradeGiftRequest(qint64 giftId,
                                             bool keepOriginalDetails);

/// Builds transferGift for an upgraded gift. newOwnerId is the private chat id
/// of the recipient; TDLib keeps a private chat id equal to its user id, so the
/// owner is sent as messageSenderUser. Channels need a messageSenderChat owner,
/// which this signature cannot express.
[[nodiscard]] QJsonObject transferGiftRequest(qint64 giftId,
                                              qint64 newOwnerId);

/// Builds convertGiftToStars for a regular gift. Conversion destroys the gift
/// and credits its Star value, so the caller confirms it with the user first.
[[nodiscard]] QJsonObject convertGiftToStarsRequest(qint64 giftId);

/// Builds the first page of the received-gifts list for chatId. limit is
/// clamped to 1..100, the range TDLib accepts.
///
/// A positive chatId reuses getReceivedGiftsRequest(chatId, 0, limit) from
/// GiftRequests.h, so every filter and the empty cursor match that builder.
/// chatId 0 asks for all received gifts: owner_id is omitted. The pinned schema
/// names one gift receiver in owner_id, so a caller that has the signed-in user
/// id should pass it instead of relying on the owner-less form.
[[nodiscard]] QJsonObject getReceivedGiftsRequest(qint64 chatId, int limit);

/// Builds the first page of gifts sent by the current user. offset is the empty
/// cursor and limit is clamped to 1..100. The list is sorted by send date.
[[nodiscard]] QJsonObject getSentGiftsRequest(int limit);

/// Builds toggleGiftIsSaved for a gift. saved true shows the gift on the
/// user's or channel's profile page; false removes it from that page.
[[nodiscard]] QJsonObject toggleGiftIsSavedRequest(qint64 giftId, bool saved);

} // namespace beygram::telegram
