#pragma once

#include "telegram/SessionListModel.h"
#include "telegram/SessionTerminationController.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace beygram::ui {

/// The device-session screen, as one object the QML panel can bind to.
///
/// The facade owns a SessionListModel (which sessions exist) and a
/// SessionTerminationController (whether a termination is being confirmed), and
/// exposes them as plain properties so the panel needs no C++ of its own. It
/// never talks to TDLib, never logs and never reads the clock: the caller sends
/// getActiveSessions, hands the answer to setSessions(), connects
/// terminateRequested() to terminateSession / terminateAllOtherSessions, and
/// reports the outcome through setRunning() and cancel(). Nothing here performs
/// I/O, so the tests stay deterministic.
///
/// Invariants:
///
///  - devices() and sessionIds() are parallel and in Telegram's order: the row
///    at index i is the device text for sessionIds().at(i). They describe the
///    sessions that may be terminated, i.e. SessionListModel::others(), so the
///    current device is never offered a "Terminate" button. Rows flagged current
///    are excluded even though setSessions() cannot flag them itself.
///  - devices() renders "device - location" (a real em dash). A row without a
///    location shows the device alone, a row without a device shows the location
///    alone, and a row with neither shows "Unknown device", so no entry is ever
///    blank.
///  - setSessions() takes three parallel lists. Ids that are not a positive
///    number are dropped (the model would drop them anyway); labels follow their
///    id, so a dropped entry never shifts the remaining device texts.
///  - requestTerminate() accepts only an id that is currently listed as
///    terminable. An unknown, malformed, already removed or current-session id
///    opens nothing, so a stale button cannot sign out a device that is not on
///    screen.
///  - requestTerminateAllOthers() is ignored when no other session is listed:
///    there is nothing to sign out, so no destructive confirmation is opened.
///  - pendingTarget() is "" while no confirmation is open, the session id while
///    one row is pending, and "all" for the whole other-device action.
///  - confirm() is the only place a request leaves the facade. It returns false
///    and emits nothing unless SessionTerminationController::canConfirm() holds,
///    so a double click cannot send the same termination twice. On success it
///    marks the action running first and then emits terminateRequested("") for
///    "all other sessions" or terminateRequested(id) for one session; the
///    confirmation stays open until the caller calls cancel(), which lets the
///    panel show progress while the request is in flight.
///  - changed() is emitted whenever either collaborator changed. The facade adds
///    no state of its own, so no-op calls stay silent exactly as the model and
///    the controller keep them.
class SessionsFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList devices READ devices NOTIFY changed)
    Q_PROPERTY(QStringList sessionIds READ sessionIds NOTIFY changed)
    Q_PROPERTY(bool confirming READ confirming NOTIFY changed)
    Q_PROPERTY(QString pendingTarget READ pendingTarget NOTIFY changed)
    Q_PROPERTY(bool running READ running NOTIFY changed)

public:
    explicit SessionsFacade(QObject* parent = nullptr);

    Q_DISABLE_COPY_MOVE(SessionsFacade)

    /// "device - location" for every terminable session, in display order.
    [[nodiscard]] QStringList devices() const;

    /// The ids of devices(), as strings, at the same indexes. QML keeps ids as
    /// text so a value can travel through requestTerminate() without losing
    /// precision in JavaScript numbers.
    [[nodiscard]] QStringList sessionIds() const;

    /// Replaces the session list from three parallel lists: ids as text, device
    /// names and locations. Entries without a positive numeric id are skipped.
    /// A confirmation for a session this answer no longer lists is dropped,
    /// because confirming it would sign out a device the user can no longer see;
    /// a pending "all other sessions" survives, since it applies to the list as
    /// it is now.
    Q_INVOKABLE void setSessions(const QStringList& ids, const QStringList& devices,
                                 const QStringList& locations);

    /// Opens a confirmation for one listed session. Unknown, malformed and
    /// non-positive ids are ignored.
    Q_INVOKABLE void requestTerminate(const QString& sessionId);

    /// Opens a confirmation for terminateAllOtherSessions. Ignored when no other
    /// session is listed.
    Q_INVOKABLE void requestTerminateAllOthers();

    /// Closes the confirmation and forgets the target and the running flag. A
    /// no-op outside a confirmation.
    Q_INVOKABLE void cancel();

    [[nodiscard]] bool confirming() const;

    /// "" when idle, the pending session id, or "all" for the whole action.
    [[nodiscard]] QString pendingTarget() const;

    [[nodiscard]] bool running() const;

    /// Marks the confirmed action as in flight so the panel can disable its
    /// buttons. Only accepted inside a confirmation.
    Q_INVOKABLE void setRunning(bool running);

    /// Sends the pending termination. Returns true and emits terminateRequested
    /// only when the action can be confirmed now; otherwise returns false and
    /// changes nothing.
    Q_INVOKABLE bool confirm();

signals:
    /// The session list, the confirmation, its target or the running flag
    /// changed.
    void changed();

    /// A confirmed termination to send. An empty `sessionId` means
    /// terminateAllOtherSessions, anything else is a session id from
    /// sessionIds().
    void terminateRequested(const QString& sessionId);

private:
    [[nodiscard]] bool isTerminable(qint64 sessionId) const;

    telegram::SessionListModel m_sessions;
    telegram::SessionTerminationController m_termination;
};

} // namespace beygram::ui
