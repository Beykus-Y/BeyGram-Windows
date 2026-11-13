#include "MusicLibraryFacade.h"

#include "telegram/music/MusicLibraryEdits.h"
#include "telegram/music/MusicSearch.h"

#include <QDateTime>
#include <QtGlobal>

namespace beygram::ui {
namespace {

/// The search engine's ceiling; the page asks for everything it may show.
constexpr int kSearchLimit = 200;

} // namespace

MusicLibraryFacade::MusicLibraryFacade(QObject* parent) : QObject(parent)
{
}

void MusicLibraryFacade::setStoragePath(const QString& path)
{
    // The library is not reloaded here: the caller decides when a path change
    // becomes visible, so a page can point the facade at a file and then load.
    m_storagePath = path;
}

void MusicLibraryFacade::load()
{
    const telegram::MusicLibraryData loaded =
        telegram::MusicLibraryStore(m_storagePath).load();
    if (loaded == m_data)
        return;

    m_data = loaded;
    emit changed();
}

void MusicLibraryFacade::addTrack(const QString& id, const QString& title,
                                  const QString& artistId, const QString& albumId,
                                  int durationSeconds, int fileId)
{
    telegram::MusicTrackModel track;
    track.id = id;
    track.title = title;
    track.artistId = artistId;
    track.albumId = albumId;
    // The store clamps these on the way to disk; clamping here as well keeps
    // the in-memory copy identical to what a reload would return.
    track.durationSeconds = qMax(0, durationSeconds);
    track.fileId = qMax(0, fileId);

    if (!telegram::addTrack(m_data, track))
        return;

    persist();
    emit changed();
}

void MusicLibraryFacade::removeTrack(const QString& id)
{
    const QString cleanId = id.trimmed();
    if (cleanId.isEmpty())
        return;

    // Remember the playlists that will lose the reference, so their updatedAt
    // can follow the edit instead of going stale.
    QVector<QString> affectedPlaylists;
    for (const telegram::MusicPlaylist& playlist : m_data.playlists) {
        if (playlist.trackIds.contains(cleanId))
            affectedPlaylists.append(playlist.id);
    }

    if (!telegram::removeTrack(m_data, cleanId))
        return;

    if (!affectedPlaylists.isEmpty()) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        for (telegram::MusicPlaylist& playlist : m_data.playlists) {
            if (affectedPlaylists.contains(playlist.id))
                playlist.updatedAt = now;
        }
    }

    persist();
    emit changed();
}

void MusicLibraryFacade::toggleFavorite(const QString& id)
{
    if (!telegram::toggleFavorite(m_data, id))
        return;

    persist();
    emit changed();
}

void MusicLibraryFacade::createPlaylist(const QString& id, const QString& title)
{
    telegram::MusicPlaylist playlist;
    playlist.id = id;
    playlist.title = title;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    playlist.createdAt = now;
    playlist.updatedAt = now;

    if (!telegram::createPlaylist(m_data, playlist))
        return;

    persist();
    emit changed();
}

void MusicLibraryFacade::deletePlaylist(const QString& id)
{
    if (!telegram::deletePlaylist(m_data, id))
        return;

    persist();
    emit changed();
}

void MusicLibraryFacade::addTrackToPlaylist(const QString& playlistId,
                                            const QString& trackId)
{
    const QString cleanPlaylistId = playlistId.trimmed();
    if (!telegram::addTrackToPlaylist(m_data, cleanPlaylistId, trackId))
        return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (telegram::MusicPlaylist& playlist : m_data.playlists) {
        if (playlist.id == cleanPlaylistId) {
            playlist.updatedAt = now;
            break;
        }
    }

    persist();
    emit changed();
}

void MusicLibraryFacade::setQuery(const QString& query)
{
    if (m_query == query)
        return;

    m_query = query;
    emit changed();
}

QStringList MusicLibraryFacade::search(const QString& query) const
{
    QStringList ids;
    const QVector<telegram::MusicTrackModel> matches = matchingTracks(query);
    ids.reserve(matches.size());
    for (const telegram::MusicTrackModel& track : matches)
        ids.append(track.id);
    return ids;
}

void MusicLibraryFacade::clear()
{
    const telegram::MusicLibraryData empty;
    if (m_data == empty && m_query.isEmpty())
        return;

    m_data = {};
    m_query.clear();
    persist();
    emit changed();
}

QStringList MusicLibraryFacade::trackTitles() const
{
    QStringList titles;
    titles.reserve(m_data.tracks.size());
    for (const telegram::MusicTrackModel& track : m_data.tracks)
        titles.append(track.title);
    return titles;
}

