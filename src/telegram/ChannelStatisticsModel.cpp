#include "ChannelStatisticsModel.h"

#include <utility>

namespace beygram::telegram {
namespace {

bool isEmpty(const ChatStatisticsSnapshot& snapshot)
{
    return snapshot.periodStart == 0 && snapshot.periodEnd == 0
        && snapshot.graphs.isEmpty();
}

} // namespace

ChannelStatisticsModel::ChannelStatisticsModel(QObject* parent)
    : QObject(parent)
{
}

void ChannelStatisticsModel::setSnapshot(ChatStatisticsSnapshot snapshot)
{
    if (snapshot == m_snapshot)
        return;

    m_snapshot = std::move(snapshot);
    emit changed();
}

ChatStatisticsSnapshot ChannelStatisticsModel::snapshot() const
{
    return m_snapshot;
}

int ChannelStatisticsModel::graphCount() const
{
    return static_cast<int>(m_snapshot.graphs.size());
}

QStringList ChannelStatisticsModel::graphNames() const
{
    QStringList names;
    names.reserve(m_snapshot.graphs.size());
    for (const StatisticsGraph& graph : m_snapshot.graphs)
        names.append(graph.name);
    return names;
}

StatisticsGraph ChannelStatisticsModel::graph(const QString& name) const
{
    for (const StatisticsGraph& graph : m_snapshot.graphs) {
        if (graph.name == name)
            return graph;
    }
    return {};
}

double ChannelStatisticsModel::totalFor(const QString& name) const
{
    for (const StatisticsGraph& graph : m_snapshot.graphs) {
        if (graph.name != name)
            continue;

        double total = 0.0;
        for (const StatisticsGraphPoint& point : graph.points)
            total += point.value;
        return total;
    }
    return 0.0;
}

void ChannelStatisticsModel::clear()
{
    if (isEmpty(m_snapshot))
        return;

    m_snapshot = ChatStatisticsSnapshot{};
    emit changed();
}

} // namespace beygram::telegram
