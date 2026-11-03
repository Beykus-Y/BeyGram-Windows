#include "platform/windows/DatabaseKeyStore.h"

#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>

using beygram::platform::DatabaseKeyStore;

class DatabaseKeyTest final : public QObject {
    Q_OBJECT

private slots:
    void createsAndReusesAKey();
    void storesOnlyADpapiBlob();
    void clearForgetsTheKey();
    void rejectsDamagedStoredKey();
};

void DatabaseKeyTest::createsAndReusesAKey()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DatabaseKeyStore store(directory.path());
    const auto first = store.loadOrCreate();
    QVERIFY2(first.hasValue(), qPrintable(first.error().message));
    QCOMPARE(first.value().size(), 64);
    QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{64}$")).match(first.value()).hasMatch());

    const auto second = store.loadOrCreate();
    QVERIFY(second.hasValue());
    QCOMPARE(second.value(), first.value());
}

void DatabaseKeyTest::storesOnlyADpapiBlob()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DatabaseKeyStore store(directory.path());
    const auto key = store.loadOrCreate();
    QVERIFY(key.hasValue());

    QFile file(store.keyFilePath());
    QVERIFY(file.exists());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray raw = file.readAll();
    QVERIFY(!raw.contains(key.value().toUtf8()));
    QVERIFY(!raw.isEmpty());
}

void DatabaseKeyTest::clearForgetsTheKey()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DatabaseKeyStore store(directory.path());
    const auto first = store.loadOrCreate();
    QVERIFY(first.hasValue());
    QVERIFY(store.clear().ok());
    QVERIFY(!QFile::exists(store.keyFilePath()));

    const auto second = store.loadOrCreate();
    QVERIFY(second.hasValue());
    QVERIFY(second.value() != first.value());
}

void DatabaseKeyTest::rejectsDamagedStoredKey()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DatabaseKeyStore store(directory.path());
    QFile file(store.keyFilePath());
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not a dpapi blob");
    file.close();

    const auto key = store.loadOrCreate();
    QVERIFY(!key.hasValue());
    QCOMPARE(key.error().code, beygram::ErrorCode::Io);
}

QTEST_GUILESS_MAIN(DatabaseKeyTest)
#include "tst_database_key.moc"
