#pragma once

#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One track of a background-music playlist.
///
/// trackId identifies the track inside the playlist and is the only field that
/// is required for an entry to be usable. title and artist are display text and
/// may be empty. durationSeconds is the whole track length and startMs the
/// offset at which playback should begin; both are clamped to zero or above.
struct BgMusicEntry {
    QString trackId;
    QString title;
    QString artist;
    int durationSeconds = 0;
    int startMs = 0;

    bool operator==(const BgMusicEntry&) const = default;
};

/// A background-music playlist.
///
/// id identifies the playlist, title is its display name and updatedAt is the
/// last change time in epoch milliseconds. entries keeps playlist order; every
/// trackId appears at most once, and the first entry for an id wins.
struct BgMusicPlaylist {
    QString id;
    QString title;
    QVector<BgMusicEntry> entries;
    qint64 updatedAt = 0;

    bool operator==(const BgMusicPlaylist&) const = default;
};

/// Serializes \a playlist as versioned JSON.
///
/// The document carries `version` 1, `id`, `title`, `updatedAt` and an
/// `entries` array. Entries without a usable trackId are dropped, repeated
/// trackIds keep their first entry, and negative durations and start offsets
/// are written as zero. The result is a compact UTF-8 JSON document.
[[nodiscard]] QString bgMusicToJson(const BgMusicPlaylist& playlist);

/// Parses a playlist written by bgMusicToJson().
///
/// A missing `version` is treated as version 1; any other version yields an
/// empty playlist. Invalid JSON and a non-object root yield an empty playlist
/// as well. A missing or non-array `entries` field yields no entries.
/// Individual malformed entries are skipped, repeated trackIds keep the first
/// entry, and negative durations and start offsets are clamped to zero.
/// `tracks` is accepted as an alias for `entries`, and snake_case field names
/// are accepted for compatibility.
[[nodiscard]] BgMusicPlaylist bgMusicFromJson(const QString& json);

/// Serializes \a playlist as an extended M3U playlist.
///
/// The output starts with `#EXTM3U`. Each entry is written as
/// `#EXTINF:<seconds>,<title> - <artist>` followed by an optional
/// `#EXTBGSTART:<milliseconds>` line and the track id. Entries without a usable
/// trackId are dropped, repeated trackIds keep their first entry, and negative
/// durations and start offsets are written as zero.
[[nodiscard]] QString bgMusicToM3u(const BgMusicPlaylist& playlist);

/// Parses an M3U or M3U8 playlist.
///
/// `#EXTINF` supplies the duration and, after the first comma, the
/// `title - artist` display text. `#EXTBGSTART` supplies the start offset in
/// milliseconds and may appear before or after its track id. Lines that are
/// not understood or carry a malformed value are ignored, blank lines are
/// skipped, and a plain track id without metadata is accepted with defaults.
/// Repeated trackIds keep the first entry, and negative durations and start
/// offsets are clamped to zero. Playlist id, title and updatedAt are not part
/// of the M3U format, so they are empty in the result.
[[nodiscard]] BgMusicPlaylist bgMusicFromM3u(const QString& m3u);

} // namespace beygram::telegram
