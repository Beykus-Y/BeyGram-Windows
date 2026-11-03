#include "TelegramConfig.h"

#include <QProcessEnvironment>
#include <QSysInfo>

namespace beygram::telegram {

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
