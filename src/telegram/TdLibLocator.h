#pragma once

#include "core/Result.h"

#include <QString>

namespace beygram::telegram {

/// Location of a tdjson.dll that BeyGram is allowed to load.
struct TdlibLibraryLocation {
    QString path;              ///< Canonical absolute path.
    QString source;            ///< "application", "application/tdlib" or "BEYGRAM_TDLIB_DLL".
    bool applicationOwned = false;
};

/// Resolves tdjson.dll without searching PATH.
///
/// Windows DLL search order is a hijacking surface. BeyGram only loads:
///   1. <application dir>/tdjson.dll
///   2. <application dir>/tdlib/tdjson.dll
///   3. an absolute path in BEYGRAM_TDLIB_DLL (explicit developer override)
///
/// The override is still required to be an existing absolute file. This is
/// intentionally narrower than QLibrary's default search.
[[nodiscard]] Result<TdlibLibraryLocation> locateTdlibLibrary(
    const QString& applicationDirectory = {});

/// True when `path` is the application directory or a descendant of it.
[[nodiscard]] bool isApplicationOwnedPath(const QString& path, const QString& applicationDirectory = {});

} // namespace beygram::telegram
