#include "MusicSearch.h"

#include <QSet>

namespace beygram::telegram {
namespace {

constexpr int kMinLimit = 1;
constexpr int kMaxLimit = 200;

bool matchesText(const MusicTrackModel& track, const QString& text)
{
    return track.title.contains(text, Qt::CaseInsensitive)
        || track.artistId.contains(text, Qt::CaseInsensitive)
        || track.albumId.contains(text, Qt::CaseInsensitive);
}

} // namespace

QVector<MusicTrackModel> MusicSearchEngine::search(const QVector<MusicTrackModel>& tracks,
                                                   const QVector<QString>& favorites,
                                                   const MusicSearchQuery& query)
{
    const QString text = query.text.trimmed();
    const QString artistId = query.artistId.trimmed();
    const QString albumId = query.albumId.trimmed();
    const int limit = qBound(kMinLimit, query.limit, kMaxLimit);

    QSet<QString> favoriteIds;
    if (query.favoritesOnly) {
        for (const QString& id : favorites) {
            if (!id.isEmpty())
                favoriteIds.insert(id);
        }
        // No track can be a favorite when the list is empty, so skip the scan.
        if (favoriteIds.isEmpty())
            return {};
    }

    QVector<MusicTrackModel> matches;
    matches.reserve(qMin<qsizetype>(limit, tracks.size()));
    for (const MusicTrackModel& track : tracks) {
        if (!text.isEmpty() && !matchesText(track, text))
            continue;
        if (!artistId.isEmpty() && track.artistId != artistId)
            continue;
        if (!albumId.isEmpty() && track.albumId != albumId)
            continue;
        if (query.favoritesOnly && !favoriteIds.contains(track.id))
            continue;

        matches.append(track);
        if (matches.size() >= limit)
            break;
    }
    return matches;
}

} // namespace beygram::telegram
