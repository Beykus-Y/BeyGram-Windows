#include "ui/MediaPlaybackController.h"

#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using beygram::ui::MediaPlaybackController;

namespace {
/// Writes a tiny placeholder file; its bytes are not real media, so a
/// backend may reject it later. The controller only validates the path.
QString writeSampleFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {};
    const QByteArray payload = QByteArrayLiteral("beygram media playback sample payload");
    if (file.write(payload) != payload.size()) {
        file.close();
        return {};
    }
    file.close();
    return path;
}
} // namespace

class MediaPlaybackControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultsAreClosed();
    void openRejectsEmptySource();
    void openRejectsRemoteSource();
    void openRejectsMissingFile();
    void openRejectsDirectory();
    void openAcceptsExistingLocalFileOrReportsError();
    void rejectedOpenClosesCurrentMedia();
    void volumeIsClamped();
    void transportIsSafeBeforeOpen();
    void openEmitsStateChanged();
};

void MediaPlaybackControllerTest::defaultsAreClosed()
{
    MediaPlaybackController controller;

    QCOMPARE(controller.source(), QUrl());
    QVERIFY(controller.title().isEmpty());
    QVERIFY(!controller.isOpen());
    QVERIFY(!controller.playing());
    QCOMPARE(controller.position(), qint64(0));
    QCOMPARE(controller.duration(), qint64(0));
    QCOMPARE(controller.volume(), 1.0);
    QVERIFY(controller.error().isEmpty());
}

void MediaPlaybackControllerTest::openRejectsEmptySource()
{
    MediaPlaybackController controller;

    QVERIFY(!controller.open(QUrl()));
    QVERIFY(!controller.error().isEmpty());
    QVERIFY(!controller.isOpen());
    QVERIFY(controller.source().isEmpty());
    QVERIFY(!controller.playing());
}

void MediaPlaybackControllerTest::openRejectsRemoteSource()
{
    MediaPlaybackController controller;

    const QList<QUrl> remote{
        QUrl(QStringLiteral("https://example.com/clip.mp3")),
        QUrl(QStringLiteral("http://example.com/clip.mp4")),
        QUrl(QStringLiteral("rtsp://example.com/stream")),
    };

    for (const QUrl& url : remote) {
        QVERIFY2(!controller.open(url), qPrintable(url.toString()));
        QVERIFY(!controller.error().isEmpty());
        QVERIFY(!controller.isOpen());
        QVERIFY(controller.source().isEmpty());
    }
}

void MediaPlaybackControllerTest::openRejectsMissingFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    MediaPlaybackController controller;
    const QUrl missing = QUrl::fromLocalFile(dir.filePath(QStringLiteral("missing.mp3")));

    QVERIFY(!controller.open(missing));
    QVERIFY(!controller.error().isEmpty());
    QVERIFY(!controller.isOpen());
    QVERIFY(controller.source().isEmpty());
}

void MediaPlaybackControllerTest::openRejectsDirectory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    MediaPlaybackController controller;

    QVERIFY(!controller.open(QUrl::fromLocalFile(dir.path())));
    QVERIFY(!controller.error().isEmpty());
    QVERIFY(!controller.isOpen());
}

void MediaPlaybackControllerTest::openAcceptsExistingLocalFileOrReportsError()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = writeSampleFile(dir.filePath(QStringLiteral("sample.mp3")));
    QVERIFY(!path.isEmpty());
    QVERIFY(QFileInfo::exists(path));

    MediaPlaybackController controller;
    const QUrl source = QUrl::fromLocalFile(path);

    const bool opened = controller.open(source, QStringLiteral("Sample track"));
    if (opened) {
        QVERIFY(controller.isOpen());
        QCOMPARE(controller.source(), source);
        QCOMPARE(controller.title(), QStringLiteral("Sample track"));
        QVERIFY(!controller.playing());
        QVERIFY(controller.error().isEmpty());

        // Re-opening without a title falls back to the file name.
        QVERIFY(controller.open(source));
        QVERIFY(controller.isOpen());
        QCOMPARE(controller.title(), QFileInfo(path).fileName());
    } else {
        // A backend without a decoder for the placeholder bytes may refuse it.
        QVERIFY(!controller.error().isEmpty());
        QVERIFY(!controller.isOpen());
    }
}

void MediaPlaybackControllerTest::rejectedOpenClosesCurrentMedia()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = writeSampleFile(dir.filePath(QStringLiteral("sample.mp4")));
    QVERIFY(!path.isEmpty());

    MediaPlaybackController controller;
    if (!controller.open(QUrl::fromLocalFile(path), QStringLiteral("Clip")))
        QSKIP("No multimedia backend accepted the local sample file");

    QVERIFY(controller.isOpen());

    QVERIFY(!controller.open(QUrl(QStringLiteral("https://example.com/clip.mp4"))));
    QVERIFY(!controller.error().isEmpty());
    QVERIFY(!controller.isOpen());
    QVERIFY(!controller.playing());
    QVERIFY(controller.source().isEmpty());
}

void MediaPlaybackControllerTest::volumeIsClamped()
{
    MediaPlaybackController controller;
    QSignalSpy spy(&controller, &MediaPlaybackController::stateChanged);

    controller.setVolume(-1.0);
    QCOMPARE(controller.volume(), 0.0);

    controller.setVolume(2.0);
    QCOMPARE(controller.volume(), 1.0);

    controller.setVolume(0.35);
    QCOMPARE(controller.volume(), 0.35);

    QCOMPARE(spy.count(), 3);
}

void MediaPlaybackControllerTest::transportIsSafeBeforeOpen()
{
    MediaPlaybackController controller;
    QSignalSpy positionSpy(&controller, &MediaPlaybackController::positionChanged);

    controller.play();
    controller.pause();
    controller.toggle();
    controller.seek(5000);
    controller.seek(-1000);
    controller.stop();

    QVERIFY(!controller.isOpen());
    QVERIFY(!controller.playing());
    QCOMPARE(controller.position(), qint64(0));
    QCOMPARE(controller.duration(), qint64(0));
    QVERIFY(controller.error().isEmpty());
    QCOMPARE(positionSpy.count(), 0);
}

void MediaPlaybackControllerTest::openEmitsStateChanged()
{
    MediaPlaybackController controller;
    QSignalSpy spy(&controller, &MediaPlaybackController::stateChanged);

    QVERIFY(!controller.open(QUrl(QStringLiteral("https://example.com/clip.mp3"))));
    QVERIFY(spy.count() >= 1);

    spy.clear();
    QVERIFY(!controller.open(QUrl()));
    QVERIFY(spy.count() >= 1);
}

QTEST_GUILESS_MAIN(MediaPlaybackControllerTest)

#include "tst_media_playback.moc"
