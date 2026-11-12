#pragma once

#include <QObject>
#include <QtGlobal>

namespace beygram::telegram {

/// Playback state of the one video the viewer currently has open.
///
/// Opening a video is transient UI state: nothing here is persisted or sent to
/// the server, and the model deliberately holds a single video at a time
/// because the viewer plays one at a time. Volume, mute and playback rate are
/// user preferences that outlive the video, so opening or closing a video keeps
/// them while the position and the play/pause flag belong to the open video and
/// are dropped with it.
///
/// A video counts as open only when both the message id and the duration are
/// positive; an invalid open() is ignored rather than half-applied, and every
/// setter is ignored while nothing is open so a stray seek or volume change
/// cannot leak into the next video.
class VideoPlayerState final : public QObject {
    Q_OBJECT

public:
    explicit VideoPlayerState(QObject* parent = nullptr);

    /// Opens messageId as the playing video with the given duration.
    ///
    /// The id must be positive and the duration positive milliseconds;
    /// anything else is ignored and the current state is left untouched. A
    /// valid open starts at position 0 and paused, while volume, mute and rate
    /// are kept because they are user preferences rather than per-video state.
    /// Reopening the same video at the start is a no-op.
    void open(qint64 messageId, int durationMs);

    /// Closes the open video, dropping its id, duration, position and play
    /// flag. Volume, mute and rate survive. Closing while nothing is open is a
    /// no-op.
    void close();

    /// Whether a video is open. False before the first open() and after
    /// close()/reset().
    [[nodiscard]] bool active() const;

    /// Id of the open message, or 0 when no video is open.
    [[nodiscard]] qint64 messageId() const;

    /// Duration of the open video in milliseconds, or 0 when no video is open.
    [[nodiscard]] int durationMs() const;

    /// Moves the playhead to ms, clamped to 0..durationMs(). Ignored while no
    /// video is open.
    void setPosition(int ms);

    /// Playhead of the open video in milliseconds, 0 when no video is open.
    [[nodiscard]] int positionMs() const;

    /// Starts or pauses the open video. Ignored while no video is open.
    void setPlaying(bool playing);

    /// Whether the open video is playing. False when no video is open.
    [[nodiscard]] bool playing() const;

    /// Sets the output volume as a percentage, clamped to 0..100. Ignored while
    /// no video is open.
    void setVolume(int percent);

    /// Volume percentage, 100 by default.
    [[nodiscard]] int volume() const;

    /// Mutes or unmutes the output. Ignored while no video is open.
    void setMuted(bool muted);

    /// Whether the output is muted, false by default.
    [[nodiscard]] bool muted() const;

    /// Sets the playback speed, clamped to 0.25..3.0. A NaN rate is ignored;
    /// infinities clamp to the nearest bound. Ignored while no video is open.
    void setRate(double rate);

    /// Playback speed, 1.0 by default.
    [[nodiscard]] double rate() const;

    /// How much of the video has played, as a fraction in 0..1. 0 while no
    /// video is open, since without a duration there is nothing to measure.
    [[nodiscard]] double progress() const;

    /// Returns every field to its default: nothing open, volume 100, unmuted
    /// and normal speed. Unlike close(), this also forgets the user
    /// preferences, which is what a fresh session or an explicit reset wants.
    void reset();

signals:
    /// Emitted whenever any part of the state above actually changed. Calls
    /// that clamp to the current value or are ignored while inactive emit
    /// nothing.
    void changed();

private:
    qint64 m_messageId = 0;
    int m_duration = 0;
    int m_position = 0;
    int m_volume = 100;
    bool m_playing = false;
    bool m_muted = false;
    double m_rate = 1.0;
};

} // namespace beygram::telegram
