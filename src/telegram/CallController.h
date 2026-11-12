#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Which part of a call's lifetime the controller is in.
///
///   Idle        no call, no ids
///   Ringing     an incoming call waiting for the user
///   Connecting  an outgoing call, or an incoming one the user accepted, whose
///               media is not up yet
///   Active      the call is up and can be muted, put on speaker, and timed
///   Ended       the call is over; the ids are kept so the ended screen can
///               still show who it was with
enum class CallPhase { Idle, Ringing, Connecting, Active, Ended };

/// The state behind the call screen: one call at a time, its phase, who it is
/// with, and the three per-call flags the user can toggle.
///
/// The controller is deliberately transport-free: it does not talk to TDLib and
/// does not run a timer. The owner reports events with startOutgoing() and
/// incoming(), forwards the user's answer with accept()/decline()/end(), and
/// pushes the duration with setElapsedSeconds(), so the phase here is always
/// what the UI was last told rather than a guess from the wall clock.
///
/// Call and user ids must be positive; a call with a zero or negative id is
/// ignored, so a malformed update cannot leave the screen in a call with
/// nobody. Starting a call from any phase replaces the previous one and clears
/// mute, speaker and elapsed, which belong to the call that just ended.
///
/// Ended keeps the ids on purpose: the ended screen shows the peer and the
/// duration, and a caller that only ever sees phase() still knows which call
/// the outcome belongs to. end() and decline() on an already-ended call are
/// no-ops, so a duplicate "call ended" update is harmless.
///
/// changed() is emitted only when something observable actually changed, so a
/// view can bind to it and repaint without chasing no-op updates.
class CallController final : public QObject {
    Q_OBJECT

public:
    explicit CallController(QObject* parent = nullptr);

    /// Starts an outgoing call to `userId`. The call goes straight to
    /// Connecting: an outgoing call is ringing for the peer, not for us.
    /// Ignored unless both ids are positive.
    void startOutgoing(qint64 callId, qint64 userId, bool video);

    /// Reports an incoming call from `userId`. The call goes to Ringing and
    /// waits for accept() or decline(). Ignored unless both ids are positive.
    void incoming(qint64 callId, qint64 userId, bool video);

    /// Answers a Ringing or Connecting call, which makes it Active. Ignored in
    /// every other phase, including a call that already ended.
    void accept();

    /// Rejects a Ringing call or, once it is up, hangs it up. Either way the
    /// call ends. Ignored while Idle or already Ended.
    void decline();

    /// Hangs up the current call. Same effect as decline(); the two are kept
    /// apart because the UI offers "decline" for a ring and "end" for a call in
    /// progress. Ignored while Idle or already Ended.
    void end();

    [[nodiscard]] CallPhase phase() const { return m_phase; }

    /// Call id, or 0 while Idle. Retained after the call ended.
    [[nodiscard]] qint64 callId() const { return m_callId; }

    /// Peer's user id, or 0 while Idle. Retained after the call ended.
    [[nodiscard]] qint64 userId() const { return m_userId; }

    [[nodiscard]] bool video() const { return m_video; }
    [[nodiscard]] bool muted() const { return m_muted; }
    [[nodiscard]] bool speaker() const { return m_speaker; }

    /// Per-call flags. Ignored while Idle, and a value that is already set
    /// emits nothing.
    void setMuted(bool muted);
    void setSpeaker(bool speaker);

    /// Reports the call duration. Negative values are clamped to 0, so a
    /// broken duration cannot render as a negative clock. Ignored while Idle.
    void setElapsedSeconds(int seconds);

    /// Call duration in seconds, never negative.
    [[nodiscard]] int elapsedSeconds() const { return m_elapsed; }

    /// Lowercase phase name for QML and logs: "idle", "ringing", "connecting",
    /// "active" or "ended".
    [[nodiscard]] QString phaseName() const;

signals:
    /// Emitted whenever the phase, the ids or one of the flags actually
    /// changed.
    void changed();

private:
    CallPhase m_phase = CallPhase::Idle;
    qint64 m_callId = 0, m_userId = 0;
    bool m_video = false, m_muted = false, m_speaker = false;
    int m_elapsed = 0;
};

} // namespace beygram::telegram
