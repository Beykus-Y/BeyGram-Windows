#pragma once

#include "MusicLibraryStore.h"

#include <QString>

namespace beygram::telegram {

/// Pure mutations of MusicLibraryData.
///
/// Every function returns true only when it changed the data; a refused or
/// already-applied edit leaves \a data exactly as it was and returns false.
/// Ids are trimmed before they are compared or stored, and a stored track or
/// playlist keeps its first entry, so repeated ids never appear.
///
/// The functions do not touch timestamps or the album/artist catalogs: the
/// caller decides what time an edit happened at and which metadata to attach.

/// Appends \a track when its trimmed id is non-empty and not already stored.
/// The added copy carries the trimmed id, artistId and albumId; every other
/// field is kept as given.
bool addTrack(MusicLibraryData& data, const MusicTrackModel& track);

/// Removes the track with \a trackId and every reference to it from the
/// playlists and favorites. Returns false when no such track is stored.
bool removeTrack(MusicLibraryData& data, const QString& trackId);

/// Adds the stored track with \a trackId to the end of the favorites, or
/// removes every favorites entry for it. Returns false for an unknown track.
bool toggleFavorite(MusicLibraryData& data, const QString& trackId);

/// Appends \a playlist when its trimmed id is non-empty and unused and its
/// trimmed title is 1..80 Unicode code points long. Unknown track ids and
/// repeated references are dropped; the remaining references keep their order.
bool createPlaylist(MusicLibraryData& data, const MusicPlaylist& playlist);

/// Appends \a trackId to the playlist \a playlistId. Both must exist and the
/// track must not be in the playlist yet.
bool addTrackToPlaylist(MusicLibraryData& data, const QString& playlistId,
                        const QString& trackId);

/// Removes \a trackId from the playlist \a playlistId. Returns false when the
/// playlist or the reference is missing.
bool removeTrackFromPlaylist(MusicLibraryData& data, const QString& playlistId,
                             const QString& trackId);

/// Replaces the title of \a playlistId with the trimmed \a title. The title
/// must be 1..80 Unicode code points long; an unchanged title is not a change.
bool renamePlaylist(MusicLibraryData& data, const QString& playlistId, const QString& title);

/// Removes the playlist \a playlistId. Tracks, favorites, other playlists and
/// the album/artist catalogs are left untouched.
bool deletePlaylist(MusicLibraryData& data, const QString& playlistId);

} // namespace beygram::telegram
