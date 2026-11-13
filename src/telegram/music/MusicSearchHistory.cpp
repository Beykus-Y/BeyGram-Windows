#include "MusicSearchHistory.h"

#include <utility>

namespace beygram::telegram {

MusicSearchHistory::MusicSearchHistory(QObject* parent)
    : QObject(parent)
{
}

void MusicSearchHistory::add(const QString& query, int maxEntries)
{
    const QString text = query.trimmed();
    if (text.isEmpty())
        return;

    const int cap = qMax(0, maxEntries);
    QStringList next;
    if (cap > 0) {
        next.reserve(qMin<qsizetype>(cap, m_entries.size() + 1));
        next.append(text);
        for (const QString& entry : std::as_const(m_entries)) {
            if (next.size() >= cap)
                break;
            if (entry.compare(text, Qt::CaseInsensitive) == 0)
                continue;
            next.append(entry);
        }
    }

    if (next == m_entries)
        return;

    m_entries = std::move(next);
    emit changed();
}

QStringList MusicSearchHistory::entries() const
{
    return m_entries;
}

QStringList MusicSearchHistory::matching(const QString& prefix) const
{
    const QString text = prefix.trimmed();
    if (text.isEmpty())
        return m_entries;

    QStringList matches;
    for (const QString& entry : m_entries) {
        if (entry.startsWith(text, Qt::CaseInsensitive))
            matches.append(entry);
    }
    return matches;
}

bool MusicSearchHistory::remove(const QString& query)
{
    const QString text = query.trimmed();
    if (text.isEmpty())
        return false;

    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).compare(text, Qt::CaseInsensitive) != 0)
            continue;

        m_entries.removeAt(i);
        emit changed();
        return true;
    }
    return false;
}

void MusicSearchHistory::clear()
{
    if (m_entries.isEmpty())
        return;

    m_entries.clear();
    emit changed();
}

bool miniPlayerVisible(const MiniPlayerState& state)
{
    return state.hasTrack && !state.sheetOpen && !state.fullScreen;
}

bool miniPlayerExpanded(const MiniPlayerState& state)
{
    return state.playing && !state.sheetOpen;
}

} // namespace beygram::telegram
