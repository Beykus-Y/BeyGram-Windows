#include "VoiceRecordingController.h"

namespace beygram::telegram {

namespace {

/// Shortest clip that may be sent, in seconds.
constexpr int kMinDurationSeconds = 1;

/// Longest clip that may be sent, in seconds.
constexpr int kMaxDurationSeconds = 60;

/// Upper bound accepted for a reported elapsed time, in seconds.
constexpr int kMaxElapsedSeconds = 3600;

int clampedElapsed(int seconds)
{
    if (seconds < 0)
        return 0;
    if (seconds > kMaxElapsedSeconds)
        return kMaxElapsedSeconds;
    return seconds;
}

} // namespace

VoiceRecordingController::VoiceRecordingController(QObject* parent)
    : QObject(parent)
{
}

void VoiceRecordingController::start()
{
    if (m_recording)
        return;

    m_recording = true;
    m_paused = false;
    m_locked = false;
    m_elapsed = 0;
    emit changed();
}

void VoiceRecordingController::cancel()
{
    if (!m_recording)
        return;

    m_recording = false;
    m_paused = false;
    m_locked = false;
    m_elapsed = 0;
    emit changed();
}

void VoiceRecordingController::finish()
{
    if (!m_recording)
        return;

    if (tooShort() || tooLong())
        return;

    m_recording = false;
    m_paused = false;
    m_locked = false;
    m_elapsed = 0;
    emit changed();
}

bool VoiceRecordingController::recording() const
{
    return m_recording;
}

bool VoiceRecordingController::paused() const
{
    return m_paused;
}

void VoiceRecordingController::setPaused(bool paused)
{
    if (!m_recording)
        return;

    if (m_paused == paused)
        return;

    m_paused = paused;
    emit changed();
}

void VoiceRecordingController::setElapsedSeconds(int seconds)
{
    if (!m_recording)
        return;

    const int elapsed = clampedElapsed(seconds);
    if (m_elapsed == elapsed)
        return;

    m_elapsed = elapsed;
    emit changed();
}

int VoiceRecordingController::elapsedSeconds() const
{
    return m_elapsed;
}

void VoiceRecordingController::setLocked(bool locked)
{
    if (!m_recording)
        return;

    if (m_locked == locked)
        return;

    m_locked = locked;
    emit changed();
}

bool VoiceRecordingController::locked() const
{
    return m_locked;
}

bool VoiceRecordingController::tooShort() const
{
    return m_elapsed < kMinDurationSeconds;
}

bool VoiceRecordingController::tooLong() const
{
    return m_elapsed >= kMaxDurationSeconds;
}

} // namespace beygram::telegram
