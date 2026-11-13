#pragma once

#include "MusicModels.h"

#include <QString>
#include <QVector>

namespace beygram::telegram {

/// The whole persistent music library.
///
/// Every collection keeps its stored order. The store guarantees that track,
/// album, artist and playlist ids are trimmed and unique (the first entry
/// wins), that playlists reference only stored track ids, and that favorites
/// list stored track ids once each. A row without an id and a playlist without
/// a name are not stored.
struct MusicLibraryData {
    QVector<MusicTrackModel> tracks;
    QVector<MusicAlbum> albums;
    QVector<MusicArtist> artists;
    QVector<MusicPlaylist> playlists;
    QVector<QString> favorites;

    bool operator==(const MusicLibraryData& other) const;
};

/// Reads and writes one music library as versioned JSON at a given path.
///
/// The document is an object with a `version` field and one array per section
/// (`tracks`, `albums`, `artists`, `playlists`, `favorites`); the Android
/// `schemaVersion` name is accepted on read as well. A missing,
/// unreadable, malformed or differently versioned document loads as an empty
/// library; a single bad array entry is skipped instead of failing the whole
/// file. Ids are trimmed, repeated ids keep their first entry, a playlist's
/// unknown track ids are dropped, and favorites are deduplicated.
///
/// save() applies the same normalization before writing, so load() always
/// returns exactly the data a successful save() stored. Writes create the
/// parent directory when needed and go through QSaveFile, so a failed or
/// interrupted write leaves the previous file intact. An empty path makes
/// load() return an empty library and save() return false.
class MusicLibraryStore final {
public:
    /// Version written by save() and accepted by load().
    static constexpr int kSchemaVersion = 1;

    explicit MusicLibraryStore(QString path);

    /// Returns the stored library, or an empty one when there is nothing
    /// usable to read.
    MusicLibraryData load() const;

    /// Replaces the stored library with a normalized copy of \a data. Returns
    /// false when the file could not be written.
    bool save(const MusicLibraryData& data) const;

    /// Returns the path this store was created with.
    [[nodiscard]] QString path() const;

private:
    QString m_path;
};

} // namespace beygram::telegram
