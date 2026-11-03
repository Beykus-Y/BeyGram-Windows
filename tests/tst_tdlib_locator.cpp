#include "telegram/TdLibLocator.h"

#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QtTest>

using beygram::telegram::isApplicationOwnedPath;
using beygram::telegram::locateTdlibLibrary;

class TdLibLocatorTest final : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void findsApplicationLibrary();
    void reportsMissingLibrary();
    void rejectsRelativeOverride();
    void acceptsExplicitAbsoluteOverride();
    void recognisesOnlyApplicationOwnedPaths();

private:
    QStringList m_environmentKeys;
    QByteArray m_savedOverride;
};

void TdLibLocatorTest::init()
{
    m_savedOverride = qgetenv("BEYGRAM_TDLIB_DLL");
    qunsetenv("BEYGRAM_TDLIB_DLL");
}

void TdLibLocatorTest::cleanup()
{
    if (m_savedOverride.isEmpty())
        qunsetenv("BEYGRAM_TDLIB_DLL");
    else
        qputenv("BEYGRAM_TDLIB_DLL", m_savedOverride);
}

void TdLibLocatorTest::findsApplicationLibrary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QFile dll(QDir(directory.path()).filePath(QStringLiteral("tdjson.dll")));
    QVERIFY(dll.open(QIODevice::WriteOnly));
    dll.write("not-a-real-dll");
    dll.close();

    const auto location = locateTdlibLibrary(directory.path());
    QVERIFY2(location.hasValue(), qPrintable(location.error().message));
    QVERIFY(location.value().applicationOwned);
    QCOMPARE(location.value().source, QStringLiteral("application"));
    QVERIFY(QFileInfo(location.value().path).isAbsolute());
}

void TdLibLocatorTest::reportsMissingLibrary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const auto location = locateTdlibLibrary(directory.path());
    QVERIFY(!location.hasValue());
    QCOMPARE(location.error().code, beygram::ErrorCode::NotFound);
    QVERIFY(location.error().message.contains(QStringLiteral("tdjson.dll")));
}

void TdLibLocatorTest::rejectsRelativeOverride()
{
    qputenv("BEYGRAM_TDLIB_DLL", "tdjson.dll");
    const auto location = locateTdlibLibrary();
    QVERIFY(!location.hasValue());
    QCOMPARE(location.error().code, beygram::ErrorCode::InvalidArgument);
}

void TdLibLocatorTest::acceptsExplicitAbsoluteOverride()
{
    QTemporaryDir applicationDirectory;
    QTemporaryDir overrideDirectory;
    QVERIFY(applicationDirectory.isValid());
    QVERIFY(overrideDirectory.isValid());

    const QString path = QDir(overrideDirectory.path()).filePath(QStringLiteral("tdjson.dll"));
    QFile dll(path);
    QVERIFY(dll.open(QIODevice::WriteOnly));
    dll.close();

    qputenv("BEYGRAM_TDLIB_DLL", path.toUtf8());
    const auto location = locateTdlibLibrary(applicationDirectory.path());
    QVERIFY2(location.hasValue(), qPrintable(location.error().message));
    QCOMPARE(location.value().source, QStringLiteral("BEYGRAM_TDLIB_DLL"));
    QCOMPARE(location.value().path, QFileInfo(path).canonicalFilePath());
    QVERIFY(!location.value().applicationOwned);
}

void TdLibLocatorTest::recognisesOnlyApplicationOwnedPaths()
{
    QTemporaryDir app;
    QTemporaryDir other;
    QVERIFY(app.isValid());
    QVERIFY(other.isValid());

    const QString child = QDir(app.path()).filePath(QStringLiteral("tdlib/tdjson.dll"));
    QVERIFY(isApplicationOwnedPath(child, app.path()));
    QVERIFY(isApplicationOwnedPath(app.path(), app.path()));
    QVERIFY(!isApplicationOwnedPath(
        QDir(other.path()).filePath(QStringLiteral("tdjson.dll")), app.path()));
}

QTEST_GUILESS_MAIN(TdLibLocatorTest)
#include "tst_tdlib_locator.moc"
