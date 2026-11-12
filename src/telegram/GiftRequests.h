#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One gift from the getAvailableGifts catalog.
///
/// title is the catalog entry's title (availableGift.title), which TDLib fills
/// only for gifts that are also sold as upgraded/resale gifts; it stays empty
/// for a plain catalog gift. The sticker itself is the gift's visual identity
/// and is referenced through fileId.
struct GiftInfo {
    qint64 id = 0;
    QString title;
    int starCount = 0;
    int fileId = 0;
};

/// Pure builders and parsers for TDLib gifts. They contain no logging, no
/// secrets and no @extra; the transport adds correlation.
///
/// Shape follows the pinned td_api.tl (TDLib 1.8.69, commit 4c6377aa):
///   getAvailableGifts = AvailableGifts;
///   availableGift gift:gift resale_count:int32 min_resale_star_count:int53
///                 title:string = AvailableGift;
///   availableGifts gifts:vector<availableGift> = AvailableGifts;
///   gift id:int64 publisher_chat_id:int53 sticker:sticker star_count:int53
///        default_sell_star_count:int53 upgrade_star_count:int53
///        ... user_limits:giftPurchaseLimits
///        overall_limits:giftPurchaseLimits ... = Gift;
///   sendGift gift_id:int64 owner_id:MessageSender text:formattedText
///            is_private:Bool pay_for_upgrade:Bool = Ok;
///   getReceivedGifts business_connection_id:string owner_id:MessageSender
///            collection_id:int32 exclude_unsaved:Bool exclude_saved:Bool
///            exclude_unlimited:Bool exclude_upgradable:Bool
///            exclude_non_upgradable:Bool exclude_upgraded:Bool
///            exclude_without_colors:Bool exclude_hosted:Bool
///            sort_by_price:Bool offset:string limit:int32 = ReceivedGifts;

/// Builds the catalog request. getAvailableGifts takes no arguments, so the
/// object carries only its @type.
[[nodiscard]] QJsonObject getAvailableGiftsRequest();

/// Builds sendGift for one catalog gift.
///
/// chatId is the private chat that receives the gift. TDLib keeps a private
/// chat id equal to its user id, so owner_id is sent as messageSenderUser.
/// Groups are not gift targets and a channel needs a messageSenderChat owner,
/// which this signature cannot express; the caller resolves the recipient
/// before calling.
///
/// text is sent verbatim as a formattedText with no entities. The schema limits
/// it to getOption("gift_text_length_max") characters (0 is valid); this builder
/// never truncates a paid message, so the caller validates the length first.
/// is_private and pay_for_upgrade are false: the gift and its text are visible
/// to everyone and the receiver pays for any upgrade itself.
[[nodiscard]] QJsonObject sendGiftRequest(qint64 giftId, qint64 chatId,
                                          const QString& text);

/// Builds the first page of the received-gifts list for chatId (same private
/// chat/user convention as sendGiftRequest). Every filter is off, so all saved,
/// unsaved, upgraded and regular gifts are returned, newest first; the sort is
/// by send date, not by price. limit is clamped to 1..100, the range TDLib
/// accepts, and an offset <= 0 is the empty cursor that asks for page one.
///
/// TDLib's cursor is an opaque string (receivedGifts.next_offset), so an
/// integer offset cannot address later pages: a positive value is sent as its
/// decimal string and will not match a real cursor. Use the QString overload
/// below for paging.
[[nodiscard]] QJsonObject getReceivedGiftsRequest(qint64 chatId, int offset,
                                                  int limit);

/// Builds a received-gifts page for the opaque cursor returned as
/// receivedGifts.next_offset; pass an empty string for the first page. limit is
/// clamped to 1..100. All other fields match getReceivedGiftsRequest above.
[[nodiscard]] QJsonObject getReceivedGiftsRequest(qint64 chatId,
                                                  const QString& offset,
                                                  int limit);

/// Converts a getAvailableGifts response (or any object with a "gifts" array)
/// into catalog entries. Each entry is normally an availableGift wrapping its
/// gift; a bare gift object is accepted as well, so a trimmed response still
/// parses. Entries without a positive gift id (a null gift, a sent/received
/// gift wrapper, a non-object) are skipped rather than reported.
///
/// starCount is gift.star_count clamped to the int range, because the schema
/// types it as int53 while GiftInfo carries an int. fileId is the full sticker
/// file (gift.sticker.sticker.id) and falls back to the sticker thumbnail when
/// the full file is absent, so a caller always has something to download.
/// receivedGift/sentGift objects are not catalog entries and are not converted.
[[nodiscard]] QVector<GiftInfo> giftsFromResponse(const QJsonObject& response);

} // namespace beygram::telegram
