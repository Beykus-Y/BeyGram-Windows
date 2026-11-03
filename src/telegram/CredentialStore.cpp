#include "CredentialStore.h"

#include "platform/windows/DpapiSecretStore.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace beygram::telegram {
namespace {
constexpr auto CredentialKey = "telegram.api.credentials";
}

CredentialStore::CredentialStore(QString rootDirectory)
    : m_rootDirectory(std::move(rootDirectory))
{
}

QString CredentialStore::filePath() const
{
    return platform::WindowsDpapiSecretStore(m_rootDirectory)
        .filePathForKey(QString::fromLatin1(CredentialKey));
}

bool CredentialStore::exists() const
{
    return platform::WindowsDpapiSecretStore(m_rootDirectory)
        .contains(QString::fromLatin1(CredentialKey));
}

Result<TelegramCredentials> CredentialStore::load() const
{
    platform::WindowsDpapiSecretStore store(m_rootDirectory);
    const auto payload = store.read(QString::fromLatin1(CredentialKey));
    if (!payload.hasValue())
        return Result<TelegramCredentials>::fail(payload.error().code, payload.error().message);

    const QJsonDocument document = QJsonDocument::fromJson(payload.value());
    if (!document.isObject())
        return Result<TelegramCredentials>::fail(
            ErrorCode::Io, QStringLiteral("Stored Telegram credentials are malformed"));

    const QJsonObject object = document.object();
    TelegramCredentials credentials;
    credentials.apiId = object.value(QStringLiteral("api_id")).toInt();
    credentials.apiHash = object.value(QStringLiteral("api_hash")).toString().trimmed();
    if (!credentials.isValid() || !isValidApiHash(credentials.apiHash))
        return Result<TelegramCredentials>::fail(
            ErrorCode::Io, QStringLiteral("Stored Telegram credentials are incomplete"));
    return Result<TelegramCredentials>::ok(credentials);
}

Status CredentialStore::save(const TelegramCredentials& credentials)
{
    if (credentials.apiId <= 0)
        return Status::failure(ErrorCode::InvalidArgument,
                               QStringLiteral("API id must be a positive number"));
    if (!isValidApiHash(credentials.apiHash))
        return Status::failure(ErrorCode::InvalidArgument,
                               QStringLiteral("API hash must be 32 hexadecimal characters"));

    const QJsonObject object{
        {QStringLiteral("api_id"), credentials.apiId},
        {QStringLiteral("api_hash"), credentials.apiHash.trimmed()},
    };
    const QByteArray payload = QJsonDocument(object).toJson(QJsonDocument::Compact);

    platform::WindowsDpapiSecretStore store(m_rootDirectory);
    return store.write(QString::fromLatin1(CredentialKey), payload);
}

Status CredentialStore::clear()
{
    return platform::WindowsDpapiSecretStore(m_rootDirectory)
        .remove(QString::fromLatin1(CredentialKey));
}

TelegramCredentials CredentialStore::loadEffective() const
{
    const TelegramCredentials environment = TelegramCredentials::fromEnvironment();
    if (environment.isValid() && isValidApiHash(environment.apiHash))
        return environment;

    const auto stored = load();
    if (stored.hasValue())
        return stored.value();

    return {};
}

bool CredentialStore::effectiveFromEnvironment() const
{
    const TelegramCredentials environment = TelegramCredentials::fromEnvironment();
    return environment.isValid() && isValidApiHash(environment.apiHash);
}

bool CredentialStore::isValidApiHash(const QString& hash)
{
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-fA-F]{32}$"));
    return pattern.match(hash.trimmed()).hasMatch();
}

} // namespace beygram::telegram
