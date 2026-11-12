#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

// Builders for sending a poll as a message. They only shape TDLib's JSON
// payload: no logging, no @extra (the transport adds the correlation id) and no
// session state. Question, option and explanation text is user content and must
// never be logged by callers.
//
// Shape follows the pinned td_api.tl (TDLib 1.8.69):
//   sendMessage chat_id:int53 topic_id:MessageTopic reply_to:InputMessageReplyTo
//       options:messageSendOptions reply_markup:ReplyMarkup
//       input_message_content:InputMessageContent = Message;
//   inputMessagePoll question:formattedText options:vector<inputPollOption>
//       description:formattedText media:InputPollMedia is_anonymous:Bool
//       allows_multiple_answers:Bool allows_revoting:Bool members_only:Bool
//       country_codes:vector<string> shuffle_options:Bool
//       hide_results_until_closes:Bool type:InputPollType open_period:int32
//       close_date:int32 is_closed:Bool = InputMessageContent;
//   inputPollOption text:formattedText media:InputPollMedia = InputPollOption;
//   inputPollTypeRegular allow_adding_options:Bool = InputPollType;
//   inputPollTypeQuiz correct_option_ids:vector<int32>
//       explanation:formattedText explanation_media:InputPollMedia
//       = InputPollType;

namespace beygram::telegram {

/// A poll composed in the chat before it is handed to TDLib.
///
/// question and each option are trimmed; options stay in their original order
/// because correctOption is an index into that order. correctOption is a
/// 0-based position in options and is only meaningful for a quiz; it is
/// ignored for a regular poll. explanation is only sent with a quiz.
struct OutgoingPoll {
    QString question;
    QVector<QString> options;
    bool anonymous = true;
    bool multiple = false;
    bool quiz = false;
    int correctOption = -1;
    QString explanation;
};

/// Builds a sendMessage request whose input_message_content is an
/// inputMessagePoll.
///
/// The poll is rejected (an empty QJsonObject is returned) when the trimmed
/// question is empty, when there are fewer than 2 or more than 10 options, when
/// a trimmed option is empty, or when a quiz has no valid correctOption index
/// (0 <= correctOption < options.size()). A regular poll never carries the
/// correct answer even if correctOption is set.
///
/// TDLib's fixed field values mirror the Android client: quizzes always force
/// is_anonymous and disable allows_revoting, regular polls allow revoting,
/// option media and poll description/media are null, and country_codes is
/// empty. A blank quiz explanation is sent as null. open_period, close_date and
/// is_closed stay 0/false so Telegram applies its defaults.
[[nodiscard]] QJsonObject sendPollRequest(qint64 chatId, const OutgoingPoll& poll);

} // namespace beygram::telegram
