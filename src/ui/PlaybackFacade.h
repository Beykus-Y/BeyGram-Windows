#pragma once

#include "telegram/AudioTrackModel.h"
#include "telegram/MediaPlaylistModel.h"
#include "telegram/VideoPlayerState.h"
#include "telegram/VoiceWaveformModel.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace beygram::ui {

/// QML-facing facade over the playback models: the open clip, its audio tags,
/// its voice envelope and the media queue.
///
/// The facade owns its models and translates between the string ids QML uses
/// and the qint64 ids the models speak. It holds no transport and loads no
/// media: playPause() only flips the stored state and announces the intent
/// through playRequested()/pauseRequested() so the owner can start or stop the
/// real player, and next()/previous() only move the queue cursor and announce
/// the entry that should be opened. The position is pushed in by whoever owns
/// the player through seek().
///
/// Invariants:
///  - active() is true exactly while a clip is open, and opening a clip is
///    refused unless both its id and its duration are positive.
///  - title() is the label of the open clip only: the caller's title for a
///    video, "Performer — Title" (or the present half, or "Unknown title") for
///    an audio track.
///  - volume() and rate() are user preferences: they survive stop() and are
///    carried into the next clip, but they are only writable while a clip is
///    open, like the state they come from.
///  - waveform() is the open clip's envelope scaled into 0..1 and formatted
///    with three decimals, so a bar view can size itself without knowing the
///    source range. It is dropped when a new clip is opened.
///  - hasNext() reports whether the queue has an entry after the current one,
///    or any entry at all when nothing is current yet.
///
/// changed() is emitted once per call that actually changed something
/// observable; a call that clamps to the current value or is ignored while
/// nothing is open stays silent.
class PlaybackFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(bool playing READ playing NOTIFY changed)
    Q_PROPERTY(int positionMs READ positionMs NOTIFY changed)
    Q_PROPERTY(int durationMs READ durationMs NOTIFY changed)
    Q_PROPERTY(int volume READ volume NOTIFY changed)
    Q_PROPERTY(double rate READ rate NOTIFY changed)
    Q_PROPERTY(QStringList waveform READ waveform NOTIFY changed)
    Q_PROPERTY(bool hasNext READ hasNext NOTIFY changed)

public:
    explicit PlaybackFacade(QObject* parent = nullptr);

    /// Whether a clip is open.
    [[nodiscard]] bool active() const;

    /// Label of the open clip, empty when nothing is open.
    [[nodiscard]] QString title() const;

    /// Whether the open clip is playing.
    [[nodiscard]] bool playing() const;

    /// Playhead of the open clip in milliseconds, 0 when nothing is open.
    [[nodiscard]] int positionMs() const;

    /// Duration of the open clip in milliseconds, 0 when nothing is open.
    [[nodiscard]] int durationMs() const;

    /// Output volume as a percentage, 100 by default.
    [[nodiscard]] int volume() const;

    /// Playback speed, 1.0 by default.
    [[nodiscard]] double rate() const;

    /// The open clip's envelope normalized into 0..1 and formatted with three
    /// decimals; empty when no envelope is set.
    [[nodiscard]] QStringList waveform() const;

    /// Whether the queue has a following entry.
    [[nodiscard]] bool hasNext() const;

    /// Opens messageId as a video with the given duration and the given label.
    /// The id must be positive and the duration positive milliseconds; anything
    /// else is ignored and the current state is left untouched. Opening starts
    /// paused at position 0 and drops the previous clip's envelope and tags.
    Q_INVOKABLE void openVideo(const QString& messageId, const QString& title, int durationMs);

    /// Opens messageId as an audio track. The label becomes "Performer — Title",
    /// or whichever half is present, or "Unknown title" when both are empty.
    /// The same id and duration rules as openVideo() apply.
    Q_INVOKABLE void openAudio(const QString& messageId, const QString& title,
                               const QString& performer, int durationMs);

    /// Toggles between playing and paused and announces the intent through
    /// playRequested()/pauseRequested(). Ignored while nothing is open.
    Q_INVOKABLE void playPause();

    /// Closes the open clip, dropping its position and play flag while keeping
    /// the volume and rate preferences. Announces pauseRequested() for the clip
    /// that was open so the owner can stop the real player. The queue survives,
    /// so next() can resume it.
    Q_INVOKABLE void stop();

    /// Moves the playhead, clamped to 0..durationMs(). Ignored while nothing is
    /// open.
    Q_INVOKABLE void seek(int ms);

    /// Sets the output volume as a percentage, clamped to 0..100. Ignored while
    /// nothing is open.
    Q_INVOKABLE void setVolume(int percent);

    /// Sets the playback speed, clamped to 0.25..3.0. A NaN rate is ignored.
    /// Ignored while nothing is open.
    Q_INVOKABLE void setRate(double rate);

    /// Replaces the envelope with the given amplitudes. One bar is kept per
    /// incoming element, so a value that cannot be read as a number becomes a
    /// zero bar instead of shifting the rest; negative amplitudes become zero.
    Q_INVOKABLE void setWaveform(const QVariantList& samples);

    /// Replaces the queue. Entries are paired by index: an id that is not a
    /// positive integer is dropped, and a missing title becomes empty. The
    /// cursor is pointed back at the open clip when the clip is in the new
    /// queue.
    Q_INVOKABLE void setQueue(const QStringList& messageIds, const QStringList& titles);

    /// Moves to the following queue entry and announces it through
    /// playRequested(). Does nothing at the end of the queue.
    Q_INVOKABLE void next();

    /// Moves to the preceding queue entry and announces it through
    /// playRequested(). Does nothing at the start of the queue.
    Q_INVOKABLE void previous();

signals:
    /// Emitted whenever any observable state of the facade changed.
    void changed();

    /// Emitted when playback of messageId was requested.
    void playRequested(const QString& messageId);

    /// Emitted when pausing messageId was requested.
    void pauseRequested(const QString& messageId);

private:
    /// Every observable field, used to emit changed() exactly once per call
    /// that actually changed something.
    struct Snapshot {
        bool active = false;
        QString title;
        bool playing = false;
        int positionMs = 0;
        int durationMs = 0;
        int volume = 100;
        double rate = 1.0;
        QStringList waveform;
        bool hasNext = false;

        bool operator==(const Snapshot&) const = default;
    };

    [[nodiscard]] Snapshot snapshot() const;

    /// Emits changed() when the current snapshot differs from \a before.
    void commit(const Snapshot& before);

    /// Points the queue cursor at the open clip when the clip is queued.
    void syncQueueCursor();

    /// Moves the cursor one entry forward or back and announces the entry.
    void advance(bool forward);

    telegram::VideoPlayerState m_state;
    telegram::AudioTrackModel m_audioTracks;
    telegram::VoiceWaveformModel m_waveform;
    telegram::MediaPlaylistModel m_playlist;
    QString m_title;
};

} // namespace beygram::ui
