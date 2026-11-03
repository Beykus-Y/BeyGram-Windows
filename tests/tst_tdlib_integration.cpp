#include "platform/windows/DatabaseKeyStore.h"
#include "telegram/TdJsonTransport.h"
#include "telegram/TdLibLocator.h"
#include "telegram/TelegramSession.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace beygram::telegram;

/// Real TDLib integration without a real Telegram account.
///
/// The test creates a throwaway database with dummy API credentials, reaches
/// the phone-number authorization state, closes the session and reopens the
/// same database. It never submits a phone number, code or password and never
/// touches an existing BeyGram account.
class TdLibIntegrationTest final : public QObject {
    Q_OBJECT

private slots:
    void transportLoadsAndExecutesLocally();
    void transportCorrelatesResponses();
    void sessionReachesPhoneStateAndRestoresDatabase();
};

void TdLibIntegrationTest::transportLoadsAndExecutesLocally()
{
    QString error;
    const auto location = locateTdlibLibrary();
    if (!location.hasValue())
        QSKIP("tdjson.dll is not available in this environment");

    TdJsonTransport transport;
    QVERIFY2(transport.load(&error), qPrintable(error));
    QVERIFY(transport.isAvailable());
    QVERIFY(!transport.loadedPath().isEmpty());

    const auto response = transport.execute(
        QJsonObject{{QStringLiteral("@type"), QStringLiteral("getTextEntities")},
                    {QStringLiteral("text"), QStringLiteral("hello @beygram_bot")}});
    QVERIFY2(response.hasValue(), qPrintable(response.error().message));
    QVERIFY(!response.value().isEmpty());
}

void TdLibIntegrationTest::transportCorrelatesResponses()
{
    if (!locateTdlibLibrary().hasValue())
        QSKIP("tdjson.dll is not available in this environment");

    TdJsonTransport transport;
    QVERIFY(transport.load());
    QString error;
    QVERIFY2(transport.start(&error), qPrintable(error));

    bool handlerCalled = false;
    QJsonObject handlerResponse;
    QSignalSpy responses(&transport, &TdJsonTransport::responseReceived);

    const auto id = transport.sendRequest(
        QJsonObject{{QStringLiteral("@type"), QStringLiteral("getMe")}},
        [&](const QJsonObject& response) {
            handlerCalled = true;
            handlerResponse = response;
        },
        15000);
    QVERIFY(id > 0);

    QTRY_VERIFY_WITH_TIMEOUT(handlerCalled, 20000);
    QVERIFY(responses.count() >= 1);
    QCOMPARE(handlerResponse.value(QStringLiteral("@type")).toString(),
             QStringLiteral("error"));
    QVERIFY(handlerResponse.contains(QStringLiteral("@extra")));
    QCOMPARE(static_cast<qulonglong>(handlerResponse.value(QStringLiteral("@extra")).toDouble()),
             static_cast<qulonglong>(id));

    transport.stop();
}

void TdLibIntegrationTest::sessionReachesPhoneStateAndRestoresDatabase()
{
    if (!locateTdlibLibrary().hasValue())
        QSKIP("tdjson.dll is not available in this environment");

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString accountDirectory = QDir(directory.path()).filePath(QStringLiteral("account"));
    beygram::platform::DatabaseKeyStore keyStore(accountDirectory);
    const auto key = keyStore.loadOrCreate();
    QVERIFY2(key.hasValue(), qPrintable(key.error().message));

    TelegramConfig config;
    config.credentials.apiId = 1;
    config.credentials.apiHash = QStringLiteral("0123456789abcdef0123456789abcdef");
    config.accountId = QStringLiteral("test");
    config.accountDirectory = accountDirectory;
    config.databaseDirectory = QDir(accountDirectory).filePath(QStringLiteral("database"));
    config.filesDirectory = QDir(accountDirectory).filePath(QStringLiteral("files"));
    config.databaseEncryptionKey = key.value();
    config.applicationVersion = QStringLiteral("test");
    config.deviceModel = QStringLiteral("BeyGram test");

    {
        TelegramSession session(config);
        session.start();
        QTRY_VERIFY_WITH_TIMEOUT(
            session.stage() == AuthStage::WaitPhoneNumber
                || session.stage() == AuthStage::Error
                || session.stage() == AuthStage::Unavailable,
            45000);

        QVERIFY(session.stage() != AuthStage::Unavailable);
        QVERIFY2(session.stage() == AuthStage::WaitPhoneNumber,
                 qPrintable(QStringLiteral("TDLib stopped at %1: %2")
                                .arg(authStageName(session.stage()), session.lastError())));

        // A second session for the same account must not open the database.
        TelegramSession second(config);
        second.start();
        QCOMPARE(second.stage(), AuthStage::Locked);
        second.abort();

        QVERIFY(session.shutdown(8000));
    }

    // The same database and key must open again without a lock leak. This is
    // the restart path used when the user reopens BeyGram.
    {
        TelegramSession session(config);
        session.start();
        QTRY_VERIFY_WITH_TIMEOUT(
            session.stage() == AuthStage::WaitPhoneNumber || session.stage() == AuthStage::Error,
            45000);
        QVERIFY2(session.stage() == AuthStage::WaitPhoneNumber,
                 qPrintable(QStringLiteral("Restart stopped at %1: %2")
                                .arg(authStageName(session.stage()), session.lastError())));
        session.abort();
    }
}

QTEST_GUILESS_MAIN(TdLibIntegrationTest)
#include "tst_tdlib_integration.moc"
