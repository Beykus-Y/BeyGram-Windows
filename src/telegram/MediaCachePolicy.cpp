#include "MediaCachePolicy.h"

namespace beygram::telegram {
namespace {
// TDLib accepts priorities from 1 (lowest) to 32 (highest); the values below
// match the ones MediaFileStore already uses for avatars and thumbnails.
constexpr int MinPriority = 1;
constexpr int MaxPriority = 32;
constexpr int VisiblePriority = 32;
constexpr int PhotoPriority = 16;
constexpr int BackgroundPriority = 8;

int clampPriority(int priority)
{
    return qBound(MinPriority, priority, MaxPriority);
}
} // namespace

bool shouldAutoDownload(const MediaCacheRules& rules, MediaKind kind, qint64 size)
{
    bool enabled = false;
    qint64 limit = 0;
    switch (kind) {
    case MediaKind::Photo:
        enabled = rules.photos;
        limit = rules.maxPhotoBytes;
        break;
    case MediaKind::Video:
        enabled = rules.videos;
        limit = rules.maxVideoBytes;
        break;
    case MediaKind::Document:
    case MediaKind::Audio:
    case MediaKind::Other:
        enabled = rules.documents;
        limit = rules.maxDocumentBytes;
        break;
    }

    if (!enabled)
        return false;
    // A size of zero (or a negative value) means TDLib has not reported it yet.
    if (size <= 0)
        return true;
    return size <= limit;
}

int priorityFor(MediaKind kind, bool visible)
{
    if (visible)
        return clampPriority(VisiblePriority);
    return clampPriority(kind == MediaKind::Photo ? PhotoPriority : BackgroundPriority);
}

} // namespace beygram::telegram
