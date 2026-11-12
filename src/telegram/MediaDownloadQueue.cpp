#include "MediaDownloadQueue.h"

#include <QtGlobal>

namespace beygram::telegram {

namespace {
constexpr int kMinConcurrent = 1;
constexpr int kMaxConcurrent = 8;
} // namespace

MediaDownloadQueue::MediaDownloadQueue(QObject* parent)
    : QObject(parent)
{
}

void MediaDownloadQueue::setMaxConcurrent(int max)
{
    const int clamped = qBound(kMinConcurrent, max, kMaxConcurrent);
    if (clamped == m_max)
        return;

    m_max = clamped;
    emit changed();
}

bool MediaDownloadQueue::enqueue(int fileId, int priority)
{
    if (fileId <= 0 || contains(fileId))
        return false;

    // Insert before the first lower priority, after every equal or higher one,
    // so a burst of equal priorities keeps the order it arrived in.
    const int count = static_cast<int>(m_pending.size());
    int index = count;
    for (int i = 0; i < count; ++i) {
        if (m_pending.at(i).second < priority) {
            index = i;
            break;
        }
    }

    m_pending.insert(index, qMakePair(fileId, priority));
    emit changed();
    return true;
}

bool MediaDownloadQueue::cancel(int fileId)
{
    if (fileId <= 0)
        return false;

    const int count = static_cast<int>(m_pending.size());
    for (int i = 0; i < count; ++i) {
        if (m_pending.at(i).first == fileId) {
            m_pending.removeAt(i);
            emit changed();
            return true;
        }
    }

    const int activeIndex = static_cast<int>(m_active.indexOf(fileId));
    if (activeIndex < 0)
        return false;

    m_active.removeAt(activeIndex);
    m_activePriorities.remove(fileId);
    emit changed();
    return true;
}

bool MediaDownloadQueue::contains(int fileId) const
{
    if (fileId <= 0)
        return false;

    const int count = static_cast<int>(m_pending.size());
    for (int i = 0; i < count; ++i) {
        if (m_pending.at(i).first == fileId)
            return true;
    }

    return m_active.contains(fileId);
}

QVector<int> MediaDownloadQueue::pending() const
{
    QVector<int> ids;
    ids.reserve(m_pending.size());
    for (const QPair<int, int>& entry : m_pending)
        ids.append(entry.first);
    return ids;
}

QVector<int> MediaDownloadQueue::active() const
{
    return m_active;
}

int MediaDownloadQueue::pendingCount() const
{
    return static_cast<int>(m_pending.size());
}

int MediaDownloadQueue::activeCount() const
{
    return static_cast<int>(m_active.size());
}

bool MediaDownloadQueue::startNext()
{
    if (m_active.size() >= m_max || m_pending.isEmpty())
        return false;

    const QPair<int, int> next = m_pending.takeFirst();
    m_active.append(next.first);
    m_activePriorities.insert(next.first, next.second);
    emit changed();
    return true;
}

bool MediaDownloadQueue::finish(int fileId)
{
    const int index = static_cast<int>(m_active.indexOf(fileId));
    if (index < 0)
        return false;

    m_active.removeAt(index);
    m_activePriorities.remove(fileId);
    emit changed();
    return true;
}

bool MediaDownloadQueue::fail(int fileId)
{
    const int index = static_cast<int>(m_active.indexOf(fileId));
    if (index < 0)
        return false;

    m_active.removeAt(index);
    const int priority = m_activePriorities.take(fileId);

    // The retry goes in front so it runs before everything queued behind it,
    // but keeps its priority for any later enqueue that has to be ordered.
    m_pending.prepend(qMakePair(fileId, priority));
    emit changed();
    return true;
}

void MediaDownloadQueue::clear()
{
    if (m_pending.isEmpty() && m_active.isEmpty())
        return;

    m_pending.clear();
    m_active.clear();
    m_activePriorities.clear();
    emit changed();
}

} // namespace beygram::telegram
