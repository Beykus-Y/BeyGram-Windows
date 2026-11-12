#pragma once

#include "telegram/CallController.h"
#include "telegram/CallHistoryModel.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace beygram::ui {

/// QML-facing shell around the call state and the call history.
///
/// CallController owns the phase machine and the per-call flags,
/// CallHistoryModel owns the ended calls. This facade adds the two things QML
/// needs and neither model has: the peer's display name (TDLib delivers a name,
/// not just an id, and the call screen shows the name) and a formatted history
/// list ready for a ListView or a Repeater.
///
/// The facade is transport-free like its models. It never sends a request:
/// startOutgoing() and incoming() are reports from the owner, and accept(),
/// decline() and end() are the user's answer, which leaves as
/// answerRequested()/endRequested() so the owner can call TDLib. A request is
/// emitted only when the controller really acted on the answer, so a duplicate
/// tap on an already-ended call neither changes the phase nor asks TDLib again.
///
/// Ids are strings because QML passes TDLib ids as strings (a 53-bit id does
/// not survive a JavaScript number). Each one is validated before it reaches
/// the controller: a non-numeric, zero or negative call or user id is refused
/// outright, so a malformed call never replaces the call on screen, never
/// overwrites the peer name and never emits outgoingRequested().
///
/// active() is true only while there is a call to show: ringing, connecting or
/// active. The ended phase is deliberately not active, so an overlay bound to
/// active() closes when the call is over, while phase() still reports "ended"
/// and peerName() keeps the name for an ended screen or a log line.
///
/// changed() is the single NOTIFY for every property. It is emitted when the
/// controller reports a real change and when the history or the peer name
/// changes, so a view binds to it and repaints without polling.
class CallsFacade final : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString phase READ phase NOTIFY changed)
    Q_PROPERTY(QString peerName READ peerName NOTIFY changed)
    Q_PROPERTY(bool video READ video NOTIFY changed)
    Q_PROPERTY(bool muted READ muted NOTIFY changed)
    Q_PROPERTY(bool speaker READ speaker NOTIFY changed)
    Q_PROPERTY(int elapsed READ elapsed NOTIFY changed)
    Q_PROPERTY(QStringList history READ history NOTIFY changed)

public:
    explicit CallsFacade(QObject* parent = nullptr);

    /// True while a call is ringing, connecting or up.
    [[nodiscard]] bool active() const;

    /// Lowercase phase name for QML: "idle", "ringing", "connecting", "active"
    /// or "ended".
    [[nodiscard]] QString phase() const;

    /// The name the caller supplied for the current call. Retained after the
    /// call ended, like the ids in CallController.
    [[nodiscard]] QString peerName() const;

    [[nodiscard]] bool video() const;
    [[nodiscard]] bool muted() const;
    [[nodiscard]] bool speaker() const;

    /// Call duration in seconds, never negative.
    [[nodiscard]] int elapsed() const;

    /// The history, newest first, one line per call:
    ///   "name — mm:ss — missed"
    /// The last field is "missed" for a missed incoming call, "cancelled" for
    /// an abandoned outgoing one, and otherwise the direction ("outgoing" or
    /// "incoming"). A record added without a name renders as "Unknown".
    [[nodiscard]] QStringList history() const;

    /// Reports that the user started a call to `userId`. The call goes to
    /// Connecting, mute/speaker/elapsed are reset, and outgoingRequested() is
    /// emitted. Ignored, with no state change, when either id is not a positive
    /// number.
    Q_INVOKABLE void startOutgoing(const QString& callId, const QString& userId,
                                   const QString& peerName, bool video);

    /// Reports an incoming call from `userId`. The call goes to Ringing and
    /// waits for accept() or decline(). Ignored when either id is not a
    /// positive number.
    Q_INVOKABLE void incoming(const QString& callId, const QString& userId,
                              const QString& peerName, bool video);

    /// Answers the current call and emits answerRequested() with its id.
    /// Ignored, silently, when the controller does not accept the answer.
    Q_INVOKABLE void accept();

    /// Rejects a ringing call or hangs up a call in progress, then emits
    /// endRequested() with its id.
    Q_INVOKABLE void decline();

    /// Hangs up the current call and emits endRequested() with its id.
    Q_INVOKABLE void end();

    /// Per-call flags. Ignored while there is no call, and a value that is
    /// already set emits nothing.
    Q_INVOKABLE void setMuted(bool muted);
    Q_INVOKABLE void setSpeaker(bool speaker);

    /// Reports the call duration. Negative values are clamped to 0 by the
    /// controller, so the clock can never run backwards below zero.
    Q_INVOKABLE void setElapsed(int seconds);

    /// Appends one ended call to the history. `duration` is clamped to at least
    /// 0; `missed` together with `outgoing` marks a call the user abandoned
    /// before the peer answered.
    Q_INVOKABLE void addHistory(const QString& name, int duration, bool missed,
                                bool outgoing, bool video);

    /// Drops the whole history. Clearing an already empty history is a no-op.
    Q_INVOKABLE void clearHistory();

signals:
    void changed();

    /// The user wants to call `userId`. The owner turns this into a TDLib
    /// request; the facade has already moved to Connecting.
    void outgoingRequested(const QString& callId, const QString& userId, bool video);

    /// The user answered `callId`.
    void answerRequested(const QString& callId);

    /// The user declined or hung up `callId`.
    void endRequested(const QString& callId);

private:
    telegram::CallController m_controller;
    telegram::CallHistoryModel m_history;
    /// Display names keyed by the record id the history model stores, because a
    /// CallRecord carries the peer's id and not the name to show.
    QHash<qint64, QString> m_historyNames;
    QString m_callId;
    QString m_peerName;
    /// Monotonic counter behind addHistory(): it is both the record id and its
    /// date, so a later call always sorts ahead of an earlier one.
    qint64 m_nextHistoryId = 0;
};

} // namespace beygram::ui
