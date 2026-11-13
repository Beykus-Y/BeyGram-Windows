#include "PresenceAggregator.h"

#include <algorithm>

namespace beygram::telegram {
namespace {

int nonNegative(int value)
{
    return std::max(0, value);
}

// A record is kept only while it has something a row could paint. Counts are
// already clamped, so zero means "nobody is doing this".
bool isEmptyRecord(const ChatPresence& record)
{
    return record.onlineMembers == 0 && record.typingMembers == 0
        && record.recordingMembers == 0 && !record.botOnline
        && record.lastSeenText.isEmpty();
}

bool sameRecord(const ChatPresence& left, const ChatPresence& right)
{
    return left.onlineMembers == right.onlineMembers
        && left.typingMembers == right.typingMembers
        && left.recordingMembers == right.recordingMembers
        && left.botOnline == right.botOnline
        && left.lastSeenText == right.lastSeenText;
}

ChatPresence normalise(ChatPresence record)
{
    record.onlineMembers = nonNegative(record.onlineMembers);
    record.typingMembers = nonNegative(record.typingMembers);
    record.recordingMembers = nonNegative(record.recordingMembers);
    record.lastSeenText = record.lastSeenText.trimmed();
    return record;
}

QString typingLabel(int count)
{
    // "+N" names the other typists, matching "typing… +2" for three typists.
    if (count <= 1)
        return QStringLiteral("typing…");
    return QStringLiteral("typing… +%1").arg(count - 1);
}

} // namespace

PresenceAggregator::PresenceAggregator(QObject* parent)
    : QObject(parent)
{
}

void PresenceAggregator::setPresence(qint64 chatId, const ChatPresence& presence)
{
    if (chatId <= 0)
        return;

    const ChatPresence record = normalise(presence);
    const auto existing = m_presences.constFind(chatId);
    const bool stored = existing != m_presences.cend();

    if (isEmptyRecord(record)) {
        // Nothing to paint: an empty record is erased rather than stored, and
        // erasing one that was never there is not a change.
        if (!stored)
            return;
        m_presences.erase(m_presences.find(chatId));
        emit changed();
        return;
    }

    if (stored && sameRecord(*existing, record))
        return;

    m_presences.insert(chatId, record);
    emit changed();
}

ChatPresence PresenceAggregator::presence(qint64 chatId) const
{
    // QHash::value() default-constructs for a missing key, which is exactly the
    // empty presence a caller should see for a chat without one.
    return m_presences.value(chatId);
}

QString PresenceAggregator::statusText(qint64 chatId) const
{
    const auto it = m_presences.constFind(chatId);
    if (it == m_presences.cend())
        return QString();

    // Recording wins over typing, and typing over online: a chat shows the
    // strongest ongoing action, not a combination of them.
    if (it->recordingMembers > 0)
        return QStringLiteral("recording…");
    if (it->typingMembers > 0)
        return typingLabel(it->typingMembers);
    if (it->onlineMembers > 0)
        return QStringLiteral("%1 online").arg(it->onlineMembers);
    // A bot has no member count, so it can only say "online". It is checked
    // after onlineMembers, where a member count is more informative.
    if (it->botOnline)
        return QStringLiteral("online");
    return it->lastSeenText;
}

void PresenceAggregator::clearChat(qint64 chatId)
{
    if (chatId <= 0)
        return;

    if (m_presences.remove(chatId) > 0)
        emit changed();
}

void PresenceAggregator::clear()
{
    if (m_presences.isEmpty())
        return;

    m_presences.clear();
    emit changed();
}

} // namespace beygram::telegram
