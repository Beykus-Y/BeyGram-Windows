#include "MusicLibraryEdits.h"

#include <QSet>

#include <utility>

namespace beygram::telegram {
namespace {

constexpr int kMaxPlaylistTitleLength = 80;

/// Number of Unicode code points, so an emoji counts as one character and a
/// title is never cut through a surrogate pair.
int codePointCount(const QString& value)
{
    int count = 0;
    for (int i = 0; i < value.size(); ++i) {
        if (value.at(i).isHighSurrogate() && i + 1 < value.size()
            && value.at(i + 1).isLowSurrogate()) {
            ++i;
        }
        ++count;
    }
    return count;
}

/// The trimmed title when it is 1..80 code points long, otherwise an empty
/// string. An empty result is always an invalid title, never a valid one.
QString normalizedTitle(const QString& raw)
{
    const QString title = raw.trimmed();
    if (title.isEmpty() || codePointCount(title) > kMaxPlaylistTitleLength)
        return {};
    return title;
}

/// Index of the stored track whose trimmed id equals \a id, or -1.
int indexOfTrack(const MusicLibraryData& data, const QString& id)
{
    for (int i = 0; i < data.tracks.size(); ++i) {
        if (data.tracks.at(i).id.trimmed() == id)
            return i;
    }
    return -1;
}

/// Index of the stored playlist whose trimmed id equals \a id, or -1.
int indexOfPlaylist(const MusicLibraryData& data, const QString& id)
{
    for (int i = 0; i < data.playlists.size(); ++i) {
        if (data.playlists.at(i).id.trimmed() == id)
            return i;
    }
    return -1;
}

/// The references that name a stored track, trimmed, without repeats, in the
/// order they first appear.
QVector<QString> knownTrackIds(const MusicLibraryData& data, const QVector<QString>& raw)
{
    QSet<QString> known;
    known.reserve(data.tracks.size());
    for (const MusicTrackModel& track : data.tracks) {
        const QString id = track.id.trimmed();
        if (!id.isEmpty())
            known.insert(id);
    }

    QVector<QString> result;
    QSet<QString> seen;
    result.reserve(raw.size());
    for (const QString& rawId : raw) {
        const QString id = rawId.trimmed();
        if (id.isEmpty() || !known.contains(id) || seen.contains(id))
            continue;
        seen.insert(id);
        result.append(id);
    }
    return result;
}

/// Whether \a reference names \a id once surrounding whitespace is ignored.
bool references(const QString& reference, const QString& id)
{
    return reference.trimmed() == id;
}

} // namespace

bool addTrack(MusicLibraryData& data, const MusicTrackModel& track)
{
    MusicTrackModel clean = track;
    clean.id = track.id.trimmed();
    clean.artistId = track.artistId.trimmed();
    clean.albumId = track.albumId.trimmed();
    if (clean.id.isEmpty() || indexOfTrack(data, clean.id) >= 0)
        return false;

    data.tracks.append(std::move(clean));
    return true;
}

bool removeTrack(MusicLibraryData& data, const QString& trackId)
{
    const QString id = trackId.trimmed();
    if (id.isEmpty())
        return false;

    const int index = indexOfTrack(data, id);
    if (index < 0)
        return false;

    data.tracks.removeAt(index);

    for (MusicPlaylist& playlist : data.playlists) {
        QVector<QString> remaining;
        remaining.reserve(playlist.trackIds.size());
        for (const QString& reference : playlist.trackIds) {
            if (!references(reference, id))
                remaining.append(reference);
        }
        playlist.trackIds = std::move(remaining);
    }

    QVector<QString> favorites;
    favorites.reserve(data.favorites.size());
    for (const QString& favorite : data.favorites) {
        if (!references(favorite, id))
            favorites.append(favorite);
    }
    data.favorites = std::move(favorites);
    return true;
}

bool toggleFavorite(MusicLibraryData& data, const QString& trackId)
{
    const QString id = trackId.trimmed();
    if (id.isEmpty() || indexOfTrack(data, id) < 0)
        return false;

    int existing = -1;
    for (int i = 0; i < data.favorites.size(); ++i) {
        if (references(data.favorites.at(i), id)) {
            existing = i;
            break;
        }
    }

    if (existing >= 0) {
        // Remove every copy, so a damaged list becomes unique again.
        QVector<QString> remaining;
        remaining.reserve(data.favorites.size());
        for (const QString& favorite : data.favorites) {
            if (!references(favorite, id))
                remaining.append(favorite);
        }
        data.favorites = std::move(remaining);
        return true;
    }

    data.favorites.append(id);
    return true;
}

bool createPlaylist(MusicLibraryData& data, const MusicPlaylist& playlist)
{
    MusicPlaylist clean = playlist;
    clean.id = playlist.id.trimmed();
    clean.title = normalizedTitle(playlist.title);
    if (clean.id.isEmpty() || clean.title.isEmpty())
        return false;
    if (indexOfPlaylist(data, clean.id) >= 0)
        return false;

    clean.trackIds = knownTrackIds(data, playlist.trackIds);
    data.playlists.append(std::move(clean));
    return true;
}

bool addTrackToPlaylist(MusicLibraryData& data, const QString& playlistId,
                        const QString& trackId)
{
    const QString cleanPlaylistId = playlistId.trimmed();
    const QString cleanTrackId = trackId.trimmed();
    if (cleanPlaylistId.isEmpty() || cleanTrackId.isEmpty())
        return false;

    const int playlistIndex = indexOfPlaylist(data, cleanPlaylistId);
    if (playlistIndex < 0 || indexOfTrack(data, cleanTrackId) < 0)
        return false;

    MusicPlaylist& playlist = data.playlists[playlistIndex];
    for (const QString& reference : playlist.trackIds) {
        if (references(reference, cleanTrackId))
            return false;
    }

    playlist.trackIds.append(cleanTrackId);
    return true;
}

bool removeTrackFromPlaylist(MusicLibraryData& data, const QString& playlistId,
                             const QString& trackId)
{
    const QString cleanPlaylistId = playlistId.trimmed();
    const QString cleanTrackId = trackId.trimmed();
    if (cleanPlaylistId.isEmpty() || cleanTrackId.isEmpty())
        return false;

    const int playlistIndex = indexOfPlaylist(data, cleanPlaylistId);
    if (playlistIndex < 0)
        return false;

    MusicPlaylist& playlist = data.playlists[playlistIndex];
    QVector<QString> remaining;
    remaining.reserve(playlist.trackIds.size());
    for (const QString& reference : playlist.trackIds) {
        if (!references(reference, cleanTrackId))
            remaining.append(reference);
    }

    if (remaining.size() == playlist.trackIds.size())
        return false;

    playlist.trackIds = std::move(remaining);
    return true;
}

bool renamePlaylist(MusicLibraryData& data, const QString& playlistId, const QString& title)
{
    const QString id = playlistId.trimmed();
    const QString cleanTitle = normalizedTitle(title);
    if (id.isEmpty() || cleanTitle.isEmpty())
        return false;

    const int index = indexOfPlaylist(data, id);
    if (index < 0)
        return false;

    MusicPlaylist& playlist = data.playlists[index];
    if (playlist.title.trimmed() == cleanTitle)
        return false;

    playlist.title = cleanTitle;
    return true;
}

bool deletePlaylist(MusicLibraryData& data, const QString& playlistId)
{
    const QString id = playlistId.trimmed();
    if (id.isEmpty())
        return false;

    const int index = indexOfPlaylist(data, id);
    if (index < 0)
        return false;

    data.playlists.removeAt(index);
    return true;
}

} // namespace beygram::telegram
