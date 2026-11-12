#pragma once

#include "ReactionCatalog.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// Holds the reaction picker's state: the two catalog lists, the reaction count
/// limit, the current selection and whether the picker is expanded.
///
/// The model is a plain value holder. It stores already-parsed
/// AvailableReaction values and never touches TDLib, the UI or logging, so the
/// picker view can bind to it and a controller can feed it parsed catalogs.
///
/// top() and recent() keep the server order and duplicates exactly as the
/// catalog supplied them; nothing is filtered or deduplicated, so an inactive
/// or premium entry stays visible and selectable. maxCount() is likewise the
/// catalog's max_reaction_count verbatim, 0 included.
///
/// The selection is a single emoji string and is always empty or one of the
/// emoji offered by top()/recent(). setSelected() ignores anything else, so
/// the picker cannot report a reaction the server does not offer. Replacing the
/// catalog with one that no longer offers the selected emoji drops the
/// selection rather than keeping it dangling.
///
/// Every mutator emits changed() only when it actually changed the state, so a
/// view can refresh on the signal without comparing snapshots itself.
class ReactionPickerModel final : public QObject {
    Q_OBJECT

public:
    explicit ReactionPickerModel(QObject* parent = nullptr);

    /// Replaces the stored catalog with a copy of `catalog`, keeping both lists
    /// and the reaction count as they are. A selection the new catalog no
    /// longer offers is cleared. Emits changed() only when the stored state
    /// differs from `catalog` (including a selection that had to be dropped).
    void setCatalog(const ReactionCatalogState& catalog);

    /// Returns a copy of the top reactions in server order.
    [[nodiscard]] QVector<AvailableReaction> top() const;

    /// Returns a copy of the recent reactions in server order. May overlap
    /// top(); the two lists are stored independently.
    [[nodiscard]] QVector<AvailableReaction> recent() const;

    /// Returns the catalog's max_reaction_count, or 0 when it carried none.
    [[nodiscard]] int maxCount() const;

    /// Selects `emoji`, which must be offered by top() or recent(). An empty
    /// emoji clears the selection. An emoji the catalog does not offer, and a
    /// selection that is already in place, are ignored. Emits changed() only
    /// when the selection actually changed.
    void setSelected(const QString& emoji);

    /// Returns the selected emoji, or an empty string when nothing is selected.
    [[nodiscard]] QString selected() const;

    /// Returns true when `emoji` is the current selection. An empty emoji is
    /// never selected, even while nothing else is.
    [[nodiscard]] bool isSelected(const QString& emoji) const;

    /// Selects `emoji` when it is not selected and clears the selection when it
    /// is. An empty emoji or one the catalog does not offer is ignored. Emits
    /// changed() only when the selection actually changed.
    void toggle(const QString& emoji);

    /// Returns true when the picker shows its full catalog instead of the
    /// collapsed top row.
    [[nodiscard]] bool expanded() const;

    /// Sets the expanded flag. Emits changed() only when the flag flips.
    void setExpanded(bool expanded);

    /// Drops both lists, the reaction count, the selection and the expanded
    /// flag. Emits changed() only when there was something to drop.
    void clear();

signals:
    void changed();

private:
    QVector<AvailableReaction> m_top, m_recent;
    int m_max = 0;
    QString m_selected;
    bool m_expanded = false;
};

} // namespace beygram::telegram
