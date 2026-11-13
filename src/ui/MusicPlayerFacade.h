#pragma once

#include "telegram/music/MusicPlayer.h"
#include "telegram/music/MusicSession.h"
#include "telegram/music/PlaybackSettings.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::ui {

/// QML-facing facade over the music queue, its playback preferences and the
/// persisted session.
///
/// The facade owns a MusicPlayer, a PlaybackSettings and a MusicSessionStore
/// and is their only writer: every transport call goes through it, so the
/// properties below and the changed() signal always describe the same state.
/// It holds no audio backend and loads no media. playPause(), next() and
/// previous() only move the in-memory state machine; whoever owns the real
/// player watches playing() and positionMs() and performs the decode. The
/// duration of a loaded track is reported back with setDurationMs().
///
/// Properties:
///  - title() is the label registered for the current track, empty when the
///    track has no title or nothing is current. Titles are not part of a saved
///    session, so a restored queue starts without them; setTitleFor() fills
///    them in once the caller knows the metadata.
///  - playing(), positionMs() and durationMs() mirror MusicPlayer. The
///    playhead is clamped to 0..durationMs().
///  - speed() is the PlaybackSettings speed, clamped to 0.5..2.0 and rounded to
///    two decimals. It survives queue changes but is not stored in the session.
///  - shuffle() and repeatMode() mirror MusicPlayer. repeatMode() is one of
///    "off", "all" or "one", which is what QML compares against.
///  - hasNext() is true when next() can keep a track current: an idle queue
///    with at least one entry, another entry in the active order, or any repeat
///    mode other than Off. With shuffle the exact end of the order belongs to
///    MusicPlayer, so the last step can still stop playback.
///
/// Transport:
///  - playPause() starts the first queue entry when nothing is current and
///    otherwise toggles the play flag. It never restarts the current track.
///  - next()/previous() start the first/last queue entry when nothing is
///    current, otherwise they delegate to MusicPlayer, including its repeat
///    rules and its behaviour at the end of the order.
///  - seek() moves the playhead, clamped to 0..durationMs(), like MusicPlayer.
///
/// Session:
///  - saveSession() stores the queue, current track, playhead, duration, play
///    flag, shuffle, repeat and crossfade with the current Unix time. An empty
///    queue is written as an empty session, so a stale session cannot be
///    restored later. Write failures are silent.
///  - restoreSession() ignores a missing, malformed, expired (older than 24 h)
///    or future-dated session and leaves the facade untouched. A valid one
///    replaces the queue and restores the current track, position, duration,
///    play flag, shuffle, repeat and crossfade. Titles already registered for
///    ids in the restored queue are kept; all others are dropped.
///  - The store lives at <data>/music/session.json. The
///    BEYGRAM_MUSIC_SESSION_PATH environment variable overrides that path; the
///    test suite uses it to keep session files inside a temporary directory.
///
/// changed() is emitted once per call that actually changed an observable
/// property. A call that clamps to the stored value, repeats a value or is
/// ignored while the queue is empty stays silent. trackChanged() is forwarded
/// from MusicPlayer and is emitted before the matching changed().
class MusicPlayerFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(bool playing READ playing NOTIFY changed)
    Q_PROPERTY(int positionMs READ positionMs NOTIFY changed)
    Q_PROPERTY(int durationMs READ durationMs NOTIFY changed)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY changed)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY changed)
    Q_PROPERTY(QString repeatMode READ repeatMode NOTIFY changed)
    Q_PROPERTY(bool hasNext READ hasNext NOTIFY changed)

