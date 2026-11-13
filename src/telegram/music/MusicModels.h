#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One artist of the music library.
///
/// id is the stable identity of the artist and name its display name. Both are
/// already-parsed values; nothing here touches TDLib.
struct MusicArtist {
    QString id;
    QString name;
};

/// One album of the music library.
///
/// id identifies the album, title is its display title, artistId names the
/// artist it belongs to, year is its release year and coverFileId the TDLib
/// file holding its cover (0 while no cover is known). A year is either 0
/// (unknown) or a plausible release year in 1000..2100; musicAlbumYear()
/// enforces that rule.
struct MusicAlbum {
    QString id;
    QString title;
    QString artistId;
    int year = 0;
    int coverFileId = 0;
};

/// One track of the music library.
///
/// id identifies the track (normally the Telegram message it came from),
/// title is its display title, artistId and albumId name the artist and album
/// it belongs to, durationSeconds is its length, fileId the TDLib file holding
/// the audio (0 while no file is known), localPath the downloaded file's path
/// (empty while it is not downloaded) and trackNumber its position on the
/// album (0 while unknown). ids are trimmed and may be empty; a caller that
/// needs an addressable track drops it when id is empty.
struct MusicTrackModel {
    QString id;
    QString title;
    QString artistId;
    QString albumId;
    int durationSeconds = 0;
    int fileId = 0;
    QString localPath;
    int trackNumber = 0;
};

/// One playlist of the music library.
///
/// id identifies the playlist, title and description are its display texts and
/// trackIds lists its tracks in play order. createdAt and updatedAt are epoch
/// milliseconds. trackIds is kept deduplicated and in order by
/// musicDedupedTrackIds(): an id is trimmed, empty ids are dropped and a
/// repeated id keeps its first position.
struct MusicPlaylist {
    QString id;
    QString title;
    QString description;
    QVector<QString> trackIds;
    qint64 createdAt = 0;
    qint64 updatedAt = 0;
};

/// Builds a track from a TDLib `audio` object or from a `messageAudio`
/// message/content.
///
/// Both a bare audio object and a wrapper carrying the audio in
/// `content.audio` or `audio` are accepted. id, artistId and albumId are
/// trimmed; id falls back to the audio file's remote unique id so a bare audio
/// object is still identifiable, and artistId/albumId fall back to the
/// `performer`/`album` tags when no explicit id is present. A negative
/// duration or track number becomes 0, and a file id outside the int range
/// becomes 0. Missing or malformed fields are treated as absent; the function
/// never fails. An empty id is returned as-is and left for the caller to drop.
[[nodiscard]] MusicTrackModel musicTrackFromAudio(const QJsonObject& audio);

/// Returns \a year when it is 0 (unknown) or a plausible release year in
/// 1000..2100, and 0 for every other value.
[[nodiscard]] int musicAlbumYear(int year);

/// Returns \a trackIds trimmed, with empty ids dropped and repeated ids kept
/// once at their first position. The incoming order is otherwise preserved.
[[nodiscard]] QVector<QString> musicDedupedTrackIds(const QVector<QString>& trackIds);

/// Formats \a seconds as "m:ss", or "h:mm:ss" from one hour upwards.
///
/// A negative value is formatted as "0:00".
[[nodiscard]] QString musicDurationText(int seconds);

} // namespace beygram::telegram
