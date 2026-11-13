#pragma once

#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One resumable music session: the queue the user was playing, the current
/// track, where its playhead stood, and the playback preferences that belong
/// to that queue.
///
/// [trackId] names the current track and [index] is its position in [queue].
/// Both are kept because a caller may know one without the other; the store
/// clamps [index] to the queue but never reorders or re-derives it from
/// [trackId]. [positionMs] is clamped to 0..[durationMs], [repeatMode] to
/// 0..2 (Off, All, One, the order used by MusicPlayer) and [crossfadeMs] to
/// 0..12000. [savedAt] is a Unix timestamp in seconds, used only to decide
/// whether the session is still fresh.
struct MusicSessionState {
    QString trackId;
    QVector<QString> queue;
    int index = -1;
    int positionMs = 0;
    int durationMs = 0;
    bool playing = false;
    bool shuffle = false;
    int repeatMode = 0;
    int crossfadeMs = 4000;
    qint64 savedAt = 0;

    bool operator==(const MusicSessionState& other) const = default;
};

/// Reads and writes one music session as versioned JSON at a given path.
///
/// The document is an object with a `version` field and the fields of
/// [MusicSessionState] under the same names. A missing, unreadable, malformed
/// or differently versioned document loads as an empty session; individual
/// fields that are absent or of the wrong JSON type fall back to their
/// defaults instead of failing the whole document.
///
/// Every value passes through the same normalization on the way in and out:
/// queue ids are trimmed, empty ids are dropped and repeated ids keep their
/// first occurrence; a queue without entries clears the current track
/// ([trackId] empty, [index] -1, [positionMs] 0, not playing); otherwise
/// [index] is clamped to a valid position, the position to 0..duration, the
/// repeat mode to 0..2 and the crossfade to 0..12000. save() applies that
/// normalization before writing, so load() returns exactly what a successful
/// save() stored.
///
/// Writes create the parent directory when needed and go through QSaveFile,
/// so a failed or interrupted write leaves the previous file intact. An empty
/// path makes load() return an empty session and save() return false.
class MusicSessionStore final {
public:
    /// Version written by save() and accepted by load().
    static constexpr int kSchemaVersion = 1;

    explicit MusicSessionStore(QString path);

    /// Returns the stored session, or an empty one when there is nothing
    /// usable to read.
    [[nodiscard]] MusicSessionState load() const;

    /// Replaces the stored session with a normalized copy of \a state.
    /// Returns false when the file could not be written.
    bool save(const MusicSessionState& state) const;

    /// Returns the path this store was created with.
    [[nodiscard]] QString path() const;

private:
    QString m_path;
};

/// Returns a normalized copy of \a saved when it is still fresh, or an empty
/// session when it is not.
///
/// A session is fresh when [MusicSessionState::savedAt] is positive, not in
/// the future and no older than \a maxAgeSeconds. A negative \a maxAgeSeconds
/// is treated as 0, so only a session saved at exactly \a nowUnix is accepted.
/// A timestamp in the future is not trusted, because it would otherwise stay
/// fresh until the clock caught up. An empty queue restores nothing. The
/// queue, index and position are clamped as described by
/// [MusicSessionState]; when [MusicSessionState::trackId] is empty or no
/// longer occurs in the queue, playback is restored paused and the queue is
/// kept, so the caller can still show it.
[[nodiscard]] MusicSessionState restoreMusicSession(const MusicSessionState& saved,
                                                    qint64 nowUnix,
                                                    int maxAgeSeconds = 86400);

} // namespace beygram::telegram
