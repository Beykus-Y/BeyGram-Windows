#pragma once

#include "telegram/NotificationScopes.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace beygram::ui {

/// The notification-scopes screen as one QML-facing object.
///
/// The facade owns no transport and no storage. setScopes() publishes what
/// notificationScopes reported, select() chooses the row being edited, the
/// setters edit that row's draft, and save() emits saveRequested() with the
/// exact payload setNotificationScope needs. The caller sends the request and
/// reports the answer by calling setScopes() again. Nothing here logs, reads
/// the clock or performs I/O, so the panel can be driven from a test.
///
/// Rows:
///  - ids() and titles() are parallel and in wire order. Scope 0 is the main
///    scope and is valid; negative, malformed and duplicate ids are dropped
///    (the first entry for an id wins), matching notificationScopesFromResponse.
///  - ids() are canonical decimal text, so " 007 " reads "7".
///  - A title is trimmed; interior whitespace is preserved. An empty title is
///    kept as empty, exactly as the parser stores it, so the panel supplies a
///    readable fallback label rather than the facade inventing one.
///  - Flags arrive as strings because QML has no bool list. "true", "1", "yes"
///    and "on" are true; "false", "0", "no" and "off" are false; a missing or
///    unrecognised value keeps the NotificationScope default (not muted,
///    preview on, sound on).
///  - A titles or flags list shorter than ids() does not shift rows: the
///    missing entries keep their defaults.
///
/// Selection and drafts:
///  - selectedIndex() is -1 while nothing is selected, which is also the only
///    value an empty list can have. setScopes() selects the first row when
///    nothing was selected, keeps a still-valid index, and returns to -1 for an
///    empty list.
///  - select() accepts -1 to clear the selection and any listed row; anything
///    else is ignored. Switching rows loads that row's saved flags and drops
///    the previous unsaved draft, so a scope is always edited from its stored
///    state.
///  - muted(), preview() and sound() describe the draft for the selected row,
///    or the NotificationScope defaults while nothing is selected.
///  - A setter that stores the same value is silent.
///
/// Saving:
///  - save() is the only place an edit leaves the facade. It returns false and
///    emits nothing without a selection or while the draft matches the stored
///    flags, so a double click cannot send the same scope twice.
///  - On success it accepts the draft as the new saved baseline and emits
///    saveRequested(scopeId, muted, preview, sound). The exposed properties do
///    not move, so save() emits no changed(). The id is decimal text so a value
///    never loses precision on its way through QML.
class NotificationScopesFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList titles READ titles NOTIFY changed)
    Q_PROPERTY(QStringList ids READ ids NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY changed)
    Q_PROPERTY(bool muted READ muted NOTIFY changed)
    Q_PROPERTY(bool preview READ preview NOTIFY changed)
    Q_PROPERTY(bool sound READ sound NOTIFY changed)

public:
    explicit NotificationScopesFacade(QObject* parent = nullptr);

    Q_DISABLE_COPY_MOVE(NotificationScopesFacade)

    /// The display names of ids(), at the same indexes.
    [[nodiscard]] QStringList titles() const;

    /// The scope ids, as canonical decimal text, in display order.
    [[nodiscard]] QStringList ids() const;

    /// The row being edited, or -1 when nothing is selected.
    [[nodiscard]] int selectedIndex() const;

    /// The draft mute flag of the selected row.
    [[nodiscard]] bool muted() const;

    /// The draft preview flag of the selected row.
    [[nodiscard]] bool preview() const;

    /// The draft sound flag of the selected row.
    [[nodiscard]] bool sound() const;

    /// Replaces the rows from the parallel string lists a TDLib-shaped payload
    /// arrives as. Invalid and duplicate ids are dropped, titles are trimmed
    /// and missing flags keep the NotificationScope defaults. The selection
    /// survives while its index is still in range, and the selected draft is
    /// reloaded from the new stored flags. Silent when the rows, the selection
    /// and the draft are all unchanged.
    Q_INVOKABLE void setScopes(const QStringList& ids, const QStringList& titles,
                               const QStringList& muted, const QStringList& preview,
                               const QStringList& sound);

    /// Selects a row, or -1 to clear the selection. Out-of-range indexes other
    /// than -1 are ignored. A real change loads the row's stored flags, so an
    /// unsaved draft on the previous row is dropped.
    Q_INVOKABLE void select(int index);

    /// Edits the selected row's draft. Ignored while nothing is selected, and
    /// silent when the value is already stored.
    Q_INVOKABLE void setMuted(bool muted);
    Q_INVOKABLE void setPreview(bool preview);
    Q_INVOKABLE void setSound(bool sound);

    /// Sends the selected row's draft. Returns true and emits saveRequested()
    /// only when a row is selected and the draft differs from its stored flags;
    /// otherwise returns false and changes nothing. On success the draft
    /// becomes the stored baseline, so a second click is a no-op.
    Q_INVOKABLE bool save();

signals:
    /// The rows, the selection or the selected draft changed.
    void changed();

    /// A save to send as setNotificationScope: the scope id as decimal text and
    /// the three flags the user chose.
    void saveRequested(const QString& scopeId, bool muted, bool preview, bool sound);

private:
    /// The editable flags of one row. The stored flags use the same shape as
    /// the draft, so a save is a plain comparison and copy.
    struct Draft {
        bool muted = false;
        bool preview = true;
        bool sound = true;

        bool operator==(const Draft&) const = default;
    };

    [[nodiscard]] Draft draftFor(int index) const;
    [[nodiscard]] bool draftMatches(int index) const;

    QVector<telegram::NotificationScope> m_scopes;
    int m_selectedIndex = -1;
    Draft m_draft;
};

} // namespace beygram::ui
