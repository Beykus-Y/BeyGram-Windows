#include "PlaybackSettings.h"

#include <algorithm>
#include <cmath>

namespace beygram::telegram {

namespace {

constexpr double kMinSpeed = 0.5;
constexpr double kMaxSpeed = 2.0;

// Speed is stored with two decimals; the UI's fine control moves in 0.05
// steps, which that resolution already represents exactly.
constexpr double kSpeedScale = 100.0;
constexpr double kSpeedEpsilon = 1e-9;

constexpr int kMinCrossfadeMs = 0;
constexpr int kMaxCrossfadeMs = 12000;

constexpr double kDefaultSpeed = 1.0;
constexpr int kDefaultCrossfadeMs = 4000;

double roundedSpeed(double speed)
{
    return std::round(speed * kSpeedScale) / kSpeedScale;
}

} // namespace

PlaybackSettings::PlaybackSettings(QObject* parent) : QObject(parent)
{
}

void PlaybackSettings::setSpeed(double speed)
{
    // A NaN has no meaningful clamp and would poison every later comparison.
    if (std::isnan(speed))
        return;

    const double clamped = std::clamp(speed, kMinSpeed, kMaxSpeed);
    const double rounded = roundedSpeed(clamped);
    if (std::abs(rounded - m_speed) < kSpeedEpsilon)
        return;

    m_speed = rounded;
    emit changed();
}

double PlaybackSettings::speed() const
{
    return m_speed;
}

QVector<double> PlaybackSettings::speedOptions() const
{
    return {0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0};
}

void PlaybackSettings::setCrossfadeMs(int ms)
{
    const int clamped = std::clamp(ms, kMinCrossfadeMs, kMaxCrossfadeMs);
    if (clamped == m_crossfadeMs)
        return;

    m_crossfadeMs = clamped;
    emit changed();
}

int PlaybackSettings::crossfadeMs() const
{
    return m_crossfadeMs;
}

void PlaybackSettings::setGapless(bool on)
{
    if (on == m_gapless)
        return;

    m_gapless = on;
    emit changed();
}

bool PlaybackSettings::gapless() const
{
    return m_gapless;
}

void PlaybackSettings::setNormalizeVolume(bool on)
{
    if (on == m_normalizeVolume)
        return;

    m_normalizeVolume = on;
    emit changed();
}

bool PlaybackSettings::normalizeVolume() const
{
    return m_normalizeVolume;
}

void PlaybackSettings::setSkipSilence(bool on)
{
    if (on == m_skipSilence)
        return;

    m_skipSilence = on;
    emit changed();
}

bool PlaybackSettings::skipSilence() const
{
    return m_skipSilence;
}

void PlaybackSettings::reset()
{
    if (m_speed == kDefaultSpeed && m_crossfadeMs == kDefaultCrossfadeMs && !m_gapless
        && !m_normalizeVolume && !m_skipSilence) {
        return;
    }

    m_speed = kDefaultSpeed;
    m_crossfadeMs = kDefaultCrossfadeMs;
    m_gapless = false;
    m_normalizeVolume = false;
    m_skipSilence = false;
    emit changed();
}

} // namespace beygram::telegram
