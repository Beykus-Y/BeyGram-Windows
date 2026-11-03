#include "telegram/MediaFileStore.h"
#include "ui/MediaViewerController.h"

#include <QFile>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using beygram::telegram::MediaFileStore;
using beygram::ui::MediaViewerController;

namespace {
constexpr int SampleFileId = 4242;

QJsonObject completedFile(int fileId, const QString& path)
{
    return QJsonObject{
        {QStringLiteral("id"), fileId},
        {QStringLiteral("local"),
         QJsonObject{{QStringLiteral("path"), path},
                     {QStringLiteral("is_downloading_completed"), true}}}};
}

QByteArray createSampleFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {};
    const QByteArray payload = QByteArrayLiteral("beygram media viewer payload");
    file.write(payload);
    file.close();
    return payload;
}

QByteArray readAll(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}
} // namespace

class MediaViewerControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void openRejectsInvalidFileId();
    void openWaitsForDownloadWhenNotCached();
    void openCachedFileProvidesSourceImmediately();
    void sourceAppearsWhenFileBecomesReady();
    void closeResetsState();
    void saveAsRequiresOpenFile();
    void saveAsRejectsDownloadingFile();
    void saveAsCopiesReadyMedia();
    void saveAsRefusesToOverwriteCachedFile();
    void nullStoreReportsError();
    void stateChangedEmittedOnOpenAndClose();
};

void MediaViewerControllerTest::openRejectsInvalidFileId()
{
    MediaFileStore store;
    MediaViewerController controller(&store);

    controller.open(0, QStringLiteral("Photo"));
    QVERIFY(!controller.isOpen());
    QCOMPARE(controller.fileId(), 0);
    QVERIFY(!controller.error().isEmpty());
    QVERIFY(!controller.loading());
    QVERIFY(controller.source().isEmpty());

    controller.open(-7, QStringLiteral("Photo"));
    QVERIFY(!controller.isOpen());
    QCOMPARE(controller.fileId(), 0);
    QVERIFY(!controller.error().isEmpty());
}

void MediaViewerControllerTest::openWaitsForDownloadWhenNotCached()
{
    MediaFileStore store;
    MediaViewerController controller(&store);

    controller.open(SampleFileId, QStringLiteral("Photo"));
    QVERIFY(controller.isOpen());
    QCOMPARE(controller.fileId(), SampleFileId);
    QCOMPARE(controller.title(), QStringLiteral("Photo"));
    QVERIFY(controller.loading());
    QVERIFY(controller.source().isEmpty());
    QVERIFY(controller.error().isEmpty());
}

void MediaViewerControllerTest::openCachedFileProvidesSourceImmediately()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("cached.jpg"));
    QVERIFY(!createSampleFile(path).isEmpty());

    MediaFileStore store;
    store.trackFileObject(completedFile(SampleFileId, path));

    MediaViewerController controller(&store);
    controller.open(SampleFileId, QStringLiteral("Photo"));

    QVERIFY(controller.isOpen());
    QVERIFY(!controller.loading());
    QVERIFY(controller.error().isEmpty());
    QCOMPARE(controller.source(),
             QStringLiteral("image://beygrammedia/%1?v=%2")
                 .arg(SampleFileId)
                 .arg(store.revision()));
}

void MediaViewerControllerTest::sourceAppearsWhenFileBecomesReady()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("arrived.jpg"));
    QVERIFY(!createSampleFile(path).isEmpty());

    MediaFileStore store;
    MediaViewerController controller(&store);
    QSignalSpy spy(&controller, &MediaViewerController::stateChanged);

    controller.open(SampleFileId, QStringLiteral("Photo"));
    QVERIFY(controller.loading());
    const int afterOpen = static_cast<int>(spy.count());

    store.trackFileObject(completedFile(SampleFileId, path));

    QVERIFY(!controller.loading());
    QVERIFY(controller.error().isEmpty());
    QVERIFY(controller.source().startsWith(
        QStringLiteral("image://beygrammedia/%1?v=").arg(SampleFileId)));
    QVERIFY(spy.count() > afterOpen);
}

