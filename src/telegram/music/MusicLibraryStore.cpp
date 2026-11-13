#include "MusicLibraryStore.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSaveFile>
#include <QSet>

#include <utility>

namespace beygram::telegram {
namespace {

QString stringField(const QJsonObject& object, const QString& key)
{
    const QJsonValue value = object.value(key);
    return value.isString() ? value.toString() : QString();
}

int intField(const QJsonObject& object, const QString& key)
{
    const QJsonValue value = object.value(key);
    return value.isDouble() ? value.toInt() : 0;
}

/// The schema version of a document. `version` is what this store writes;
/// `schemaVersion` is accepted as the name used by the Android library file.
int documentVersion(const QJsonObject& root)
{
    const QJsonValue version = root.value(QStringLiteral("version"));
    if (version.isDouble())
        return version.toInt();
    return root.value(QStringLiteral("schemaVersion")).toInt();
}

qint64 integerField(const QJsonObject& object, const QString& key)
{
    const QJsonValue value = object.value(key);
    return value.isDouble() ? static_cast<qint64>(value.toDouble()) : 0;
}

QVector<QString> stringArray(const QJsonValue& value)
{
    QVector<QString> result;
    if (!value.isArray())
        return result;

    const QJsonArray array = value.toArray();
    result.reserve(array.size());
    for (const QJsonValue& entry : array) {
        if (entry.isString())
            result.append(entry.toString());
    }
    return result;
}

QVector<MusicTrackModel> tracksFromJson(const QJsonValue& value)
{
    QVector<MusicTrackModel> tracks;
    if (!value.isArray())
        return tracks;

    const QJsonArray array = value.toArray();
    tracks.reserve(array.size());
    for (const QJsonValue& entry : array) {
        if (!entry.isObject())
            continue;

        const QJsonObject object = entry.toObject();
        MusicTrackModel track;
        track.id = stringField(object, QStringLiteral("id"));
        track.title = stringField(object, QStringLiteral("title"));
        track.artistId = stringField(object, QStringLiteral("artistId"));
        track.albumId = stringField(object, QStringLiteral("albumId"));
        track.durationSeconds = intField(object, QStringLiteral("durationSeconds"));
        track.fileId = intField(object, QStringLiteral("fileId"));
        track.localPath = stringField(object, QStringLiteral("localPath"));
        track.trackNumber = intField(object, QStringLiteral("trackNumber"));
        tracks.append(std::move(track));
    }
    return tracks;
}

QVector<MusicAlbum> albumsFromJson(const QJsonValue& value)
{
    QVector<MusicAlbum> albums;
    if (!value.isArray())
        return albums;

    const QJsonArray array = value.toArray();
    albums.reserve(array.size());
    for (const QJsonValue& entry : array) {
        if (!entry.isObject())
            continue;

        const QJsonObject object = entry.toObject();
        MusicAlbum album;
        album.id = stringField(object, QStringLiteral("id"));
        album.title = stringField(object, QStringLiteral("title"));
        album.artistId = stringField(object, QStringLiteral("artistId"));
        album.year = intField(object, QStringLiteral("year"));
        album.coverFileId = intField(object, QStringLiteral("coverFileId"));
        albums.append(std::move(album));
    }
    return albums;
}

QVector<MusicArtist> artistsFromJson(const QJsonValue& value)
{
    QVector<MusicArtist> artists;
    if (!value.isArray())
        return artists;

    const QJsonArray array = value.toArray();
    artists.reserve(array.size());
    for (const QJsonValue& entry : array) {
        if (!entry.isObject())
            continue;

        const QJsonObject object = entry.toObject();
        MusicArtist artist;
        artist.id = stringField(object, QStringLiteral("id"));
        artist.name = stringField(object, QStringLiteral("name"));
        artists.append(std::move(artist));
    }
    return artists;
}

QVector<MusicPlaylist> playlistsFromJson(const QJsonValue& value)
{
    QVector<MusicPlaylist> playlists;
    if (!value.isArray())
        return playlists;

    const QJsonArray array = value.toArray();
    playlists.reserve(array.size());
    for (const QJsonValue& entry : array) {
        if (!entry.isObject())
            continue;

        const QJsonObject object = entry.toObject();
        MusicPlaylist playlist;
        playlist.id = stringField(object, QStringLiteral("id"));
        playlist.title = stringField(object, QStringLiteral("title"));
        playlist.description = stringField(object, QStringLiteral("description"));
        playlist.trackIds = stringArray(object.value(QStringLiteral("trackIds")));
        playlist.createdAt = integerField(object, QStringLiteral("createdAt"));
        playlist.updatedAt = integerField(object, QStringLiteral("updatedAt"));
        playlists.append(std::move(playlist));
    }
    return playlists;
}

/// Applies every storage invariant. Both directions go through this function:
/// decoding normalizes what it read, and encoding normalizes what it is about
/// to write, so a stored document always loads back unchanged.
MusicLibraryData normalized(const MusicLibraryData& input)
{
    MusicLibraryData result;

    QSet<QString> trackIds;
    trackIds.reserve(input.tracks.size());
    result.tracks.reserve(input.tracks.size());
    for (MusicTrackModel track : input.tracks) {
        track.id = track.id.trimmed();
        if (track.id.isEmpty() || trackIds.contains(track.id))
            continue;
        track.artistId = track.artistId.trimmed();
        track.albumId = track.albumId.trimmed();
        track.durationSeconds = qMax(0, track.durationSeconds);
        track.fileId = qMax(0, track.fileId);
        track.trackNumber = qMax(0, track.trackNumber);
        trackIds.insert(track.id);
        result.tracks.append(std::move(track));
    }

    QSet<QString> albumIds;
    albumIds.reserve(input.albums.size());
    result.albums.reserve(input.albums.size());
    for (MusicAlbum album : input.albums) {
        album.id = album.id.trimmed();
        if (album.id.isEmpty() || albumIds.contains(album.id))
            continue;
        album.artistId = album.artistId.trimmed();
        // The model stores 0 for an unknown year and nothing implausible.
        if (album.year < 1000 || album.year > 2100)
            album.year = 0;
        album.coverFileId = qMax(0, album.coverFileId);
        albumIds.insert(album.id);
        result.albums.append(std::move(album));
    }

    QSet<QString> artistIds;
    artistIds.reserve(input.artists.size());
    result.artists.reserve(input.artists.size());
    for (MusicArtist artist : input.artists) {
        artist.id = artist.id.trimmed();
        if (artist.id.isEmpty() || artistIds.contains(artist.id))
            continue;
        artistIds.insert(artist.id);
        result.artists.append(std::move(artist));
    }

    QSet<QString> playlistIds;
    playlistIds.reserve(input.playlists.size());
    result.playlists.reserve(input.playlists.size());
    for (MusicPlaylist playlist : input.playlists) {
        playlist.id = playlist.id.trimmed();
        playlist.title = playlist.title.trimmed();
        // A playlist without an id or a name cannot be shown or addressed.
        if (playlist.id.isEmpty() || playlist.title.isEmpty()
            || playlistIds.contains(playlist.id)) {
            continue;
        }

        QVector<QString> references;
        QSet<QString> seenReferences;
        references.reserve(playlist.trackIds.size());
        for (const QString& rawId : playlist.trackIds) {
            const QString trackId = rawId.trimmed();
            if (trackId.isEmpty() || !trackIds.contains(trackId)
                || seenReferences.contains(trackId)) {
                continue;
            }
            seenReferences.insert(trackId);
            references.append(trackId);
        }
        playlist.trackIds = std::move(references);
        playlist.createdAt = qMax<qint64>(0, playlist.createdAt);
        playlist.updatedAt = qMax<qint64>(0, playlist.updatedAt);
        playlistIds.insert(playlist.id);
        result.playlists.append(std::move(playlist));
    }

    QSet<QString> favoriteIds;
    favoriteIds.reserve(input.favorites.size());
    result.favorites.reserve(input.favorites.size());
    for (const QString& rawId : input.favorites) {
        const QString trackId = rawId.trimmed();
        if (trackId.isEmpty() || !trackIds.contains(trackId)
            || favoriteIds.contains(trackId)) {
            continue;
        }
        favoriteIds.insert(trackId);
        result.favorites.append(trackId);
    }

    return result;
}

MusicLibraryData decode(const QJsonObject& root)
{
    MusicLibraryData data;
    data.tracks = tracksFromJson(root.value(QStringLiteral("tracks")));
    data.albums = albumsFromJson(root.value(QStringLiteral("albums")));
    data.artists = artistsFromJson(root.value(QStringLiteral("artists")));
    data.playlists = playlistsFromJson(root.value(QStringLiteral("playlists")));
    data.favorites = stringArray(root.value(QStringLiteral("favorites")));
    return normalized(data);
}

QJsonObject trackToJson(const MusicTrackModel& track)
{
    return QJsonObject{
        {QStringLiteral("id"), track.id},
        {QStringLiteral("title"), track.title},
        {QStringLiteral("artistId"), track.artistId},
        {QStringLiteral("albumId"), track.albumId},
        {QStringLiteral("durationSeconds"), static_cast<double>(track.durationSeconds)},
        {QStringLiteral("fileId"), static_cast<double>(track.fileId)},
        {QStringLiteral("localPath"), track.localPath},
        {QStringLiteral("trackNumber"), static_cast<double>(track.trackNumber)},
    };
}

QJsonObject albumToJson(const MusicAlbum& album)
{
    return QJsonObject{
        {QStringLiteral("id"), album.id},
        {QStringLiteral("title"), album.title},
        {QStringLiteral("artistId"), album.artistId},
        {QStringLiteral("year"), static_cast<double>(album.year)},
        {QStringLiteral("coverFileId"), static_cast<double>(album.coverFileId)},
    };
}

QJsonObject artistToJson(const MusicArtist& artist)
{
    return QJsonObject{
        {QStringLiteral("id"), artist.id},
        {QStringLiteral("name"), artist.name},
    };
}

QJsonObject playlistToJson(const MusicPlaylist& playlist)
{
    QJsonArray trackIds;
    for (const QString& trackId : playlist.trackIds)
        trackIds.append(trackId);

    return QJsonObject{
        {QStringLiteral("id"), playlist.id},
        {QStringLiteral("title"), playlist.title},
        {QStringLiteral("description"), playlist.description},
        {QStringLiteral("trackIds"), trackIds},
        {QStringLiteral("createdAt"), static_cast<double>(playlist.createdAt)},
        {QStringLiteral("updatedAt"), static_cast<double>(playlist.updatedAt)},
    };
}

QJsonObject encode(const MusicLibraryData& data)
{
    const MusicLibraryData clean = normalized(data);

    QJsonArray tracks;
    for (const MusicTrackModel& track : clean.tracks)
        tracks.append(trackToJson(track));

    QJsonArray albums;
    for (const MusicAlbum& album : clean.albums)
        albums.append(albumToJson(album));

    QJsonArray artists;
    for (const MusicArtist& artist : clean.artists)
        artists.append(artistToJson(artist));

    QJsonArray playlists;
    for (const MusicPlaylist& playlist : clean.playlists)
        playlists.append(playlistToJson(playlist));

    QJsonArray favorites;
    for (const QString& trackId : clean.favorites)
        favorites.append(trackId);

    return QJsonObject{
        {QStringLiteral("version"), MusicLibraryStore::kSchemaVersion},
        {QStringLiteral("tracks"), tracks},
        {QStringLiteral("albums"), albums},
        {QStringLiteral("artists"), artists},
        {QStringLiteral("playlists"), playlists},
        {QStringLiteral("favorites"), favorites},
    };
}

bool sameTrack(const MusicTrackModel& left, const MusicTrackModel& right)
{
    return left.id == right.id
        && left.title == right.title
        && left.artistId == right.artistId
        && left.albumId == right.albumId
        && left.durationSeconds == right.durationSeconds
        && left.fileId == right.fileId
        && left.localPath == right.localPath
        && left.trackNumber == right.trackNumber;
}

bool sameAlbum(const MusicAlbum& left, const MusicAlbum& right)
{
    return left.id == right.id
        && left.title == right.title
        && left.artistId == right.artistId
        && left.year == right.year
        && left.coverFileId == right.coverFileId;
}

bool sameArtist(const MusicArtist& left, const MusicArtist& right)
{
    return left.id == right.id && left.name == right.name;
}

bool samePlaylist(const MusicPlaylist& left, const MusicPlaylist& right)
{
    return left.id == right.id
        && left.title == right.title
        && left.description == right.description
        && left.trackIds == right.trackIds
        && left.createdAt == right.createdAt
        && left.updatedAt == right.updatedAt;
}

template <typename T, typename Same>
bool sameVectors(const QVector<T>& left, const QVector<T>& right, Same same)
{
    if (left.size() != right.size())
        return false;
    for (int i = 0; i < left.size(); ++i) {
        if (!same(left.at(i), right.at(i)))
            return false;
    }
    return true;
}

} // namespace

MusicLibraryStore::MusicLibraryStore(QString path) : m_path(std::move(path))
{
}

MusicLibraryData MusicLibraryStore::load() const
{
    if (m_path.isEmpty())
        return {};

    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return {};

    const QJsonObject root = document.object();
    // A document written by another schema is left untouched and is not
    // guessed at; the caller can still overwrite it with save().
    if (documentVersion(root) != kSchemaVersion)
        return {};

    return decode(root);
}

bool MusicLibraryStore::save(const MusicLibraryData& data) const
{
    if (m_path.isEmpty())
        return false;

    const QDir parent = QFileInfo(m_path).absoluteDir();
    if (!parent.exists() && !QDir().mkpath(parent.absolutePath()))
        return false;

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    const QByteArray payload = QJsonDocument(encode(data)).toJson(QJsonDocument::Compact);
    if (file.write(payload) != payload.size()) {
        file.cancelWriting();
        return false;
    }

    return file.commit();
}

QString MusicLibraryStore::path() const
{
    return m_path;
}

bool MusicLibraryData::operator==(const MusicLibraryData& other) const
{
    return sameVectors(tracks, other.tracks, sameTrack)
        && sameVectors(albums, other.albums, sameAlbum)
        && sameVectors(artists, other.artists, sameArtist)
        && sameVectors(playlists, other.playlists, samePlaylist)
        && favorites == other.favorites;
}

} // namespace beygram::telegram
