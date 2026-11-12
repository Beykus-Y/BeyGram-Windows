#include "telegram/MediaFileState.h"

#include <QJsonObject>
#include <QPair>
#include <QVector>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

QJsonObject localBlock(qint64 downloaded, bool active, bool completed, const QString& path)
{
    QJsonObject local;
    local.insert(QStringLiteral("downloaded_size"), downloaded);
    local.insert(QStringLiteral("is_downloading_active"), active);
    local.insert(QStringLiteral("is_downloading_completed"), completed);
    local.insert(QStringLiteral("path"), path);
    return local;
}

QJsonObject fileUpdate(int id, qint64 expectedSize, qint64 downloaded, bool active,
                       bool completed, const QString& path = QString())
{
    QJsonObject file;
    file.insert(QStringLiteral("id"), id);
    file.insert(QStringLiteral("expected_size"), expectedSize);
    file.insert(QStringLiteral("local"), localBlock(downloaded, active, completed, path));
    return file;
}

QJsonObject errorObject(int code, const QString& message)
{
    QJsonObject error;
    error.insert(QStringLiteral("code"), code);
    error.insert(QStringLiteral("message"), message);
    return error;
}

} // namespace

class MediaFileStateTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultsDescribeAnUnknownFile();
    void untouchedFileIsIdle();
    void activeDownloadIsDownloading();
    void partialPausedDownloadIsPending();
    void readyFileExposesItsPathAndSize();
    void completedDownloadCountsAsFullyDownloaded();
    void completedWithoutPathIsStillReady();
    void failedFromErrorObject();
    void failedFromErrorString();
    void failedFromLocalError();
    void errorWinsOverCompletedAndActive();
    void failedTransferHidesItsPartialPath();
    void expectedSizeWinsOverDeprecatedSize();
    void deprecatedSizeIsTheFallback();
    void missingSizesStayUnknown();
    void downloadedSizeIsClampedToTheKnownSize();
    void negativeProgressIsTreatedAsZero();
    void stringEncodedNumbersAreAccepted();
    void missingIdYieldsAnEmptyState();
    void outOfRangeIdYieldsAnEmptyState();
    void pathIsOnlyExposedWhenReady();
    void readyIsHundredPercent();
    void failedHasNoProgress();
    void idleAndUnknownSizeHaveNoProgress();
    void downloadingProgressTruncatesDown();
    void runningTransferNeverLooksFinished();
    void pendingReportsItsPartialProgress();
    void hugeNumbersDoNotOverflow();
};

void MediaFileStateTest::defaultsDescribeAnUnknownFile()
{
    const MediaFileState state;
    QCOMPARE(state.fileId, 0);
    QCOMPARE(state.state, DownloadState::Idle);
    QCOMPARE(state.downloaded, 0);
    QCOMPARE(state.size, 0);
    QVERIFY(state.path.isEmpty());
    QCOMPARE(progressPercent(state), 0);
}

void MediaFileStateTest::untouchedFileIsIdle()
{
    const MediaFileState state = mediaFileStateFromUpdate(fileUpdate(7, 1000, 0, false, false));
    QCOMPARE(state.fileId, 7);
    QCOMPARE(state.state, DownloadState::Idle);
    QCOMPARE(state.downloaded, 0);
    QCOMPARE(state.size, 1000);
    QVERIFY(state.path.isEmpty());
}

void MediaFileStateTest::activeDownloadIsDownloading()
{
    const MediaFileState state = mediaFileStateFromUpdate(fileUpdate(7, 1000, 250, true, false));
    QCOMPARE(state.state, DownloadState::Downloading);
    QCOMPARE(state.downloaded, 250);
    QCOMPARE(state.size, 1000);
    QVERIFY(state.path.isEmpty());
}

void MediaFileStateTest::partialPausedDownloadIsPending()
{
    // Not running and not finished, but bytes are already on disk.
    const MediaFileState state = mediaFileStateFromUpdate(fileUpdate(7, 1000, 400, false, false));
    QCOMPARE(state.state, DownloadState::Pending);
    QCOMPARE(state.downloaded, 400);
    QVERIFY(state.path.isEmpty());
}

void MediaFileStateTest::readyFileExposesItsPathAndSize()
{
    const MediaFileState state = mediaFileStateFromUpdate(
        fileUpdate(7, 1000, 1000, false, true, QStringLiteral("C:/cache/photo.jpg")));
    QCOMPARE(state.fileId, 7);
    QCOMPARE(state.state, DownloadState::Ready);
    QCOMPARE(state.downloaded, 1000);
    QCOMPARE(state.size, 1000);
    QCOMPARE(state.path, QStringLiteral("C:/cache/photo.jpg"));
}

