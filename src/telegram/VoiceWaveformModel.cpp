#include "VoiceWaveformModel.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace beygram::telegram {

VoiceWaveformModel::VoiceWaveformModel(QObject* parent) : QObject(parent)
{
}

void VoiceWaveformModel::setSamples(QVector<int> samples)
{
    for (int& sample : samples) {
        if (sample < 0)
            sample = 0;
    }

    if (samples == m_samples)
        return;

    m_samples = std::move(samples);
    emit changed();
}

QVector<int> VoiceWaveformModel::samples() const
{
    return m_samples;
}

int VoiceWaveformModel::count() const
{
    return static_cast<int>(m_samples.size());
}

QVector<double> VoiceWaveformModel::normalized() const
{
    if (m_samples.isEmpty())
        return {};

    const int peak = *std::max_element(m_samples.cbegin(), m_samples.cend());

    QVector<double> result;
    result.reserve(m_samples.size());

    if (peak <= 0) {
        result.fill(0.0, m_samples.size());
        return result;
    }

    const double scale = static_cast<double>(peak);
    for (const int sample : m_samples)
        result.append(static_cast<double>(sample) / scale);

    return result;
}

void VoiceWaveformModel::setProgress(double progress)
{
    if (!std::isfinite(progress))
        return;

    const double clamped = std::clamp(progress, 0.0, 1.0);
    if (clamped == m_progress)
        return;

    m_progress = clamped;
    emit changed();
}

double VoiceWaveformModel::progress() const
{
    return m_progress;
}

int VoiceWaveformModel::playedSampleCount() const
{
    const auto total = m_samples.size();
    if (total <= 0 || m_progress <= 0.0)
        return 0;

    const double scaled = m_progress * static_cast<double>(total);
    if (scaled >= static_cast<double>(total))
        return static_cast<int>(total);

    return static_cast<int>(std::floor(scaled));
}

void VoiceWaveformModel::clear()
{
    if (m_samples.isEmpty() && m_progress == 0.0)
        return;

    m_samples.clear();
    m_progress = 0.0;
    emit changed();
}

} // namespace beygram::telegram
