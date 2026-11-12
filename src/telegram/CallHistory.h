#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// One ended call carried by a messageCall message. Parsed from TDLib without
/// touching the UI, so the model layer can hold it directly.
///
/// Shape follows the pinned td_api.tl (TDLib 1.8.69):
///   messageCall unique_id:int64 is_video:Bool
///               discard_reason:CallDiscardReason duration:int32 = MessageContent;
///   call id:int32 unique_id:int64 user_id:int53 is_outgoing:Bool
///        is_video:Bool state:CallState = Call;
///   callDiscardReasonEmpty;
///   callDiscardReasonMissed;
///   callDiscardReasonDeclined;
///   callDiscardReasonDisconnected;
///   callDiscardReasonHungUp;
///   callDiscardReasonUpgradeToGroupCall invite_link:string;
///
/// 1.8.69 sends every call field flat on messageCall and no longer nests a
/// call object there, so outgoing is read from the message's own is_outgoing
/// flag. Older revisions that still nested call (with its state) are accepted
/// as a fallback for direction, video and the discard reason.
struct CallRecord {
    qint64 messageId = 0;
    qint64 chatId = 0;
    /// Direction of the call, from message.is_outgoing.
    bool outgoing = false;
    bool video = false;
    /// Outcome of the call. Empty when the message carries no call at all;
    /// otherwise one of:
    ///   "ended"        callDiscardReasonEmpty or callDiscardReasonHungUp
    ///   "missed"       callDiscardReasonMissed on an incoming call
    ///   "cancelled"    callDiscardReasonMissed on an outgoing call
    ///   "declined"     callDiscardReasonDeclined
    ///   "disconnected" callDiscardReasonDisconnected
    ///   "upgraded"     callDiscardReasonUpgradeToGroupCall
    ///   "failed"       a legacy call whose state is callStateError
    ///   "unknown"      no discard reason, or a callDiscardReason* this
    ///                  revision does not know
    QString status;
    /// Call duration in seconds as reported by TDLib; 0 when the message
    /// carries no duration and never negative.
    int duration = 0;
    qint64 date = 0;
};

/// Parses message["content"]["messageCall"] of a TDLib message. Returns a
/// default-constructed record (status empty) when the message is not a call.
[[nodiscard]] CallRecord callFromMessage(const QJsonObject& message);

/// Builds a TDLib searchCallMessages request that walks the account's call
/// history, newest first:
///   searchCallMessages offset:string limit:int32 only_missed:Bool = FoundMessages;
///
/// The limit is clamped to 1..100, the range the server accepts. only_missed
/// is false, so answered and missed calls are both returned.
///
/// fromMessageId is accepted for symmetry with the other search builders, but
/// the pinned schema has no from_message_id field here: the first chunk is
/// requested with an empty offset and the next page with the opaque
/// "next_offset" string that the FoundMessages response carries.
[[nodiscard]] QJsonObject searchCallMessagesRequest(qint64 fromMessageId, int limit);

} // namespace beygram::telegram
