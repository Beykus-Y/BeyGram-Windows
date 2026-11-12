#include "StickerSearchController.h"

#include <QSet>
#include <utility>

namespace beygram::telegram {

StickerSearchController::StickerSearchController(QObject* parent) : QObject(parent)
{
}

void StickerSearchController::setQuery(const QString& query)
{
    const QString trimmed = query.trimmed();

    if (trimmed.isEmpty()) {
        if (m_query.isEmpty() && m_ids.isEmpty() && !m_loading)
            return;

        m_query.clear();
        m_ids.clear();
        m_loading = false;
        emit changed();
        return;
    }

    // The picker calls setQuery() on every keystroke, so typing inside the same
    // query must not throw away the results it already shows.
    if (m_query == trimmed)
        return;

    m_query = trimmed;
    m_ids.clear();
    m_loading = false;
    emit changed();
}

QString StickerSearchController::query() const
{
    return m_query;
}

void StickerSearchController::setResults(QVector<qint64> stickerIds)
{
    // A response that arrives after clear() or before setQuery() has no search
    // to attach to.
    if (m_query.isEmpty())
        return;

    QVector<qint64> ids;
    ids.reserve(stickerIds.size());
    QSet<qint64> seen;
    seen.reserve(stickerIds.size());

    for (const qint64 id : stickerIds) {
        if (id <= 0 || seen.contains(id))
            continue;
        seen.insert(id);
        ids.append(id);
    }

    if (ids == m_ids)
        return;

    m_ids = std::move(ids);
    emit changed();
}

QVector<qint64> StickerSearchController::results() const
{
    return m_ids;
}

void StickerSearchController::setLoading(bool loading)
{
    if (m_loading == loading)
        return;

    m_loading = loading;
    emit changed();
}

bool StickerSearchController::loading() const
{
    return m_loading;
}

void StickerSearchController::appendResults(QVector<qint64> stickerIds)
{
    if (m_query.isEmpty())
        return;

    QSet<qint64> seen;
    seen.reserve(m_ids.size() + stickerIds.size());
    for (const qint64 id : m_ids)
        seen.insert(id);

    QVector<qint64> added;
    added.reserve(stickerIds.size());

    for (const qint64 id : stickerIds) {
        if (id <= 0 || seen.contains(id))
            continue;
        seen.insert(id);
        added.append(id);
    }

    if (added.isEmpty())
        return;

    m_ids += added;
    emit changed();
}

void StickerSearchController::clear()
{
    if (m_query.isEmpty() && m_ids.isEmpty() && !m_loading)
        return;

    m_query.clear();
    m_ids.clear();
    m_loading = false;
    emit changed();
}

} // namespace beygram::telegram
