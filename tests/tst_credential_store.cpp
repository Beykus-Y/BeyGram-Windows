#include "telegram/CredentialStore.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using beygram::telegram::CredentialStore;
using beygram::telegram::TelegramCredentials;

class CredentialStoreTest final : public QObject {
    Q_OBJECT

private slots:
    void validatesApiHash();
    void savesAndLoadsThroughDpapi();
    void storesNoPlaintextSecret();
    void rejectsInvalidValues();
    void clearsStoredCredentials();
    void environmentTakesPrecedence();
};

void CredentialStoreTest::validatesApiHash()
{
    QVERIFY(CredentialStore::isValidApiHash(QStringLiteral("0123456789abcdef0123456789abcdef")));
    QVERIFY(CredentialStore::isValidApiHash(QStringLiteral("0123456789ABCDEF0123456789ABCDEF")));
    QVERIFY(!CredentialStore::isValidApiHash(QStringLiteral("short")));
    QVERIFY(!CredentialStore::isValidApiHash(QString(33, QLatin1Char('a'))));
    QVERIFY(!CredentialStore::isValidApiHash(QStringLiteral("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz")));
}

void CredentialStoreTest::savesAndLoadsThroughDpapi()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    CredentialStore store(directory.path());
    TelegramCredentials credentials;
    credentials.apiId = 123456;
    credentials.apiHash = QStringLiteral("0123456789abcdef0123456789abcdef");

    const auto saved = store.save(credentials);
    QVERIFY2(saved.ok(), qPrintable(saved.error().message));
    QVERIFY(store.exists());

    const auto loaded = store.load();
    QVERIFY2(loaded.hasValue(), qPrintable(loaded.error().message));
    QCOMPARE(loaded.value().apiId, 123456);
    QCOMPARE(loaded.value().apiHash, credentials.apiHash);
}

void CredentialStoreTest::storesNoPlaintextSecret()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    CredentialStore store(directory.path());
    TelegramCredentials credentials;
    credentials.apiId = 42;
    credentials.apiHash = QStringLiteral("0123456789abcdef0123456789abcdef");
    QVERIFY(store.save(credentials).ok());

    QFile file(store.filePath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray raw = file.readAll();
    QVERIFY(!raw.contains(credentials.apiHash.toUtf8()));
    QVERIFY(!raw.contains("api_hash"));
}

void CredentialStoreTest::rejectsInvalidValues()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    CredentialStore store(directory.path());

    TelegramCredentials credentials;
    credentials.apiId = 0;
    credentials.apiHash = QStringLiteral("0123456789abcdef0123456789abcdef");
    QVERIFY(!store.save(credentials).ok());

    credentials.apiId = 1;
    credentials.apiHash = QStringLiteral("not-hex");
    QVERIFY(!store.save(credentials).ok());
    QVERIFY(!store.exists());
}

void CredentialStoreTest::clearsStoredCredentials()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    CredentialStore store(directory.path());
    TelegramCredentials credentials;
    credentials.apiId = 7;
    credentials.apiHash = QStringLiteral("0123456789abcdef0123456789abcdef");
    QVERIFY(store.save(credentials).ok());
    QVERIFY(store.clear().ok());
    QVERIFY(!store.exists());

    const auto loaded = store.load();
    QVERIFY(!loaded.hasValue());
    QCOMPARE(loaded.error().code, beygram::ErrorCode::NotFound);
}

void CredentialStoreTest::environmentTakesPrecedence()
{
    const QByteArray savedId = qgetenv("BEYGRAM_TELEGRAM_API_ID");
    const QByteArray savedHash = qgetenv("BEYGRAM_TELEGRAM_API_HASH");
    qputenv("BEYGRAM_TELEGRAM_API_ID", "4242");
    qputenv("BEYGRAM_TELEGRAM_API_HASH", "ffffffffffffffffffffffffffffffff");

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    CredentialStore store(directory.path());
    TelegramCredentials stored;
    stored.apiId = 7;
    stored.apiHash = QStringLiteral("0123456789abcdef0123456789abcdef");
    QVERIFY(store.save(stored).ok());

    const TelegramCredentials effective = store.loadEffective();
    QCOMPARE(effective.apiId, 4242);
    QCOMPARE(effective.apiHash, QStringLiteral("ffffffffffffffffffffffffffffffff"));
    QVERIFY(store.effectiveFromEnvironment());
    QCOMPARE(store.load().value().apiId, 7);

    if (savedId.isEmpty())
        qunsetenv("BEYGRAM_TELEGRAM_API_ID");
    else
        qputenv("BEYGRAM_TELEGRAM_API_ID", savedId);
    if (savedHash.isEmpty())
        qunsetenv("BEYGRAM_TELEGRAM_API_HASH");
    else
        qputenv("BEYGRAM_TELEGRAM_API_HASH", savedHash);
}

QTEST_GUILESS_MAIN(CredentialStoreTest)
#include "tst_credential_store.moc"
