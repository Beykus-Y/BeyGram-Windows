#pragma once

#include "telegram/ShortcutModel.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace beygram::ui {

/// The keyboard-shortcut editor as one QML-facing object.
///
/// This is a thin presentation facade, not a second source of truth: the rows,
/// the defaults, the duplicate rules and the definition of "modified" all live
/// in the telegram::ShortcutModel this class owns, and every property is read
/// straight from it. The facade only does what QML cannot: it accepts the
/// parallel string lists a settings payload arrives as, exposes the rows as two
/// parallel QStringLists instead of a QVector<ShortcutRow>, and rebinds by row
/// index because a repeater knows a row by its position and not by its action.
///
/// The lists are the *visible* rows: actions() and sequences() are parallel,
/// already filtered by query(), and in display order, so the panel indexes both
/// with the same repeater index. rebind() takes that same visible index.
///
/// The model's changed() is forwarded as this object's changed(), so a QML
/// binding repaints on a real update and stays quiet on a redundant one (the
/// model already suppresses no-op changes).
///
/// Invariants:
///  - actions().size() == sequences().size() always, and entry i of each
///    describes the same row.
///  - actions() never contains an empty action and never repeats one; the model
///    drops such rows on the way in.
///  - sequences() entries are trimmed; an empty entry means "this action has no
///    key bound", not "unknown action".
///  - modified() is true exactly when the current rows differ from the first
///    non-empty table handed to setShortcuts(), as the model defines it.
///
/// The facade never reads or writes the settings store, never parses a key
/// sequence and never logs: persisting the result is the caller's job, exactly
/// as it is for the model.
class ShortcutsFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList actions READ actions NOTIFY changed)
    Q_PROPERTY(QStringList sequences READ sequences NOTIFY changed)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY changed)
    Q_PROPERTY(bool modified READ modified NOTIFY changed)

public:
    explicit ShortcutsFacade(QObject* parent = nullptr);

    /// Action ids of the visible rows, in display order. With an empty query
    /// this is the whole table.
    [[nodiscard]] QStringList actions() const;

    /// The sequence bound to each entry of actions(), same order and length.
    [[nodiscard]] QStringList sequences() const;

    /// Replaces the table from the parallel string lists a settings payload
    /// arrives as: action is the stable id, sequence is Qt portable text and
    /// description is the human-readable label. The lists are read positionally
    /// and may be shorter than actions; a missing sequence or description reads
    /// as empty.
    ///
    /// The model's normalization applies: rows with an empty action are
    /// dropped, only the first row of each action is kept, sequences are
    /// trimmed, and the first non-empty result is captured as the defaults that
    /// resetToDefaults() returns to. The current query is left alone, so a
    /// reload does not clear the search box.
    Q_INVOKABLE void setShortcuts(const QStringList& actions, const QStringList& sequences,
                                  const QStringList& descriptions);

    /// The filter applied to actions() and sequences(). Matched by the model
    /// against action, sequence and description, ignoring case, as written: the
    /// facade neither trims nor splits it.
    [[nodiscard]] QString query() const;
    void setQuery(const QString& query);

    /// Binds the visible row at `index` to `sequence` (trimmed) and reports
    /// whether the model accepted it.
    ///
    /// The index is into actions(), i.e. into the filtered rows, so an active
    /// query shifts it. False, with the rows untouched, when the index is out
    /// of range, when the sequence is empty after trimming, when another action
    /// already holds that exact sequence, or when the row already has it.
    Q_INVOKABLE bool rebind(int index, const QString& sequence);

    /// Restores the table captured by the first non-empty setShortcuts(). A
    /// facade that never received one has nothing to restore, so the call is a
    /// silent no-op.
    Q_INVOKABLE void resetToDefaults();

    /// Whether the rows differ from the captured defaults; false for a fresh
    /// facade and for one still holding what shipped.
    [[nodiscard]] bool modified() const;

    /// Announces that `action` was triggered, for the caller to perform it.
    ///
    /// An empty or unknown action is ignored rather than announced, because
    /// nothing in the application can be triggered by an id the table does not
    /// know. A known action is announced even when it currently has no key
    /// bound: the caller may invoke it programmatically, and whether the
    /// binding exists is the editor's business, not the trigger's.
    Q_INVOKABLE void trigger(const QString& action);

signals:
    /// Emitted whenever any observable state of the facade changed.
    void changed();

    /// Emitted by trigger() with the action the caller should perform.
    void shortcutTriggered(const QString& action);

private:
    /// The rows the editor currently shows, i.e. the model filtered by query().
    [[nodiscard]] QVector<telegram::ShortcutRow> visibleRows() const;

    telegram::ShortcutModel m_model;
    QString m_query;
};

} // namespace beygram::ui
