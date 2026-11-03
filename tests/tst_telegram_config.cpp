#include "telegram/AuthState.h"
#include "telegram/TelegramConfig.h"

#include <QtTest>

using namespace beygram::telegram;

class TelegramConfigTest final : public QObject {
    Q_OBJECT

private slots:
    void credentialsRequireBothFields();
    void tdlibParametersCarryDirectoriesAndSecrets();
    void configRequiresDirectoriesAndKey();
    void authorizationStatesMap();
    void qrAndEmailStatesMap();
    void unknownStateIsLoading();
    void stageNamesAreStable();
    void parsesProxyEnvironment();
};

void TelegramConfigTest::credentialsRequireBothFields()
{
    TelegramCredentials credentials;
    QVERIFY(!credentials.isValid());

    credentials.apiId = 12345;
    QVERIFY(!credentials.isValid());

    credentials.apiHash = QStringLiteral("hash");
    QVERIFY(credentials.isValid());

    credentials.apiId = 0;
    QVERIFY(!credentials.isValid());
}

void TelegramConfigTest::tdlibParametersCarryDirectoriesAndSecrets()
{
    TelegramConfig config;
    config.credentials.apiId = 4242;
    config.credentials.apiHash = QStringLiteral("secret-hash");
    config.databaseDirectory = QStringLiteral("C:/data/database");
    config.filesDirectory = QStringLiteral("C:/data/files");
    config.databaseEncryptionKey = QString(64, QLatin1Char('a'));

    const QJsonObject parameters = config.tdlibParameters();
    QCOMPARE(parameters.value(QStringLiteral("api_id")).toInt(), 4242);
    QCOMPARE(parameters.value(QStringLiteral("api_hash")).toString(),
             QStringLiteral("secret-hash"));
    QCOMPARE(parameters.value(QStringLiteral("database_directory")).toString(),
             QStringLiteral("C:/data/database"));
    QCOMPARE(parameters.value(QStringLiteral("files_directory")).toString(),
             QStringLiteral("C:/data/files"));
    QCOMPARE(parameters.value(QStringLiteral("database_encryption_key")).toString(),
             config.databaseEncryptionKey);

    const QJsonObject request = config.setTdlibParametersRequest();
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setTdlibParameters"));
}

void TelegramConfigTest::configRequiresDirectoriesAndKey()
{
    TelegramConfig config;
    config.credentials.apiId = 1;
    config.credentials.apiHash = QStringLiteral("h");
    config.databaseDirectory = QStringLiteral("db");
    config.filesDirectory = QStringLiteral("files");
    QVERIFY(!config.isConfigured());
    QVERIFY(!config.hasDatabaseKey());

    config.databaseEncryptionKey = QString(64, QLatin1Char('b'));
    QVERIFY(config.hasDatabaseKey());
    QVERIFY(config.isConfigured());
}

void TelegramConfigTest::authorizationStatesMap()
{
    QCOMPARE(authStageFromTdlib(QStringLiteral("authorizationStateWaitTdlibParameters")),
             AuthStage::WaitTdlibParameters);
    QCOMPARE(authStageFromTdlib(QStringLiteral("authorizationStateWaitPhoneNumber")),
             AuthStage::WaitPhoneNumber);
    QCOMPARE(authStageFromTdlib(QStringLiteral("authorizationStateWaitCode")),
             AuthStage::WaitCode);
    QCOMPARE(authStageFromTdlib(QStringLiteral("authorizationStateWaitPassword")),
             AuthStage::WaitPassword);
    QCOMPARE(authStageFromTdlib(QStringLiteral("authorizationStateReady")),
             AuthStage::Ready);
    QCOMPARE(authStageFromTdlib(QStringLiteral("authorizationStateClosed")),
             AuthStage::Closed);
}

void TelegramConfigTest::qrAndEmailStatesMap()
{
    QCOMPARE(authStageFromTdlib(QStringLiteral("authorizationStateWaitQrCode")),
             AuthStage::WaitQrCode);
    QCOMPARE(authStageFromTdlib(
                 QStringLiteral("authorizationStateWaitOtherDeviceConfirmation")),
             AuthStage::WaitQrCode);
    QCOMPARE(authStageFromTdlib(QStringLiteral("authorizationStateWaitEmailAddress")),
             AuthStage::WaitEmailAddress);
    QCOMPARE(authStageFromTdlib(QStringLiteral("authorizationStateWaitEmailCode")),
             AuthStage::WaitEmailCode);
}

void TelegramConfigTest::unknownStateIsLoading()
{
    QCOMPARE(authStageFromTdlib(QStringLiteral("authorizationStateSomethingNew")),
             AuthStage::Loading);
}

void TelegramConfigTest::stageNamesAreStable()
{
    QCOMPARE(authStageName(AuthStage::Unavailable), QStringLiteral("unavailable"));
    QCOMPARE(authStageName(AuthStage::Locked), QStringLiteral("locked"));
    QCOMPARE(authStageName(AuthStage::Ready), QStringLiteral("ready"));
    QCOMPARE(authStageName(AuthStage::WaitEmailCode), QStringLiteral("wait_email_code"));
    QVERIFY(authStageIsTerminalFailure(AuthStage::Locked));
    QVERIFY(!authStageIsTerminalFailure(AuthStage::WaitCode));
}

void TelegramConfigTest::parsesProxyEnvironment()
{
    const QByteArray saved = qgetenv("BEYGRAM_PROXY");
    qputenv("BEYGRAM_PROXY", "socks5://user:pass@127.0.0.1:7897");
    const TelegramProxySettings proxy = TelegramProxySettings::fromEnvironment();
    QVERIFY(proxy.isValid());
    QCOMPARE(proxy.type, QStringLiteral("socks5"));
    QCOMPARE(proxy.server, QStringLiteral("127.0.0.1"));
    QCOMPARE(proxy.port, 7897);
    QCOMPARE(proxy.username, QStringLiteral("user"));
    QCOMPARE(proxy.password, QStringLiteral("pass"));
    QVERIFY(proxy.summary().contains(QStringLiteral("127.0.0.1:7897")));
    QVERIFY(!proxy.summary().contains(QStringLiteral("pass")));

    if (saved.isEmpty())
        qunsetenv("BEYGRAM_PROXY");
    else
        qputenv("BEYGRAM_PROXY", saved);
}

QTEST_GUILESS_MAIN(TelegramConfigTest)
#include "tst_telegram_config.moc"
