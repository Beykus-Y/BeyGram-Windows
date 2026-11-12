#include "SearchFacade.h"

#include "telegram/GlobalSearchController.h"
#include "telegram/SearchHistoryStore.h"

#include <QSignalBlocker>
#include <QtGlobal>

#include <utility>

namespace beygram::ui {
namespace {

/// The bar keeps the newest 20 queries; older ones fall off the end.
constexpr int kMaxHistoryEntries = 20;

} // namespace

SearchFacade::SearchFacade(QObject* parent)
    : QObject(parent),
      m_history(new telegram::SearchHistoryStore(this)),
      m_search(new telegram::GlobalSearchController(this))
{
    // Both children suppress no-op updates themselves, but their signals are
    // still a path through which a change can reach QML.
    connect(m_history, &telegram::SearchHistoryStore::changed,
            this, &SearchFacade::syncChanged);
    connect(m_search, &telegram::GlobalSearchController::changed,
            this, &SearchFacade::syncChanged);

    // Seed the diff so the empty state the bar starts in stays silent.
    m_visible = visibleState();
}

QString SearchFacade::query() const
{
    return m_search->query();
}

QStringList SearchFacade::history() const
{
    return m_history->entries();
}

QStringList SearchFacade::suggestions() const
{
    const QString current = m_search->query();
    if (current.isEmpty())
        return {};
    return m_history->matching(current);
}

bool SearchFacade::loading() const
{
    return m_search->loading();
}

bool SearchFacade::hasMore() const
{
    return m_search->hasMore();
}

QString SearchFacade::summary() const
{
    return QStringLiteral("%1 chats, %2 messages, %3 files")
        .arg(m_chatCount)
        .arg(m_messageCount)
        .arg(m_fileCount);
}

void SearchFacade::begin(const QString& query)
{
    // Trim here as well, not just in the controller, so the counts can be
    // dropped before the new query is announced: no listener may see the new
    // query with the previous page's counts.
    const QString applied = query.trimmed();

    // Both children would signal halfway through this submit. Hold those
    // signals so the query, the history and the counts land as one change
    // instead of an intermediate state.
    const QSignalBlocker blockSearch(m_search);
    const QSignalBlocker blockHistory(m_history);

    if (applied != m_search->query())
        forgetResultCounts();

    // The controller ignores a repeat of the same query, restarts pagination
    // for a different one, and clears itself for a blank one.
    m_search->begin(query);

    if (!applied.isEmpty())
        m_history->add(applied, kMaxHistoryEntries);

    syncChanged();

    if (applied.isEmpty())
        return;

    emit searchRequested(applied);
}

void SearchFacade::setLoading(bool loading)
{
    // A plain pass-through: the request layer owns when a request starts and
    // ends, and begin()/clear() drop a flag that belongs to an older query.
    m_search->setLoading(loading);
    syncChanged();
}

void SearchFacade::setPage(const QString& nextOffset, bool hasMore)
{
    m_search->setPage(nextOffset, hasMore);
    syncChanged();
}

void SearchFacade::clear()
{
    const QSignalBlocker blockSearch(m_search);

    forgetResultCounts();
    m_search->clear();

    syncChanged();
}

void SearchFacade::remember(const QString& query)
{
    // The store trims, ignores blanks and dedupes case-insensitively; it only
    // signals when the visible list actually changed.
    m_history->add(query, kMaxHistoryEntries);
    syncChanged();
}

void SearchFacade::removeHistory(const QString& query)
{
    m_history->remove(query);
    syncChanged();
}

void SearchFacade::clearHistory()
{
    m_history->clear();
    syncChanged();
}

void SearchFacade::setResultCounts(int chats, int messages, int files)
{
    // A negative count can only come from a caller bug; report it as zero
    // rather than a summary like "-1 chats".
    m_chatCount = qMax(0, chats);
    m_messageCount = qMax(0, messages);
    m_fileCount = qMax(0, files);
    syncChanged();
}

SearchFacade::VisibleState SearchFacade::visibleState() const
{
    return VisibleState{m_search->query(), m_history->entries(), suggestions(),
                        m_search->loading(), m_search->hasMore(),
                        m_chatCount, m_messageCount, m_fileCount};
}

void SearchFacade::syncChanged()
{
    VisibleState next = visibleState();
    if (next == m_visible)
        return;

    m_visible = std::move(next);
    emit changed();
}

void SearchFacade::forgetResultCounts()
{
    m_chatCount = 0;
    m_messageCount = 0;
    m_fileCount = 0;
}

} // namespace beygram::ui
