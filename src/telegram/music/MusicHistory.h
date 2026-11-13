#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One play of a music track.
///
/// trackId is the identity the track has in the library, playedAt is the epoch
/// time in milliseconds at which playback started, positionMs is how far into
/// the track playback was when the event was recorded, and completed marks a
/// track that played to its end. Values are already parsed; nothing here
/// touches TDLib.
struct MusicPlayEvent {
    QString trackId;
    qint64 playedAt = 0;
    int positionMs = 0;
    bool completed = false;

    bool operator==(const MusicPlayEvent&) const = default;
};

/// The listen history of the active account.
///
/// Events are kept newest first, ordered by playedAt. record() ignores an event
/// whose trackId is blank after trimming or whose playedAt is not positive,
/// clamps a negative positionMs to zero and keeps at most maxEntries events
/// (the oldest fall off). clear() drops every event.
///
/// changed() is emitted only when the stored events actually change: an ignored
/// event and clear() on an empty history stay silent. A record whose event is
/// older than every kept entry of a full history also changes nothing, so it
/// stays silent as well.
class MusicHistory final : public QObject {
    Q_OBJECT

public:
    explicit MusicHistory(QObject* parent = nullptr);

    /// Adds \a event, keeping at most \a maxEntries events, newest first.
    /// maxEntries is clamped to zero, so a non-positive value keeps no events
    /// and empties the history.
    void record(const MusicPlayEvent& event, int maxEntries = 200);

    /// Returns a copy of the stored events, newest first.
    [[nodiscard]] QVector<MusicPlayEvent> events() const;

    /// Returns the ids of the most recently played tracks, newest first, each
    /// track once. At most \a limit ids are returned; a non-positive limit
    /// returns none.
    [[nodiscard]] QVector<QString> recentlyPlayed(int limit = 20) const;

    /// Returns how many stored events belong to \a trackId. The id is trimmed
    /// before it is compared and ids are compared exactly; a blank id matches
    /// nothing.
    [[nodiscard]] int playCount(const QString& trackId) const;

    /// Drops every event, emitting changed() when there was anything to drop.
    void clear();

signals:
    void changed();

private:
    QVector<MusicPlayEvent> m_events;
};

} // namespace beygram::telegram
