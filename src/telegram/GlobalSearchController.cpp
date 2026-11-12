#include "GlobalSearchController.h"

#include <utility>

namespace beygram::telegram {

GlobalSearchController::GlobalSearchController(QObject* parent) : QObject(parent)
{
}

void GlobalSearchController::begin(const QString& query)
{
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        clear();
        return;
    }

    // begin() is called on every keystroke, so typing inside the same query
    // must not restart pagination or repaint the panel.
    if (m_query == trimmed && m_offset.isEmpty() && !m_more && !m_loading)
        return;

    m_query = trimmed;
    m_offset.clear();
    m_more = false;
    m_loading = false;
    emit changed();
}

QString GlobalSearchController::query() const
{
    return m_query;
}

QString GlobalSearchController::nextOffset() const
{
    return m_offset;
}

bool GlobalSearchController::hasMore() const
{
    return m_more;
}

bool GlobalSearchController::loading() const
{
    return m_loading;
}

void GlobalSearchController::setPage(QString nextOffset, bool hasMore)
{
    // A response that arrives after clear() or before begin() has no search to
    // attach to.
    if (m_query.isEmpty())
        return;

    if (m_offset == nextOffset && m_more == hasMore)
        return;

    m_offset = std::move(nextOffset);
    m_more = hasMore;
    emit changed();
}

void GlobalSearchController::setLoading(bool loading)
{
    if (m_loading == loading)
        return;

    m_loading = loading;
    emit changed();
}

void GlobalSearchController::clear()
{
    if (m_query.isEmpty() && m_offset.isEmpty() && !m_more && !m_loading)
        return;

    m_query.clear();
    m_offset.clear();
    m_more = false;
    m_loading = false;
    emit changed();
}

} // namespace beygram::telegram
