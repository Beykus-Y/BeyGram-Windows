#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One answer of a poll. TDLib names an option by its 0-based position in
/// PollInfo::options, which is exactly what votePollRequest() sends, so the
/// order of this vector is meaningful and must never be compacted.
struct PollOption {
    QString text;
    int voterCount = 0;
    bool chosen = false;
};

/// The poll carried by a messagePoll message. Parsed from TDLib without
/// touching the UI, so the model layer can hold it directly.
struct PollInfo {
    qint64 id = 0;
    QString question;
    QVector<PollOption> options;
    int totalVoterCount = 0;
    bool isAnonymous = true;
    bool isClosed = false;
    bool isQuiz = false;
    bool multipleChoice = false;
    bool hasVoted = false;
};

/// Parses message["content"]["poll"] of a TDLib message.
///
/// Shape follows the pinned td_api.tl (TDLib 1.8.69):
///   messagePoll poll:Poll description:formattedText media:PollMedia
///               can_add_option:Bool = MessageContent;
///   poll id:int64 question:formattedText options:vector<pollOption>
///        total_voter_count:int32 is_anonymous:Bool
///        allows_multiple_answers:Bool option_order:vector<int32>
///        type:PollType is_closed:Bool = Poll;
///   pollOption id:string text:formattedText voter_count:int32
///              vote_percentage:int32 is_chosen:Bool = PollOption;
///
/// isQuiz reads poll.type == pollTypeQuiz and falls back to the legacy is_quiz
/// boolean that older revisions sent. hasVoted is derived as well: poll has no
/// has_voted flag, so it becomes true as soon as one option reports is_chosen.
///
/// options stay in TDLib's wire order because votes name options by that
/// position; poll.option_order is a display hint and is not applied here.
[[nodiscard]] PollInfo pollFromMessage(const QJsonObject& message);

/// Builds the poll-vote request. optionIds are 0-based positions in
/// PollInfo::options and are passed through unchanged, so the caller decides
/// about sorting or deduping.
///
/// TDLib 1.8.69 has no votePoll method, so this builds setPollAnswer:
///   setPollAnswer chat_id:int53 message_id:int53 option_ids:vector<int32> = Ok;
[[nodiscard]] QJsonObject votePollRequest(qint64 chatId, qint64 messageId,
                                          const QVector<int>& optionIds);

/// Builds a stopPoll request that closes a poll the user created.
///
///   stopPoll chat_id:int53 message_id:int53 reply_markup:ReplyMarkup = Ok;
///
/// reply_markup is part of the schema and is sent as null, meaning no new inline
/// keyboard is attached.
[[nodiscard]] QJsonObject stopPollRequest(qint64 chatId, qint64 messageId);

} // namespace beygram::telegram
