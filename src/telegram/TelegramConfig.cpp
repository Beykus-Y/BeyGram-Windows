#include "TelegramConfig.h"

#include "core/AppPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>
#include <QSysInfo>

namespace beygram::telegram {
namespace {

/// Environment first (CI and power users), then a portable proxy.txt next to
/// the executable, then the per-user config directory. This lets a packaged
/// build start by double-clicking beygram.exe without a .cmd launcher.
QString configuredProxyUrl()
{
    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    QString url = environment.value(QStringLiteral("BEYGRAM_PROXY")).trimmed();
    if (!url.isEmpty())
        return url;

    const QStringList candidates{
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("proxy.txt")),
        QDir(AppPaths::configDirectory()).filePath(QStringLiteral("proxy.txt")),
    };
    for (const QString& path : candidates) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        QTextStream stream(&file);
        while (!stream.atEnd()) {
            const QString line = stream.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;
            return line;
        }
    }
    return {};
}

} // namespace

bool TelegramCredentials::isValid() const
{
    return apiId > 0 && !apiHash.trimmed().isEmpty();
}

TelegramCredentials TelegramCredentials::fromEnvironment()
{
    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

    TelegramCredentials credentials;
    bool ok = false;
    credentials.apiId = environment.value(QStringLiteral("BEYGRAM_TELEGRAM_API_ID")).toInt(&ok);
    if (!ok)
        credentials.apiId = 0;
    credentials.apiHash = environment.value(QStringLiteral("BEYGRAM_TELEGRAM_API_HASH")).trimmed();
    return credentials;
}

bool TelegramProxySettings::isValid() const
{
    return !server.trimmed().isEmpty() && port > 0 && port <= 65535
        && (type == QLatin1String("socks5") || type == QLatin1String("http")
            || type == QLatin1String("mtproto"));
}

QString TelegramProxySettings::summary() const
{
    if (!isValid())
        return {};
    return QStringLiteral("%1 %2:%3").arg(type, server).arg(port);
}

TelegramProxySettings TelegramProxySettings::fromEnvironment()
{
    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    TelegramProxySettings settings;

    const QString url = configuredProxyUrl();
    if (!url.isEmpty()) {
        const QUrl parsed(url);
        const QString scheme = parsed.scheme().toLower();
        settings.type = scheme == QLatin1String("https") ? QStringLiteral("http") : scheme;
        settings.server = parsed.host();
        settings.port = parsed.port();
        settings.username = parsed.userName();
        settings.password = parsed.password();
        if (settings.type == QLatin1String("mtproto") && settings.password.isEmpty())
            settings.password = QUrlQuery(parsed).queryItemValue(QStringLiteral("secret"));
        return settings;
    }

    settings.server = environment.value(QStringLiteral("BEYGRAM_PROXY_SERVER")).trimmed();
    settings.port = environment.value(QStringLiteral("BEYGRAM_PROXY_PORT")).toInt();
    settings.type = environment.value(QStringLiteral("BEYGRAM_PROXY_TYPE"),
                                      QStringLiteral("socks5")).toLower();
    settings.username = environment.value(QStringLiteral("BEYGRAM_PROXY_USERNAME"));
    settings.password = environment.value(QStringLiteral("BEYGRAM_PROXY_PASSWORD"));
    return settings;
}

bool TelegramConfig::isConfigured() const
{
    return credentials.isValid() && !databaseDirectory.isEmpty() && !filesDirectory.isEmpty()
        && hasDatabaseKey();
}

bool TelegramConfig::hasDatabaseKey() const
{
    return databaseEncryptionKey.size() == 64;
}

QJsonObject TelegramConfig::tdlibParameters() const
{
    // Mirrors TdApi.SetTdlibParameters. The api_hash and the database key are
    // included only in the request sent to TDLib; this object must never be
    // logged.
    return QJsonObject{
        {QStringLiteral("use_test_dc"), false},
        {QStringLiteral("database_directory"), databaseDirectory},
        {QStringLiteral("files_directory"), filesDirectory},
        {QStringLiteral("database_encryption_key"), databaseEncryptionKey},
        {QStringLiteral("use_file_database"), useFileDatabase},
        {QStringLiteral("use_chat_info_database"), useChatInfoDatabase},
        {QStringLiteral("use_message_database"), useMessageDatabase},
        {QStringLiteral("use_secret_chats"), useSecretChats},
        {QStringLiteral("api_id"), credentials.apiId},
        {QStringLiteral("api_hash"), credentials.apiHash},
        {QStringLiteral("system_language_code"), systemLanguageCode},
        {QStringLiteral("device_model"), deviceModel},
        {QStringLiteral("system_version"), systemVersion},
        {QStringLiteral("application_version"), applicationVersion},
        {QStringLiteral("enable_storage_optimizer"), enableStorageOptimizer},
        {QStringLiteral("ignore_file_names"), false},
    };
}

QJsonObject TelegramConfig::setTdlibParametersRequest() const
{
    QJsonObject request = tdlibParameters();
    request.insert(QStringLiteral("@type"), QStringLiteral("setTdlibParameters"));
    return request;
}

} // namespace beygram::telegram
