#include "MusicHistory.h"

#include <QSet>

namespace beygram::telegram {

MusicHistory::MusicHistory(QObject* parent)
    : QObject(parent)
{
}

void MusicHistory::record(const MusicPlayEvent& event, int maxEntries)
{
    MusicPlayEvent stored = event;
    stored.trackId = event.trackId.trimmed();
    if (stored.trackId.isEmpty() || stored.playedAt <= 0)
        return;
    stored.positionMs = qMax(0, event.positionMs);

    const int cap = qMax(0, maxEntries);
    if (cap == 0) {
        if (m_events.isEmpty())
            return;
        m_events.clear();
        emit changed();
        return;
    }

    // Insert before the first strictly older event, so the list stays newest
    // first and events with the same time keep their arrival order (the later
    // record comes first).
    const qsizetype oldSize = m_events.size();
    int index = 0;
    while (index < m_events.size() && m_events.at(index).playedAt > stored.playedAt)
        ++index;

    m_events.insert(index, stored);
    if (m_events.size() > cap)
        m_events.resize(cap);

    // The new event fell beyond the cap and was dropped, and a cap that is not
    // smaller than the old list kept every remaining entry: nothing changed.
    if (index >= cap && oldSize <= cap)
        return;

    emit changed();
}

QVector<MusicPlayEvent> MusicHistory::events() const
{
    return m_events;
}

QVector<QString> MusicHistory::recentlyPlayed(int limit) const
{
    QVector<QString> ids;
    if (limit <= 0)
        return ids;

    ids.reserve(qMin<qsizetype>(limit, m_events.size()));
    QSet<QString> seen;
    for (const MusicPlayEvent& event : m_events) {
        if (seen.contains(event.trackId))
            continue;
        seen.insert(event.trackId);
        ids.append(event.trackId);
        if (ids.size() >= limit)
            break;
    }
    return ids;
}

int MusicHistory::playCount(const QString& trackId) const
{
    const QString id = trackId.trimmed();
    if (id.isEmpty())
        return 0;

    int count = 0;
    for (const MusicPlayEvent& event : m_events) {
        if (event.trackId == id)
            ++count;
    }
    return count;
}

void MusicHistory::clear()
{
    if (m_events.isEmpty())
        return;

    m_events.clear();
    emit changed();
}

} // namespace beygram::telegram
