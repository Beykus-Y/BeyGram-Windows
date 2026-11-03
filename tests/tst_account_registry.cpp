#include "telegram/AccountRegistry.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using beygram::telegram::AccountRegistry;

class AccountRegistryTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void createsPrimaryAndPersists();
    void keepsActiveAccount();
    void touchesLastUsed();
    void rejectsCorruptRegistry();
    void accountDirectoryIsStable();
};

void AccountRegistryTest::startsEmpty()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    AccountRegistry registry(directory.path());
    QVERIFY(registry.load().ok());
    QVERIFY(registry.accounts().isEmpty());
    QVERIFY(registry.activeAccountId().isEmpty());
    QVERIFY(!registry.active().hasValue());
}

void AccountRegistryTest::createsPrimaryAndPersists()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    AccountRegistry registry(directory.path());
    QVERIFY(registry.ensurePrimary().ok());
    QCOMPARE(registry.activeAccountId(), QStringLiteral("primary"));
    QVERIFY(QFile::exists(registry.registryPath()));

    AccountRegistry reloaded(directory.path());
    QVERIFY(reloaded.load().ok());
    QCOMPARE(reloaded.accounts().size(), 1);
    QCOMPARE(reloaded.activeAccountId(), QStringLiteral("primary"));
}

void AccountRegistryTest::keepsActiveAccount()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    AccountRegistry registry(directory.path());
    QVERIFY(registry.ensurePrimary().ok());
    QVERIFY(!registry.setActive(QStringLiteral("missing")).ok());
    QVERIFY(registry.setActive(QStringLiteral("primary")).ok());
}

void AccountRegistryTest::touchesLastUsed()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    AccountRegistry registry(directory.path());
    QVERIFY(registry.ensurePrimary().ok());
    const qint64 before = registry.active().value().lastUsedAtMs;
    QTest::qWait(2);
    QVERIFY(registry.touch(QStringLiteral("primary")).ok());
    QVERIFY(registry.active().value().lastUsedAtMs >= before);
}

void AccountRegistryTest::rejectsCorruptRegistry()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    AccountRegistry registry(directory.path());
    QVERIFY(QDir().mkpath(QFileInfo(registry.registryPath()).absolutePath()));
    QFile file(registry.registryPath());
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{ not valid json");
    file.close();

    const auto status = registry.load();
    QVERIFY(!status.ok());
    QVERIFY(!status.error().message.isEmpty());
}

void AccountRegistryTest::accountDirectoryIsStable()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    AccountRegistry registry(directory.path());
    const QString first = registry.accountDirectory(QStringLiteral("primary"));
    const QString second = registry.accountDirectory(QStringLiteral("primary"));
    QCOMPARE(first, second);
    QVERIFY(!first.isEmpty());
}

QTEST_GUILESS_MAIN(AccountRegistryTest)
#include "tst_account_registry.moc"
