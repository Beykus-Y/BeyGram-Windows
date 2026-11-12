#pragma once

#include "AutoDownloadPolicy.h"

#include <QObject>
#include <QPair>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Per-network size ceiling enforced by MediaDownloadService before a transfer
/// is queued.
///
/// The service knows the network but not the media kind, so it uses one ceiling
/// per network. MediaCacheRules in MediaCachePolicy.h is the per-kind rule set
/// used by the auto-download decision; this type is the service's coarse gate.
///
/// A ceiling of zero (or less) means "never" for that network, matching the
/// zero-means-off convention of DownloadLimits. setPolicy() stores a negative
/// ceiling as zero. Sizes are bytes.
struct MediaCachePolicy {
    qint64 wifiBytes = 50LL * 1024 * 1024;
    qint64 mobileBytes = 5LL * 1024 * 1024;
    qint64 roamingBytes = 0;

    /// The ceiling for `network`, never negative.
    [[nodiscard]] qint64 limitBytes(NetworkKind network) const;

    bool operator==(const MediaCachePolicy& other) const = default;
};

/// Coordinates automatic media downloads: gates requests against the
/// per-network policy, keeps the pending and active lists in start order, and
/// counts finished transfers.
///
/// request() refuses a non-positive file id, an id that is already pending or
/// active, a network whose ceiling is not positive, and a known expectedSize
/// above that ceiling. A non-positive expectedSize means TDLib has not reported
/// the size yet and is accepted while the network is enabled, so the transfer
/// can start and be checked again later. Priorities follow MediaDownloadQueue:
/// a larger value is more urgent, equal values keep FIFO order, and the value is
/// stored verbatim. A queued request emits downloadRequested() with that
/// priority.
///
/// The owner calls onStarted() when a transfer really starts and onFinished()
/// when it ends. onStarted() moves that id from pending to active and is
/// idempotent for an id that is already active; an unknown id is ignored.
/// onFinished() only applies to an active id: success increments the completed
/// counter, failure increments the failed counter, and both remove the id. A
/// failure is not re-queued; the owner decides whether to request it again.
///
/// cancel() removes an id from either list and emits cancelled() once. An
/// unknown id is a silent no-op. clear() drops both lists and resets the
/// counters; it emits changed() once and no per-id cancelled() signals.
///
/// changed() is emitted only when the pending list, the active list, the policy
/// or a counter really changes, so rejected calls and no-op setters stay silent.
/// The class performs no I/O and no logging.
class MediaDownloadService final : public QObject {
    Q_OBJECT

public:
    explicit MediaDownloadService(QObject* parent = nullptr);

    /// Installs the per-network ceilings. Negative ceilings are stored as zero.
    /// Queued requests are not re-checked. changed() is emitted only when the
    /// normalised policy differs from the stored one.
    void setPolicy(const MediaCachePolicy& policy);

    /// Queues a download. See the class comment for the exact refusals.
    bool request(int fileId, int priority, qint64 expectedSize, NetworkKind network);

    /// Drops a pending or active download and emits cancelled() for it.
    void cancel(int fileId);

    /// Moves a pending download to the active list. Idempotent while active.
    void onStarted(int fileId);

    /// Ends an active download. `ok` selects the counter to increment.
    void onFinished(int fileId, bool ok);

    /// Pending ids in the order they would start: priority first, then FIFO.
    [[nodiscard]] QVector<int> pending() const;

    /// Active ids in the order they were started.
    [[nodiscard]] QVector<int> active() const;

    [[nodiscard]] int completedCount() const;
    [[nodiscard]] int failedCount() const;

    /// Drops both lists and resets both counters.
    void clear();

signals:
    /// A list, the policy or a counter really changed.
    void changed();

    /// A request was queued with this priority.
    void downloadRequested(int fileId, int priority);

    /// A known pending or active download was cancelled.
    void cancelled(int fileId);

private:
    [[nodiscard]] int indexOfPending(int fileId) const;
    [[nodiscard]] bool contains(int fileId) const;

    MediaCachePolicy m_policy;
    QVector<QPair<int, int>> m_pending; ///< (fileId, priority), in start order
    QVector<int> m_active;
    int m_completed = 0;
    int m_failed = 0;
};

} // namespace beygram::telegram
