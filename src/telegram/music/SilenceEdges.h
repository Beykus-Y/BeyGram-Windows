#pragma once

#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Silence measured at the two ends of one decoded music track.
///
/// leadingMs and trailingMs are whole milliseconds of samples whose absolute
/// value stays at or below the analysis threshold, each capped at 5000 ms.
/// analyzed is false when the track could not be measured: an unsupported
/// sample rate, no samples at all, or a buffer without a single audible sample.
/// Both edges are then zero, and nothing is trimmed.
struct SilenceEdges {
    int leadingMs = 0;
    int trailingMs = 0;
    bool analyzed = false;
};

/// Measures the silence at both ends of \a samples.
///
/// \a samples is PCM16. Interleaved audio works as well as mono, because every
/// sample is checked on its own. \a sampleRate must be 8000..192000; any other
/// value returns an unanalyzed result. \a threshold is clamped to 0..32767 and
/// is the largest absolute sample value that still counts as silence, so a
/// sample at the threshold is silent and one step above it is audible.
/// \a minSilenceMs is clamped to 0..5000: a shorter silent run is reported as
/// zero, because skipping it is not worth it. Each reported edge is capped at
/// 5000 ms.
///
/// An empty buffer, or one whose every sample is silent, has no audible point
/// to measure against and returns an unanalyzed result, so a fully silent
/// track is never trimmed down to nothing.
[[nodiscard]] SilenceEdges analyzeSilence(const QVector<qint16>& samples,
                                          int sampleRate,
                                          int threshold = 500,
                                          int minSilenceMs = 250);

/// Parses "leading,trailing" as written by encodeSilenceEdges().
///
/// Spaces around the numbers are ignored. An empty string, a different field
/// count, a non-numeric field, or a value outside 0..5000 returns an
/// unanalyzed result instead of a partial one.
[[nodiscard]] SilenceEdges decodeSilenceEdges(const QString& encoded);

/// Formats \a edges as "leading,trailing", or as an empty string when they are
/// not analyzed. Values are clamped to 0..5000 first, so the text always
/// decodes back to a valid measurement.
[[nodiscard]] QString encodeSilenceEdges(const SilenceEdges& edges);

/// Milliseconds to skip at the start of the track: leadingMs limited by
/// \a capMs, which is clamped to 0..5000. Zero when the edges are unanalyzed.
[[nodiscard]] int trimStartMs(const SilenceEdges& edges, int capMs);

/// Milliseconds to skip at the end of the track: trailingMs limited by
/// \a capMs (clamped to 0..5000) and by \a durationMs, so the result never
/// exceeds the track. Zero when the edges are unanalyzed or the duration is
/// not positive.
[[nodiscard]] int trimEndMs(const SilenceEdges& edges, int durationMs, int capMs);

} // namespace beygram::telegram
