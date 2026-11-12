#include "ChatMemory.h"

#include <algorithm>

namespace beygram::telegram {
namespace {

bool sameEntry(const ChatMemoryEntry& left, const ChatMemoryEntry& right)
{
    return left.chatId == right.chatId
        && left.title == right.title
        && left.lastPreview == right.lastPreview
        && left.unreadCount == right.unreadCount
        && left.lastMessageDate == right.lastMessageDate
        && left.updatedAt == right.updatedAt;
}

} // namespace

ChatMemory::ChatMemory(QObject* parent)
    : QObject(parent)
{
}

void ChatMemory::setCapacity(int capacity)
{
    const int clamped = qBound(kMinimumCapacity, capacity, kMaximumCapacity);
    if (clamped == m_capacity)
        return;

    m_capacity = clamped;
    evictOverflow();
    emit changed();
}

int ChatMemory::capacity() const
{
    return m_capacity;
}

void ChatMemory::remember(const ChatMemoryEntry& entry, qint64 nowMs)
{
    // The chat id is the identity of the row; TDLib never sends zero or a
    // negative one, and an entry without an identity cannot be looked up.
    if (entry.chatId <= 0)
        return;

    ChatMemoryEntry stored = entry;
    stored.updatedAt = nowMs;

    const int existing = indexOf(stored.chatId);
    if (existing >= 0) {
        if (sameEntry(m_entries.at(existing), stored))
            return;
        m_entries[existing] = stored;
    } else {
        m_entries.append(stored);
    }

    const bool added = existing < 0;
    evictOverflow();

    // A newcomer older than everything already stored is itself the victim, so
    // the eviction removed exactly what this call added: nothing really changed.
    if (added && !contains(stored.chatId))
        return;

    emit changed();
}

ChatMemoryEntry ChatMemory::entry(qint64 chatId) const
{
    const int index = indexOf(chatId);
    return index >= 0 ? m_entries.at(index) : ChatMemoryEntry();
}

bool ChatMemory::contains(qint64 chatId) const
{
    return indexOf(chatId) >= 0;
}

QVector<ChatMemoryEntry> ChatMemory::recent(int limit) const
{
    const int wanted = qBound(1, limit, m_capacity);

    QVector<ChatMemoryEntry> sorted = m_entries;
    std::sort(sorted.begin(), sorted.end(),
              [](const ChatMemoryEntry& left, const ChatMemoryEntry& right) {
                  if (left.updatedAt != right.updatedAt)
                      return left.updatedAt > right.updatedAt;
                  // Equal recency reads by chat id, so the list does not depend
                  // on the order the chats happened to be remembered in.
                  return left.chatId < right.chatId;
              });

    if (sorted.size() > wanted)
        sorted.resize(wanted);
    return sorted;
}

void ChatMemory::forget(qint64 chatId)
{
    const int index = indexOf(chatId);
    if (index < 0)
        return;
    m_entries.removeAt(index);
    emit changed();
}

void ChatMemory::clear()
{
    if (m_entries.isEmpty())
        return;
    m_entries.clear();
    emit changed();
}

int ChatMemory::count() const
{
    return int(m_entries.size());
}

int ChatMemory::indexOf(qint64 chatId) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).chatId == chatId)
            return i;
    }
    return -1;
}

void ChatMemory::evictOverflow()
{
    while (m_entries.size() > m_capacity) {
        int victim = 0;
        for (int i = 1; i < m_entries.size(); ++i) {
            const ChatMemoryEntry& candidate = m_entries.at(i);
            const ChatMemoryEntry& oldest = m_entries.at(victim);
            if (candidate.updatedAt < oldest.updatedAt
                || (candidate.updatedAt == oldest.updatedAt
                    && candidate.chatId < oldest.chatId)) {
                victim = i;
            }
        }
        m_entries.removeAt(victim);
    }
}

} // namespace beygram::telegram
