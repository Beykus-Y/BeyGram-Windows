#pragma once

#include "ChannelStatistics.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::telegram {

/// Holds one parsed chat statistics snapshot for a view.
///
/// The model is a plain value holder: it never talks to TDLib, the network or
/// the UI. The caller parses an answer with chatStatisticsFromResponse() and
/// hands the result to setSnapshot(); the model only stores it and answers
/// lookups by graph name.
///
/// Invariants:
///
///  - The snapshot is stored exactly as given, including its graph order and
///    any empty-named graph. chatStatisticsFromResponse() already drops graphs
///    without points, so setSnapshot() does not filter again.
///  - graph() and totalFor() match names exactly and case-sensitively. When a
///    snapshot carries the same name twice, the first graph wins, mirroring
///    the order graphNames() reports.
///  - changed() is emitted only when the stored snapshot really differs.
///    Re-sending the same period and points, and clearing an already empty
///    model, are silent, so a refresh that brings the same data does not
///    repaint the screen.
class ChannelStatisticsModel final : public QObject {
    Q_OBJECT

public:
    explicit ChannelStatisticsModel(QObject* parent = nullptr);

    /// Replaces the stored snapshot. Emits changed() only when the period or
    /// any graph name, point date or point value differs from what was stored.
    void setSnapshot(ChatStatisticsSnapshot snapshot);

    /// Returns a copy of the stored snapshot.
    [[nodiscard]] ChatStatisticsSnapshot snapshot() const;

    /// Number of graphs in the stored snapshot.
    [[nodiscard]] int graphCount() const;

    /// Names of the stored graphs, in snapshot order.
    [[nodiscard]] QStringList graphNames() const;

    /// Returns the first graph with `name`, or an empty graph when none
    /// matches.
    [[nodiscard]] StatisticsGraph graph(const QString& name) const;

    /// Sum of every point value of the first graph named `name`, or 0 when no
    /// graph matches. Negative values are summed as stored.
    [[nodiscard]] double totalFor(const QString& name) const;

    /// Drops the stored snapshot, emitting changed() once when there was
    /// anything to drop. Clearing an already empty model is a no-op.
    void clear();

signals:
    /// The stored snapshot changed. Not emitted for a no-op call.
    void changed();

private:
    ChatStatisticsSnapshot m_snapshot;
};

} // namespace beygram::telegram
