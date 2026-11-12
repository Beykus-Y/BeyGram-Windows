#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One music track returned by the music search or playlist requests.
///
/// id identifies the track across pages and is the only field required for a
/// track to be usable. title and performer are the already-parsed tags and may
/// be empty. durationSeconds is the track length in whole seconds, fileId is
/// the TDLib file id of the audio (0 when the response carries no file), and
/// localPath is the downloaded file's local path (empty while it is not
/// downloaded). All fields are already-parsed values; nothing here touches
/// TDLib.
struct MusicTrack {
    qint64 id = 0;
    QString title;
    QString performer;
    int durationSeconds = 0;
    int fileId = 0;
    QString localPath;

    bool operator==(const MusicTrack&) const = default;
};

/// Builds a music search request for \a query.
///
/// The query is trimmed of surrounding whitespace. A query that is empty after
/// trimming produces an empty object, because there is nothing to search for.
/// \a limit is clamped to 1..100.
[[nodiscard]] QJsonObject searchMusicRequest(const QString& query, int limit);

/// Builds a request for the music playlist with \a playlistId.
[[nodiscard]] QJsonObject getMusicPlaylistRequest(qint64 playlistId);

/// Parses tracks from a music search or playlist response.
///
/// A response may carry its tracks in `tracks`, `music`, `results`, `messages`
/// or in `playlist.tracks`; a TDLib `messageAudio` message is accepted as well
/// as a plain track object. Entries that are not objects or that have no
/// positive id are dropped. A repeated id keeps its first track, and the
/// incoming order is otherwise preserved. Negative durations are clamped to
/// zero. Malformed fields are treated as absent rather than failing the whole
/// response.
[[nodiscard]] QVector<MusicTrack> musicTracksFromResponse(const QJsonObject& response);

/// Formats \a seconds as "m:ss", or "h:mm:ss" from one hour upwards.
///
/// A negative value is formatted as "0:00".
[[nodiscard]] QString formatDuration(int seconds);

/// The queue behind the music player.
///
/// Tracks are stored in order and every track is identified by its id.
/// setTracks() drops tracks without a positive id, keeps the first copy of a
/// repeated id, clamps negative durations to zero and preserves the incoming
/// order. Replacing the queue stops playback: the current index goes back to
/// -1, so a new queue never starts a track by itself.
///
/// playAt() accepts only an index inside the queue. next() and previous() step
/// through the queue in order. next() wraps from the last track to the first
/// only when repeatAll is true; previous() wraps from the first track to the
/// last only while the most recent next() call requested repeat-all. Replacing
/// the queue with different tracks, or clearing it, forgets that recorded mode.
/// Without a current track next() starts at the first track and previous() at
/// the last one.
///
/// Every mutation that changes the stored tracks or the current index emits
/// changed(); a call that leaves both as they were stays silent.
class MusicQueue final : public QObject {
    Q_OBJECT

public:
    explicit MusicQueue(QObject* parent = nullptr);

    /// Replaces the queue with a cleaned copy of \a incoming and clears the
    /// current index. Emits changed() only when the tracks or the index
    /// actually differ from the stored state.
    void setTracks(QVector<MusicTrack> incoming);

    /// Returns a copy of the stored tracks in stored order.
    [[nodiscard]] QVector<MusicTrack> tracks() const;

    /// Returns the index of the current track, or -1 when no track is current.
    [[nodiscard]] int currentIndex() const;

    /// Returns the current track, or a default MusicTrack when no track is
    /// current.
    [[nodiscard]] MusicTrack current() const;

    /// Makes \a index current. An index outside the queue is ignored, and
    /// selecting the already current index is a no-op.
    void playAt(int index);

    /// Moves to the following track. Returns false at the end of the queue
    /// unless \a repeatAll is true, in which case it wraps to the first track.
    bool next(bool repeatAll = false);

    /// Moves to the preceding track. Returns false at the start of the queue
    /// unless the most recent next() call requested repeat-all, in which case
    /// it wraps to the last track.
    bool previous();

    /// Drops every track and the current index, emitting changed() when there
    /// was anything to drop.
    void clear();

signals:
    void changed();

private:
    QVector<MusicTrack> m_tracks;
    int m_index = -1;
    bool m_repeatAll = false;
};

} // namespace beygram::telegram
