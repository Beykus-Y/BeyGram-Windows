#pragma once

#include "BgMusic.h"

#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// The outcome of checking a background-music file before importing it.
///
/// accepted is true only when the file type, size and content are all usable.
/// reason is empty on success and a short machine-readable tag on rejection:
/// `empty`, `too-large`, `unsupported-extension` or `no-entries`. playlistId
/// and entryCount describe the parsed playlist and are zero when nothing was
/// parsed.
struct ImportDecision {
    bool accepted = false;
    QString reason;
    QString playlistId;
    int entryCount = 0;

    bool operator==(const ImportDecision&) const = default;
};

/// Decides whether a file can be imported as a background-music playlist.
///
/// \a fileName selects the parser by its extension, ignoring case: `.json` uses
/// bgMusicFromJson(), `.m3u` and `.m3u8` use bgMusicFromM3u(). Other extensions
/// are rejected. A file of zero bytes, or with blank content, is rejected as
/// empty, and a file of 2 MiB or more is rejected as too large. The content
/// must parse to at least one entry; entries that the parser drops do not
/// count.
///
/// This function is pure: it reads no file and touches no session state.
[[nodiscard]] ImportDecision planBgMusicImport(const QString& fileName,
                                               qint64 sizeBytes,
                                               const QString& content);

/// Returns a safe export file name for \a playlist.
///
/// The name starts from the playlist title. Every character outside
/// `[A-Za-z0-9._-]` becomes `_`, surrounding whitespace and dots are removed,
/// and the base is capped at 80 characters. An empty result falls back to
/// `playlist`. The returned name always ends in `.json` and never contains a
/// path separator.
[[nodiscard]] QString exportBgMusicFileName(const BgMusicPlaylist& playlist);

} // namespace beygram::telegram
