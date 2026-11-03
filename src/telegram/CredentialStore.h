#pragma once

#include "TelegramConfig.h"
#include "core/Result.h"

#include <QString>

namespace beygram::telegram {

/// Stores the Telegram api_id/api_hash with Windows DPAPI.
///
/// Environment variables still win for development and CI. The file contains
/// only the DPAPI blob; the key names and values are not written in plaintext.
class CredentialStore {
public:
    explicit CredentialStore(QString rootDirectory);

    [[nodiscard]] bool exists() const;
    [[nodiscard]] Result<TelegramCredentials> load() const;
    [[nodiscard]] Status save(const TelegramCredentials& credentials);
    [[nodiscard]] Status clear();
    [[nodiscard]] QString filePath() const;

    /// Effective credentials: a valid environment pair wins, otherwise the
    /// DPAPI store is used.
    [[nodiscard]] TelegramCredentials loadEffective() const;
    [[nodiscard]] bool effectiveFromEnvironment() const;

    /// Telegram api_hash is exactly 32 hexadecimal characters.
    [[nodiscard]] static bool isValidApiHash(const QString& hash);

private:
    QString m_rootDirectory;
};

} // namespace beygram::telegram
