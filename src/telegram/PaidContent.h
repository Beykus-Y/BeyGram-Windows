#pragma once

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One paid-media bundle, reduced to what the chat bubble and the unlock
/// request need. The bundle is the content of a messagePaidMedia message:
///
///   messagePaidMedia star_count:int53 media:vector<PaidMedia>
///                    caption:formattedText show_caption_above_media:Bool
///                    = MessageContent;
///
/// starCount is the price TDLib reports for the whole bundle, not for one
/// part. partCount is the number of media parts in the bundle. messageIds is
/// optional metadata: a messagePaidMedia content normally carries no message
/// ids, but a wrapper returned for a bundle may carry message_ids, and the
/// same positive/deduped/capped rules as getPaidMediaRequest() apply to it.
struct PaidMediaInfo {
    qint64 starCount = 0;
    int partCount = 0;
    QVector<qint64> messageIds;
};

/// Parses the content object of a messagePaidMedia message.
///
/// A content object is accepted when it is tagged messagePaidMedia or carries
/// no @type at all; any other content type is not paid media and returns the
/// default-constructed PaidMediaInfo. The parse is all-or-nothing: a missing or
/// non-positive star_count, a missing media array, an empty media array, or a
/// media array with a non-object entry is malformed content and returns the
/// default value rather than a partially filled bundle.
///
/// message_ids is optional and never makes otherwise valid content malformed.
/// Its entries are read with the same rules as getPaidMediaRequest(): only
/// positive ids survive, duplicates keep their first position, and at most 100
/// ids are kept.
[[nodiscard]] PaidMediaInfo paidMediaFromContent(const QJsonObject& content);

/// Builds a getPaidMedia request for the paid-media bundle named by
/// messageIds in chatId. onlyPreviews asks TDLib for the blurred previews only
/// instead of the purchased media; it is sent as only_preview.
///
/// A non-positive chatId names no chat, so the builder returns an empty object,
/// which the caller must not send. messageIds are normalized before they are
/// put on the wire: non-positive ids are dropped, duplicates keep their first
/// position, and at most 100 ids are kept. If no usable id remains, there is no
/// bundle to request and the builder returns an empty object. The input order
/// of the surviving ids is preserved.
[[nodiscard]] QJsonObject getPaidMediaRequest(qint64 chatId,
                                              const QVector<qint64>& messageIds,
                                              bool onlyPreviews);

/// Builds openPaidReactionPrivacy, which takes no arguments. Opening the
/// privacy screen spends nothing; it only lets the user inspect or change the
/// paid-reaction privacy setting.
[[nodiscard]] QJsonObject openPaidReactionPrivacyRequest();

} // namespace beygram::telegram
