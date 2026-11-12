#pragma once

#include <QObject>
#include <QtGlobal>

namespace beygram::telegram {

/// Playback clock for a single story.
///
/// The viewer owns the timer; this model only holds the position so the
/// progress bar, the pause button and the "story ended" decision all read from
/// one source of truth. Positions are absolute: tick() receives the current
/// playback offset in milliseconds, not a delta, which keeps the model
/// idempotent when a timer fires late or twice.
///
/// A model is either idle (no story) or active. begin() is the only way to make
/// it active; tick(), pause() and resume() are silent no-ops while idle.
class StoryProgressModel final : public QObject {
    Q_OBJECT

public:
    explicit StoryProgressModel(QObject* parent = nullptr);

    /// Starts (or restarts) playback of storyId with the given duration.
    /// Non-positive story ids or durations are ignored and leave the current
    /// state untouched, so a malformed server payload cannot wipe a running
    /// story.
    void begin(qint64 storyId, int durationMs);

    /// Sets the absolute playback position in milliseconds. Ignored while idle
    /// or paused. Negative values are treated as 0 and values past the duration
    /// clamp to it; reaching the end emits finishedChanged() exactly once.
    void tick(int elapsedMs);

    /// Freezes the position. No-op while idle or already paused.
    void pause();

    /// Continues after pause(). No-op while idle or already running.
    void resume();

    [[nodiscard]] bool active() const;
    [[nodiscard]] qint64 storyId() const;
    [[nodiscard]] int durationMs() const;
    [[nodiscard]] int elapsedMs() const;

    /// Playback position as 0.0..1.0. Returns 0.0 when the duration is unknown.
    [[nodiscard]] double progress() const;

    [[nodiscard]] bool paused() const;

    /// True once the position reached the duration. An idle model is never
    /// finished.
    [[nodiscard]] bool finished() const;

    /// Returns to the idle state. Emits changed() only when there was something
    /// to clear.
    void reset();

signals:
    /// Emitted whenever the visible state (story, position, pause flag) changed.
    void changed();

    /// Emitted when the finished() value flips, in either direction.
    void finishedChanged();

private:
    [[nodiscard]] bool isFinished() const;

    qint64 m_storyId = 0;
    int m_duration = 0;
    int m_elapsed = 0;
    bool m_paused = false;
};

} // namespace beygram::telegram
