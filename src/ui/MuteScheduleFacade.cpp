#include "ui/MuteScheduleFacade.h"

#include <QDateTime>
#include <QString>
#include <QStringList>

namespace beygram::ui {
namespace {

constexpr qint64 kForever = -1;
constexpr qint64 kHourSeconds = 60 * 60;
constexpr qint64 kDaySeconds = 24 * kHourSeconds;
constexpr qint64 kWeekSeconds = 7 * kDaySeconds;

// The fixed sheet order. choose() recognizes exactly these tokens; the QML
// sheet is the only place they are turned into display text.
const QStringList& optionTokens()
{
    static const QStringList tokens = {
        QStringLiteral("1h"),
        QStringLiteral("8h"),
        QStringLiteral("1d"),
        QStringLiteral("1w"),
        QStringLiteral("forever"),
        QStringLiteral("off"),
    };
    return tokens;
}

// A TDLib chat id from the text QML holds. Zero and malformed text never name
// a chat, so both read as "no chat" instead of being guessed at.
qint64 parseChatId(const QString& text)
{
    bool ok = false;
    const qint64 id = text.trimmed().toLongLong(&ok);
    return ok && id > 0 ? id : 0;
}

// The expiry a token means at `now`. Returns false for a token the sheet does
// not offer, leaving `until` untouched.
bool expiryForOption(const QString& option, qint64 now, qint64* until)
{
    if (option == QStringLiteral("1h")) {
        *until = now + kHourSeconds;
        return true;
    }
    if (option == QStringLiteral("8h")) {
        *until = now + 8 * kHourSeconds;
        return true;
    }
    if (option == QStringLiteral("1d")) {
        *until = now + kDaySeconds;
        return true;
    }
    if (option == QStringLiteral("1w")) {
        *until = now + kWeekSeconds;
        return true;
    }
    if (option == QStringLiteral("forever")) {
        *until = kForever;
        return true;
    }
    if (option == QStringLiteral("off")) {
        *until = 0;
        return true;
    }
    return false;
}

} // namespace

MuteScheduleFacade::MuteScheduleFacade(QObject* parent)
    : QObject(parent)
    , m_schedule(this)
{
}

QString MuteScheduleFacade::label() const
{
    // No chat is the same visible state as an unmuted one: the sheet has
    // nothing to report, and the schedule's own spelling keeps the two paths
    // consistent.
    if (m_chatKey <= 0)
        return QStringLiteral("Unmuted");

    return m_schedule.label(m_chatKey, QDateTime::currentSecsSinceEpoch());
}

bool MuteScheduleFacade::muted() const
{
    return m_chatKey > 0
        && m_schedule.isMuted(m_chatKey, QDateTime::currentSecsSinceEpoch());
}

QStringList MuteScheduleFacade::options() const
{
    return optionTokens();
}

void MuteScheduleFacade::setChat(const QString& chatId, qint64 mutedUntil)
{
    const QString text = chatId.trimmed();
    const qint64 key = parseChatId(text);

    // An unusable id loads no chat. clear() also drops the previous record, so
    // a later choose() cannot act on a chat the sheet no longer shows.
    if (key <= 0) {
        clear();
        return;
    }

    // A malformed expiry must not replace a working mute with "forever" or
    // with an unmuted state; the whole load is ignored instead.
    if (mutedUntil < kForever)
        return;

    if (key == m_chatKey) {
        // Same chat: only a real move of the stored expiry is a change. The
        // schedule suppresses a redundant write itself, but the facade has to
        // know whether to announce it.
        if (m_schedule.mutedUntil(key) == mutedUntil)
            return;

        m_schedule.setMutedUntil(key, mutedUntil);
        emit changed();
        return;
    }

    // Only one chat is loaded, so the previous record is forgotten rather than
    // left behind to answer for a chat the sheet no longer shows.
    if (m_chatKey > 0)
        m_schedule.clearChat(m_chatKey);

    m_chatId = text;
    m_chatKey = key;
    m_schedule.setMutedUntil(key, mutedUntil);
    emit changed();
}

void MuteScheduleFacade::choose(const QString& option)
{
    if (m_chatKey <= 0)
        return;

    qint64 until = 0;
    if (!expiryForOption(option, QDateTime::currentSecsSinceEpoch(), &until))
        return;

    // A choice that matches the stored expiry has nothing to persist. Checking
    // before the write also keeps the schedule from seeing a redundant change.
    const qint64 previous = m_schedule.mutedUntil(m_chatKey);
    if (previous == until)
        return;

    m_schedule.setMutedUntil(m_chatKey, until);

    emit muteRequested(m_chatId, until);
    emit changed();
}

void MuteScheduleFacade::clear()
{
    if (m_chatKey <= 0)
        return;

    m_schedule.clear();
    m_chatId.clear();
    m_chatKey = 0;
    emit changed();
}

} // namespace beygram::ui
