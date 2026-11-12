#include "StoryProgressModel.h"

#include <algorithm>

namespace beygram::telegram {

StoryProgressModel::StoryProgressModel(QObject* parent) : QObject(parent)
{
}

void StoryProgressModel::begin(qint64 storyId, int durationMs)
{
    if (storyId <= 0 || durationMs <= 0)
        return;

    const bool wasFinished = isFinished();

    m_storyId = storyId;
    m_duration = durationMs;
    m_elapsed = 0;
    m_paused = false;

    emit changed();

    // Restarting a finished story leaves the finished state behind.
    if (wasFinished)
        emit finishedChanged();
}

void StoryProgressModel::tick(int elapsedMs)
{
    if (!active() || m_paused)
        return;

    const int clamped = std::clamp(elapsedMs, 0, m_duration);
    if (clamped == m_elapsed)
        return;

    const bool wasFinished = isFinished();
    m_elapsed = clamped;

    emit changed();

    if (!wasFinished && isFinished())
        emit finishedChanged();
}

void StoryProgressModel::pause()
{
    if (!active() || m_paused)
        return;

    m_paused = true;
    emit changed();
}

void StoryProgressModel::resume()
{
    if (!active() || !m_paused)
        return;

    m_paused = false;
    emit changed();
}

bool StoryProgressModel::active() const
{
    return m_storyId > 0 && m_duration > 0;
}

qint64 StoryProgressModel::storyId() const
{
    return m_storyId;
}

int StoryProgressModel::durationMs() const
{
    return m_duration;
}

int StoryProgressModel::elapsedMs() const
{
    return m_elapsed;
}

double StoryProgressModel::progress() const
{
    if (m_duration <= 0)
        return 0.0;

    const double ratio = static_cast<double>(m_elapsed) / static_cast<double>(m_duration);
    return std::clamp(ratio, 0.0, 1.0);
}

bool StoryProgressModel::paused() const
{
    return m_paused;
}

bool StoryProgressModel::finished() const
{
    return isFinished();
}

void StoryProgressModel::reset()
{
    const bool wasDefault = m_storyId == 0 && m_duration == 0 && m_elapsed == 0 && !m_paused;
    if (wasDefault)
        return;

    const bool wasFinished = isFinished();

    m_storyId = 0;
    m_duration = 0;
    m_elapsed = 0;
    m_paused = false;

    emit changed();

    if (wasFinished)
        emit finishedChanged();
}

bool StoryProgressModel::isFinished() const
{
    return active() && m_elapsed >= m_duration;
}

} // namespace beygram::telegram
