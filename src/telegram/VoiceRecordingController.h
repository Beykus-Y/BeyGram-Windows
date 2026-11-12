#pragma once

#include <QObject>
#include <QtGlobal>

namespace beygram::telegram {

/// Tracks the state of an in-progress voice message recording: whether the
/// recorder is running, whether it is paused, whether the lock gesture was used,
/// and how long the recording has been running.
///
/// A recording starts with start() and ends with either finish() (the user sent
/// it) or cancel() (the user aborted it). Both reset every field, so no state
/// from a previous recording can leak into the next one.
///
/// finish() refuses to complete a clip that is too short or too long, leaving
/// the recorder running so the caller can keep going or cancel. Elapsed time is
/// clamped to a sane range, and changed() is emitted only when the observed
/// state actually changed.
class VoiceRecordingController final : public QObject {
    Q_OBJECT

public:
    explicit VoiceRecordingController(QObject* parent = nullptr);

    /// Begins a recording, resetting paused, locked and elapsed state. Ignored
    /// when a recording is already in progress.
    void start();

    /// Aborts the recording and clears all state. No-op when not recording.
    void cancel();

    /// Completes the recording and clears all state. Ignored when not
    /// recording, or when the clip is too short or too long.
    void finish();

    /// True while a recording is in progress.
    [[nodiscard]] bool recording() const;

    /// True while the active recording is paused.
    [[nodiscard]] bool paused() const;

    /// Pauses or resumes the active recording. Ignored when not recording.
    void setPaused(bool paused);

    /// Sets the elapsed duration, clamped to 0..3600 seconds. Ignored when not
    /// recording.
    void setElapsedSeconds(int seconds);

    /// Elapsed duration in seconds, or 0 when not recording.
    [[nodiscard]] int elapsedSeconds() const;

    /// Marks the recording as locked, meaning the user may lift their finger
    /// without ending it. Ignored when not recording.
    void setLocked(bool locked);

    /// True while the active recording is locked.
    [[nodiscard]] bool locked() const;

    /// True when the clip is shorter than the one second minimum.
    [[nodiscard]] bool tooShort() const;

    /// True when the clip has reached the sixty second maximum.
    [[nodiscard]] bool tooLong() const;

signals:
    /// Emitted whenever the recording state actually changed.
    void changed();

private:
    bool m_recording = false;
    bool m_paused = false;
    bool m_locked = false;
    int m_elapsed = 0;
};

} // namespace beygram::telegram
