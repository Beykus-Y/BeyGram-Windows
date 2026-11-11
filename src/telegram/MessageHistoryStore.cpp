#include "MessageHistoryStore.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

/// Removed ids are remembered so a late history page cannot resurrect a
/// message the user already deleted. The oldest entries are forgotten first,
/// matching the Android client's 256-id window.
constexpr int MaxTombstones = 256;

/// Display order: chronological, ties broken by id so the result is stable.
bool olderFirst(const MessageData& left, const MessageData& right)
{
    if (left.date != right.date)
        return left.date < right.date;
    return left.id < right.id;
}

} // namespace

MessageHistoryStore::MessageHistoryStore(QObject* parent) : QObject(parent)
{
}

int MessageHistoryStore::count() const
{
    return static_cast<int>(m_messages.size());
}

QVector<MessageData> MessageHistoryStore::messages() const
{
    return m_messages;
}

qint64 MessageHistoryStore::firstMessageId() const
{
    return m_messages.isEmpty() ? 0 : m_messages.constFirst().id;
}

qint64 MessageHistoryStore::lastMessageId() const
{
    return m_messages.isEmpty() ? 0 : m_messages.constLast().id;
}

void MessageHistoryStore::clear()
{
    if (m_messages.isEmpty() && m_tombstones.isEmpty())
        return;

    m_messages.clear();
    m_tombstones.clear();
    emit changed();
}

void MessageHistoryStore::setMessages(QVector<MessageData> messages)
{
    QVector<MessageData> next = dedupe(std::move(messages));
    QVector<MessageData> kept;
    kept.reserve(next.size());
    for (const MessageData& message : next) {
        if (!isTombstoned(message.id))
            kept.append(message);
    }
    sortMessages(kept);

    const bool hadRows = !m_messages.isEmpty();
    m_messages = std::move(kept);
    if (hadRows || !m_messages.isEmpty())
        emit changed();
}

void MessageHistoryStore::addOrUpdate(const MessageData& message)
{
    if (message.id == 0 || isTombstoned(message.id))
        return;

    const int existing = indexForId(message.id);
    if (existing >= 0)
        m_messages[existing] = message;
    else
        m_messages.append(message);

    sortMessages(m_messages);
    emit changed();
}

void MessageHistoryStore::replaceId(qint64 oldId, const MessageData& message)
{
    if (message.id == 0)
        return;

    bool touched = false;
    if (oldId != 0 && oldId != message.id) {
        // Retire the temporary id before the confirmed one is inserted, so a
        // page that still carries the pending row cannot re-add it later.
        retire(oldId);
        touched = true;
        const int existing = indexForId(oldId);
        if (existing >= 0)
            m_messages.removeAt(existing);
    }

    if (isTombstoned(message.id)) {
        if (touched)
            emit changed();
        return;
    }

    const int existing = indexForId(message.id);
    if (existing >= 0)
        m_messages[existing] = message;
    else
        m_messages.append(message);

    sortMessages(m_messages);
    emit changed();
}

void MessageHistoryStore::removeIds(const QVector<qint64>& ids)
{
    if (ids.isEmpty())
        return;

    bool touched = false;
    for (const qint64 id : ids) {
        if (id == 0)
            continue;
        if (retire(id))
            touched = true;
        const int existing = indexForId(id);
        if (existing >= 0) {
            m_messages.removeAt(existing);
            touched = true;
        }
    }

    if (touched)
        emit changed();
}

void MessageHistoryStore::mergeOlder(QVector<MessageData> messages)
{
    QVector<MessageData> next = dedupe(std::move(messages));
    if (next.isEmpty())
        return;

    QSet<qint64> loaded;
    loaded.reserve(m_messages.size());
    for (const MessageData& message : m_messages)
        loaded.insert(message.id);

    // A loaded row wins over the page copy: an edit applied while the page was
    // in flight must not be rolled back by the older snapshot.
    bool added = false;
    for (const MessageData& message : next) {
        if (loaded.contains(message.id) || isTombstoned(message.id))
            continue;
        loaded.insert(message.id);
        m_messages.append(message);
        added = true;
    }

    if (!added)
        return;

    sortMessages(m_messages);
    emit changed();
}

bool MessageHistoryStore::contains(qint64 id) const
{
    return indexForId(id) >= 0;
}

int MessageHistoryStore::indexForId(qint64 id) const
{
    for (qsizetype row = 0; row < m_messages.size(); ++row) {
        if (m_messages.at(row).id == id)
            return static_cast<int>(row);
    }
    return -1;
}

bool MessageHistoryStore::isTombstoned(qint64 id) const
{
    return id != 0 && m_tombstones.contains(id);
}

bool MessageHistoryStore::retire(qint64 id)
{
    if (id == 0 || m_tombstones.contains(id))
        return false;

    m_tombstones.append(id);
    const qsizetype excess = m_tombstones.size() - MaxTombstones;
    if (excess > 0)
        m_tombstones.remove(0, excess);
    return true;
}

void MessageHistoryStore::sortMessages(QVector<MessageData>& messages)
{
    std::sort(messages.begin(), messages.end(), olderFirst);
}

QVector<MessageData> MessageHistoryStore::dedupe(QVector<MessageData> messages)
{
    QHash<qint64, int> positions;
    QVector<MessageData> unique;
    unique.reserve(messages.size());

    for (const MessageData& message : messages) {
        if (message.id == 0)
            continue;

        const auto it = positions.constFind(message.id);
        if (it != positions.cend()) {
            unique[*it] = message;
            continue;
        }

        positions.insert(message.id, static_cast<int>(unique.size()));
        unique.append(message);
    }

    return unique;
}

} // namespace beygram::telegram
