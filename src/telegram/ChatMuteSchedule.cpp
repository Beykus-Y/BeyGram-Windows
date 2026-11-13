#include "ChatMuteSchedule.h"

namespace beygram::telegram {
namespace {

constexpr qint64 kForever = -1;
constexpr quint64 kMinuteSeconds = 60;
constexpr quint64 kHourSeconds = 60 * kMinuteSeconds;
constexpr quint64 kDaySeconds = 24 * kHourSeconds;

// -1 is the only negative value that means "forever". Anything else below zero
// is not an expiry this model understands, and guessing "forever" for it would
// let a malformed value silence a chat.
bool isSupportedUntil(qint64 untilUnix)
{
    return untilUnix == kForever || untilUnix >= 0;
}

} // namespace

ChatMuteSchedule::ChatMuteSchedule(QObject* parent)
    : QObject(parent)
{
}

void ChatMuteSchedule::setMutedUntil(qint64 chatId, qint64 untilUnix)
{
    if (chatId <= 0 || !isSupportedUntil(untilUnix))
        return;

    const auto existing = m_mutedUntil.constFind(chatId);
    const bool stored = existing != m_mutedUntil.cend();

    // Zero is "unmuted", so it is represented by the absence of a record
    // rather than by a stored zero.
    if (untilUnix == 0) {
        if (!stored)
            return;
        m_mutedUntil.remove(chatId);
        emit changed();
        return;
    }

    if (stored && *existing == untilUnix)
        return;

    m_mutedUntil.insert(chatId, untilUnix);
    emit changed();
}

qint64 ChatMuteSchedule::mutedUntil(qint64 chatId) const
{
    if (chatId <= 0)
        return 0;
    return m_mutedUntil.value(chatId, 0);
}

bool ChatMuteSchedule::isMuted(qint64 chatId, qint64 nowUnix) const
{
    if (chatId <= 0)
        return false;

    const auto it = m_mutedUntil.constFind(chatId);
    if (it == m_mutedUntil.cend())
        return false;

    const qint64 until = *it;
    if (until == kForever)
        return true;

    // A timed mute is over at exactly its expiry second. The `until > 0` guard
    // keeps a zero or otherwise malformed record from reporting a mute when
    // nowUnix itself is negative.
    if (until > 0 && until > nowUnix)
        return true;

    // The entry has expired, so it is dropped here instead of lingering until a
    // full sweep. This is silent: the chat reports unmuted either way.
    m_mutedUntil.remove(chatId);
    return false;
}

QString ChatMuteSchedule::label(qint64 chatId, qint64 nowUnix) const
{
    if (chatId <= 0)
        return QStringLiteral("Unmuted");

    const auto it = m_mutedUntil.constFind(chatId);
    if (it == m_mutedUntil.cend())
        return QStringLiteral("Unmuted");

    const qint64 until = *it;
    if (until == kForever)
        return QStringLiteral("Muted forever");

    if (until <= 0 || until <= nowUnix) {
        m_mutedUntil.remove(chatId);
        return QStringLiteral("Unmuted");
    }

    // Both operands are qint64, so their signed difference can overflow when
    // nowUnix is far in the past. Subtracting as unsigned yields the exact
    // distance whenever until > now, which is the only case that reaches here.
    const quint64 remaining =
        static_cast<quint64>(until) - static_cast<quint64>(nowUnix);

    if (remaining >= kDaySeconds)
        return QStringLiteral("Muted for %1d").arg(remaining / kDaySeconds);
    if (remaining >= kHourSeconds)
        return QStringLiteral("Muted for %1h").arg(remaining / kHourSeconds);

    // Less than a whole minute is still a running mute, so it is shown as 1m
    // rather than as a contradictory "Muted for 0m".
    const quint64 minutes = remaining / kMinuteSeconds;
    return QStringLiteral("Muted for %1m").arg(minutes == 0 ? 1 : minutes);
}

void ChatMuteSchedule::clearChat(qint64 chatId)
{
    if (chatId <= 0)
        return;

    if (m_mutedUntil.remove(chatId))
        emit changed();
}

void ChatMuteSchedule::clear()
{
    if (m_mutedUntil.isEmpty())
        return;

    m_mutedUntil.clear();
    emit changed();
}

} // namespace beygram::telegram
