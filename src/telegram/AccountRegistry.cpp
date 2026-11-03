#include "AccountRegistry.h"

#include "core/AppPaths.h"

#include <algorithm>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace beygram::telegram {
namespace {

constexpr int RegistryVersion = 1;

QJsonObject toJson(const AccountRecord& account)
{
    return QJsonObject{
        {QStringLiteral("id"), account.id},
        {QStringLiteral("created_at_ms"), static_cast<double>(account.createdAtMs)},
        {QStringLiteral("last_used_at_ms"), static_cast<double>(account.lastUsedAtMs)},
        {QStringLiteral("label"), account.label},
    };
}

AccountRecord fromJson(const QJsonObject& object)
{
    AccountRecord account;
    account.id = object.value(QStringLiteral("id")).toString();
    account.createdAtMs =
        static_cast<qint64>(object.value(QStringLiteral("created_at_ms")).toDouble());
    account.lastUsedAtMs =
        static_cast<qint64>(object.value(QStringLiteral("last_used_at_ms")).toDouble());
    account.label = object.value(QStringLiteral("label")).toString();
    return account;
}

} // namespace

AccountRegistry::AccountRegistry(QString dataDirectory)
    : m_dataDirectory(std::move(dataDirectory))
{
}

QString AccountRegistry::registryPath() const
{
    return QDir(QDir(m_dataDirectory).filePath(QStringLiteral("accounts")))
        .filePath(QStringLiteral("accounts.json"));
}

QString AccountRegistry::accountDirectory(const QString& accountId) const
{
    return AppPaths::tdlibAccountDirectory(accountId);
}

QString AccountRegistry::activeAccountId() const
{
    return m_activeId;
}

bool AccountRegistry::isLoaded() const
{
    return m_loaded;
}

Status AccountRegistry::load()
{
    m_accounts.clear();
    m_activeId.clear();
    m_loaded = false;

    QFile file(registryPath());
    if (!file.exists()) {
        m_loaded = true;
        return Status::success();
    }
    if (!file.open(QIODevice::ReadOnly))
        return Status::failure(ErrorCode::Io, file.errorString());

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!document.isObject()) {
        return Status::failure(
            ErrorCode::Io,
            QStringLiteral("Account registry is malformed: %1").arg(parseError.errorString()));
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != RegistryVersion) {
        return Status::failure(ErrorCode::Io,
                               QStringLiteral("Unsupported account registry version"));
    }

    const QJsonArray accounts = root.value(QStringLiteral("accounts")).toArray();
    for (const QJsonValue& value : accounts) {
        if (!value.isObject())
            continue;
        AccountRecord account = fromJson(value.toObject());
        if (AppPaths::safeComponent(account.id).isEmpty())
            continue;
        m_accounts.append(account);
    }

    const QString active = root.value(QStringLiteral("active_id")).toString();
    if (!active.isEmpty()
        && std::any_of(m_accounts.cbegin(), m_accounts.cend(),
                       [&active](const AccountRecord& account) { return account.id == active; })) {
        m_activeId = active;
    } else if (!m_accounts.isEmpty()) {
        m_activeId = m_accounts.first().id;
    }

    m_loaded = true;
    return Status::success();
}

Status AccountRegistry::ensurePrimary()
{
    if (!m_loaded) {
        const Status loaded = load();
        if (!loaded.ok())
            return loaded;
    }

    const QString primary = QString::fromLatin1(PrimaryAccountId);
    auto it = std::find_if(m_accounts.begin(), m_accounts.end(),
                           [&primary](const AccountRecord& account) {
                               return account.id == primary;
                           });

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (it == m_accounts.end()) {
        AccountRecord account;
        account.id = primary;
        account.createdAtMs = now;
        account.lastUsedAtMs = now;
        account.label = QStringLiteral("Telegram account");
        m_accounts.append(account);
    }
    if (m_activeId.isEmpty())
        m_activeId = primary;

    return save();
}

Result<AccountRecord> AccountRegistry::active() const
{
    if (!m_loaded)
        return Result<AccountRecord>::fail(ErrorCode::Unavailable,
                                           QStringLiteral("Account registry is not loaded"));
    if (m_activeId.isEmpty())
        return Result<AccountRecord>::fail(ErrorCode::NotFound,
                                           QStringLiteral("No active account"));
    return find(m_activeId);
}

Result<AccountRecord> AccountRegistry::find(const QString& accountId) const
{
    for (const AccountRecord& account : m_accounts) {
        if (account.id == accountId)
            return Result<AccountRecord>::ok(account);
    }
    return Result<AccountRecord>::fail(ErrorCode::NotFound,
                                       QStringLiteral("Account not found"));
}

QList<AccountRecord> AccountRegistry::accounts() const
{
    return m_accounts;
}

Status AccountRegistry::setActive(const QString& accountId)
{
    if (!find(accountId).hasValue())
        return Status::failure(ErrorCode::NotFound, QStringLiteral("Account not found"));
    m_activeId = accountId;
    return save();
}

Status AccountRegistry::touch(const QString& accountId)
{
    for (AccountRecord& account : m_accounts) {
        if (account.id == accountId) {
            account.lastUsedAtMs = QDateTime::currentMSecsSinceEpoch();
            return save();
        }
    }
    return Status::failure(ErrorCode::NotFound, QStringLiteral("Account not found"));
}

Status AccountRegistry::save() const
{
    QDir directory(QDir(m_dataDirectory).filePath(QStringLiteral("accounts")));
    if (!directory.exists() && !QDir().mkpath(directory.path()))
        return Status::failure(ErrorCode::Io,
                               QStringLiteral("Could not create the account registry directory"));

    QJsonArray accounts;
    for (const AccountRecord& account : m_accounts)
        accounts.append(toJson(account));

    const QJsonObject root{
        {QStringLiteral("version"), RegistryVersion},
        {QStringLiteral("active_id"), m_activeId},
        {QStringLiteral("accounts"), accounts},
    };

    QSaveFile file(registryPath());
    if (!file.open(QIODevice::WriteOnly))
        return Status::failure(ErrorCode::Io, file.errorString());
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
        file.cancelWriting();
        return Status::failure(ErrorCode::Io, file.errorString());
    }
    if (!file.commit())
        return Status::failure(ErrorCode::Io, file.errorString());
    return Status::success();
}

} // namespace beygram::telegram
