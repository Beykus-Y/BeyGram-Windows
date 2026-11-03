#pragma once

#include "core/Result.h"

#include <QByteArray>
#include <QString>

namespace beygram::platform {

/// Per-user encrypted secret storage. On Windows the payload is protected with
/// DPAPI (CryptProtectData), so another Windows account cannot read it even if
/// it can read the file. Phase 0 uses it for the storage contract and tests;
/// the Telegram credential provisioning UI will build on it in Phase 1.
class SecretStore {
public:
    virtual ~SecretStore() = default;

    [[nodiscard]] virtual Status write(const QString& key, const QByteArray& secret) = 0;
    [[nodiscard]] virtual Result<QByteArray> read(const QString& key) const = 0;
    [[nodiscard]] virtual Status remove(const QString& key) = 0;
    [[nodiscard]] virtual bool contains(const QString& key) const = 0;
};

/// Windows DPAPI implementation. Files live under `rootDirectory`; the file
/// name is a SHA-256 of the logical key, never the key itself.
class WindowsDpapiSecretStore final : public SecretStore {
public:
    explicit WindowsDpapiSecretStore(QString rootDirectory);
    ~WindowsDpapiSecretStore() override;

    WindowsDpapiSecretStore(const WindowsDpapiSecretStore&) = delete;
    WindowsDpapiSecretStore& operator=(const WindowsDpapiSecretStore&) = delete;

    [[nodiscard]] Status write(const QString& key, const QByteArray& secret) override;
    [[nodiscard]] Result<QByteArray> read(const QString& key) const override;
    [[nodiscard]] Status remove(const QString& key) override;
    [[nodiscard]] bool contains(const QString& key) const override;

    [[nodiscard]] QString filePathForKey(const QString& key) const;

private:
    QString m_rootDirectory;
};

} // namespace beygram::platform
