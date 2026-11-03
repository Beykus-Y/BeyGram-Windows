#include "DatabaseKeyStore.h"

#include "DpapiSecretStore.h"

#include <QDir>
#include <QRegularExpression>
#include <QRandomGenerator>

namespace beygram::platform {
namespace {
constexpr auto KeyName = "tdlib.database.key";
}

DatabaseKeyStore::DatabaseKeyStore(QString accountDirectory)
    : m_accountDirectory(std::move(accountDirectory))
{
}

QString DatabaseKeyStore::keyFilePath() const
{
    return WindowsDpapiSecretStore(m_accountDirectory)
        .filePathForKey(QString::fromLatin1(KeyName));
}

Result<QString> DatabaseKeyStore::loadOrCreate()
{
    if (m_accountDirectory.isEmpty())
        return Result<QString>::fail(ErrorCode::InvalidArgument,
                                     QStringLiteral("Account directory is empty"));

    if (!QDir().mkpath(m_accountDirectory))
        return Result<QString>::fail(ErrorCode::Io,
                                     QStringLiteral("Could not create the account directory"));

    WindowsDpapiSecretStore store(m_accountDirectory);
    const QString keyName = QString::fromLatin1(KeyName);

    const auto existing = store.read(keyName);
    if (existing.hasValue()) {
        const QString key = QString::fromUtf8(existing.value());
        if (key.size() == 64 && key.contains(QRegularExpression(QStringLiteral("^[0-9a-f]+$"))))
            return Result<QString>::ok(key);
        // A damaged key must not silently create a new database: that would
        // look like data loss. Report it instead.
        return Result<QString>::fail(
            ErrorCode::Io,
            QStringLiteral("The stored TDLib database key is damaged; refusing to open the database"));
    }

    if (existing.error().code != ErrorCode::NotFound)
        return Result<QString>::fail(existing.error().code,
                                     QStringLiteral("Could not read the TDLib database key: %1")
                                         .arg(existing.error().message));

    QString key;
    key.reserve(64);
    for (int index = 0; index < 8; ++index) {
        key += QStringLiteral("%1").arg(QRandomGenerator::system()->generate(), 8, 16,
                                        QLatin1Char('0'));
    }

    const Status saved = store.write(keyName, key.toUtf8());
    if (!saved.ok())
        return Result<QString>::fail(saved.error().code,
                                     QStringLiteral("Could not protect the TDLib database key: %1")
                                         .arg(saved.error().message));
    return Result<QString>::ok(key);
}

Status DatabaseKeyStore::clear()
{
    return WindowsDpapiSecretStore(m_accountDirectory)
        .remove(QString::fromLatin1(KeyName));
}

} // namespace beygram::platform
