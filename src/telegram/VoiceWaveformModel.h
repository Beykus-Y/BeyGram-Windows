#pragma once

#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Holds the amplitude envelope of a voice message plus its playback progress.
///
/// TDLib delivers a voice waveform as a series of small amplitude values, one
/// per bar of the envelope. The model keeps those raw values untouched apart
/// from clamping negative amplitudes to zero, and exposes a normalized copy so
/// the UI can size bars without knowing the source range. Progress is stored as
/// a fraction of the clip so the played part of the waveform can be highlighted
/// without re-reading the samples.
class VoiceWaveformModel final : public QObject {
    Q_OBJECT

public:
    explicit VoiceWaveformModel(QObject* parent = nullptr);

    /// Replaces the envelope. Negative amplitudes become zero; every other
    /// value, including zeros and duplicates, is kept in order.
    void setSamples(QVector<int> samples);
    [[nodiscard]] QVector<int> samples() const;

    /// Number of amplitude bars in the envelope.
    [[nodiscard]] int count() const;

    /// Amplitudes scaled into 0.0..1.0 by the largest sample. Returns an empty
    /// vector for an empty envelope and all zeros when every sample is zero.
    [[nodiscard]] QVector<double> normalized() const;

    /// Sets how much of the clip has been played, clamped to 0.0..1.0.
    /// Non-finite values are ignored.
    void setProgress(double progress);
    [[nodiscard]] double progress() const;

    /// Number of leading bars covered by the current progress, i.e.
    /// floor(progress * count()).
    [[nodiscard]] int playedSampleCount() const;

    /// Drops the envelope and resets progress to zero.
    void clear();

signals:
    /// Emitted whenever the samples or the progress actually changed.
    void changed();

private:
    QVector<int> m_samples;
    double m_progress = 0.0;
};

} // namespace beygram::telegram
