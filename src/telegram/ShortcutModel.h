#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One row of the keyboard-shortcut editor: the action it triggers, the key
/// sequence currently bound to it, and the label shown next to that sequence.
struct ShortcutRow {
    /// Stable identifier for the action, e.g. "message.send". It is the key the
    /// rest of the application asks the model with, so it is compared exactly as
    /// written and is never trimmed or case-folded. A row whose action is empty
    /// identifies nothing and is dropped by ShortcutModel.
    QString action;
    /// Key sequence in Qt's portable text form, e.g. "Ctrl+Enter". The model
    /// stores it as text and never parses it, so it does not know whether a
    /// sequence is valid; surrounding whitespace is trimmed on the way in and an
    /// empty sequence simply means "this action has no key bound".
    QString sequence;
    /// Human-readable name for the action, e.g. "Send message". Only used for
    /// display and for searching; it never identifies a row.
    QString description;
};

/// The keyboard shortcuts of the application, as a plain value holder for the
/// shortcut editor.
///
/// The model never talks to TDLib, never reads the settings store, never logs
/// and never reads the clock: the caller loads the saved bindings, hands them to
/// setShortcuts(), listens for changed() and writes the result back itself.
/// Because nothing here performs I/O, the tests stay deterministic.
///
/// Invariants:
///
///  - `action` is unique. setShortcuts() drops rows whose action is empty and
///    keeps the first row of each action, so a caller that merges a saved
///    binding file over a default table cannot end up with two rows fighting
///    over one action.
///  - Order is the caller's order. setShortcuts() only removes rows; it never
///    sorts, so the editor lists shortcuts in the order the caller supplied.
///  - `sequence` is stored trimmed. Two rows may carry the same sequence after
///    setShortcuts(), because a half-edited binding file is the caller's
///    problem; only rebind() enforces that a sequence belongs to one action.
///  - The defaults are the first non-empty set handed to setShortcuts(), kept
///    as normalised. Later calls replace the current rows but never the
///    defaults, so resetToDefaults() still returns to what the application
///    shipped even after the user has reloaded their own bindings.
///  - changed() is emitted only when the rows really differ. Re-sending the same
///    shortcuts, rebinding an action to the sequence it already has, rejecting a
///    duplicate or resetting an unmodified model are all silent, so a refresh
///    that brings the same bindings does not repaint the editor or re-trigger a
///    binding loop.
class ShortcutModel final : public QObject {
    Q_OBJECT

public:
    explicit ShortcutModel(QObject* parent = nullptr);

    /// Replaces every row with `shortcuts`, in the given order. Rows with an
    /// empty action are dropped, later rows repeating an action are dropped
    /// (the first one wins) and every sequence is trimmed. changed() is emitted
    /// only when the resulting rows differ from the current ones. The first
    /// non-empty result is also captured as the defaults; see resetToDefaults().
    void setShortcuts(QVector<ShortcutRow> shortcuts);

    /// A copy of the rows, in display order. The caller owns the copy, so it
    /// cannot change the model behind changed()'s back.
    [[nodiscard]] QVector<ShortcutRow> shortcuts() const;

    /// Number of rows currently held. Always equal to shortcuts().size().
    [[nodiscard]] int count() const;

    /// The rows whose action, sequence or description contains `query`,
    /// ignoring case, in display order. An empty query returns every row rather
    /// than none, which is what the editor's search box shows before the user
    /// types. The query is matched as written: it is not trimmed and not split
    /// into words.
    [[nodiscard]] QVector<ShortcutRow> filtered(const QString& query) const;

    /// The sequence bound to `action`, or an empty string when the action is
    /// unknown or currently has no key. The two cases are deliberately not
    /// distinguished: both mean "there is no key to show for this action".
    [[nodiscard]] QString sequenceFor(const QString& action) const;

    /// Binds `action` to `sequence` (trimmed). Returns true only when an
    /// existing row's sequence actually changed, which is also the only case
    /// that emits changed().
    ///
    /// The call is refused, leaving the model untouched, when the action is
    /// unknown, when the trimmed sequence is empty (a binding is never removed
    /// through this call; an action is left unbound by handing setShortcuts() a
    /// row with an empty sequence), or when another action already holds that
    /// exact sequence. Sequences are compared as trimmed text, so the caller
    /// should pass QKeySequence::toString(QKeySequence::PortableText) and not
    /// mix "ctrl+enter" with "Ctrl+Enter". Rebinding an action to the sequence
    /// it already has is a no-op that returns false.
    bool rebind(const QString& action, const QString& sequence);

    /// Restores the rows captured by the first non-empty setShortcuts(), in the
    /// order they were supplied there. changed() is emitted only when something
    /// actually differed. A model that never received a non-empty set has no
    /// defaults to restore, so the call is a silent no-op.
    void resetToDefaults();

    /// Whether the current rows differ from the captured defaults. False for a
    /// fresh model and for one whose rows are still the defaults.
    ///
    /// The comparison is over whole rows, so a changed description counts as a
    /// modification just like a changed sequence does; the editor asks this to
    /// decide whether to offer a reset, and a row that no longer matches what
    /// shipped is worth offering it for.
    [[nodiscard]] bool isModified() const;

signals:
    /// The visible rows changed. Not emitted for a rejected or no-op call.
    void changed();

private:
    QVector<ShortcutRow> m_rows;
    QVector<ShortcutRow> m_defaults;
};

} // namespace beygram::telegram
