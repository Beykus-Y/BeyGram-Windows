#include "AppPaths.h"

#include <QCryptographicHash>
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>

namespace beygram {

QString AppPaths::dataDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString AppPaths::logDirectory()
{
    return QDir(dataDirectory()).filePath(QStringLiteral("logs"));
}

QString AppPaths::cacheDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

QString AppPaths::configDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

QString AppPaths::safeComponent(const QString& value)
{
    static const QRegularExpression allowed(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$"));
    if (allowed.match(value).hasMatch() && !value.contains(QStringLiteral("..")))
        return value;

    if (value.isEmpty())
        return QString();

    // Non-ASCII or otherwise unusual registry identifiers become a stable hash.
    return QStringLiteral("account-")
        + QString::fromLatin1(QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha256)
                                  .toHex()
                                  .left(16));
}

QString AppPaths::tdlibAccountDirectory(const QString& accountId)
{
    const QString safe = safeComponent(accountId);
    if (safe.isEmpty())
        return QString();
    return QDir(QDir(dataDirectory()).filePath(QStringLiteral("tdlib"))).filePath(safe);
}

bool AppPaths::ensureDirectory(const QString& path, QString* error)
{
    if (path.isEmpty()) {
        if (error)
            *error = QStringLiteral("Empty directory path");
        return false;
    }
    if (QDir().mkpath(path))
        return true;
    if (error)
        *error = QStringLiteral("Could not create directory: %1").arg(path);
    return false;
}

} // namespace beygram