public:
    explicit MusicPlayerFacade(QObject* parent = nullptr);

    /// Label of the current track, empty when nothing is current or the track
    /// has no registered title.
    [[nodiscard]] QString title() const;

    /// Whether playback should be running.
    [[nodiscard]] bool playing() const;

    /// Playhead of the current track in milliseconds, 0 when unknown.
    [[nodiscard]] int positionMs() const;

    /// Duration of the current track in milliseconds, 0 while unknown.
    [[nodiscard]] int durationMs() const;

    /// Playback speed, 1.0 by default.
    [[nodiscard]] double speed() const;

    /// Sets the speed, clamped to 0.5..2.0 and rounded to two decimals. A NaN
    /// is ignored, as in PlaybackSettings.
    void setSpeed(double speed);

    /// Whether the queue advances in shuffled order.
    [[nodiscard]] bool shuffle() const;

    /// Turns the shuffled order on or off. The current track and playhead are
    /// kept; only the order next()/previous() follow changes.
    void setShuffle(bool on);

    /// The repeat mode as "off", "all" or "one".
    [[nodiscard]] QString repeatMode() const;

    /// Whether next() can keep a track current, see the class comment.
    [[nodiscard]] bool hasNext() const;

    /// Replaces the queue. Ids and titles are paired by index; an empty id is
    /// dropped, duplicate ids keep their first occurrence and a missing or
    /// empty title leaves that track untitled. Nothing starts playing on its
    /// own: the current track stays current only while its id is still queued.
    Q_INVOKABLE void setQueue(const QStringList& trackIds, const QStringList& titles);

    /// Starts the first queue entry when nothing is current, otherwise toggles
    /// between playing and paused. Ignored when the queue is empty.
    Q_INVOKABLE void playPause();

    /// Moves to the next track in the active order and starts it. From an idle
    /// queue it starts the first entry. At the end of the order it follows
    /// MusicPlayer: wrap with RepeatMode::All, stay with RepeatMode::One, or
    /// stop playback while keeping the last track current.
    Q_INVOKABLE void next();

    /// Moves to the previous track in the active order and starts it. From an
    /// idle queue it starts the last entry. At the start of the order it wraps
    /// only with RepeatMode::All.
    Q_INVOKABLE void previous();

    /// Moves the playhead, clamped to 0..durationMs(). A backend may also use
    /// this to publish its current position.
    Q_INVOKABLE void seek(int ms);

    /// Reports the length of the current track, clamped to >= 0. A playback
    /// backend calls this after loading the media; without it durationMs()
    /// would stay 0 and the seek control could never enable itself.
    Q_INVOKABLE void setDurationMs(int ms);

    /// Advances Off -> All -> One -> Off.
    Q_INVOKABLE void cycleRepeat();

    /// Writes the current session to the store. No-op state-wise; write
    /// failures are silent. Titles are not persisted.
    Q_INVOKABLE void saveSession();

    /// Loads the stored session and applies it. A missing, malformed, expired
    /// or future-dated session leaves the facade untouched.
    Q_INVOKABLE void restoreSession();

    /// Registers the display title for track \a id. An empty id is ignored and
    /// an empty title removes the registration. changed() is emitted only when
    /// the title of the current track changes.
    Q_INVOKABLE void setTitleFor(const QString& id, const QString& title);

signals:
    /// Emitted whenever any observable property above changed.
    void changed();

    /// Emitted when the current track id really changed, including when the
    /// current track was removed and the id became empty.
    void trackChanged(const QString& id);

private:
    /// Every observable field, used to emit changed() exactly once per call
    /// that actually changed something.
    struct Snapshot {
        QString title;
        bool playing = false;
        int positionMs = 0;
        int durationMs = 0;
        double speed = 1.0;
        bool shuffle = false;
        QString repeatMode;
        bool hasNext = false;

        bool operator==(const Snapshot&) const = default;
    };

    [[nodiscard]] Snapshot snapshot() const;

    /// Emits changed() when the current snapshot differs from \a before.
    void commit(const Snapshot& before);

    telegram::MusicPlayer m_player;
    telegram::PlaybackSettings m_settings;
    telegram::MusicSessionStore m_sessionStore;
    QHash<QString, QString> m_titles;
};

} // namespace beygram::ui
