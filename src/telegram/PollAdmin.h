#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Pure builders for poll administration: reading voters, adding an option and
/// stopping a poll. They contain no logging; poll option text is user content
/// and must never be written to the log or the diagnostics dump.
///
/// Shapes follow the TDLib schema pinned by this client:
///
///   getPollVoters chat_id:int53 message_id:int53 option_id:int32
///                 offset:int32 limit:int32 = PollVoters;
///   pollVoters total_count:int32 voters:vector<pollVoter> = PollVoters;
///   pollVoter voter_id:MessageSender date:int32 = PollVoter;
///   addPollOption chat_id:int53 message_id:int53
///                 option:inputPollOption = Ok;
///   inputPollOption text:formattedText media:InputPollMedia
///                   = InputPollOption;
///   stopPoll chat_id:int53 message_id:int53 reply_markup:ReplyMarkup = Ok;

/// Builds getPollVoters for one option. optionId is the 0-based position in the
/// poll's option vector and is passed through unchanged; the caller owns
/// checking it against the current poll.
///
/// The schema requires offset to be non-negative and limit to be positive and
/// no greater than 50, so offset is clamped to at least 0 and limit to 1..50.
/// TDLib may still return fewer voters than limit even when more remain.
[[nodiscard]] QJsonObject getPollVotersRequest(qint64 chatId, qint64 messageId,
                                               int optionId, int offset, int limit);

/// Builds addPollOption with the option text wrapped in a formattedText and a
/// null media, which is what the schema requires. The text is passed through
/// unchanged: trimming, length validation and the can_add_option check belong
/// to the caller, which can re-read the message before sending.
[[nodiscard]] QJsonObject addPollOptionRequest(qint64 chatId, qint64 messageId,
                                               const QString& option);

/// Builds stopPoll that closes a poll the user is allowed to stop. reply_markup
/// is part of the schema and is sent as null, so no inline keyboard is attached.
///
/// Named stopPollAdminRequest because PollInfo.h already declares
/// stopPollRequest for the same TDLib method; both build an identical request.
[[nodiscard]] QJsonObject stopPollAdminRequest(qint64 chatId, qint64 messageId);

/// Extracts the voter sender ids from a pollVoters answer, in wire order.
///
/// A pollVoter.voter_id is either messageSenderUser or messageSenderChat, and
/// this vector cannot carry the variant, so the id follows the same rule as the
/// Android client: a positive user_id wins, otherwise chat_id is returned (it
/// is negative for supergroups and channels). Entries with no usable id, or
/// whose voter_id is not an object, are skipped. A missing or malformed voters
/// array yields an empty vector; duplicates are preserved so paging code can
/// de-duplicate across pages itself.
[[nodiscard]] QVector<qint64> voterIdsFromResponse(const QJsonObject& response);

} // namespace beygram::telegram
