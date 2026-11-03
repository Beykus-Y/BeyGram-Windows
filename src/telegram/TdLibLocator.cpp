#include "TdLibLocator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>

namespace beygram::telegram {
namespace {

QString applicationDirectoryOrDefault(const QString& supplied)
{
    if (!supplied.isEmpty())
        return QDir::cleanPath(supplied);
    if (QCoreApplication::instance() != nullptr)
        return QDir::cleanPath(QCoreApplication::applicationDirPath());
    return {};
}

} // namespace

bool isApplicationOwnedPath(const QString& path, const QString& applicationDirectory)
{
    const QString app = applicationDirectoryOrDefault(applicationDirectory);
    if (app.isEmpty() || path.isEmpty())
        return false;

    auto normalize = [](const QString& value) {
        QFileInfo info(value);
        const QString absolute = info.isAbsolute() ? info.absoluteFilePath() : value;
        const QString canonical = QFileInfo(absolute).canonicalFilePath();
        return QDir::fromNativeSeparators(canonical.isEmpty() ? QDir::cleanPath(absolute)
                                                              : canonical);
    };

    const QString canonicalApp = normalize(app);
    const QString canonicalPath = normalize(path);
    if (canonicalApp.isEmpty() || canonicalPath.isEmpty())
        return false;

    const QString prefix = canonicalApp + QLatin1Char('/');
    return canonicalPath.compare(canonicalApp, Qt::CaseInsensitive) == 0
        || canonicalPath.startsWith(prefix, Qt::CaseInsensitive);
}

Result<TdlibLibraryLocation> locateTdlibLibrary(const QString& applicationDirectory)
{
    const QString app = applicationDirectoryOrDefault(applicationDirectory);
    if (app.isEmpty())
        return Result<TdlibLibraryLocation>::fail(
            ErrorCode::Unavailable, QStringLiteral("Application directory is unknown"));

    struct Candidate {
        QString path;
        QString source;
        bool appOwned;
        bool explicitOverride;
    };

    QList<Candidate> candidates;
    candidates.append({QDir(app).filePath(QStringLiteral("tdjson.dll")),
                       QStringLiteral("application"), true, false});
    candidates.append({QDir(app).filePath(QStringLiteral("tdlib/tdjson.dll")),
                       QStringLiteral("application/tdlib"), true, false});

    const QString override =
        QProcessEnvironment::systemEnvironment().value(QStringLiteral("BEYGRAM_TDLIB_DLL"));
    if (!override.trimmed().isEmpty()) {
        QFileInfo info(override.trimmed());
        if (!info.isAbsolute())
            return Result<TdlibLibraryLocation>::fail(
                ErrorCode::InvalidArgument,
                QStringLiteral("BEYGRAM_TDLIB_DLL must be an absolute path"));

        const QString canonical = info.canonicalFilePath();
        if (canonical.isEmpty() || !QFileInfo(canonical).isFile())
            return Result<TdlibLibraryLocation>::fail(
                ErrorCode::NotFound,
                QStringLiteral("BEYGRAM_TDLIB_DLL does not point to an existing file"));

        candidates.append({canonical, QStringLiteral("BEYGRAM_TDLIB_DLL"),
                           isApplicationOwnedPath(canonical, app), true});
    }

    QStringList looked;
    for (const Candidate& candidate : candidates) {
        looked.append(candidate.path);
        const QFileInfo info(candidate.path);
        if (!info.isFile())
            continue;

        const QString canonical = info.canonicalFilePath();
        if (canonical.isEmpty())
            continue;

        // An explicit override may live outside the application directory, but
        // it is never resolved through PATH and is reported to the user.
        return Result<TdlibLibraryLocation>::ok(
            TdlibLibraryLocation{canonical, candidate.source, candidate.appOwned});
    }

    return Result<TdlibLibraryLocation>::fail(
        ErrorCode::NotFound,
        QStringLiteral("tdjson.dll was not found in trusted locations: %1")
            .arg(looked.join(QStringLiteral(", "))));
}

} // namespace beygram::telegram
