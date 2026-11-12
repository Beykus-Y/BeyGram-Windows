#include "ProxyLatencyModel.h"

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int MaxSamples = 5;
constexpr int MaxLatencyMs = 60000;

int clampLatency(int latencyMs)
{
    return std::clamp(latencyMs, 0, MaxLatencyMs);
}

/// Median of a copy of `samples`, or -1 when there are none. An even count
/// averages the two middle values with integer division, matching the documented
/// truncation.
int medianOf(QVector<int> samples)
{
    if (samples.isEmpty())
        return -1;
    std::sort(samples.begin(), samples.end());
    const qsizetype count = samples.size();
    if (count % 2 != 0)
        return samples.at(count / 2);
    return (samples.at(count / 2 - 1) + samples.at(count / 2)) / 2;
}

} // namespace

ProxyLatencyModel::ProxyLatencyModel(QObject* parent)
    : QObject(parent)
{
}

void ProxyLatencyModel::addSample(const QString& proxyId, int latencyMs)
{
    if (proxyId.isEmpty())
        return;

    const int clamped = clampLatency(latencyMs);
    auto it = m_samples.find(proxyId);
    if (it == m_samples.end()) {
        m_samples.insert(proxyId, QVector<int>{clamped});
        // A new id is visible in proxyIds() even when its latency is 0.
        emit changed();
        return;
    }

    const int previousMedian = medianOf(it.value());
    QVector<int>& samples = it.value();
    samples.append(clamped);
    if (samples.size() > MaxSamples)
        samples.removeFirst();

    // A probe whose result leaves the median unchanged does not alter what a
    // view shows, so it is stored without a repaint. The window still moves,
    // which matters for later evictions.
    if (medianOf(samples) != previousMedian)
        emit changed();
}

int ProxyLatencyModel::latencyFor(const QString& proxyId) const
{
    const auto it = m_samples.constFind(proxyId);
    if (it == m_samples.cend())
        return -1;
    return medianOf(it.value());
}

QStringList ProxyLatencyModel::proxyIds() const
{
    QStringList ids = m_samples.keys();
    ids.sort();
    return ids;
}

void ProxyLatencyModel::remove(const QString& proxyId)
{
    if (m_samples.remove(proxyId))
        emit changed();
}

void ProxyLatencyModel::clear()
{
    if (m_samples.isEmpty())
        return;
    m_samples.clear();
    emit changed();
}

} // namespace beygram::telegram
