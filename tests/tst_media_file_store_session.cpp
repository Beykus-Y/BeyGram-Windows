#include "telegram/MediaFileStore.h"
#include "telegram/TelegramSession.h"

#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using beygram::telegram::MediaFileStore;
using beygram::telegram::TelegramConfig;
using beygram::telegram::TelegramSession;

namespace {
constexpr int CachedFileId = 501;
constexpr int OtherFileId = 502;

QJsonObject completedFile(int fileId, const QString& path)
{
    return QJsonObject{
        {QStringLiteral("id"), fileId},
        {QStringLiteral("local"),
         QJsonObject{{QStringLiteral("path"), path},
                     {QStringLiteral("is_downloading_completed"), true}}}};
}

QJsonObject fileUpdate(const QJsonObject& file)
{
    return QJsonObject{{QStringLiteral("@type"), QStringLiteral("updateFile")},
                       {QStringLiteral("file"), file}};
}

TelegramConfig sessionConfig(const QString& accountDirectory, const QString& accountId)
{
    TelegramConfig config;
    config.accountId = accountId;
    config.accountDirectory = accountDirectory;
    config.databaseDirectory = accountDirectory + QStringLiteral("/db");
    config.filesDirectory = accountDirectory + QStringLiteral("/files");
    return config;
}
} // namespace

class MediaFileStoreSessionTest final : public QObject {
    Q_OBJECT

private slots:
    void switchingSessionsClearsCachedState();
    void newSessionCanReuseAnOldFileId();
    void sameSessionKeepsCachedState();
    void oldSessionUpdatesAreIgnored();
    void staleDownloadResponseIsIgnored();
    void clearingSessionDropsCachedState();
};

void MediaFileStoreSessionTest::switchingSessionsClearsCachedState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    TelegramSession first(sessionConfig(directory.filePath(QStringLiteral("first")),
                                        QStringLiteral("first")));
    TelegramSession second(sessionConfig(directory.filePath(QStringLiteral("second")),
                                         QStringLiteral("second")));

    MediaFileStore store;
    QSignalSpy readySpy(&store, &MediaFileStore::fileReady);
    QSignalSpy revisionSpy(&store, &MediaFileStore::revisionChanged);

    store.setSession(&first);
    const QString firstPath = directory.filePath(QStringLiteral("first/501.jpg"));
    const QString otherPath = directory.filePath(QStringLiteral("first/502.jpg"));
    store.trackFileObject(completedFile(CachedFileId, firstPath));
    store.trackFileObject(completedFile(OtherFileId, otherPath));

    QVERIFY(store.isReady(CachedFileId));
    QVERIFY(store.isReady(OtherFileId));
    QCOMPARE(store.path(CachedFileId), firstPath);
    QVERIFY(store.revision() > 0);
    QCOMPARE(readySpy.count(), 2);

    readySpy.clear();
    revisionSpy.clear();

    store.setSession(&second);

    QVERIFY(!store.isReady(CachedFileId));
    QVERIFY(!store.isReady(OtherFileId));
    QVERIFY(store.path(CachedFileId).isEmpty());
    QVERIFY(store.path(OtherFileId).isEmpty());
    QCOMPARE(store.revision(), 0);
    QCOMPARE(readySpy.count(), 0);
    QCOMPARE(revisionSpy.count(), 1);
}

void MediaFileStoreSessionTest::newSessionCanReuseAnOldFileId()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    TelegramSession first(sessionConfig(directory.filePath(QStringLiteral("first")),
                                        QStringLiteral("first")));
    TelegramSession second(sessionConfig(directory.filePath(QStringLiteral("second")),
                                         QStringLiteral("second")));

    MediaFileStore store;
    store.setSession(&first);
    const QString oldAccountPath = directory.filePath(QStringLiteral("first/501.jpg"));
    store.trackFileObject(completedFile(CachedFileId, oldAccountPath));
    QCOMPARE(store.path(CachedFileId), oldAccountPath);

    store.setSession(&second);
    QVERIFY(store.path(CachedFileId).isEmpty());
    QCOMPARE(store.revision(), 0);

    // The same TDLib id now belongs to the second account. It must resolve to
    // the new account's file, not to the cached path of the first one.
    const QString newAccountPath = directory.filePath(QStringLiteral("second/501.jpg"));
    store.trackFileObject(completedFile(CachedFileId, newAccountPath));

    QVERIFY(store.isReady(CachedFileId));
    QCOMPARE(store.path(CachedFileId), newAccountPath);
    QCOMPARE(store.revision(), 1);
}

void MediaFileStoreSessionTest::sameSessionKeepsCachedState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    TelegramSession first(sessionConfig(directory.filePath(QStringLiteral("first")),
                                        QStringLiteral("first")));

    MediaFileStore store;
    QSignalSpy readySpy(&store, &MediaFileStore::fileReady);
    QSignalSpy revisionSpy(&store, &MediaFileStore::revisionChanged);

    store.setSession(&first);
    const QString path = directory.filePath(QStringLiteral("first/501.jpg"));
    store.trackFileObject(completedFile(CachedFileId, path));
    const int revision = store.revision();
    QVERIFY(revision > 0);

    readySpy.clear();
    revisionSpy.clear();

    store.setSession(&first);
    store.setSession(&first, false);

    QVERIFY(store.isReady(CachedFileId));
    QCOMPARE(store.path(CachedFileId), path);
    QCOMPARE(store.revision(), revision);
    QCOMPARE(readySpy.count(), 0);
    QCOMPARE(revisionSpy.count(), 0);
}

