#pragma once

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One sender row of a paid-reaction leaderboard. senderUserId names the user
/// who sent the Stars; a reaction sent as a chat has no user id and is skipped
/// by the parser rather than being reported as user 0. starCount is clamped to
/// 0..10000. anonymous mirrors TDLib's is_anonymous flag: the sender does not
/// appear in the public leaderboard.
struct PaidReactionRow {
    qint64 senderUserId = 0;
    qint64 starCount = 0;
    bool anonymous = false;
};

/// Builds a sendPaidReaction request for one message.
///
/// chatId and messageId must both be positive; a non-positive id names no
/// message and returns an empty object, which the caller must not send.
/// starCount must be in 1..10000, the range a single paid reaction may carry.
/// A value outside that range is refused instead of being clamped, because
/// spending a different number of Stars than the user confirmed is never
/// acceptable.
///
/// anonymous selects whether the sender's name is hidden from the public
/// leaderboard; it is sent as is_anonymous.
[[nodiscard]] QJsonObject sendPaidReactionRequest(qint64 chatId, qint64 messageId,
                                                  qint64 starCount, bool anonymous);

/// Builds getPaidReactionPrivacy, which takes no arguments. The answer is the
/// account's default paid-reaction privacy; reading it spends nothing.
[[nodiscard]] QJsonObject getPaidReactionPrivacyRequest();

/// Parses the paid_reactions list of an updatePaidReactions update:
///
///   updatePaidReactions chat_id:int53 message_id:int53
///                        paid_reactions:vector<paidReaction> = Update;
///   paidReaction sender_id:MessageSender star_count:int53
///                is_anonymous:Bool = PaidReaction;
///
/// The parser accepts the list under update["paid_reactions"] and preserves its
/// order, including two rows from the same sender. Each row is read from a
/// messageSenderUser sender_id; the flattened sender_user_id (or user_id) form
/// is accepted as well, so a trimmed update still parses. The anonymous flag is
/// read from is_anonymous, with the flattened anonymous form as a fallback.
/// Rows with no positive user id (a null entry, a non-object, a
/// messageSenderChat sender, a missing sender) are malformed and are skipped. A
/// missing or non-array paid_reactions member yields an empty list.
///
/// star_count is clamped to 0..10000: a negative or missing count becomes 0 and
/// a larger count becomes 10000. The count is display data here; the send path
/// validates its own amount separately.
[[nodiscard]] QVector<PaidReactionRow> paidReactionsFromUpdate(const QJsonObject& update);

} // namespace beygram::telegram
