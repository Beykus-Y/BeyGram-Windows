#include "VideoPlayerState.h"

#include <algorithm>
#include <cmath>

namespace beygram::telegram {

namespace {

// Volume is a percentage, so it cannot leave 0..100.
constexpr int kMinVolume = 0;
constexpr int kMaxVolume = 100;

// Anything outside this range would either stall or distort playback, so the
// rate is clamped instead of being rejected.
constexpr double kMinRate = 0.25;
constexpr double kMaxRate = 3.0;

} // namespace

VideoPlayerState::VideoPlayerState(QObject* parent) : QObject(parent)
{
}

void VideoPlayerState::open(qint64 messageId, int durationMs)
{
    if (messageId <= 0 || durationMs <= 0)
        return;

    // Reopening the same video at the start changes nothing observable.
    if (m_messageId == messageId && m_duration == durationMs && m_position == 0 && !m_playing)
        return;

    m_messageId = messageId;
    m_duration = durationMs;
    m_position = 0;
    m_playing = false;
    emit changed();
}

void VideoPlayerState::close()
{
    if (!active())
        return;

    m_messageId = 0;
    m_duration = 0;
    m_position = 0;
    m_playing = false;
    emit changed();
}

bool VideoPlayerState::active() const
{
    return m_messageId > 0 && m_duration > 0;
}

qint64 VideoPlayerState::messageId() const
{
    return m_messageId;
}

int VideoPlayerState::durationMs() const
{
    return m_duration;
}

void VideoPlayerState::setPosition(int ms)
{
    if (!active())
        return;

    const int clamped = std::clamp(ms, 0, m_duration);
    if (clamped == m_position)
        return;

    m_position = clamped;
    emit changed();
}

int VideoPlayerState::positionMs() const
{
    return m_position;
}

void VideoPlayerState::setPlaying(bool playing)
{
    if (!active() || playing == m_playing)
        return;

    m_playing = playing;
    emit changed();
}

bool VideoPlayerState::playing() const
{
    return m_playing;
}

void VideoPlayerState::setVolume(int percent)
{
    if (!active())
        return;

    const int clamped = std::clamp(percent, kMinVolume, kMaxVolume);
    if (clamped == m_volume)
        return;

    m_volume = clamped;
    emit changed();
}

int VideoPlayerState::volume() const
{
    return m_volume;
}

void VideoPlayerState::setMuted(bool muted)
{
    if (!active() || muted == m_muted)
        return;

    m_muted = muted;
    emit changed();
}

bool VideoPlayerState::muted() const
{
    return m_muted;
}

void VideoPlayerState::setRate(double rate)
{
    // A NaN rate has no meaningful clamp, so it is dropped rather than stored.
    if (!active() || std::isnan(rate))
        return;

    const double clamped = std::clamp(rate, kMinRate, kMaxRate);
    if (clamped == m_rate)
        return;

    m_rate = clamped;
    emit changed();
}

double VideoPlayerState::rate() const
{
    return m_rate;
}

double VideoPlayerState::progress() const
{
    if (m_duration <= 0)
        return 0.0;

    const double fraction =
        static_cast<double>(m_position) / static_cast<double>(m_duration);
    return std::clamp(fraction, 0.0, 1.0);
}

void VideoPlayerState::reset()
{
    const bool alreadyDefault = m_messageId == 0 && m_duration == 0 && m_position == 0
        && m_volume == kMaxVolume && !m_playing && !m_muted && m_rate == 1.0;
    if (alreadyDefault)
        return;

    m_messageId = 0;
    m_duration = 0;
    m_position = 0;
    m_volume = kMaxVolume;
    m_playing = false;
    m_muted = false;
    m_rate = 1.0;
    emit changed();
}

} // namespace beygram::telegram
