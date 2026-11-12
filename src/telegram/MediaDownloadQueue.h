#pragma once

#include <QHash>
#include <QObject>
#include <QPair>
#include <QVector>

namespace beygram::telegram {

/// Priority-ordered queue of TDLib file downloads with a bounded number of
/// concurrent transfers.
///
/// Priorities reuse the TDLib convention documented on priorityFor() in
/// MediaCachePolicy.h: an int where a larger value is more urgent (32 for
/// visible media, 16 for an invisible photo, 8 for every other invisible kind).
/// The queue therefore starts the highest value first and keeps FIFO order
/// between equal values. The value is stored verbatim; it is only compared,
/// never clamped, so any int a caller has is accepted.
///
/// A file id is unique across both lists: enqueue() refuses an id that is
/// already pending or active, and cancel() removes it from wherever it is.
/// startNext() is the only way into the active list, so the owner decides when
/// a transfer really starts; finish() and fail() are the only ways out of it.
/// A failed transfer returns to the front of the pending list, so its retry
/// runs before everything queued behind it.
///
/// Every mutator is a no-op when it would not change the stored lists, so
/// changed() is emitted once per real change and views never repaint for
/// nothing. The class performs no I/O and no logging.
class MediaDownloadQueue final : public QObject {
    Q_OBJECT

public:
    explicit MediaDownloadQueue(QObject* parent = nullptr);

    /// Caps how many transfers may be active at once. The value is clamped to
    /// 1..8. Lowering it below the current active count keeps those transfers
    /// running and only blocks startNext() until enough of them finish.
    void setMaxConcurrent(int max);
    [[nodiscard]] int maxConcurrent() const { return m_max; }

    /// Queues a download. fileId must be positive, and an id that is already
    /// pending or active is refused, so a duplicate is a no-op. The entry is
    /// inserted before every entry of a lower priority and after every entry of
    /// the same or a higher one, which keeps same-priority order stable.
    bool enqueue(int fileId, int priority);

    /// Drops a download from the pending or the active list. An unknown,
    /// finished or already cancelled id is refused.
    bool cancel(int fileId);

    /// True while the id is pending or active.
    [[nodiscard]] bool contains(int fileId) const;

    /// Ids in the order startNext() will hand them out, including a retry that
    /// fail() moved back to the front.
    [[nodiscard]] QVector<int> pending() const;

    /// Ids that are running right now, in the order they were started.
    [[nodiscard]] QVector<int> active() const;

    [[nodiscard]] int pendingCount() const;
    [[nodiscard]] int activeCount() const;

    /// Moves the first pending id to the active list. Refused while the queue is
    /// empty or maxConcurrent() transfers already run.
    bool startNext();

    /// Marks an active download as done and forgets it.
    bool finish(int fileId);

    /// Takes an active download back and re-queues it at the front of the
    /// pending list with its original priority, so the retry is the next one
    /// startNext() picks. An id that is not active is refused.
    bool fail(int fileId);

    /// Empties both lists. A queue that is already empty is left alone.
    void clear();

signals:
    /// Emitted whenever the pending list, the active list or the concurrency cap
    /// really changed.
    void changed();

private:
    int m_max = 3;
    QVector<QPair<int, int>> m_pending; ///< (fileId, priority), in start order
    QVector<int> m_active;
    /// Priority of every active id, kept so fail() can re-queue it unchanged.
    QHash<int, int> m_activePriorities;
};

} // namespace beygram::telegram
