#include "telegram/SessionLock.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

using beygram::telegram::SessionLock;

class SessionLockTest final : public QObject {
    Q_OBJECT

private slots:
    void preventsTwoSessions();
    void releasesCleanly();
    void reportsPath();
};

void SessionLockTest::preventsTwoSessions()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = QDir(directory.path()).filePath(QStringLiteral("session.lock"));

    SessionLock first(path);
    QVERIFY2(first.acquire().ok(), "first lock should succeed");
    QVERIFY(first.isLocked());

    SessionLock second(path);
    const auto status = second.acquire();
    QVERIFY(!status.ok());
    QCOMPARE(status.error().code, beygram::ErrorCode::Unavailable);
    QVERIFY(status.error().message.contains(QStringLiteral("another BeyGram process")));
}

void SessionLockTest::releasesCleanly()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = QDir(directory.path()).filePath(QStringLiteral("session.lock"));

    SessionLock first(path);
    QVERIFY(first.acquire().ok());
    first.release();
    QVERIFY(!first.isLocked());

    SessionLock second(path);
    QVERIFY(second.acquire().ok());
}

void SessionLockTest::reportsPath()
{
    SessionLock lock(QStringLiteral("C:/tmp/beygram-test.lock"));
    QCOMPARE(lock.path(), QStringLiteral("C:/tmp/beygram-test.lock"));
}

QTEST_GUILESS_MAIN(SessionLockTest)
#include "tst_session_lock.moc"
