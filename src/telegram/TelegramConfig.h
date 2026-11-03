#pragma once

#include <QJsonObject>
#include <QString>

namespace beygram::telegram {

/// Telegram API credentials. Never written to logs, never committed.
struct TelegramCredentials {
    qint32 apiId = 0;
    QString apiHash;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] static TelegramCredentials fromEnvironment();
};

struct TelegramProxySettings {
    QString server;
    int port = 0;
    QString type = QStringLiteral("socks5"); // socks5 | http | mtproto
    QString username;
    QString password; // also the MTProto secret

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] QString summary() const;
    [[nodiscard]] static TelegramProxySettings fromEnvironment();
};

/// All parameters needed for one independent TDLib session. The Windows client
/// is multi-account from the start: each session owns its directory pair.
struct TelegramConfig {
    TelegramCredentials credentials;
    QString accountId;
    QString accountDirectory;
    QString databaseDirectory;
    QString filesDirectory;
    QString databaseEncryptionKey;
    TelegramProxySettings proxy;
    QString systemLanguageCode = QStringLiteral("en");
    QString deviceModel = QStringLiteral("BeyGram Windows");
    QString systemVersion;
    QString applicationVersion;

    bool useMessageDatabase = true;
    bool useChatInfoDatabase = true;
    bool useFileDatabase = true;
    bool useSecretChats = true;
    bool enableStorageOptimizer = true;

    [[nodiscard]] bool isConfigured() const;
    [[nodiscard]] bool hasDatabaseKey() const;
    [[nodiscard]] bool hasProxy() const { return proxy.isValid(); }
    [[nodiscard]] QJsonObject tdlibParameters() const;
    [[nodiscard]] QJsonObject setTdlibParametersRequest() const;
};

} // namespace beygram::telegram
