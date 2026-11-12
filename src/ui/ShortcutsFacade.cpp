#include "ShortcutsFacade.h"

#include <algorithm>
#include <utility>

namespace beygram::ui {

ShortcutsFacade::ShortcutsFacade(QObject* parent)
    : QObject(parent)
{
    // The model already suppresses no-op changes, so forwarding is enough: a
    // redundant update emits nothing, a real one repaints every binding.
    connect(&m_model, &telegram::ShortcutModel::changed, this, &ShortcutsFacade::changed);
}

QVector<telegram::ShortcutRow> ShortcutsFacade::visibleRows() const
{
    return m_model.filtered(m_query);
}

QStringList ShortcutsFacade::actions() const
{
    const QVector<telegram::ShortcutRow> rows = visibleRows();

    QStringList result;
    result.reserve(rows.size());
    for (const telegram::ShortcutRow& row : rows)
        result.append(row.action);

    return result;
}

QStringList ShortcutsFacade::sequences() const
{
    const QVector<telegram::ShortcutRow> rows = visibleRows();

    QStringList result;
    result.reserve(rows.size());
    for (const telegram::ShortcutRow& row : rows)
        result.append(row.sequence);

    return result;
}

void ShortcutsFacade::setShortcuts(const QStringList& actions, const QStringList& sequences,
                                   const QStringList& descriptions)
{
    QVector<telegram::ShortcutRow> rows;
    rows.reserve(actions.size());

    for (qsizetype i = 0; i < actions.size(); ++i) {
        telegram::ShortcutRow row;
        row.action = actions.at(i);
        row.sequence = i < sequences.size() ? sequences.at(i) : QString();
        row.description = i < descriptions.size() ? descriptions.at(i) : QString();
        rows.append(std::move(row));
    }

    // The model owns the rules (drop empty actions, keep the first duplicate,
    // trim, capture defaults), so the facade does not repeat them.
    m_model.setShortcuts(std::move(rows));
}

QString ShortcutsFacade::query() const
{
    return m_query;
}

void ShortcutsFacade::setQuery(const QString& query)
{
    if (m_query == query)
        return;

    m_query = query;

    // The query is a property of its own, so its NOTIFY fires even when the
    // filter happens to leave exactly the same rows visible.
    emit changed();
}

bool ShortcutsFacade::rebind(int index, const QString& sequence)
{
    const QVector<telegram::ShortcutRow> rows = visibleRows();
    if (index < 0 || index >= rows.size())
        return false;

    // Snapshot the action: a slot on changed() may re-enter the facade, and the
    // visible rows are rebuilt from the model every time.
    const QString action = rows.at(index).action;
    return m_model.rebind(action, sequence);
}

void ShortcutsFacade::resetToDefaults()
{
    m_model.resetToDefaults();
}

bool ShortcutsFacade::modified() const
{
    return m_model.isModified();
}

void ShortcutsFacade::trigger(const QString& action)
{
    if (action.isEmpty())
        return;

    // sequenceFor() cannot answer this: it returns an empty string both for an
    // unknown action and for a known one with no key bound, and only the second
    // is triggerable.
    const QVector<telegram::ShortcutRow> rows = m_model.shortcuts();
    const bool known = std::any_of(rows.cbegin(), rows.cend(),
                                   [&action](const telegram::ShortcutRow& row) {
                                       return row.action == action;
                                   });
    if (!known)
        return;

    emit shortcutTriggered(action);
}

} // namespace beygram::ui
