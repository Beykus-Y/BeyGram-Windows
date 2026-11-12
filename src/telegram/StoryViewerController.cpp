#include "StoryViewerController.h"

#include <utility>

namespace beygram::telegram {

namespace {

// Playback order is what TDLib reported, so normalisation only drops the ids
// that cannot name a story and keeps the first occurrence of everything else.
QVector<qint64> normalizedIds(const QVector<qint64>& ids)
{
    QVector<qint64> result;
    result.reserve(ids.size());
    for (const qint64 id : ids) {
        if (id <= 0 || result.contains(id))
            continue;
        result.append(id);
    }
    return result;
}

} // namespace

StoryViewerController::StoryViewerController(QObject* parent)
    : QObject(parent)
{
}

void StoryViewerController::open(qint64 chatId, QVector<qint64> storyIds)
{
    if (chatId <= 0)
        return;

    QVector<qint64> next = normalizedIds(storyIds);
    if (next.isEmpty())
        return;

    // Reopening the session that is already on screen must not rewind the
    // viewer or forget the stories the user has already watched.
    if (active() && m_chatId == chatId && m_ids == next)
        return;

    m_chatId = chatId;
    m_ids = std::move(next);
    m_index = 0;
    m_seen.clear();
    m_paused = false;
    emit changed();
}

void StoryViewerController::close()
{
    if (!active())
        return;

    m_chatId = 0;
    m_ids.clear();
    m_index = 0;
    m_seen.clear();
    m_paused = false;
    emit closed();
    emit changed();
}

bool StoryViewerController::active() const
{
    return m_chatId > 0 && !m_ids.isEmpty();
}

qint64 StoryViewerController::chatId() const
{
    return m_chatId;
}

QVector<qint64> StoryViewerController::storyIds() const
{
    return m_ids;
}

int StoryViewerController::currentIndex() const
{
    return m_index;
}

qint64 StoryViewerController::currentStoryId() const
{
    if (!active() || m_index < 0 || m_index >= m_ids.size())
        return 0;
    return m_ids.at(m_index);
}

void StoryViewerController::next()
{
    if (!active())
        return;

    if (m_index + 1 >= m_ids.size()) {
        close();
        return;
    }

    ++m_index;
    emit changed();
}

void StoryViewerController::previous()
{
    if (!active() || m_index <= 0)
        return;

    --m_index;
    emit changed();
}

void StoryViewerController::markSeen(qint64 storyId)
{
    if (storyId <= 0 || !m_ids.contains(storyId) || m_seen.contains(storyId))
        return;

    m_seen.append(storyId);
    emit changed();
}

QVector<qint64> StoryViewerController::unseen() const
{
    QVector<qint64> result;
    result.reserve(m_ids.size());
    for (const qint64 id : m_ids) {
        if (!m_seen.contains(id))
            result.append(id);
    }
    return result;
}

void StoryViewerController::setPaused(bool paused)
{
    if (!active())
        return;

    if (m_paused == paused)
        return;

    m_paused = paused;
    emit changed();
}

bool StoryViewerController::paused() const
{
    return m_paused;
}

} // namespace beygram::telegram