void MediaFileStateTest::completedDownloadCountsAsFullyDownloaded()
{
    // A stale downloaded_size cannot contradict a completed transfer.
    const MediaFileState state =
        mediaFileStateFromUpdate(fileUpdate(7, 1000, 100, false, true, QStringLiteral("p")));
    QCOMPARE(state.state, DownloadState::Ready);
    QCOMPARE(state.downloaded, 1000);
    QCOMPARE(progressPercent(state), 100);
}

void MediaFileStateTest::completedWithoutPathIsStillReady()
{
    const MediaFileState state = mediaFileStateFromUpdate(fileUpdate(7, 10, 10, false, true));
    QCOMPARE(state.state, DownloadState::Ready);
    QVERIFY(state.path.isEmpty());
}

void MediaFileStateTest::failedFromErrorObject()
{
    QJsonObject file = fileUpdate(7, 1000, 300, true, false);
    file.insert(QStringLiteral("error"), errorObject(400, QStringLiteral("FILE_TOO_BIG")));

    const MediaFileState state = mediaFileStateFromUpdate(file);
    QCOMPARE(state.state, DownloadState::Failed);
    QCOMPARE(state.downloaded, 300);
    QCOMPARE(state.size, 1000);
    QVERIFY(state.path.isEmpty());
}

void MediaFileStateTest::failedFromErrorString()
{
    QJsonObject file = fileUpdate(7, 1000, 300, false, false);
    file.insert(QStringLiteral("error"), QStringLiteral("download cancelled"));

    QCOMPARE(mediaFileStateFromUpdate(file).state, DownloadState::Failed);
}

void MediaFileStateTest::failedFromLocalError()
{
    QJsonObject local = localBlock(200, false, false, QString());
    local.insert(QStringLiteral("error"), errorObject(404, QStringLiteral("NOT_FOUND")));

    QJsonObject file;
    file.insert(QStringLiteral("id"), 7);
    file.insert(QStringLiteral("expected_size"), 1000);
    file.insert(QStringLiteral("local"), local);

    QCOMPARE(mediaFileStateFromUpdate(file).state, DownloadState::Failed);
}

void MediaFileStateTest::errorWinsOverCompletedAndActive()
{
    QJsonObject completed = fileUpdate(7, 1000, 1000, true, true, QStringLiteral("p"));
    completed.insert(QStringLiteral("error"), errorObject(500, QStringLiteral("internal")));
    const MediaFileState failed = mediaFileStateFromUpdate(completed);
    QCOMPARE(failed.state, DownloadState::Failed);
    QVERIFY(failed.path.isEmpty());
    QCOMPARE(progressPercent(failed), 0);

    // An empty error object is not an error, so the flags still decide.
    QJsonObject emptyError = fileUpdate(7, 1000, 500, true, false);
    emptyError.insert(QStringLiteral("error"), QJsonObject());
    QCOMPARE(mediaFileStateFromUpdate(emptyError).state, DownloadState::Downloading);

    // An empty error string is not an error either.
    QJsonObject emptyString = fileUpdate(7, 1000, 500, true, false);
    emptyString.insert(QStringLiteral("error"), QString());
    QCOMPARE(mediaFileStateFromUpdate(emptyString).state, DownloadState::Downloading);
}

void MediaFileStateTest::failedTransferHidesItsPartialPath()
{
    QJsonObject local = localBlock(500, false, false, QStringLiteral("C:/cache/partial.bin"));
    local.insert(QStringLiteral("error"), errorObject(420, QStringLiteral("FAILED")));

    QJsonObject file;
    file.insert(QStringLiteral("id"), 7);
    file.insert(QStringLiteral("expected_size"), 1000);
    file.insert(QStringLiteral("local"), local);

    const MediaFileState state = mediaFileStateFromUpdate(file);
    QCOMPARE(state.state, DownloadState::Failed);
    QVERIFY(state.path.isEmpty());
}

void MediaFileStateTest::expectedSizeWinsOverDeprecatedSize()
{
    QJsonObject file = fileUpdate(7, 1000, 100, true, false);
    file.insert(QStringLiteral("size"), 4096);

    const MediaFileState state = mediaFileStateFromUpdate(file);
    QCOMPARE(state.size, 1000);
}

