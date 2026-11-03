#include "platform/windows/DpapiSecretStore.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using beygram::platform::WindowsDpapiSecretStore;

class SecretStoreTest final : public QObject {
    Q_OBJECT

private slots:
    void roundTripsASecret();
    void overwritesExistingSecret();
    void reportsMissingSecret();
    void removesSecret();
    void rejectsCorruptPayload();
    void fileNamesDoNotLeakKeys();
};

void SecretStoreTest::roundTripsASecret()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    WindowsDpapiSecretStore store(directory.path());
    const QByteArray secret = QByteArrayLiteral("api-hash-value");

    const auto status = store.write(QStringLiteral("telegram.credentials"), secret);
    QVERIFY2(status.ok(), qPrintable("DPAPI write failed: " + status.error().codeName() + " " + status.error().message));
    QVERIFY(store.contains(QStringLiteral("telegram.credentials")));

    const auto result = store.read(QStringLiteral("telegram.credentials"));
    QVERIFY2(result.hasValue(), qPrintable(result.error().message));
    QCOMPARE(result.value(), secret);
}

void SecretStoreTest::overwritesExistingSecret()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    WindowsDpapiSecretStore store(directory.path());
    QVERIFY(store.write(QStringLiteral("key"), QByteArrayLiteral("one")).ok());
    QVERIFY(store.write(QStringLiteral("key"), QByteArrayLiteral("two")).ok());

    const auto result = store.read(QStringLiteral("key"));
    QVERIFY(result.hasValue());
    QCOMPARE(result.value(), QByteArrayLiteral("two"));
}

void SecretStoreTest::reportsMissingSecret()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    WindowsDpapiSecretStore store(directory.path());
    const auto result = store.read(QStringLiteral("absent"));
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, beygram::ErrorCode::NotFound);
}

void SecretStoreTest::removesSecret()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    WindowsDpapiSecretStore store(directory.path());
    QVERIFY(store.write(QStringLiteral("key"), QByteArrayLiteral("value")).ok());
    QVERIFY(store.remove(QStringLiteral("key")).ok());
    QVERIFY(!store.contains(QStringLiteral("key")));
    QVERIFY(!store.read(QStringLiteral("key")).hasValue());
}

void SecretStoreTest::rejectsCorruptPayload()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    WindowsDpapiSecretStore store(directory.path());
    const QString path = store.filePathForKey(QStringLiteral("key"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArrayLiteral("this is not a DPAPI blob"));
    file.close();

    const auto result = store.read(QStringLiteral("key"));
    QVERIFY(!result.hasValue());
}

void SecretStoreTest::fileNamesDoNotLeakKeys()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    WindowsDpapiSecretStore store(directory.path());
    const QString path = store.filePathForKey(QStringLiteral("telegram.credentials"));
    QVERIFY(!path.contains(QStringLiteral("telegram")));
    QVERIFY(path.endsWith(QStringLiteral(".dpapi")));
}

QTEST_GUILESS_MAIN(SecretStoreTest)
#include "tst_secret_store.moc"
