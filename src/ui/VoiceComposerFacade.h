#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace beygram::telegram {
class VoiceRecordingController;
class VoiceWaveformModel;
}

namespace beygram::ui {

/// QML-facing view of an in-progress voice message recording.
///
/// The facade owns a `VoiceRecordingController` (recording / paused / locked /
/// elapsed state machine) and a `VoiceWaveformModel` (the amplitude envelope),
/// and republishes both as one flat set of read-only properties so the
/// composer bar can bind to a single object. Every property shares the same
/// `changed` notification because the bar always reads them together, and
/// because the backing pieces already suppress no-op updates.
///
/// A recording starts with start() and ends with either finish() (the user
/// sent it) or cancel() (the user aborted it). finish() only completes clips
/// the controller accepts, so it emits sendRequested(duration) when the clip is
/// between one and fifty-nine seconds and does nothing otherwise, leaving the
/// recording running. cancel() emits cancelRequested() only when a recording
/// was actually active. Both paths drop the envelope so the next recording
/// starts from an empty strip; sendRequested is emitted first, while the final
/// envelope is still readable.
///
/// The waveform is exposed as normalized 0.000..1.000 bar heights formatted
/// with three decimals, which is what the QML strip multiplies by its height.
class VoiceComposerFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool recording READ recording NOTIFY changed)
    Q_PROPERTY(bool paused READ paused NOTIFY changed)
    Q_PROPERTY(bool locked READ locked NOTIFY changed)
    Q_PROPERTY(int elapsed READ elapsed NOTIFY changed)
    Q_PROPERTY(QString elapsedText READ elapsedText NOTIFY changed)
    Q_PROPERTY(QStringList waveform READ waveform NOTIFY changed)
    Q_PROPERTY(bool canSend READ canSend NOTIFY changed)

public:
    explicit VoiceComposerFacade(QObject* parent = nullptr);

    /// True while a recording is in progress.
    [[nodiscard]] bool recording() const;

    /// True while the active recording is paused.
    [[nodiscard]] bool paused() const;

    /// True once the lock gesture marked the recording as hands-free.
    [[nodiscard]] bool locked() const;

    /// Elapsed duration of the active recording in seconds, or 0 when idle.
    [[nodiscard]] int elapsed() const;

    /// The elapsed duration as `m:ss`, e.g. "0:07", "1:05", "60:00".
    [[nodiscard]] QString elapsedText() const;

    /// Normalized envelope bars, each formatted as a 0.000..1.000 string.
    /// Empty when no samples were reported yet.
    [[nodiscard]] QStringList waveform() const;

    /// True when the current clip is long enough and short enough to send.
    /// Paused and locked recordings can still be sent.
    [[nodiscard]] bool canSend() const;

    /// Begins a recording and drops any leftover envelope. Ignored while a
    /// recording is already in progress, so a second press cannot restart the
    /// timer under the user.
    Q_INVOKABLE void start();

    /// Aborts the active recording, clears the envelope and emits
    /// cancelRequested(). No-op when idle.
    Q_INVOKABLE void cancel();

    /// Completes the active recording and emits sendRequested(duration) with
    /// the duration in seconds. Ignored when idle or when the clip is too short
    /// or too long, in which case the recording keeps running.
    Q_INVOKABLE void finish();

    /// Pauses or resumes the active recording. Ignored when idle.
    Q_INVOKABLE void setPaused(bool paused);

    /// Marks the active recording as locked or unlocked. Ignored when idle.
    Q_INVOKABLE void setLocked(bool locked);

    /// Reports the elapsed duration from the recorder's timer. The controller
    /// clamps it to 0..3600 seconds and ignores it when idle.
    Q_INVOKABLE void tick(int elapsedSeconds);

    /// Replaces the envelope with raw amplitudes, typically the values TDLib
    /// reports while recording. Negative amplitudes become zero and entries
    /// that are not numbers are dropped. Accepted regardless of recording
    /// state, but start() resets the strip so a new clip never inherits the
    /// previous envelope.
    Q_INVOKABLE void setWaveform(const QVariantList& samples);

signals:
    /// Emitted whenever any exposed property actually changed.
    void changed();

    /// The user accepted the clip; the caller should send a voice message of
    /// `durationSeconds` seconds.
    void sendRequested(int durationSeconds);

    /// The user aborted the clip; the caller should discard it.
    void cancelRequested();

private:
    telegram::VoiceRecordingController* m_controller = nullptr;
    telegram::VoiceWaveformModel* m_waveform = nullptr;
};

} // namespace beygram::ui