void MediaFileStateTest::deprecatedSizeIsTheFallback()
{
    QJsonObject file;
    file.insert(QStringLiteral("id"), 7);
    file.insert(QStringLiteral("size"), 4096);
    file.insert(QStringLiteral("local"), localBlock(1024, true, false, QString()));

    const MediaFileState state = mediaFileStateFromUpdate(file);
    QCOMPARE(state.size, 4096);
    QCOMPARE(state.downloaded, 1024);
    QCOMPARE(progressPercent(state), 25);

    // A non-positive expected_size also falls back to the old field.
    QJsonObject zeroExpected = fileUpdate(7, 0, 512, true, false);
    zeroExpected.insert(QStringLiteral("size"), 2048);
    QCOMPARE(mediaFileStateFromUpdate(zeroExpected).size, 2048);
}

void MediaFileStateTest::missingSizesStayUnknown()
{
    QJsonObject file;
    file.insert(QStringLiteral("id"), 7);
    file.insert(QStringLiteral("local"), localBlock(0, false, false, QString()));

    const MediaFileState state = mediaFileStateFromUpdate(file);
    QCOMPARE(state.size, 0);
    QCOMPARE(state.downloaded, 0);
    QCOMPARE(state.state, DownloadState::Idle);

    // No local block at all: nothing is known, but the id survives.
    QJsonObject bare;
    bare.insert(QStringLiteral("id"), 7);
    const MediaFileState bareState = mediaFileStateFromUpdate(bare);
    QCOMPARE(bareState.fileId, 7);
    QCOMPARE(bareState.state, DownloadState::Idle);
    QCOMPARE(bareState.size, 0);
}

void MediaFileStateTest::downloadedSizeIsClampedToTheKnownSize()
{
    const MediaFileState state = mediaFileStateFromUpdate(fileUpdate(7, 1000, 1500, true, false));
    QCOMPARE(state.downloaded, 1000);
    QCOMPARE(state.size, 1000);
    QCOMPARE(progressPercent(state), 99);
}

void MediaFileStateTest::negativeProgressIsTreatedAsZero()
{
    const MediaFileState state = mediaFileStateFromUpdate(fileUpdate(7, 1000, -5, false, false));
    QCOMPARE(state.downloaded, 0);
    QCOMPARE(state.state, DownloadState::Idle);
    QCOMPARE(progressPercent(state), 0);

    // A negative expected size is unknown, not a ceiling.
    const MediaFileState negativeSize =
        mediaFileStateFromUpdate(fileUpdate(7, -1000, 10, false, false));
    QCOMPARE(negativeSize.size, 0);
    QCOMPARE(negativeSize.downloaded, 10);
    QCOMPARE(negativeSize.state, DownloadState::Pending);
    QCOMPARE(progressPercent(negativeSize), 0);
}

void MediaFileStateTest::stringEncodedNumbersAreAccepted()
{
    QJsonObject local = localBlock(0, true, false, QString());
    local.insert(QStringLiteral("downloaded_size"), QStringLiteral("500"));

    QJsonObject file;
    file.insert(QStringLiteral("id"), QStringLiteral("42"));
    file.insert(QStringLiteral("expected_size"), QStringLiteral("2000"));
    file.insert(QStringLiteral("local"), local);

    const MediaFileState state = mediaFileStateFromUpdate(file);
    QCOMPARE(state.fileId, 42);
    QCOMPARE(state.size, 2000);
    QCOMPARE(state.downloaded, 500);
    QCOMPARE(state.state, DownloadState::Downloading);
    QCOMPARE(progressPercent(state), 25);
}

void MediaFileStateTest::missingIdYieldsAnEmptyState()
{
    QJsonObject file = fileUpdate(0, 1000, 1000, false, true, QStringLiteral("p"));
    const MediaFileState state = mediaFileStateFromUpdate(file);
    QCOMPARE(state.fileId, 0);
    QCOMPARE(state.state, DownloadState::Idle);
    QCOMPARE(state.size, 0);
    QCOMPARE(state.downloaded, 0);
    QVERIFY(state.path.isEmpty());

    const MediaFileState empty = mediaFileStateFromUpdate(QJsonObject());
    QCOMPARE(empty.fileId, 0);
    QCOMPARE(empty.state, DownloadState::Idle);
}

void MediaFileStateTest::outOfRangeIdYieldsAnEmptyState()
{
    const qint64 tooLarge = static_cast<qint64>(std::numeric_limits<int>::max()) + 1;

    QJsonObject oversized;
    oversized.insert(QStringLiteral("id"), tooLarge);
    QCOMPARE(mediaFileStateFromUpdate(oversized).fileId, 0);

    QJsonObject negative;
    negative.insert(QStringLiteral("id"), -3);
    QCOMPARE(mediaFileStateFromUpdate(negative).fileId, 0);
}

