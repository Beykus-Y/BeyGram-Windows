#include "ShortcutModel.h"

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

/// Drops rows with an empty action and keeps only the first row per action,
/// then trims each surviving sequence. Descriptions are left exactly as given:
/// they are display text, so trimming them would be cosmetic work the model has
/// no business doing.
QVector<ShortcutRow> normalizedRows(QVector<ShortcutRow> rows)
{
    QVector<ShortcutRow> kept;
    kept.reserve(rows.size());

    for (ShortcutRow& row : rows) {
        if (row.action.isEmpty())
            continue;

        const bool duplicate =
            std::any_of(kept.cbegin(), kept.cend(), [&row](const ShortcutRow& existing) {
                return existing.action == row.action;
            });
        if (duplicate)
            continue;

        row.sequence = row.sequence.trimmed();
        kept.append(std::move(row));
    }

    return kept;
}

/// ShortcutRow carries no equality operator, so row sets are compared field by
/// field. Both sides are already normalised, which is what lets a re-sent set
/// that normalises down to the same rows stay silent.
bool sameRows(const QVector<ShortcutRow>& left, const QVector<ShortcutRow>& right)
{
    if (left.size() != right.size())
        return false;

    for (qsizetype i = 0; i < left.size(); ++i) {
        const ShortcutRow& a = left.at(i);
        const ShortcutRow& b = right.at(i);
        if (a.action != b.action || a.sequence != b.sequence
            || a.description != b.description)
            return false;
    }
    return true;
}

/// Index of the row for `action`, or -1 when the model has no such action.
qsizetype indexOfAction(const QVector<ShortcutRow>& rows, const QString& action)
{
    for (qsizetype i = 0; i < rows.size(); ++i) {
        if (rows.at(i).action == action)
            return i;
    }
    return -1;
}

/// Whether any row already holds `sequence`. Callers pass a non-empty sequence,
/// so an unbound row (empty sequence) never counts as a conflict.
bool sequenceInUse(const QVector<ShortcutRow>& rows, const QString& sequence)
{
    return std::any_of(rows.cbegin(), rows.cend(), [&sequence](const ShortcutRow& row) {
        return row.sequence == sequence;
    });
}

} // namespace

ShortcutModel::ShortcutModel(QObject* parent)
    : QObject(parent)
{
}

void ShortcutModel::setShortcuts(QVector<ShortcutRow> shortcuts)
{
    QVector<ShortcutRow> next = normalizedRows(std::move(shortcuts));
    if (sameRows(next, m_rows))
        return;

    m_rows = std::move(next);

    // The first non-empty set is the application's own table, so it becomes the
    // defaults. A caller that loads saved bindings later only replaces the rows.
    if (m_defaults.isEmpty() && !m_rows.isEmpty())
        m_defaults = m_rows;

    emit changed();
}

QVector<ShortcutRow> ShortcutModel::shortcuts() const
{
    return m_rows;
}

int ShortcutModel::count() const
{
    return static_cast<int>(m_rows.size());
}

QVector<ShortcutRow> ShortcutModel::filtered(const QString& query) const
{
    if (query.isEmpty())
        return m_rows;

    QVector<ShortcutRow> matches;
    matches.reserve(m_rows.size());

    for (const ShortcutRow& row : m_rows) {
        if (row.action.contains(query, Qt::CaseInsensitive)
            || row.sequence.contains(query, Qt::CaseInsensitive)
            || row.description.contains(query, Qt::CaseInsensitive)) {
            matches.append(row);
        }
    }

    return matches;
}

QString ShortcutModel::sequenceFor(const QString& action) const
{
    const qsizetype index = indexOfAction(m_rows, action);
    return index < 0 ? QString() : m_rows.at(index).sequence;
}

bool ShortcutModel::rebind(const QString& action, const QString& sequence)
{
    const qsizetype index = indexOfAction(m_rows, action);
    if (index < 0)
        return false;

    const QString next = sequence.trimmed();
    if (next.isEmpty())
        return false;

    if (m_rows.at(index).sequence == next)
        return false;

    // A sequence belongs to one action. Rebinding to a key another action holds
    // is refused rather than silently stolen, so the editor can report the
    // conflict and leave both rows as they were.
    if (sequenceInUse(m_rows, next))
        return false;

    // Nothing below reads the row after the signal: a slot may re-enter
    // setShortcuts(), which would reallocate the vector.
    m_rows[index].sequence = next;
    emit changed();
    return true;
}

void ShortcutModel::resetToDefaults()
{
    if (m_defaults.isEmpty())
        return;
    if (sameRows(m_rows, m_defaults))
        return;

    m_rows = m_defaults;
    emit changed();
}

bool ShortcutModel::isModified() const
{
    return !sameRows(m_rows, m_defaults);
}

} // namespace beygram::telegram