QStringList MusicLibraryFacade::trackIds() const
{
    QStringList ids;
    ids.reserve(m_data.tracks.size());
    for (const telegram::MusicTrackModel& track : m_data.tracks)
        ids.append(track.id);
    return ids;
}

QStringList MusicLibraryFacade::playlistTitles() const
{
    QStringList titles;
    titles.reserve(m_data.playlists.size());
    for (const telegram::MusicPlaylist& playlist : m_data.playlists)
        titles.append(playlist.title);
    return titles;
}

int MusicLibraryFacade::trackCount() const
{
    return static_cast<int>(m_data.tracks.size());
}

int MusicLibraryFacade::playlistCount() const
{
    return static_cast<int>(m_data.playlists.size());
}

QString MusicLibraryFacade::query() const
{
    return m_query;
}

QVariantList MusicLibraryFacade::tracks() const
{
    return trackRows(m_data.tracks);
}

QVariantList MusicLibraryFacade::playlists() const
{
    QVariantList cards;
    cards.reserve(m_data.playlists.size());
    for (const telegram::MusicPlaylist& playlist : m_data.playlists) {
        QVariantMap card;
        card.insert(QStringLiteral("id"), playlist.id);
        card.insert(QStringLiteral("title"), playlist.title);
        card.insert(QStringLiteral("description"), playlist.description);
        card.insert(QStringLiteral("trackCount"),
                    static_cast<int>(playlist.trackIds.size()));
        card.insert(QStringLiteral("createdAt"), playlist.createdAt);
        card.insert(QStringLiteral("updatedAt"), playlist.updatedAt);
        card.insert(QStringLiteral("trackIds"), playlist.trackIds);
        cards.append(card);
    }
    return cards;
}

QStringList MusicLibraryFacade::favoriteIds() const
{
    QStringList ids;
    ids.reserve(m_data.favorites.size());
    for (const QString& id : m_data.favorites)
        ids.append(id);
    return ids;
}

bool MusicLibraryFacade::isFavorite(const QString& trackId) const
{
    const QString cleanId = trackId.trimmed();
    return !cleanId.isEmpty() && m_data.favorites.contains(cleanId);
}

int MusicLibraryFacade::playlistTrackCount(const QString& playlistId) const
{
    const QString cleanId = playlistId.trimmed();
    if (cleanId.isEmpty())
        return 0;

    for (const telegram::MusicPlaylist& playlist : m_data.playlists) {
        if (playlist.id == cleanId)
            return static_cast<int>(playlist.trackIds.size());
    }
    return 0;
}

QVariantList MusicLibraryFacade::searchTracks(const QString& query) const
{
    return trackRows(matchingTracks(query));
}

QVector<telegram::MusicTrackModel> MusicLibraryFacade::matchingTracks(
    const QString& text) const
{
    telegram::MusicSearchQuery query;
    query.text = text;
    query.limit = kSearchLimit;
    return telegram::MusicSearchEngine::search(m_data.tracks, m_data.favorites,
                                               query);
}

QVariantList MusicLibraryFacade::trackRows(
    const QVector<telegram::MusicTrackModel>& tracks) const
{
    QVariantList rows;
    rows.reserve(tracks.size());
    for (const telegram::MusicTrackModel& track : tracks)
        rows.append(trackRow(track));
    return rows;
}

QVariantMap MusicLibraryFacade::trackRow(
    const telegram::MusicTrackModel& track) const
{
    QVariantMap row;
    row.insert(QStringLiteral("id"), track.id);
    row.insert(QStringLiteral("title"), track.title);
    row.insert(QStringLiteral("artistId"), track.artistId);
    row.insert(QStringLiteral("albumId"), track.albumId);
    row.insert(QStringLiteral("durationSeconds"), track.durationSeconds);
    row.insert(QStringLiteral("duration"),
               telegram::musicDurationText(track.durationSeconds));
    row.insert(QStringLiteral("localPath"), track.localPath);
    row.insert(QStringLiteral("fileId"), track.fileId);
    row.insert(QStringLiteral("favorite"), m_data.favorites.contains(track.id));
    return row;
}

void MusicLibraryFacade::persist() const
{
    if (m_storagePath.isEmpty())
        return;

    // Best-effort: the edit is already visible in memory, and a failed write
    // is retried by the next accepted edit.
    telegram::MusicLibraryStore(m_storagePath).save(m_data);
}

} // namespace beygram::ui