void MediaFileStateTest::pathIsOnlyExposedWhenReady()
{
    const QString path = QStringLiteral("C:/cache/file.bin");
    const QVector<QPair<MediaFileState, bool>> cases = {
        {mediaFileStateFromUpdate(fileUpdate(7, 100, 0, false, false, path)), false},
        {mediaFileStateFromUpdate(fileUpdate(7, 100, 40, false, false, path)), false},
        {mediaFileStateFromUpdate(fileUpdate(7, 100, 40, true, false, path)), false},
        {mediaFileStateFromUpdate(fileUpdate(7, 100, 100, false, true, path)), true},
    };

    for (const auto& [state, expectedPath] : cases) {
        if (expectedPath) {
            QCOMPARE(state.path, path);
        } else {
            QVERIFY2(state.path.isEmpty(), "an incomplete download has no usable path");
        }
    }
}

void MediaFileStateTest::readyIsHundredPercent()
{
    MediaFileState state;
    state.state = DownloadState::Ready;
    QCOMPARE(progressPercent(state), 100);

    // Even without a size, a finished file is complete.
    state.size = 0;
    state.downloaded = 0;
    QCOMPARE(progressPercent(state), 100);
}

void MediaFileStateTest::failedHasNoProgress()
{
    MediaFileState state;
    state.state = DownloadState::Failed;
    state.size = 100;
    state.downloaded = 60;
    QCOMPARE(progressPercent(state), 0);
}

void MediaFileStateTest::idleAndUnknownSizeHaveNoProgress()
{
    MediaFileState idle;
    idle.size = 100;
    idle.downloaded = 0;
    QCOMPARE(progressPercent(idle), 0);

    MediaFileState unknown;
    unknown.state = DownloadState::Downloading;
    unknown.size = 0;
    unknown.downloaded = 50;
    QCOMPARE(progressPercent(unknown), 0);

    MediaFileState negativeSize;
    negativeSize.state = DownloadState::Pending;
    negativeSize.size = -100;
    negativeSize.downloaded = 50;
    QCOMPARE(progressPercent(negativeSize), 0);
}

void MediaFileStateTest::downloadingProgressTruncatesDown()
{
    const MediaFileState half = mediaFileStateFromUpdate(fileUpdate(7, 200, 100, true, false));
    QCOMPARE(progressPercent(half), 50);

    const MediaFileState third = mediaFileStateFromUpdate(fileUpdate(7, 3, 1, true, false));
    QCOMPARE(progressPercent(third), 33);

    const MediaFileState twoThirds = mediaFileStateFromUpdate(fileUpdate(7, 3, 2, true, false));
    QCOMPARE(progressPercent(twoThirds), 66);

    const MediaFileState tiny = mediaFileStateFromUpdate(fileUpdate(7, 1000, 1, true, false));
    QCOMPARE(progressPercent(tiny), 0);
}

void MediaFileStateTest::runningTransferNeverLooksFinished()
{
    for (const DownloadState running : {DownloadState::Downloading, DownloadState::Pending}) {
        MediaFileState state;
        state.state = running;
        state.size = 1000;
        state.downloaded = 1000;
        QCOMPARE(progressPercent(state), 99);

        state.downloaded = 1500;
        QCOMPARE(progressPercent(state), 99);

        state.downloaded = 999;
        QCOMPARE(progressPercent(state), 99);

        state.downloaded = 990;
        QCOMPARE(progressPercent(state), 99);
    }
}

void MediaFileStateTest::pendingReportsItsPartialProgress()
{
    const MediaFileState state = mediaFileStateFromUpdate(fileUpdate(7, 400, 100, false, false));
    QCOMPARE(state.state, DownloadState::Pending);
    QCOMPARE(progressPercent(state), 25);
}

void MediaFileStateTest::hugeNumbersDoNotOverflow()
{
    // Exactly representable as a double, so QJsonValue keeps the value intact.
    const qint64 huge = qint64(1) << 60;
    const MediaFileState full = mediaFileStateFromUpdate(fileUpdate(7, huge, huge, true, false));
    QCOMPARE(full.size, huge);
    QCOMPARE(full.downloaded, huge);
    QCOMPARE(progressPercent(full), 99);

    const MediaFileState tiny = mediaFileStateFromUpdate(fileUpdate(7, huge, 1, true, false));
    QCOMPARE(progressPercent(tiny), 0);

    const MediaFileState ready = mediaFileStateFromUpdate(fileUpdate(7, huge, huge, false, true));
    QCOMPARE(ready.state, DownloadState::Ready);
    QCOMPARE(progressPercent(ready), 100);
}

QTEST_GUILESS_MAIN(MediaFileStateTest)
#include "tst_media_file_state.moc"
