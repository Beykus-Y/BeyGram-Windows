#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One music-bot track resolved from an audio message.
///
/// trackId identifies the track inside MusicBotCache and is always trimmed in a
/// stored entry. title and performer are the audio tags, fileUniqueId the
/// Telegram file's remote unique id (empty while unknown), durationSeconds the
/// clamped track length, fileId the TDLib file id (0 while unknown) and
/// cachedAt the Unix second the cache stored the entry (0 outside a cache).
struct BotTrack {
    QString trackId;
    QString title;
    QString performer;
    QString fileUniqueId;
    int durationSeconds = 0;
    int fileId = 0;
    qint64 cachedAt = 0;
};

/// Builds a track from a TDLib `message` whose content is `messageAudio`.
///
/// Only a message carrying a `messageAudio` content with an `audio` object is
/// accepted; every other message yields a default BotTrack with an empty
/// trackId. title and performer are taken from the audio tags, durationSeconds
/// is clamped to a non-negative int, fileId is the nested file id (0 when
/// absent, negative or out of the int range) and fileUniqueId is the file's
/// remote unique id. trackId is the trimmed title, falling back to
/// fileUniqueId when the title is blank. cachedAt is left at 0 because
/// recency belongs to MusicBotCache::put().
[[nodiscard]] BotTrack botTrackFromMessage(const QJsonObject& message);

/// Bounded memory of music-bot tracks, keyed by trackId.
///
/// Entries are identified by their trimmed trackId: putting an id that is
/// already here replaces its fields instead of adding a second row, and makes
/// it the most recent entry because its cachedAt becomes the new `nowUnix`.
/// A track whose trackId is empty after trimming cannot be looked up later and
/// is ignored.
///
/// The store is bounded. capacity() is clamped to 16..1000 and defaults to 100,
/// and once a put() or a smaller capacity would exceed it, the least recently
/// cached entries go first. Equal cachedAt values are evicted by ascending
/// trackId, so the outcome never depends on the order tracks happened to
/// arrive in.
///
/// Reading is pure: get() and contains() never change recency, so a track that
/// is looked at often but not re-cached can still be evicted. Only put() moves
/// an entry to the most recent position.
///
/// changed() is emitted only when the stored set really differs: an empty
/// trackId is ignored, re-putting an entry with the same fields and the same
/// `nowUnix` is a no-op, and so is removing an absent track or clearing an
/// empty store. A newcomer that arrives at capacity and is older than
/// everything already stored is evicted immediately, which also leaves the
/// store unchanged. Changing the capacity is a real change and signals even
/// when no entry had to be evicted.
///
/// Nothing here is persisted and the class is not thread-safe; the owner keeps
/// it on the thread that resolves bot tracks.
class MusicBotCache final : public QObject {
    Q_OBJECT

public:
    explicit MusicBotCache(QObject* parent = nullptr);

    /// Sets how many tracks are kept, clamped to 16..1000.
    ///
    /// Lowering the capacity evicts the least recently cached entries at once.
    /// Setting the value it already has does nothing and does not signal.
    void setCapacity(int capacity);

    [[nodiscard]] int capacity() const;

    /// Stores `track` as it was cached at `nowUnix`.
    ///
    /// The trackId is trimmed; an empty one is ignored. The entry is stored
    /// with `cachedAt = nowUnix` and every other field is kept as given. A
    /// trackId already present is replaced and becomes the most recent entry.
    /// An older `nowUnix` is accepted: the entry simply becomes the oldest one
    /// and can be evicted right away.
    void put(const BotTrack& track, qint64 nowUnix);

    /// The stored track, or a default-constructed one (empty trackId) when the
    /// id is not cached. The lookup id is trimmed; reading does not touch
    /// recency. Use contains() to tell a miss apart from an empty track.
    [[nodiscard]] BotTrack get(const QString& trackId) const;

    [[nodiscard]] bool contains(const QString& trackId) const;

    /// Drops one track. Removing an absent or empty id does nothing.
    void remove(const QString& trackId);

    /// Drops every track. Clearing an empty store does nothing.
    void clear();

    [[nodiscard]] int count() const;

signals:
    void changed();

private:
    static constexpr int kMinimumCapacity = 16;
    static constexpr int kMaximumCapacity = 1000;
    static constexpr int kDefaultCapacity = 100;

    /// Index of the trimmed `trackId` in m_entries, or -1 when empty or absent.
    [[nodiscard]] int indexOf(const QString& trackId) const;

    /// Removes the least recently cached entries until count() fits capacity().
    void evictOverflow();

    QVector<BotTrack> m_entries;
    int m_capacity = kDefaultCapacity;
};

} // namespace beygram::telegram
