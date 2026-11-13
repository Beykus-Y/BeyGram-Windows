#include "SilenceEdges.h"

#include <QStringList>

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int kMinSampleRate = 8000;
constexpr int kMaxSampleRate = 192000;
constexpr int kMaxThreshold = 32767;
constexpr int kMaxEdgeMs = 5000;

/// A cap outside the contract is brought back to a usable number of
/// milliseconds rather than rejecting the whole call.
int clampedCap(int capMs)
{
    return std::clamp(capMs, 0, kMaxEdgeMs);
}

/// Whole milliseconds covered by \a sampleCount samples, capped at 5000.
int edgeMs(qint64 sampleCount, int sampleRate)
{
    const qint64 ms = sampleCount * 1000 / sampleRate;
    return static_cast<int>(qMin<qint64>(ms, kMaxEdgeMs));
}

/// A silent run shorter than \a minSilenceMs is not worth skipping.
int reportedMs(qint64 silentSamples, int sampleRate, int minSilenceMs)
{
    const int ms = edgeMs(silentSamples, sampleRate);
    return ms >= minSilenceMs ? ms : 0;
}

bool isSilent(qint16 sample, int threshold)
{
    return qAbs(static_cast<int>(sample)) <= threshold;
}

} // namespace

SilenceEdges analyzeSilence(const QVector<qint16>& samples,
                            int sampleRate,
                            int threshold,
                            int minSilenceMs)
{
    SilenceEdges edges;
    if (sampleRate < kMinSampleRate || sampleRate > kMaxSampleRate)
        return edges;
    if (samples.isEmpty())
        return edges;

    threshold = std::clamp(threshold, 0, kMaxThreshold);
    minSilenceMs = std::clamp(minSilenceMs, 0, kMaxEdgeMs);

    qsizetype leading = 0;
    while (leading < samples.size() && isSilent(samples.at(leading), threshold))
        ++leading;
    // Without a single audible sample there is no edge to measure: the whole
    // track is silent, and trimming it would leave nothing to play.
    if (leading == samples.size())
        return edges;

    qsizetype trailing = 0;
    while (trailing < samples.size()
           && isSilent(samples.at(samples.size() - 1 - trailing), threshold)) {
        ++trailing;
    }

    edges.leadingMs = reportedMs(leading, sampleRate, minSilenceMs);
    edges.trailingMs = reportedMs(trailing, sampleRate, minSilenceMs);
    edges.analyzed = true;
    return edges;
}

SilenceEdges decodeSilenceEdges(const QString& encoded)
{
    const QStringList parts = encoded.split(QLatin1Char(','));
    if (parts.size() != 2)
        return {};

    int values[2] = {0, 0};
    for (int i = 0; i < 2; ++i) {
        bool ok = false;
        const int value = parts.at(i).trimmed().toInt(&ok);
        if (!ok || value < 0 || value > kMaxEdgeMs)
            return {};
        values[i] = value;
    }
    return SilenceEdges{values[0], values[1], true};
}

QString encodeSilenceEdges(const SilenceEdges& edges)
{
    if (!edges.analyzed)
        return {};
    return QStringLiteral("%1,%2")
        .arg(clampedCap(edges.leadingMs))
        .arg(clampedCap(edges.trailingMs));
}

int trimStartMs(const SilenceEdges& edges, int capMs)
{
    if (!edges.analyzed)
        return 0;
    return qBound(0, edges.leadingMs, clampedCap(capMs));
}

int trimEndMs(const SilenceEdges& edges, int durationMs, int capMs)
{
    if (!edges.analyzed || durationMs <= 0)
        return 0;
    return qMin(qBound(0, edges.trailingMs, clampedCap(capMs)), durationMs);
}

} // namespace beygram::telegram
