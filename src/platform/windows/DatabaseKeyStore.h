#pragma once

#include "core/Result.h"

#include <QString>

namespace beygram::platform {

/// Creates and remembers the encryption key for one TDLib database.
///
/// The key is generated locally (32 random bytes, stored as 64 hex characters)
/// and wrapped with Windows DPAPI. It is never written in plaintext and never
/// logged. Losing the Windows profile or deleting the key makes that local
/// database unreadable, which is the intended security property.
class DatabaseKeyStore {
public:
    explicit DatabaseKeyStore(QString accountDirectory);

    /// Returns the existing key or creates one. Failure is fatal for the
    /// session: BeyGram does not fall back to an unencrypted database.
    [[nodiscard]] Result<QString> loadOrCreate();

    [[nodiscard]] Status clear();

    [[nodiscard]] QString keyFilePath() const;

private:
    QString m_accountDirectory;
};

} // namespace beygram::platform
