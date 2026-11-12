#include "CallController.h"

namespace beygram::telegram {

CallController::CallController(QObject* parent)
    : QObject(parent)
{
}

void CallController::startOutgoing(qint64 callId, qint64 userId, bool video)
{
    if (callId <= 0 || userId <= 0)
        return;

    // An outgoing call is ringing for the peer, not for us: the local side goes
    // straight to Connecting and waits for the media to come up.
    const bool differs = m_phase != CallPhase::Connecting || m_callId != callId
        || m_userId != userId || m_video != video || m_muted || m_speaker
        || m_elapsed != 0;

    m_phase = CallPhase::Connecting;
    m_callId = callId;
    m_userId = userId;
    m_video = video;
    // These belong to the previous call and never carry over.
    m_muted = false;
    m_speaker = false;
    m_elapsed = 0;

    if (differs)
        emit changed();
}

void CallController::incoming(qint64 callId, qint64 userId, bool video)
{
    if (callId <= 0 || userId <= 0)
        return;

    const bool differs = m_phase != CallPhase::Ringing || m_callId != callId
        || m_userId != userId || m_video != video || m_muted || m_speaker
        || m_elapsed != 0;

    m_phase = CallPhase::Ringing;
    m_callId = callId;
    m_userId = userId;
    m_video = video;
    m_muted = false;
    m_speaker = false;
    m_elapsed = 0;

    if (differs)
        emit changed();
}

void CallController::accept()
{
    // Accepting is only meaningful for a call that is still being set up: an
    // ended call must not come back to life, and there is nothing to accept
    // while Idle or once Active.
    if (m_phase != CallPhase::Ringing && m_phase != CallPhase::Connecting)
        return;

    m_phase = CallPhase::Active;
    emit changed();
}

void CallController::decline()
{
    // Idle has nothing to end, and a repeated decline/end of the same call is a
    // duplicate update rather than a new event.
    if (m_phase == CallPhase::Idle || m_phase == CallPhase::Ended)
        return;

    // The ids stay: the ended screen still shows the peer and the duration.
    m_phase = CallPhase::Ended;
    emit changed();
}

void CallController::end()
{
    if (m_phase == CallPhase::Idle || m_phase == CallPhase::Ended)
        return;

    m_phase = CallPhase::Ended;
    emit changed();
}

void CallController::setMuted(bool muted)
{
    if (m_phase == CallPhase::Idle || m_muted == muted)
        return;
    m_muted = muted;
    emit changed();
}

void CallController::setSpeaker(bool speaker)
{
    if (m_phase == CallPhase::Idle || m_speaker == speaker)
        return;
    m_speaker = speaker;
    emit changed();
}

void CallController::setElapsedSeconds(int seconds)
{
    // A negative duration would render as a negative clock; clamp instead of
    // rejecting so the rest of the value is still usable.
    const int clamped = seconds < 0 ? 0 : seconds;

    if (m_phase == CallPhase::Idle || m_elapsed == clamped)
        return;
    m_elapsed = clamped;
    emit changed();
}

QString CallController::phaseName() const
{
    switch (m_phase) {
    case CallPhase::Idle:
        return QStringLiteral("idle");
    case CallPhase::Ringing:
        return QStringLiteral("ringing");
    case CallPhase::Connecting:
        return QStringLiteral("connecting");
    case CallPhase::Active:
        return QStringLiteral("active");
    case CallPhase::Ended:
        return QStringLiteral("ended");
    }
    return QStringLiteral("idle");
}

} // namespace beygram::telegram
