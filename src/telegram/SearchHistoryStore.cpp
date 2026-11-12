#include "SearchHistoryStore.h"

#include <QtGlobal>

namespace beygram::telegram {

SearchHistoryStore::SearchHistoryStore(QObject* parent)
    : QObject(parent)
{
}

int SearchHistoryStore::indexOf(const QString& query) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (QString::compare(m_entries.at(i), query, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

void SearchHistoryStore::add(const QString& query, int maxEntries)
{
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty())
        return;

    // A caller asking for zero rows still needs the query they just typed, so
    // the cap never drops below one.
    m_max = qMax(1, maxEntries);

    const QStringList before = m_entries;

    const int existing = indexOf(trimmed);
    if (existing >= 0)
        m_entries.removeAt(existing);
    m_entries.prepend(trimmed);

    while (m_entries.size() > m_max)
        m_entries.removeLast();

    // Re-adding the entry that is already first changes nothing visible.
    if (m_entries != before)
        emit changed();
}

QStringList SearchHistoryStore::entries() const
{
    return m_entries;
}

int SearchHistoryStore::count() const
{
    return int(m_entries.size());
}

bool SearchHistoryStore::remove(const QString& query)
{
    const int index = indexOf(query.trimmed());
    if (index < 0)
        return false;
    m_entries.removeAt(index);
    emit changed();
    return true;
}

QStringList SearchHistoryStore::matching(const QString& prefix) const
{
    QStringList result;
    result.reserve(m_entries.size());
    for (const QString& entry : m_entries) {
        if (entry.startsWith(prefix, Qt::CaseInsensitive))
            result.append(entry);
    }
    return result;
}

void SearchHistoryStore::clear()
{
    if (m_entries.isEmpty())
        return;
    m_entries.clear();
    emit changed();
}

} // namespace beygram::telegram
