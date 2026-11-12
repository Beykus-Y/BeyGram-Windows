#include "InlineQueryController.h"

#include <QSet>
#include <utility>

namespace beygram::telegram {
namespace {

// An answer is order preserving, so normalisation walks the input once: a row
// without an id cannot be selected or sent and is dropped, and the first row
// carrying an id wins over any later duplicate.
QVector<InlineResultRow> normalizedRows(const QVector<InlineResultRow>& rows)
{
    QVector<InlineResultRow> result;
    result.reserve(rows.size());
    QSet<QString> seen;
    for (const InlineResultRow& row : rows) {
        if (row.id.isEmpty() || seen.contains(row.id))
            continue;
        seen.insert(row.id);
        result.append(row);
    }
    return result;
}

// Rows carry display text, so "the same answer" means every field is equal, not
// just the ids: a bot that rewrites a title must refresh the view.
bool sameRows(const QVector<InlineResultRow>& left, const QVector<InlineResultRow>& right)
{
    if (left.size() != right.size())
        return false;
    for (qsizetype i = 0; i < left.size(); ++i) {
        const InlineResultRow& a = left.at(i);
        const InlineResultRow& b = right.at(i);
        if (a.id != b.id || a.title != b.title || a.description != b.description
            || a.messageId != b.messageId)
            return false;
    }
    return true;
}

} // namespace

InlineQueryController::InlineQueryController(QObject* parent)
    : QObject(parent)
{
}

void InlineQueryController::open(qint64 botUserId)
{
    // TDLib user ids are positive, so anything else is a caller bug and never
    // opens a session.
    if (botUserId <= 0)
        return;

    if (m_botUserId == botUserId)
        return;

    // A different bot means a different answer: keep nothing from the old
    // session. The bot id itself already changed, so this is always a real
    // transition and is reported exactly once.
    m_botUserId = botUserId;
    m_query.clear();
    m_results.clear();
    m_loading = false;
    m_selected = -1;
    emit changed();
}

void InlineQueryController::close()
{
    clear();
}

void InlineQueryController::setQuery(const QString& query)
{
    const QString trimmed = query.trimmed();
    if (trimmed == m_query)
        return;

    // A new query invalidates the old answer, so the results, the selection and
    // the pending flag go with it.
    m_query = trimmed;
    m_results.clear();
    m_loading = false;
    m_selected = -1;
    emit changed();
}

void InlineQueryController::setResults(QVector<InlineResultRow> results)
{
    // An answer is only meaningful for an open bot with a typed query.
    if (!active() || m_query.isEmpty())
        return;

    QVector<InlineResultRow> next = normalizedRows(results);
    if (sameRows(next, m_results))
        return;

    m_results = std::move(next);

    // The answer may be shorter than the old one, so a stale selection is
    // dropped rather than left pointing past the end.
    if (m_selected >= m_results.size())
        m_selected = -1;

    emit changed();
}

void InlineQueryController::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit changed();
}

void InlineQueryController::setSelectedIndex(int index)
{
    if (index == -1) {
        if (m_selected == -1)
            return;
        m_selected = -1;
        emit changed();
        return;
    }

    if (index < 0 || index >= m_results.size())
        return;
    if (m_selected == index)
        return;

    m_selected = index;
    emit changed();
}

InlineResultRow InlineQueryController::selected() const
{
    if (m_selected < 0 || m_selected >= m_results.size())
        return {};
    return m_results.at(m_selected);
}

void InlineQueryController::clear()
{
    if (m_botUserId == 0 && m_query.isEmpty() && m_results.isEmpty() && !m_loading
        && m_selected == -1)
        return;

    m_botUserId = 0;
    m_query.clear();
    m_results.clear();
    m_loading = false;
    m_selected = -1;
    emit changed();
}

} // namespace beygram::telegram
