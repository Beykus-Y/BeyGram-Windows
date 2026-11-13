#pragma once

#include "MusicModels.h"

#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One virtual collection built from the tracks of the library.
///
/// id is the stable collection id ("recent", "long", "short" or "no-album"),
/// title is its display title and trackIds lists the matching tracks in the
/// order they were given.
struct SmartCollection {
    QString id;
    QString title;
    QVector<QString> trackIds;
};

/// Builds the virtual collections that have at least one track.
///
/// The collections are returned in this order:
/// - "recent": the last 30 distinct track ids of the given order, so the
///   newest tracks when the list is ordered oldest first;
/// - "long": tracks of at least 600 seconds;
/// - "short": tracks of at most 120 seconds (a negative length counts as 0);
/// - "no-album": tracks whose albumId is empty after trimming.
///
/// Every collection keeps the given order, lists each trimmed non-empty track
/// id once and is omitted when it has no tracks. A track without an id cannot
/// be addressed and is left out of every collection.
/// \a nowUnix is reserved for time-based collections and currently does not
/// affect the result.
[[nodiscard]] QVector<SmartCollection> smartCollections(
    const QVector<MusicTrackModel>& tracks, qint64 nowUnix);

} // namespace beygram::telegram
