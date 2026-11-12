#pragma once

#include <QObject>
#include <QtGlobal>

namespace beygram::telegram {

/// The confirmation state of the "terminate session" action.
///
/// Terminating a session signs another device out, so both entry points only
/// open a confirmation; nothing is sent from here. A view calls
/// requestTerminate() for one row or requestTerminateAllOthers() for the whole
/// other-device action, shows a dialog while confirming() is true, disables its
/// confirm button unless canConfirm(), and sends terminateSession or
/// terminateAllOtherSessions itself once the user accepts. The controller never
/// talks to TDLib, never logs and never reads the clock, so its tests stay
/// deterministic.
///
/// Invariants:
///
///  - A pending target is a positive session id, or 0 meaning "all other
///    sessions". pendingSessionId() is only meaningful while confirming();
///    outside a confirmation it is 0.
///  - requestTerminate() refuses a non-positive id, because such a row could
///    never have come from the parser and cannot be terminated. A refused
///    request changes nothing.
///  - While an action is running the target is frozen: a new request is
///    ignored instead of silently replacing the session whose termination is
///    already in flight.
///  - running() can only become true inside a confirmation, so a stale answer
///    cannot mark an idle controller busy.
///  - changed() is emitted only when the state really moved. Repeating the
///    pending target, cancelling outside a confirmation, setting the current
///    running value and clearing an idle controller are all silent.
class SessionTerminationController final : public QObject {
    Q_OBJECT

public:
    explicit SessionTerminationController(QObject* parent = nullptr);

    /// Opens a confirmation for the session with `sessionId`. The id must be
    /// positive; a non-positive id is ignored. Requesting the target that is
    /// already pending is a no-op, a different target replaces the pending one,
    /// and every request is ignored while an action is running.
    void requestTerminate(qint64 sessionId);

    /// Opens a confirmation for terminateAllOtherSessions, i.e. a pending
    /// target of 0. Behaves like requestTerminate() for repeats and while
    /// running.
    void requestTerminateAllOthers();

    /// True while a confirmation is open, whether or not the action is running.
    [[nodiscard]] bool confirming() const;

    /// The session the open confirmation targets; 0 means all other sessions.
    /// 0 while no confirmation is open.
    [[nodiscard]] qint64 pendingSessionId() const;

    /// Closes the confirmation and forgets the target and the running flag.
    /// Ignored outside a confirmation, where there is nothing to cancel.
    void cancel();

    /// Marks the confirmed action as in flight so the view can disable its
    /// confirm button. Only accepted inside a confirmation; setting the current
    /// value is a no-op.
    void setRunning(bool running);
    [[nodiscard]] bool running() const;

    /// True when the user could confirm now: a confirmation is open and no
    /// action is already running.
    [[nodiscard]] bool canConfirm() const;

    /// Forgets the confirmation, the target and the running flag, e.g. when the
    /// screen closes or the account signs out. Emits changed() only if anything
    /// was set.
    void clear();

signals:
    /// The confirmation, its target or the running flag really changed.
    /// Rejected requests, no-op setters and no-op clears stay silent.
    void changed();

private:
    qint64 m_pending = 0;
    bool m_confirming = false;
    bool m_running = false;
};

} // namespace beygram::telegram
