#pragma once

#include <QtGlobal>

namespace beygram::telegram {

/// Local cache rules for incoming media, mirroring the conservative Android
/// auto-download defaults (data/media/AutoDownloadPolicy.kt): photos are
/// fetched automatically, videos and files are not, and every kind carries its
/// own size ceiling. cacheLimitBytes is the budget for the whole on-disk cache,
/// so it never gates a single file on its own.
struct MediaCacheRules {
    bool photos = true;
    bool videos = false;
    bool documents = false;
    qint64 maxPhotoBytes = 10 * 1024 * 1024;
    qint64 maxVideoBytes = 50 * 1024 * 1024;
    qint64 maxDocumentBytes = 20 * 1024 * 1024;
    qint64 cacheLimitBytes = 1024LL * 1024 * 1024;
};

/// Incoming media kinds the cache policy distinguishes. Audio and Other share
/// the document rule: they are files rather than previews, and the rules carry
/// a single file budget for them.
enum class MediaKind { Photo, Video, Document, Audio, Other };

/// Decides whether a file may be downloaded without the user asking for it.
/// A disabled kind is always refused. A non-positive size means TDLib has not
/// reported the size yet, and such a file is accepted so the transfer can start
/// and be re-checked later. A known size is accepted only when it is within the
/// limit of its kind, so a non-positive limit refuses every known size. The
/// function is pure: it performs no I/O, no logging and has no side effects.
[[nodiscard]] bool shouldAutoDownload(const MediaCacheRules& rules, MediaKind kind, qint64 size);

/// TDLib download priority for a file: visible media is 32, an invisible photo
/// 16, and every other invisible kind 8. The result is clamped to the 1..32
/// range TDLib accepts.
[[nodiscard]] int priorityFor(MediaKind kind, bool visible);

} // namespace beygram::telegram
