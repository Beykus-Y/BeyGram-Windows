#pragma once

#include "core/Result.h"

#include <QList>
#include <QString>

namespace beygram::telegram {

struct AccountRecord {
    QString id;
    qint64 createdAtMs = 0;
    qint64 lastUsedAtMs = 0;
    QString label;
};

/// Durable list of local Telegram accounts. Phase 1 uses a single primary
/// account, but the registry and the per-account directory layout are ready
/// for more. No credentials or message data are stored here.
class AccountRegistry {
public:
    explicit AccountRegistry(QString dataDirectory);

    /// Loads accounts.json. A missing file is an empty registry; a corrupt file
    /// is an error so the application never silently replaces account data.
    [[nodiscard]] Status load();
    [[nodiscard]] Status ensurePrimary();

    [[nodiscard]] Result<AccountRecord> active() const;
    [[nodiscard]] Result<AccountRecord> find(const QString& accountId) const;
    [[nodiscard]] QList<AccountRecord> accounts() const;
    [[nodiscard]] Status setActive(const QString& accountId);
    [[nodiscard]] Status touch(const QString& accountId);

    [[nodiscard]] QString registryPath() const;
    [[nodiscard]] QString accountDirectory(const QString& accountId) const;
    [[nodiscard]] QString activeAccountId() const;
    [[nodiscard]] bool isLoaded() const;

    static constexpr const char* PrimaryAccountId = "primary";

private:
    [[nodiscard]] Status save() const;

    QString m_dataDirectory;
    QList<AccountRecord> m_accounts;
    QString m_activeId;
    bool m_loaded = false;
};

} // namespace beygram::telegram
