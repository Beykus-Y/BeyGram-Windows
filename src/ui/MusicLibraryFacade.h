#pragma once

#include "telegram/music/MusicLibraryStore.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

namespace beygram::ui {

/// QML-facing facade over one music library file.
///
/// The facade owns the MusicLibraryData in memory and reads/writes it through
/// MusicLibraryStore at the path given to setStoragePath(). Every accepted edit
/// is written back immediately, so a library survives the page being rebuilt;
/// without a path the library lives in memory only and no file is touched.
/// Saving is best-effort: a failed write leaves the in-memory library intact
/// and the next accepted edit tries again.
///
/// Edits go through the pure functions of MusicLibraryEdits, which keeps the
/// store invariants here as well: ids are trimmed, a repeated id keeps its
/// first entry, a playlist needs a non-empty title of at most 80 code points,
/// and removing a track drops it from every playlist and from the favorites.
/// A refused edit changes nothing and emits nothing. Creating a playlist and
/// adding a track to one stamp the playlist's timestamps; removing a track
/// refreshes updatedAt of the playlists that referenced it.
///
/// query is the search field's text and is stored exactly as typed, without
/// trimming, so a QML field can round-trip it while the user types. search()
/// trims its own argument and matches case-insensitively against title,
/// artistId and albumId, returning track ids in stored order.
///
/// changed() is emitted once per call that actually changed something
/// observable. Loading the same library, a refused edit, an unchanged query and
/// clear() on an already empty facade all stay silent.
///
/// The page API is trackTitles()/trackIds()/playlistTitles() plus the counts.
/// tracks(), playlists() and searchTracks() are read-only conveniences that
/// carry the extra fields a row needs (duration text, favorite state, playlist
/// size), so QML does not have to pair parallel lists itself.
class MusicLibraryFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList trackTitles READ trackTitles NOTIFY changed)
    Q_PROPERTY(QStringList trackIds READ trackIds NOTIFY changed)
    Q_PROPERTY(QStringList playlistTitles READ playlistTitles NOTIFY changed)
    Q_PROPERTY(int trackCount READ trackCount NOTIFY changed)
    Q_PROPERTY(int playlistCount READ playlistCount NOTIFY changed)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY changed)

    /// Row data for the library and favorites lists. Every entry is a map with
    /// id, title, artistId, albumId, durationSeconds, duration and favorite.
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY changed)

    /// Card data for the playlists tab. Every entry is a map with id, title,
    /// description, trackCount, createdAt, updatedAt and trackIds.
    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY changed)

    /// Ids of the favorite tracks, in the order they were favorited.
    Q_PROPERTY(QStringList favoriteIds READ favoriteIds NOTIFY changed)

public:
    explicit MusicLibraryFacade(QObject* parent = nullptr);

    /// Sets the file load() reads and every accepted edit writes. The path is
    /// kept verbatim and the library is not touched, so call load() afterwards
    /// to read the new file. An empty path makes load() return an empty library
    /// and turns saving into a no-op.
    void setStoragePath(const QString& path);

    /// Replaces the in-memory library with the file's content. A missing,
    /// unreadable or malformed file loads as an empty library. The query is
    /// kept. Emits changed() only when the loaded library differs.
    Q_INVOKABLE void load();

    /// Appends a track. The id, artistId and albumId are trimmed by the edit
    /// layer; a blank or already stored id is refused. A negative duration or
    /// file id is stored as zero.
    Q_INVOKABLE void addTrack(const QString& id, const QString& title,
                              const QString& artistId, const QString& albumId,
                              int durationSeconds, int fileId);

    /// Removes a stored track together with its playlist references and its
    /// favorite entry. An unknown id is ignored.
    Q_INVOKABLE void removeTrack(const QString& id);

    /// Adds the stored track to the favorites, or removes it when it already is
    /// one. An unknown id is ignored.
    Q_INVOKABLE void toggleFavorite(const QString& id);

    /// Appends a playlist with the given id and title. Both are trimmed; the
    /// title must be 1..80 code points long. createdAt and updatedAt are set to
    /// the current time.
    Q_INVOKABLE void createPlaylist(const QString& id, const QString& title);

    /// Removes a playlist. Its tracks, the favorites and the other playlists
    /// are untouched. An unknown id is ignored.
    Q_INVOKABLE void deletePlaylist(const QString& id);

    /// Appends the stored track to the stored playlist when both exist and the
    /// track is not in it yet, then refreshes the playlist's updatedAt.
    Q_INVOKABLE void addTrackToPlaylist(const QString& playlistId,
                                        const QString& trackId);

    /// Stores the search field's text verbatim. A repeat of the current text is
    /// silent. search() trims, so surrounding spaces do not change matches.
    Q_INVOKABLE void setQuery(const QString& query);

    /// Ids of the tracks matching \a query through MusicSearchEngine, in stored
    /// order and at most 200. A blank query matches every track.
    Q_INVOKABLE QStringList search(const QString& query) const;

    /// Drops every track, playlist, favorite and the query, then writes the
    /// empty library. An already empty facade stays silent.
    Q_INVOKABLE void clear();

    [[nodiscard]] QStringList trackTitles() const;
    [[nodiscard]] QStringList trackIds() const;
    [[nodiscard]] QStringList playlistTitles() const;
    [[nodiscard]] int trackCount() const;
    [[nodiscard]] int playlistCount() const;
    [[nodiscard]] QString query() const;
    [[nodiscard]] QVariantList tracks() const;
    [[nodiscard]] QVariantList playlists() const;
    [[nodiscard]] QStringList favoriteIds() const;

    /// Whether the stored track with \a trackId is a favorite.
    Q_INVOKABLE bool isFavorite(const QString& trackId) const;

    /// Number of tracks in the stored playlist \a playlistId, 0 when unknown.
    Q_INVOKABLE int playlistTrackCount(const QString& playlistId) const;

    /// The same rows as search(), but with the row fields tracks() carries.
    Q_INVOKABLE QVariantList searchTracks(const QString& query) const;

signals:
    /// Emitted whenever any observable state of the facade changed.
    void changed();

private:
    [[nodiscard]] QVector<telegram::MusicTrackModel> matchingTracks(
        const QString& text) const;
    [[nodiscard]] QVariantList trackRows(
        const QVector<telegram::MusicTrackModel>& tracks) const;
    [[nodiscard]] QVariantMap trackRow(const telegram::MusicTrackModel& track) const;

    /// Writes the current library when a storage path is set.
    void persist() const;

    QString m_storagePath;
    telegram::MusicLibraryData m_data;
    QString m_query;
};

} // namespace beygram::ui
