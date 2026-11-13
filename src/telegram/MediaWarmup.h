#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Bounded, priority-ordered warm-up queue for avatars and thumbnails.
///
/// A caller announces the media it wants with requestAvatar() or
/// requestThumbnail(), drains work with nextBatch(), and reports the outcome of
/// every id it was given through markReady() or markFailed(). An id is in
/// exactly one of three places: the pending queue, the in-flight set (handed out
/// by nextBatch() but not resolved yet) or the ready set. Requesting an id that
/// is already in flight or ready does nothing, so a repaint storm cannot start
/// the same download twice.
///
/// Ordering is by priority (a larger value is more urgent, matching the TDLib
/// convention documented on priorityFor() in MediaCachePolicy.h), then avatars
/// before thumbnails at equal priority, then FIFO by the order the ids were
/// first requested. A duplicate request keeps the highest priority and the
/// avatar kind: a thumbnail request never downgrades an avatar and a lower
/// priority never wins. An upgrade keeps the original FIFO position, so an id
/// that has been waiting longer still goes first within its new rank.
///
/// Every mutator is a no-op when it would not change the stored state, so
/// changed() is emitted once per real change and views never repaint for
/// nothing. The class performs no I/O and no logging.
class MediaWarmup final : public QObject {
    Q_OBJECT

public:
    explicit MediaWarmup(QObject* parent = nullptr);

    /// Caps how many ids one nextBatch() call hands out. The value is clamped to
    /// 1..64. The default is 12.
    void setBudget(int maxPerBatch);
    [[nodiscard]] int budget() const { return m_budget; }

    /// Queues an avatar for fileId. fileId must be positive. A queued duplicate
    /// only raises its priority, and an id that is in flight or ready is left
    /// alone.
    void requestAvatar(int fileId, int priority);

    /// Queues a thumbnail. The rules match requestAvatar(), but at equal
    /// priority an avatar is handed out first.
    void requestThumbnail(int fileId, int priority);

    /// Records that fileId is warm: it is removed from the queue or the
    /// in-flight set and remembered as ready, so later requests for it are
    /// ignored. An id that is already ready, or that was never requested, is
    /// left alone.
    void markReady(int fileId);

    /// Records that fileId could not be fetched. It is dropped from the queue,
    /// the in-flight set and the ready set, so it can be requested again later.
    /// An id that is not tracked at all is left alone.
    void markFailed(int fileId);

    /// Removes up to budget() ids in rank order and reports them as in flight.
    /// The returned ids are no longer queued; markReady() or markFailed()
    /// resolves them. An empty queue returns an empty vector.
    [[nodiscard]] QVector<int> nextBatch();

    /// True once markReady() accepted fileId and no later markFailed() dropped
    /// it.
    [[nodiscard]] bool isReady(int fileId) const;

    /// True while fileId waits in the queue. Ids handed out by nextBatch() are
    /// in flight, not queued.
    [[nodiscard]] bool isQueued(int fileId) const;

    /// Number of ids waiting in the queue, not counting in-flight ids.
    [[nodiscard]] int queuedCount() const;

    /// Forgets the queue, the in-flight set and the ready set. The budget is
    /// kept. A warm-up that is already empty is left alone.
    void clear();

signals:
    /// Emitted whenever the queue, the in-flight set, the ready set or the
    /// budget really changed.
    void changed();

    /// Emitted when requestAvatar() or requestThumbnail() accepts a new id, or
    /// raises the effective rank of a queued one, carrying the id and its
    /// priority. It is a hint that work exists; nextBatch() is the pull side.
    void downloadRequested(int fileId, int priority);

private:
    struct Entry {
        int priority = 0;
        bool avatar = false;
        quint64 sequence = 0;
    };

    void request(int fileId, int priority, bool avatar);

    int m_budget = 12;
    quint64 m_nextSequence = 0;
    QHash<int, Entry> m_pending;
    QSet<int> m_inFlight;
    QSet<int> m_ready;
};

} // namespace beygram::telegram
