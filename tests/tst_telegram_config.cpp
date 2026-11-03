#include "telegram/AuthState.h"
#include "telegram/TelegramConfig.h"

#include <QtTest>

using namespace beygram::telegram;

class TelegramConfigTest final : public QObject {
    Q_OBJECT

private slots:
    void credentialsRequireBothFields();
    void tdlibParametersCarryDirectoriesAndSecret();
    void configRequiresDirectories();
    void authorizationStatesMap();
    void unknownStateIsLoading();
    void stageNamesAreStable();
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

void TelegramConfigTest::tdlibParametersCarryDirectoriesAndSecret()
{
    TelegramConfig config;
    config.credentials.apiId = 4242;
    config.credentials.apiHash = QStringLiteral("secret-hash");
    config.databaseDirectory = QStringLiteral("C:/data/database");
    config.filesDirectory = QStringLiteral("C:/data/files");

    const QJsonObject parameters = config.tdlibParameters();
    QCOMPARE(parameters.value(QStringLiteral("api_id")).toInt(), 4242);
    QCOMPARE(parameters.value(QStringLiteral("api_hash")).toString(),
             QStringLiteral("secret-hash"));
    QCOMPARE(parameters.value(QStringLiteral("database_directory")).toString(),
             QStringLiteral("C:/data/database"));
    QCOMPARE(parameters.value(QStringLiteral("files_directory")).toString(),
             QStringLiteral("C:/data/files"));

    const QJsonObject request = config.setTdlibParametersRequest();
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setTdlibParameters"));
}

void TelegramConfigTest::configRequiresDirectories()
{
    TelegramConfig config;
    config.credentials.apiId = 1;
    config.credentials.apiHash = QStringLiteral("h");
    QVERIFY(!config.isConfigured());

    config.databaseDirectory = QStringLiteral("db");
    config.filesDirectory = QStringLiteral("files");
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

void TelegramConfigTest::unknownStateIsLoading()
{
    QCOMPARE(authStageFromTdlib(QStringLiteral("authorizationStateSomethingNew")),
             AuthStage::Loading);
}

void TelegramConfigTest::stageNamesAreStable()
{
    QCOMPARE(authStageName(AuthStage::Unavailable), QStringLiteral("unavailable"));
    QCOMPARE(authStageName(AuthStage::Ready), QStringLiteral("ready"));
    QVERIFY(authStageIsTerminalFailure(AuthStage::NotConfigured));
    QVERIFY(!authStageIsTerminalFailure(AuthStage::WaitCode));
}

QTEST_GUILESS_MAIN(TelegramConfigTest)
#include "tst_telegram_config.moc"
