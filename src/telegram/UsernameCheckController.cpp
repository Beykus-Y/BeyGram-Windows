#include "UsernameCheckController.h"

namespace beygram::telegram {
namespace {

/// Telegram's own bounds for a username, in characters.
constexpr int kMinLength = 5;
constexpr int kMaxLength = 32;

/// Reduces what the user typed to the form TDLib stores: a leading '@', as
/// pasted from a t.me link, is dropped (repeatedly, because a copy can carry
/// more than one), the text is trimmed on both sides and lower cased. Trimming
/// runs again after the '@' so " @ alice " still ends up as "alice".
QString canonical(const QString& raw)
{
    QString text = raw.trimmed();
    while (text.startsWith(QLatin1Char('@')))
        text.remove(0, 1);
    return text.trimmed().toLower();
}

} // namespace

UsernameCheckController::UsernameCheckController(QObject* parent) : QObject(parent)
{
}

void UsernameCheckController::setInput(const QString& input)
{
    const QString text = canonical(input);

    // A repeated canonical text is not an edit: the request in flight or the
    // answer already on screen belongs to exactly this name, so restarting
    // would throw it away. Typing a letter and deleting it again lands here.
    if (text == m_input)
        return;

    m_input = text;
    if (m_input.isEmpty())
        m_status = Status::Idle;
    else if (validFormat(m_input))
        m_status = Status::Checking;
    else
        m_status = Status::Invalid;
    emit changed();
}

QString UsernameCheckController::input() const
{
    return m_input;
}

UsernameCheckController::Status UsernameCheckController::status() const
{
    return m_status;
}

bool UsernameCheckController::canSubmit() const
{
    return m_status == Status::Available;
}

void UsernameCheckController::setResult(bool available)
{
    // An answer only means something for the name that is being checked right
    // now. Anything else is a late reply to a question the user has left, and
    // showing it would label the wrong name.
    if (m_status != Status::Checking)
        return;

    m_status = available ? Status::Available : Status::Taken;
    emit changed();
}

void UsernameCheckController::reset()
{
    if (m_input.isEmpty() && m_status == Status::Idle)
        return;

    m_input.clear();
    m_status = Status::Idle;
    emit changed();
}

bool UsernameCheckController::validFormat(const QString& username)
{
    if (username.size() < kMinLength || username.size() > kMaxLength)
        return false;

    // A username may not start with a digit or an underscore, only a letter.
    const QChar first = username.at(0);
    if (first < QLatin1Char('a') || first > QLatin1Char('z'))
        return false;

    for (const QChar character : username) {
        const bool letter = character >= QLatin1Char('a') && character <= QLatin1Char('z');
        const bool digit = character >= QLatin1Char('0') && character <= QLatin1Char('9');
        if (!letter && !digit && character != QLatin1Char('_'))
            return false;
    }
    return true;
}

} // namespace beygram::telegram