void MediaViewerControllerTest::closeResetsState()
{
    MediaFileStore store;
    MediaViewerController controller(&store);
    controller.open(SampleFileId, QStringLiteral("Photo"));
    QVERIFY(controller.isOpen());

    controller.close();
    QVERIFY(!controller.isOpen());
    QCOMPARE(controller.fileId(), 0);
    QVERIFY(controller.title().isEmpty());
    QVERIFY(controller.source().isEmpty());
    QVERIFY(!controller.loading());
    QVERIFY(controller.error().isEmpty());
}

void MediaViewerControllerTest::saveAsRequiresOpenFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    MediaFileStore store;
    MediaViewerController controller(&store);

    QVERIFY(!controller.saveAs(directory.filePath(QStringLiteral("out.jpg"))));
    QVERIFY(!controller.error().isEmpty());
    QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("out.jpg"))));
}

void MediaViewerControllerTest::saveAsRejectsDownloadingFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    MediaFileStore store;
    MediaViewerController controller(&store);
    controller.open(SampleFileId, QStringLiteral("Photo"));

    QVERIFY(!controller.saveAs(directory.filePath(QStringLiteral("out.jpg"))));
    QVERIFY(!controller.error().isEmpty());
}

void MediaViewerControllerTest::saveAsCopiesReadyMedia()
{
    QTemporaryDir sourceDirectory;
    QTemporaryDir targetDirectory;
    QVERIFY(sourceDirectory.isValid());
    QVERIFY(targetDirectory.isValid());

    const QString sourcePath = sourceDirectory.filePath(QStringLiteral("cached.jpg"));
    const QByteArray payload = createSampleFile(sourcePath);
    QVERIFY(!payload.isEmpty());

    MediaFileStore store;
    store.trackFileObject(completedFile(SampleFileId, sourcePath));

    MediaViewerController controller(&store);
    controller.open(SampleFileId, QStringLiteral("Photo"));
    QVERIFY(controller.isOpen());

    const QString targetPath = targetDirectory.filePath(QStringLiteral("saved.jpg"));
    QVERIFY(controller.saveAs(targetPath));
    QVERIFY(controller.error().isEmpty());
    QCOMPARE(readAll(targetPath), payload);
    QCOMPARE(readAll(sourcePath), payload);
}

void MediaViewerControllerTest::saveAsRefusesToOverwriteCachedFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("cached.jpg"));
    const QByteArray payload = createSampleFile(sourcePath);
    QVERIFY(!payload.isEmpty());

    MediaFileStore store;
    store.trackFileObject(completedFile(SampleFileId, sourcePath));

    MediaViewerController controller(&store);
    controller.open(SampleFileId, QStringLiteral("Photo"));

    QVERIFY(!controller.saveAs(sourcePath));
    QVERIFY(!controller.error().isEmpty());
    QCOMPARE(readAll(sourcePath), payload);
}

void MediaViewerControllerTest::nullStoreReportsError()
{
    MediaViewerController controller(nullptr);

    controller.open(SampleFileId, QStringLiteral("Photo"));
    QVERIFY(!controller.isOpen());
    QCOMPARE(controller.fileId(), 0);
    QVERIFY(!controller.loading());
    QVERIFY(!controller.error().isEmpty());
    QVERIFY(controller.source().isEmpty());
    QVERIFY(!controller.saveAs(QStringLiteral("out.jpg")));
}

void MediaViewerControllerTest::stateChangedEmittedOnOpenAndClose()
{
    MediaFileStore store;
    MediaViewerController controller(&store);
    QSignalSpy spy(&controller, &MediaViewerController::stateChanged);

    controller.open(SampleFileId, QStringLiteral("Photo"));
    QCOMPARE(spy.count(), 1);

    controller.close();
    QCOMPARE(spy.count(), 2);
}

QTEST_GUILESS_MAIN(MediaViewerControllerTest)
#include "tst_media_viewer.moc"
