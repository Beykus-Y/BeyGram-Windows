#include "PollInfo.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>

namespace beygram::telegram {
namespace {

/// Reads the plain text of a TDLib formattedText object. A bare string is
/// accepted too because older poll revisions carried question and option text
/// as plain strings.
QString formattedTextPlain(const QJsonValue& value)
{
    if (value.isString())
        return value.toString();
    return value.toObject().value(QStringLiteral("text")).toString();
}

/// TDLib 1.8.69 describes a quiz with poll.type == pollTypeQuiz. Older
/// revisions used a bare is_quiz flag, which is still honoured, and an unknown
/// pollType* class falls back to that flag rather than guessing.
bool quizFlag(const QJsonObject& poll)
{
    const QString pollType = poll.value(QStringLiteral("type"))
                                 .toObject()
                                 .value(QStringLiteral("@type"))
                                 .toString();
    if (pollType == QLatin1String("pollTypeQuiz"))
        return true;
    if (pollType == QLatin1String("pollTypeRegular"))
        return false;
    return poll.value(QStringLiteral("is_quiz")).toBool(false);
}

PollOption optionFrom(const QJsonObject& entry)
{
    PollOption option;
    option.text = formattedTextPlain(entry.value(QStringLiteral("text")));

    const int voterCount = entry.value(QStringLiteral("voter_count")).toInt(0);
    option.voterCount = voterCount > 0 ? voterCount : 0;
    option.chosen = entry.value(QStringLiteral("is_chosen")).toBool(false);
    return option;
}

} // namespace

PollInfo pollFromMessage(const QJsonObject& message)
{
    PollInfo poll;

    const QJsonValue pollValue =
        message.value(QStringLiteral("content")).toObject().value(QStringLiteral("poll"));
    if (!pollValue.isObject())
        return poll;

    const QJsonObject source = pollValue.toObject();
    poll.id = jsonInt64(source.value(QStringLiteral("id")));
    poll.question = formattedTextPlain(source.value(QStringLiteral("question")));

    const QJsonArray options = source.value(QStringLiteral("options")).toArray();
    poll.options.reserve(options.size());
    for (const QJsonValue& value : options) {
        // Only a non-object entry is skipped: it carries no position TDLib could
        // vote on, and real polls always send pollOption objects.
        if (!value.isObject())
            continue;
        poll.options.append(optionFrom(value.toObject()));
    }

    const int totalVoterCount = source.value(QStringLiteral("total_voter_count")).toInt(0);
    poll.totalVoterCount = totalVoterCount > 0 ? totalVoterCount : 0;

    // TDLib defaults is_anonymous to true, so an omitted flag still means an
    // anonymous poll.
    poll.isAnonymous = source.value(QStringLiteral("is_anonymous")).toBool(true);
    poll.isClosed = source.value(QStringLiteral("is_closed")).toBool(false);
    poll.isQuiz = quizFlag(source);
    poll.multipleChoice = source.value(QStringLiteral("allows_multiple_answers")).toBool(false);

    for (const PollOption& option : poll.options) {
        if (option.chosen) {
            poll.hasVoted = true;
            break;
        }
    }

    return poll;
}

QJsonObject votePollRequest(qint64 chatId, qint64 messageId, const QVector<int>& optionIds)
{
    QJsonArray ids;
    for (const int optionId : optionIds)
        ids.append(optionId);

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setPollAnswer")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
        {QStringLiteral("option_ids"), ids},
    };
}

QJsonObject stopPollRequest(qint64 chatId, qint64 messageId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("stopPoll")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
        {QStringLiteral("reply_markup"), QJsonValue::Null},
    };
}

} // namespace beygram::telegram
