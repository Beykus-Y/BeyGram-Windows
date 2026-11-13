#pragma once

#include "MusicModels.h"

#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One music search request. Every set filter must match (filters are ANDed).
///
/// text is matched against title, artistId and albumId ignoring case; a text
/// that is blank after trimming matches every track. artistId and albumId are
/// trimmed and, when set, must equal the track's artistId/albumId exactly.
/// favoritesOnly keeps only the tracks whose id appears in the favorites handed
/// to the engine. limit is clamped to 1..200.
struct MusicSearchQuery {
    QString text;
    QString artistId;
    QString albumId;
    bool favoritesOnly = false;
    int limit = 50;
};

/// Filters a track list for the music screen.
///
/// The engine is pure: it owns no state, performs no I/O and leaves both input
/// lists untouched. Tracks are returned in their incoming order, at most
/// query.limit of them, and only when every filter of \a query matches.
/// Favorites are compared by exact MusicTrackModel::id; an empty favorites list
/// with favoritesOnly set therefore matches nothing.
class MusicSearchEngine final {
public:
    [[nodiscard]] static QVector<MusicTrackModel> search(
        const QVector<MusicTrackModel>& tracks,
        const QVector<QString>& favorites,
        const MusicSearchQuery& query);
};

} // namespace beygram::telegram
