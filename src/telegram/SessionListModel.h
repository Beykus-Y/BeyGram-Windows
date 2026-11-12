#pragma once

#include "SessionRequests.h"

#include <QObject>
#include <QVector>

namespace beygram::telegram {

/// Name used by the device-session screen for one row of this model.
///
/// The row is the value type SessionRequests.h already parses out of TDLib
/// (id, device, location, ip, lastActive, current), so a Sessions answer the
/// caller fetched once can be shown, split into "this device" and "other
/// devices", and pruned after a termination without a second struct that would
/// drift from the parser. The alias exists only so call sites can talk about
/// "a session row" without repeating the wire-level type name.
using SessionRow = SessionInfo;

/// The active sessions of one account, as a plain value holder for the
/// device-management screen.
///
/// The model never talks to TDLib, never logs and never reads the clock: the
/// caller sends getActiveSessions, hands the parsed answer to setSessions(),
/// and sends terminateSession itself after remove() reports a removal. Because
/// nothing here performs I/O, the tests stay deterministic.
///
/// Invariants:
///
///  - `id` is unique and positive. setSessions() drops sessions whose id is 0
///    or negative (an unparsed entry, which could be neither shown nor
///    terminated) and keeps the first session of each id, so a caller merging
///    several answers cannot end up with two rows fighting over one session.
///  - At most one session is flagged current. TDLib marks exactly one, but a
///    merged or stale answer can flag several; the first flagged row keeps the
///    flag and every later one is demoted, so others() cannot hide a second
///    "this device" row that the screen would render as a peer.
///  - Order is the caller's order. setSessions() only removes entries and
///    clears a redundant flag; it never sorts, so the screen shows sessions in
///    the order Telegram returned them.
///  - changed() is emitted only when the visible list really differs. Reloading
///    the same sessions, removing an absent id or clearing an empty model are
///    all silent, so a refresh that brings the same data does not repaint the
///    list or re-trigger a binding loop.
class SessionListModel final : public QObject {
    Q_OBJECT

public:
    explicit SessionListModel(QObject* parent = nullptr);

    /// Replaces every session with `sessions`, in the given order. Sessions
    /// without a positive id and later duplicates of an id (the first
    /// occurrence wins) are dropped, and all but the first current session are
    /// demoted. changed() is emitted only when the resulting list differs from
    /// the current one.
    void setSessions(QVector<SessionRow> sessions);

    /// A copy of the sessions, in display order, including the current one.
    [[nodiscard]] QVector<SessionRow> sessions() const;

    /// Number of sessions currently held. Always equal to sessions().size().
    [[nodiscard]] int count() const;

    /// The sessions that are not the current one, in display order. These are
    /// the rows the other-device actions may terminate; the current session is
    /// never among them.
    [[nodiscard]] QVector<SessionRow> others() const;

    /// Forgets the session with `sessionId`, e.g. after TDLib confirms a
    /// termination. Returns true only when a session was removed, which is also
    /// the only case that emits changed(). An absent id is a silent no-op, so a
    /// second termination answer for the same session does not repaint the
    /// list. The model does not check whether the session is the current one:
    /// refusing that belongs to the caller, which knows the confirmation state.
    bool remove(qint64 sessionId);

    /// Forgets every session, e.g. when the account is signed out. Emits
    /// changed() only if there was something to forget.
    void clear();

signals:
    /// The visible session list changed. Not emitted for a rejected or no-op
    /// call.
    void changed();

private:
    QVector<SessionRow> m_sessions;
};

} // namespace beygram::telegram
