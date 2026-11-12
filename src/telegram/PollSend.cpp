#include "PollSend.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QString>

namespace beygram::telegram {
namespace {

constexpr int kMinPollOptions = 2;
constexpr int kMaxPollOptions = 10;

/// Plain formattedText without entities. Poll text is sent exactly as typed;
/// only surrounding whitespace is removed by the caller.
QJsonObject formattedText(const QString& text)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("formattedText")},
        {QStringLiteral("text"), text},
        {QStringLiteral("entities"), QJsonArray()},
    };
}

QJsonObject pollType(const OutgoingPoll& poll)
{
    if (!poll.quiz) {
        // allow_adding_options stays off: the composer has no "add option"
        // authoring flow, and Telegram only supports it for some polls.
        return QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("inputPollTypeRegular")},
            {QStringLiteral("allow_adding_options"), false},
        };
    }

    const QString explanation = poll.explanation.trimmed();
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("inputPollTypeQuiz")},
        // A single correct answer is always an increasing one-element list.
        {QStringLiteral("correct_option_ids"), QJsonArray{poll.correctOption}},
        {QStringLiteral("explanation"),
         explanation.isEmpty() ? QJsonValue(QJsonValue::Null)
                               : QJsonValue(formattedText(explanation))},
        {QStringLiteral("explanation_media"), QJsonValue::Null},
    };
}

QJsonObject pollContent(const OutgoingPoll& poll)
{
    QJsonArray options;
    for (const QString& raw : poll.options) {
        options.append(QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("inputPollOption")},
            {QStringLiteral("text"), formattedText(raw.trimmed())},
            {QStringLiteral("media"), QJsonValue::Null},
        });
    }

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("inputMessagePoll")},
        {QStringLiteral("question"), formattedText(poll.question.trimmed())},
        {QStringLiteral("options"), options},
        {QStringLiteral("description"), QJsonValue::Null},
        {QStringLiteral("media"), QJsonValue::Null},
        // Telegram quizzes are always anonymous; a regular poll keeps the
        // composer choice.
        {QStringLiteral("is_anonymous"), poll.anonymous || poll.quiz},
        {QStringLiteral("allows_multiple_answers"), poll.multiple},
        // A quiz can be answered once, a regular poll can be re-answered.
        {QStringLiteral("allows_revoting"), !poll.quiz},
        {QStringLiteral("members_only"), false},
        {QStringLiteral("country_codes"), QJsonArray()},
        {QStringLiteral("shuffle_options"), false},
        {QStringLiteral("hide_results_until_closes"), false},
        {QStringLiteral("type"), pollType(poll)},
        {QStringLiteral("open_period"), 0},
        {QStringLiteral("close_date"), 0},
        {QStringLiteral("is_closed"), false},
    };
}

} // namespace

QJsonObject sendPollRequest(qint64 chatId, const OutgoingPoll& poll)
{
    if (poll.question.trimmed().isEmpty())
        return {};

    if (poll.options.size() < kMinPollOptions || poll.options.size() > kMaxPollOptions)
        return {};

    for (const QString& option : poll.options) {
        if (option.trimmed().isEmpty())
            return {};
    }

    if (poll.quiz && (poll.correctOption < 0 || poll.correctOption >= poll.options.size()))
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("sendMessage")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("topic_id"), QJsonValue::Null},
        {QStringLiteral("reply_to"), QJsonValue::Null},
        {QStringLiteral("options"), QJsonValue::Null},
        {QStringLiteral("reply_markup"), QJsonValue::Null},
        {QStringLiteral("input_message_content"), pollContent(poll)},
    };
}

} // namespace beygram::telegram
