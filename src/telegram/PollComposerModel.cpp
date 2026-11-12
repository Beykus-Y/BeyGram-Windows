#include "PollComposerModel.h"

#include <QChar>

namespace beygram::telegram {
namespace {

// Telegram's limits for poll text, in UTF-16 units.
constexpr int kMaxQuestionLength = 255;
constexpr int kMaxOptionLength = 100;
constexpr int kMaxExplanationLength = 200;

constexpr int kMinOptions = 2;
constexpr int kMaxOptions = 10;

/// Truncates to at most limit UTF-16 units without splitting a surrogate pair,
/// so the stored text never ends in half of a character.
QString capped(const QString& text, int limit)
{
    if (text.size() <= limit)
        return text;

    QString result = text.left(limit);
    if (result.at(result.size() - 1).isHighSurrogate())
        result.chop(1);
    return result;
}

/// What the model stores for any user-supplied text: trimmed, then capped, so
/// trailing spaces cannot eat the length budget.
QString normalized(const QString& text, int limit)
{
    return capped(text.trimmed(), limit);
}

} // namespace

PollComposerModel::PollComposerModel(QObject* parent)
    : QObject(parent)
{
}

void PollComposerModel::setQuestion(const QString& question)
{
    const QString next = normalized(question, kMaxQuestionLength);
    if (next == m_question)
        return;

    m_question = next;
    emit changed();
}

QString PollComposerModel::question() const
{
    return m_question;
}

bool PollComposerModel::addOption(const QString& option)
{
    if (m_options.size() >= kMaxOptions)
        return false;

    const QString next = normalized(option, kMaxOptionLength);
    if (next.isEmpty() || m_options.contains(next, Qt::CaseSensitive))
        return false;

    m_options.append(next);
    emit changed();
    return true;
}

bool PollComposerModel::removeOption(int index)
{
    if (index < 0 || index >= m_options.size())
        return false;

    m_options.removeAt(index);

    // The answer is an index into the list, so it has to stay inside it. An
    // answer that fell past the new end goes back to the first option.
    if (m_correct >= m_options.size())
        m_correct = 0;

    emit changed();
    return true;
}

bool PollComposerModel::setOption(int index, const QString& option)
{
    if (index < 0 || index >= m_options.size())
        return false;

    const QString next = normalized(option, kMaxOptionLength);
    if (next.isEmpty())
        return false;

    for (qsizetype i = 0; i < m_options.size(); ++i) {
        if (i != index && m_options.at(i) == next)
            return false;
    }

    if (m_options.at(index) == next)
        return true;

    m_options[index] = next;
    emit changed();
    return true;
}

QStringList PollComposerModel::options() const
{
    return m_options;
}

void PollComposerModel::setAnonymous(bool anonymous)
{
    if (m_anonymous == anonymous)
        return;

    m_anonymous = anonymous;
    emit changed();
}

bool PollComposerModel::anonymous() const
{
    return m_anonymous;
}

void PollComposerModel::setMultiple(bool multiple)
{
    if (m_multiple == multiple)
        return;

    m_multiple = multiple;
    emit changed();
}

bool PollComposerModel::multiple() const
{
    return m_multiple;
}

void PollComposerModel::setQuiz(bool quiz)
{
    if (m_quiz == quiz)
        return;

    // m_correct and m_explanation are left untouched: they belong to the poll
    // the user may switch back to.
    m_quiz = quiz;
    emit changed();
}

bool PollComposerModel::quiz() const
{
    return m_quiz;
}

void PollComposerModel::setCorrectOption(int index)
{
    if (index < 0 || index >= m_options.size())
        return;

    if (m_correct == index)
        return;

    m_correct = index;
    emit changed();
}

int PollComposerModel::correctOption() const
{
    return m_correct;
}

void PollComposerModel::setExplanation(const QString& explanation)
{
    const QString next = normalized(explanation, kMaxExplanationLength);
    if (next == m_explanation)
        return;

    m_explanation = next;
    emit changed();
}

QString PollComposerModel::explanation() const
{
    return m_explanation;
}

bool PollComposerModel::valid() const
{
    if (m_question.isEmpty())
        return false;

    if (m_options.size() < kMinOptions || m_options.size() > kMaxOptions)
        return false;

    for (const QString& option : m_options) {
        if (option.isEmpty())
            return false;
    }

    if (m_quiz && (m_correct < 0 || m_correct >= m_options.size()))
        return false;

    return true;
}

OutgoingPoll PollComposerModel::build() const
{
    OutgoingPoll poll;
    poll.anonymous = m_anonymous;
    poll.multiple = m_multiple;
    poll.quiz = m_quiz;
    poll.explanation = m_explanation;

    // An incomplete composition becomes an empty poll, which sendPollRequest
    // rejects, instead of a poll with a question but no answers.
    if (!valid())
        return poll;

    poll.question = m_question;
    poll.options = m_options;
    // A regular poll never carries an answer, even if one is stored.
    poll.correctOption = m_quiz ? m_correct : -1;
    return poll;
}

void PollComposerModel::reset()
{
    if (m_question.isEmpty() && m_options.isEmpty() && m_anonymous && !m_multiple && !m_quiz
        && m_correct == 0 && m_explanation.isEmpty()) {
        return;
    }

    m_question.clear();
    m_options.clear();
    m_anonymous = true;
    m_multiple = false;
    m_quiz = false;
    m_correct = 0;
    m_explanation.clear();

    emit changed();
}

} // namespace beygram::telegram
