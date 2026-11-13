#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// How the player behaves when it reaches the end of the queue.
enum class RepeatMode {
    Off, ///< next() stops at the last track.
    All, ///< next() wraps from the last track to the first, previous() from the
         ///< first to the last.
    One, ///< next() stays on the current track.
};

/// The playback state of the music queue: which track is current, where the
/// playhead is, and in which order the queue advances.
///
/// This is a pure state machine. It never opens an audio device, downloads a
/// file or talks to TDLib; a playback backend watches the signals and does the
/// actual work. Everything here is in-memory, so closing the player loses the
/// queue rather than persisting it.
///
/// The queue is the list of track ids the user chose, in the order they chose
/// them. Empty ids are dropped and duplicates keep their first occurrence.
/// Shuffle does not reorder queue(): it only changes the order in which
/// next()/previous() walk the queue. That order is a permutation derived from
/// the ids alone with a fixed stable hash, so the same queue always shuffles
/// the same way and no random number generator is involved.
///
/// Track ids are opaque strings; the player never parses them.
class MusicPlayer final : public QObject {
    Q_OBJECT

public:
    explicit MusicPlayer(QObject* parent = nullptr);

    /// Replaces the queue with trackIds, dropping empty ids and keeping only
    /// the first occurrence of each id. Nothing starts playing on its own: the
    /// current track stays current only if its id is still in the new queue,
    /// and a queue edit that removes the current track stops playback.
    ///
    /// Setting a queue equal to the stored one is a no-op. Otherwise changed()
    /// is emitted once; trackChanged() is emitted with an empty id when the
    /// current track disappeared (including when the new queue is empty).
    void setQueue(const QVector<QString>& trackIds);

    /// The cleaned queue, in the order it was given. A copy, so editing it does
    /// not reach the player.
    [[nodiscard]] QVector<QString> queue() const;

    /// Position of the current track in queue(), or -1 when nothing is
    /// current.
    [[nodiscard]] int currentIndex() const;

    /// Id of the current track, or an empty string when nothing is current.
    [[nodiscard]] QString currentTrackId() const;

    /// Makes the track at index the current one, rewinds it to the start and
    /// starts playing. The index is a position in queue(), also while shuffle
    /// is on; the shuffled order continues from wherever that track sits in
    /// it. An index outside 0..queue().size()-1 is ignored.
    ///
    /// Calling it for the track that is already current restarts it without a
    /// trackChanged() signal.
    void playAt(int index);

    /// Moves to the next track in the active order and starts it. With
    /// RepeatMode::One it stays on the current track and leaves the position
    /// alone. At the end of the order it wraps with RepeatMode::All, otherwise
    /// it stops playback and keeps the last track current.
    ///
    /// Returns true when a track is current afterwards (including the
    /// RepeatMode::One stay), false when nothing was playing or playback
    /// stopped at the end.
    bool next();

    /// Moves to the previous track in the active order and starts it. At the
    /// start of the order it wraps to the last track only with
    /// RepeatMode::All; RepeatMode::One does not change this. Returns true when
    /// a track is current afterwards.
    bool previous();

    /// Turns the shuffled order on or off. The current track, its position and
    /// the play flag are kept; only the order next()/previous() follow changes.
    /// The permutation is a pure function of the queue ids, so turning shuffle
    /// on, off and on again yields the same order.
    void setShuffle(bool on);

    /// Whether the shuffled order is active, false by default.
    [[nodiscard]] bool shuffle() const;

    /// Sets how the player repeats, RepeatMode::Off by default. An out-of-range
    /// value is ignored.
    void setRepeatMode(RepeatMode mode);

    /// The current repeat mode.
    [[nodiscard]] RepeatMode repeatMode() const;

    /// Sets the crossfade length in milliseconds, clamped to 0..12000. 0 means
    /// no overlap. This is a preference of the playback backend, kept across
    /// clear(); the default is 4000, matching the Android player.
    void setCrossfadeMs(int ms);

    /// The crossfade length in milliseconds, 4000 by default.
    [[nodiscard]] int crossfadeMs() const;

    /// Moves the playhead, clamped to 0..durationMs(). With an unknown
    /// duration (0) any non-zero position therefore becomes 0.
    void setPositionMs(int ms);

    /// The playhead in milliseconds, 0 by default.
    [[nodiscard]] int positionMs() const;

    /// Sets the length of the current track in milliseconds; negative values
    /// become 0. A position past the new duration is clamped to it. Starting a
    /// different track resets the duration to 0 until the backend reports the
    /// new one.
    void setDurationMs(int ms);

    /// The duration of the current track in milliseconds, 0 while unknown.
    [[nodiscard]] int durationMs() const;

    /// Records whether playback should be running. This is intent only: the
    /// player produces no audio. playAt(), next() and previous() set it to true,
    /// next() at the end without RepeatMode::All sets it to false.
    void setPlaying(bool playing);

    /// Whether playback should be running, false by default.
    [[nodiscard]] bool playing() const;

    /// Drops the queue and the current track, stopping playback. Shuffle,
    /// repeat and crossfade are user preferences and survive. Emits
    /// trackChanged() with an empty id when a track was current, then
    /// changed(). Calling it while already empty and stopped is a no-op.
    void clear();

signals:
    /// Emitted whenever anything observable above actually changed. Calls that
    /// clamp to the stored value, repeat a value or are ignored emit nothing.
    void changed();

    /// Emitted when the current track id really changed: a different track
    /// became current, or the current track was removed by setQueue()/clear()
    /// and the id became empty. Restarting or re-selecting the same track does
    /// not emit it. It is emitted before the matching changed().
    void trackChanged(const QString& trackId);

private:
    void rebuildOrder(int focusIndex);
    void activate(int orderPosition);

    QVector<QString> m_queue;
    // Queue indices in playback order: identity without shuffle, the stable
    // hash permutation with it. m_orderPosition is the current track's slot.
    QVector<int> m_order;
    int m_orderPosition = -1;
    bool m_shuffle = false;
    RepeatMode m_repeatMode = RepeatMode::Off;
    int m_crossfadeMs = 4000;
    int m_positionMs = 0;
    int m_durationMs = 0;
    bool m_playing = false;
};

} // namespace beygram::telegram
