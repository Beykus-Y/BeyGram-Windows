#include "LinkPreviewCache.h"

namespace beygram::telegram {
namespace {

bool samePreview(const CachedLinkPreview& left, const CachedLinkPreview& right)
{
    return left.url == right.url
        && left.title == right.title
        && left.description == right.description
        && left.siteName == right.siteName
        && left.imageFileId == right.imageFileId
        && left.isVideo == right.isVideo
        && left.fetchedAt == right.fetchedAt;
}

} // namespace

LinkPreviewCache::LinkPreviewCache(QObject* parent)
    : QObject(parent)
{
}

void LinkPreviewCache::setCapacity(int capacity)
{
    const int clamped = qBound(kMinimumCapacity, capacity, kMaximumCapacity);
    if (clamped == m_capacity)
        return;

    m_capacity = clamped;
    evictOverflow();
    emit changed();
}

int LinkPreviewCache::capacity() const
{
    return m_capacity;
}

void LinkPreviewCache::put(const CachedLinkPreview& preview, qint64 nowUnix)
{
    // The URL is the identity of the row; a preview without one cannot be
    // looked up later and would only waste a slot.
    CachedLinkPreview stored = preview;
    stored.url = preview.url.trimmed();
    if (stored.url.isEmpty())
        return;

    // The cache owns recency: the caller's fetchedAt is only a placeholder, and
    // the moment of this put() is what eviction order is measured by.
    stored.fetchedAt = nowUnix;

    const int existing = indexOf(stored.url);
    if (existing >= 0) {
        if (samePreview(m_entries.at(existing), stored))
            return;
        m_entries[existing] = stored;
    } else {
        m_entries.append(stored);
    }

    const bool added = existing < 0;
    evictOverflow();

    // A newcomer older than everything already stored is itself the victim, so
    // the eviction removed exactly what this call added: nothing really changed.
    if (added && !contains(stored.url))
        return;

    emit changed();
}

CachedLinkPreview LinkPreviewCache::get(const QString& url) const
{
    const int index = indexOf(url);
    return index >= 0 ? m_entries.at(index) : CachedLinkPreview();
}

bool LinkPreviewCache::contains(const QString& url) const
{
    return indexOf(url) >= 0;
}

void LinkPreviewCache::remove(const QString& url)
{
    const int index = indexOf(url);
    if (index < 0)
        return;
    m_entries.removeAt(index);
    emit changed();
}

void LinkPreviewCache::clear()
{
    if (m_entries.isEmpty())
        return;
    m_entries.clear();
    emit changed();
}

int LinkPreviewCache::count() const
{
    return int(m_entries.size());
}

int LinkPreviewCache::indexOf(const QString& url) const
{
    const QString key = url.trimmed();
    if (key.isEmpty())
        return -1;

    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).url == key)
            return i;
    }
    return -1;
}

void LinkPreviewCache::evictOverflow()
{
    while (m_entries.size() > m_capacity) {
        int victim = 0;
        for (int i = 1; i < m_entries.size(); ++i) {
            const CachedLinkPreview& candidate = m_entries.at(i);
            const CachedLinkPreview& oldest = m_entries.at(victim);
            if (candidate.fetchedAt < oldest.fetchedAt
                || (candidate.fetchedAt == oldest.fetchedAt
                    && candidate.url < oldest.url)) {
                victim = i;
            }
        }
        m_entries.removeAt(victim);
    }
}

} // namespace beygram::telegram
