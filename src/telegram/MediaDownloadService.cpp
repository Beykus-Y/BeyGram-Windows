#include "MediaDownloadService.h"

namespace beygram::telegram {

qint64 MediaCachePolicy::limitBytes(NetworkKind network) const
{
    switch (network) {
    case NetworkKind::WiFi:
        return qMax<qint64>(0, wifiBytes);
    case NetworkKind::Mobile:
        return qMax<qint64>(0, mobileBytes);
    case NetworkKind::Roaming:
        return qMax<qint64>(0, roamingBytes);
    }
    return 0;
}

MediaDownloadService::MediaDownloadService(QObject* parent) : QObject(parent)
{
}

void MediaDownloadService::setPolicy(const MediaCachePolicy& policy)
{
    MediaCachePolicy next = policy;
    next.wifiBytes = qMax<qint64>(0, next.wifiBytes);
    next.mobileBytes = qMax<qint64>(0, next.mobileBytes);
    next.roamingBytes = qMax<qint64>(0, next.roamingBytes);

    if (next == m_policy)
        return;

    m_policy = next;
    emit changed();
}

bool MediaDownloadService::request(int fileId, int priority, qint64 expectedSize,
                                   NetworkKind network)
{
    if (fileId <= 0 || contains(fileId))
        return false;

    const qint64 limit = m_policy.limitBytes(network);
    // A non-positive ceiling disables the network; an unknown size is only
    // accepted while the network is enabled.
    if (limit <= 0)
        return false;
    if (expectedSize > limit)
        return false;

    // Insert before the first lower priority, after every equal or higher one,
    // so equal priorities keep their arrival order.
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
    emit downloadRequested(fileId, priority);
    return true;
}

void MediaDownloadService::cancel(int fileId)
{
    if (fileId <= 0)
        return;

    int index = indexOfPending(fileId);
    if (index >= 0) {
        m_pending.removeAt(index);
    } else {
        index = m_active.indexOf(fileId);
        if (index < 0)
            return;
        m_active.removeAt(index);
    }

    emit changed();
    emit cancelled(fileId);
}

void MediaDownloadService::onStarted(int fileId)
{
    if (fileId <= 0 || m_active.contains(fileId))
        return;

    const int index = indexOfPending(fileId);
    if (index < 0)
        return;

    m_pending.removeAt(index);
    m_active.append(fileId);
    emit changed();
}

void MediaDownloadService::onFinished(int fileId, bool ok)
{
    const int index = m_active.indexOf(fileId);
    if (index < 0)
        return;

    m_active.removeAt(index);
    if (ok)
        ++m_completed;
    else
        ++m_failed;

    emit changed();
}

QVector<int> MediaDownloadService::pending() const
{
    QVector<int> ids;
    ids.reserve(m_pending.size());
    for (const QPair<int, int>& entry : m_pending)
        ids.append(entry.first);
    return ids;
}

QVector<int> MediaDownloadService::active() const
{
    return m_active;
}

int MediaDownloadService::completedCount() const
{
    return m_completed;
}

int MediaDownloadService::failedCount() const
{
    return m_failed;
}

void MediaDownloadService::clear()
{
    if (m_pending.isEmpty() && m_active.isEmpty() && m_completed == 0 && m_failed == 0)
        return;

    m_pending.clear();
    m_active.clear();
    m_completed = 0;
    m_failed = 0;
    emit changed();
}

int MediaDownloadService::indexOfPending(int fileId) const
{
    const int count = static_cast<int>(m_pending.size());
    for (int i = 0; i < count; ++i) {
        if (m_pending.at(i).first == fileId)
            return i;
    }
    return -1;
}

bool MediaDownloadService::contains(int fileId) const
{
    return indexOfPending(fileId) >= 0 || m_active.contains(fileId);
}

} // namespace beygram::telegram
