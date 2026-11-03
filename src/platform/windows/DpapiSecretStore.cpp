#include "DpapiSecretStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QSaveFile>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif

namespace beygram::platform {
namespace {

QString windowsError(DWORD code)
{
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    QString message;
    if (length > 0 && buffer)
        message = QString::fromWCharArray(buffer, static_cast<int>(length)).trimmed();
    if (buffer)
        LocalFree(buffer);
    if (message.isEmpty())
        message = QStringLiteral("Windows error %1").arg(code);
    return message;
}

#ifdef Q_OS_WIN
QByteArray protect(const QByteArray& plain, QString* error)
{
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.constData()));
    input.cbData = static_cast<DWORD>(plain.size());

    static const QByteArray entropyBytes = QByteArrayLiteral("BeyGram.Windows.SecretStore.v1");
    DATA_BLOB entropy{};
    entropy.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(entropyBytes.constData()));
    entropy.cbData = static_cast<DWORD>(entropyBytes.size());

    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"BeyGram Windows secret", &entropy, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        if (error)
            *error = windowsError(GetLastError());
        return {};
    }

    const QByteArray result(reinterpret_cast<const char*>(output.pbData),
                            static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return result;
}

QByteArray unprotect(const QByteArray& cipher, QString* error)
{
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(cipher.constData()));
    input.cbData = static_cast<DWORD>(cipher.size());

    static const QByteArray entropyBytes = QByteArrayLiteral("BeyGram.Windows.SecretStore.v1");
    DATA_BLOB entropy{};
    entropy.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(entropyBytes.constData()));
    entropy.cbData = static_cast<DWORD>(entropyBytes.size());

    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, &entropy, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        if (error)
            *error = windowsError(GetLastError());
        return {};
    }

    const QByteArray result(reinterpret_cast<const char*>(output.pbData),
                            static_cast<int>(output.cbData));
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return result;
}
#endif

} // namespace

WindowsDpapiSecretStore::WindowsDpapiSecretStore(QString rootDirectory)
    : m_rootDirectory(std::move(rootDirectory))
{
}

WindowsDpapiSecretStore::~WindowsDpapiSecretStore() = default;

QString WindowsDpapiSecretStore::filePathForKey(const QString& key) const
{
    const QString digest = QString::fromLatin1(
        QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex());
    return QDir(m_rootDirectory).filePath(digest + QStringLiteral(".dpapi"));
}

Status WindowsDpapiSecretStore::write(const QString& key, const QByteArray& secret)
{
    if (key.isEmpty())
        return Status::failure(ErrorCode::InvalidArgument, QStringLiteral("Secret key is empty"));
    if (secret.isEmpty())
        return Status::failure(ErrorCode::InvalidArgument, QStringLiteral("Secret is empty"));

    if (!QDir().mkpath(m_rootDirectory))
        return Status::failure(ErrorCode::Io,
                               QStringLiteral("Could not create secret directory: %1")
                                   .arg(m_rootDirectory));

#ifdef Q_OS_WIN
    QString error;
    const QByteArray protectedData = protect(secret, &error);
    if (protectedData.isEmpty())
        return Status::failure(ErrorCode::Io,
                               QStringLiteral("DPAPI encryption failed: %1").arg(error));

    QSaveFile file(filePathForKey(key));
    if (!file.open(QIODevice::WriteOnly))
        return Status::failure(ErrorCode::Io, file.errorString());
    if (file.write(protectedData) != protectedData.size()) {
        file.cancelWriting();
        return Status::failure(ErrorCode::Io, file.errorString());
    }
    if (!file.commit())
        return Status::failure(ErrorCode::Io, file.errorString());
    return Status::success();
#else
    Q_UNUSED(secret)
    return Status::failure(ErrorCode::Unavailable,
                           QStringLiteral("DPAPI is only available on Windows"));
#endif
}

Result<QByteArray> WindowsDpapiSecretStore::read(const QString& key) const
{
    if (key.isEmpty())
        return Result<QByteArray>::fail(ErrorCode::InvalidArgument,
                                        QStringLiteral("Secret key is empty"));

    QFile file(filePathForKey(key));
    if (!file.exists())
        return Result<QByteArray>::fail(ErrorCode::NotFound,
                                        QStringLiteral("No secret stored for this key"));
    if (!file.open(QIODevice::ReadOnly))
        return Result<QByteArray>::fail(ErrorCode::Io, file.errorString());

    const QByteArray stored = file.readAll();

#ifdef Q_OS_WIN
    QString error;
    QByteArray plain = unprotect(stored, &error);
    if (plain.isEmpty() && !error.isEmpty())
        return Result<QByteArray>::fail(ErrorCode::Io,
                                        QStringLiteral("DPAPI decryption failed: %1").arg(error));
    return Result<QByteArray>::ok(std::move(plain));
#else
    Q_UNUSED(stored)
    return Result<QByteArray>::fail(ErrorCode::Unavailable,
                                    QStringLiteral("DPAPI is only available on Windows"));
#endif
}

Status WindowsDpapiSecretStore::remove(const QString& key)
{
    const QString path = filePathForKey(key);
    if (!QFile::exists(path))
        return Status::success();
    if (!QFile::remove(path))
        return Status::failure(ErrorCode::Io, QStringLiteral("Could not remove secret file"));
    return Status::success();
}

bool WindowsDpapiSecretStore::contains(const QString& key) const
{
    return QFile::exists(filePathForKey(key));
}

} // namespace beygram::platform
