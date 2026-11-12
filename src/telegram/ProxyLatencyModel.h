#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace beygram::telegram {

/// Keeps the recent connection latency of every proxy the app has probed.
///
/// This is presentation state only: it does not measure anything, does not
/// talk to TDLib and performs no I/O. A caller that measured a proxy reports
/// the result with addSample(); a view asks latencyFor() for the value to show
/// and repaints on changed().
///
/// Invariants:
///
///  - Only the last five samples of a proxy matter. Older samples are dropped
///    as new ones arrive, so a single slow probe ages out instead of skewing
///    the value forever.
///  - A reported latency is clamped into [0, 60000] ms. Negative values are
///    measurement noise and become 0; anything above a minute is no more
///    useful than a minute, so it is capped. 0 is a real latency and is
///    different from "unknown".
///  - latencyFor() returns the median of the stored samples. For an even
///    number of samples the two middle values are averaged and the result is
///    truncated; the odd windows of up to five samples that normally occur
///    have an exact middle value.
///  - An unknown proxy, including the empty id, has latency -1. A proxy id is
///    opaque and is used exactly as passed, so the caller should keep it
///    stable between probes.
///  - proxyIds() is sorted, so iterating it is deterministic even though the
///    samples live in a hash.
///  - changed() reports changes a view can observe: a new proxy id appears,
///    the median of an existing proxy moves, a proxy is removed, or clear()
///    actually discards samples. Adding a sample that leaves the median where
///    it was is stored but silent, and removing an unknown id or clearing an
///    already empty model is silent too.
class ProxyLatencyModel final : public QObject {
    Q_OBJECT

public:
    explicit ProxyLatencyModel(QObject* parent = nullptr);

    /// Records one measurement for `proxyId`, clamped as described above and
    /// kept as the newest of at most five samples. An empty id is ignored.
    /// Emits changed() only when the visible latency of that proxy moves.
    void addSample(const QString& proxyId, int latencyMs);

    /// The median of the stored samples in milliseconds, or -1 when the proxy
    /// has never been measured.
    [[nodiscard]] int latencyFor(const QString& proxyId) const;

    /// Every measured proxy id, sorted ascending.
    [[nodiscard]] QStringList proxyIds() const;

    /// Forgets all samples of `proxyId`. Emits changed() only when it existed.
    void remove(const QString& proxyId);

    /// Forgets every proxy. Emits changed() only when there was something to
    /// forget.
    void clear();

signals:
    /// Emitted once per observable change, as described in the class comment.
    void changed();

private:
    QHash<QString, QVector<int>> m_samples;
};

} // namespace beygram::telegram