void MediaFileStoreSessionTest::oldSessionUpdatesAreIgnored()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    TelegramSession first(sessionConfig(directory.filePath(QStringLiteral("first")),
                                        QStringLiteral("first")));
    TelegramSession second(sessionConfig(directory.filePath(QStringLiteral("second")),
                                         QStringLiteral("second")));

    MediaFileStore store;
    QSignalSpy readySpy(&store, &MediaFileStore::fileReady);
    QSignalSpy revisionSpy(&store, &MediaFileStore::revisionChanged);

    store.setSession(&first);
    const QString oldAccountPath = directory.filePath(QStringLiteral("first/501.jpg"));
    store.trackFileObject(completedFile(CachedFileId, oldAccountPath));

    store.setSession(&second);
    readySpy.clear();
    revisionSpy.clear();

    // A late update from the previous account must not repopulate the store.
    emit first.updateReceived(fileUpdate(completedFile(CachedFileId, oldAccountPath)));

    QVERIFY(!store.isReady(CachedFileId));
    QVERIFY(store.path(CachedFileId).isEmpty());
    QCOMPARE(store.revision(), 0);
    QCOMPARE(readySpy.count(), 0);
    QCOMPARE(revisionSpy.count(), 0);

    // The new session is still connected.
    const QString newAccountPath = directory.filePath(QStringLiteral("second/501.jpg"));
    emit second.updateReceived(fileUpdate(completedFile(CachedFileId, newAccountPath)));

    QVERIFY(store.isReady(CachedFileId));
    QCOMPARE(store.path(CachedFileId), newAccountPath);
    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(revisionSpy.count(), 1);
}

void MediaFileStoreSessionTest::staleDownloadResponseIsIgnored()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    TelegramSession first(sessionConfig(directory.filePath(QStringLiteral("first")),
                                        QStringLiteral("first")));
    TelegramSession second(sessionConfig(directory.filePath(QStringLiteral("second")),
                                         QStringLiteral("second")));

    MediaFileStore store;
    QSignalSpy readySpy(&store, &MediaFileStore::fileReady);
    QSignalSpy revisionSpy(&store, &MediaFileStore::revisionChanged);

    store.setSession(&first);
    const QString oldAccountPath = directory.filePath(QStringLiteral("first/501.jpg"));
    store.trackFileObject(completedFile(CachedFileId, oldAccountPath));

    store.setSession(&second);
    readySpy.clear();
    revisionSpy.clear();

    // Replays what a downloadFile callback issued by the first session does
    // when its response arrives after the switch. Generation -1 never matches.
    const QJsonObject lateResponse = completedFile(CachedFileId, oldAccountPath);
    const int staleGeneration = -1;
    QVERIFY(QMetaObject::invokeMethod(&store, "handleDownloadResponse",
                                      Qt::DirectConnection,
                                      Q_ARG(int, staleGeneration),
                                      Q_ARG(QJsonObject, lateResponse)));

    QVERIFY(!store.isReady(CachedFileId));
    QVERIFY(store.path(CachedFileId).isEmpty());
    QCOMPARE(store.revision(), 0);
    QCOMPARE(readySpy.count(), 0);
    QCOMPARE(revisionSpy.count(), 0);
}

void MediaFileStoreSessionTest::clearingSessionDropsCachedState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    TelegramSession first(sessionConfig(directory.filePath(QStringLiteral("first")),
                                        QStringLiteral("first")));

    MediaFileStore store;
    QSignalSpy readySpy(&store, &MediaFileStore::fileReady);
    QSignalSpy revisionSpy(&store, &MediaFileStore::revisionChanged);

    store.setSession(&first);
    const QString path = directory.filePath(QStringLiteral("first/501.jpg"));
    store.trackFileObject(completedFile(CachedFileId, path));
    QVERIFY(store.isReady(CachedFileId));

    readySpy.clear();
    revisionSpy.clear();

    store.setSession(nullptr);

    QVERIFY(!store.isReady(CachedFileId));
    QVERIFY(store.path(CachedFileId).isEmpty());
    QCOMPARE(store.revision(), 0);
    QCOMPARE(readySpy.count(), 0);
    QCOMPARE(revisionSpy.count(), 1);

    // The detached session no longer reaches the store.
    emit first.updateReceived(fileUpdate(completedFile(CachedFileId, path)));
    QVERIFY(!store.isReady(CachedFileId));
    QCOMPARE(readySpy.count(), 0);
}

QTEST_GUILESS_MAIN(MediaFileStoreSessionTest)
#include "tst_media_file_store_session.moc"
