#include "core/AppPaths.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

class AppPathsTest final : public QObject {
    Q_OBJECT

private slots:
    void safeComponentAcceptsSimpleIdentifiers();
    void safeComponentRejectsTraversal();
    void safeComponentHashesUnusualIdentifiers();
    void tdlibDirectoryStaysUnderDataDirectory();
    void ensureDirectoryCreatesNestedPath();
    void ensureDirectoryRejectsEmptyPath();
};

void AppPathsTest::safeComponentAcceptsSimpleIdentifiers()
{
    QCOMPARE(beygram::AppPaths::safeComponent(QStringLiteral("primary")),
             QStringLiteral("primary"));
    QCOMPARE(beygram::AppPaths::safeComponent(QStringLiteral("account-1.2_x")),
             QStringLiteral("account-1.2_x"));
}

void AppPathsTest::safeComponentRejectsTraversal()
{
    const QString result = beygram::AppPaths::safeComponent(QStringLiteral("../evil"));
    QVERIFY(!result.contains(QStringLiteral("..")));
    QVERIFY(result.startsWith(QStringLiteral("account-")));
}

void AppPathsTest::safeComponentHashesUnusualIdentifiers()
{
    const QString result = beygram::AppPaths::safeComponent(QStringLiteral("аккаунт"));
    QVERIFY(result.startsWith(QStringLiteral("account-")));
    QCOMPARE(result.size(), 24);
}

void AppPathsTest::tdlibDirectoryStaysUnderDataDirectory()
{
    const QString path = beygram::AppPaths::tdlibAccountDirectory(QStringLiteral("primary"));
    QVERIFY(!path.isEmpty());
    QVERIFY(path.startsWith(beygram::AppPaths::dataDirectory()));
}

void AppPathsTest::ensureDirectoryCreatesNestedPath()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());

    const QString nested = QDir(temporary.path()).filePath(QStringLiteral("a/b/c"));
    QString error;
    QVERIFY(beygram::AppPaths::ensureDirectory(nested, &error));
    QVERIFY(QDir(nested).exists());
    QVERIFY(error.isEmpty());
}

void AppPathsTest::ensureDirectoryRejectsEmptyPath()
{
    QString error;
    QVERIFY(!beygram::AppPaths::ensureDirectory(QString(), &error));
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(AppPathsTest)
#include "tst_app_paths.moc"
