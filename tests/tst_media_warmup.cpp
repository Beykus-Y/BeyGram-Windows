#include "telegram/MediaWarmup.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class MediaWarmupTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmptyWithDefaultBudget();
    void setBudgetClampsToRange();
    void setBudgetEmitsOnlyOnRealChange();
    void requestRejectsNonPositiveIds();
    void requestQueuesAndEmits();
    void requestKeepsHighestPriority();
    void requestLowerPriorityIsNoOp();
    void thumbnailRequestUpgradesToAvatar();
    void avatarRanksAboveThumbnailAtEqualPriority();
    void priorityRanksAboveKind();
    void nextBatchKeepsFifoWithinARank();
    void nextBatchRespectsBudget();
    void nextBatchRemovesOnlyReturnedIds();
    void nextBatchIsSilentWithoutWork();
    void nextBatchOrdersByUpgradedPriority();
    void batchedIdIsInFlightNotQueued();
    void requestOnInFlightIdIsNoOp();
    void markReadyRemovesQueuedEntry();
    void markReadyAcceptsInFlightId();
    void markReadyIsNoOpForUnknownIds();
    void markFailedDropsQueuedEntry();
    void markFailedDropsInFlightId();
    void markFailedIsNoOpForUnknownIds();
    void markFailedInvalidatesReady();
    void requestOnReadyFileIsNoOp();
    void clearEmptiesQueueAndReady();
    void clearIsSilentWhenEmpty();
    void changedTracksOnlyRealChanges();
};

void MediaWarmupTest::startsEmptyWithDefaultBudget()
{
    MediaWarmup warmup;

    QCOMPARE(warmup.budget(), 12);
    QCOMPARE(warmup.queuedCount(), 0);
    QVERIFY(!warmup.isReady(1));
    QVERIFY(!warmup.isQueued(1));
    QVERIFY(!warmup.isReady(0));
    QVERIFY(!warmup.isQueued(-3));
    QVERIFY(warmup.nextBatch().isEmpty());
}

void MediaWarmupTest::setBudgetClampsToRange()
{
    MediaWarmup warmup;

    warmup.setBudget(-5);
    QCOMPARE(warmup.budget(), 1);

    warmup.setBudget(0);
    QCOMPARE(warmup.budget(), 1);

    warmup.setBudget(1);
    QCOMPARE(warmup.budget(), 1);

    warmup.setBudget(12);
    QCOMPARE(warmup.budget(), 12);

    // The upper bound is inclusive and everything above it is cut down.
    warmup.setBudget(64);
    QCOMPARE(warmup.budget(), 64);

    warmup.setBudget(65);
    QCOMPARE(warmup.budget(), 64);

    warmup.setBudget(1000);
    QCOMPARE(warmup.budget(), 64);
}

void MediaWarmupTest::setBudgetEmitsOnlyOnRealChange()
{
    MediaWarmup warmup;
    QSignalSpy spy(&warmup, &MediaWarmup::changed);

    warmup.setBudget(12);
    QCOMPARE(spy.count(), 0);

    warmup.setBudget(13);
    QCOMPARE(spy.count(), 1);

    warmup.setBudget(13);
    QCOMPARE(spy.count(), 1);

    warmup.setBudget(64);
    QCOMPARE(spy.count(), 2);

    warmup.setBudget(65);
    QCOMPARE(spy.count(), 2);

    // Both of these clamp to 1, so the second call changes nothing.
    warmup.setBudget(0);
    QCOMPARE(spy.count(), 3);

    warmup.setBudget(-3);
    QCOMPARE(spy.count(), 3);
}

void MediaWarmupTest::requestRejectsNonPositiveIds()
{
    MediaWarmup warmup;
    QSignalSpy changedSpy(&warmup, &MediaWarmup::changed);
    QSignalSpy downloadSpy(&warmup, &MediaWarmup::downloadRequested);

    warmup.requestAvatar(0, 32);
    warmup.requestAvatar(-1, 32);
    warmup.requestThumbnail(0, 8);
    warmup.requestThumbnail(-1000, 8);

    QCOMPARE(warmup.queuedCount(), 0);
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(downloadSpy.count(), 0);
}

void MediaWarmupTest::requestQueuesAndEmits()
{
    MediaWarmup warmup;
    QSignalSpy changedSpy(&warmup, &MediaWarmup::changed);
    QSignalSpy downloadSpy(&warmup, &MediaWarmup::downloadRequested);

    warmup.requestAvatar(42, 9);

    QCOMPARE(warmup.queuedCount(), 1);
    QVERIFY(warmup.isQueued(42));
    QVERIFY(!warmup.isReady(42));
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(downloadSpy.count(), 1);
    QCOMPARE(downloadSpy.at(0).at(0).toInt(), 42);
    QCOMPARE(downloadSpy.at(0).at(1).toInt(), 9);
}

void MediaWarmupTest::requestKeepsHighestPriority()
{
    MediaWarmup warmup;
    QSignalSpy downloadSpy(&warmup, &MediaWarmup::downloadRequested);

    warmup.requestAvatar(1, 3);
    warmup.requestThumbnail(1, 9);

    // One entry only, carrying the higher priority.
    QCOMPARE(warmup.queuedCount(), 1);
    QCOMPARE(downloadSpy.count(), 2);
    QCOMPARE(downloadSpy.at(1).at(0).toInt(), 1);
    QCOMPARE(downloadSpy.at(1).at(1).toInt(), 9);

    // The upgraded id now outranks an unrelated mid-priority avatar.
    warmup.requestAvatar(2, 5);
    QCOMPARE(warmup.nextBatch(), QVector<int>({1, 2}));
}

void MediaWarmupTest::requestLowerPriorityIsNoOp()
{
    MediaWarmup warmup;
    warmup.requestAvatar(1, 9);

    QSignalSpy changedSpy(&warmup, &MediaWarmup::changed);
    QSignalSpy downloadSpy(&warmup, &MediaWarmup::downloadRequested);

    warmup.requestThumbnail(1, 4);
    warmup.requestAvatar(1, 9);

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(downloadSpy.count(), 0);
    QCOMPARE(warmup.queuedCount(), 1);
}

void MediaWarmupTest::thumbnailRequestUpgradesToAvatar()
{
    MediaWarmup warmup;
    warmup.requestThumbnail(1, 5);
    warmup.requestThumbnail(2, 5);

    QSignalSpy changedSpy(&warmup, &MediaWarmup::changed);
    QSignalSpy downloadSpy(&warmup, &MediaWarmup::downloadRequested);

    // Same priority, but the kind changes, which is a real upgrade.
    warmup.requestAvatar(1, 5);

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(downloadSpy.count(), 1);
    QCOMPARE(downloadSpy.at(0).at(0).toInt(), 1);
    QCOMPARE(downloadSpy.at(0).at(1).toInt(), 5);

    QCOMPARE(warmup.nextBatch(), QVector<int>({1, 2}));
}

void MediaWarmupTest::avatarRanksAboveThumbnailAtEqualPriority()
{
    MediaWarmup warmup;

    warmup.requestThumbnail(10, 8);
    warmup.requestAvatar(11, 8);
    warmup.requestThumbnail(12, 8);
    warmup.requestAvatar(13, 8);

    QCOMPARE(warmup.nextBatch(), QVector<int>({11, 13, 10, 12}));
}

void MediaWarmupTest::priorityRanksAboveKind()
{
    MediaWarmup warmup;

    warmup.requestAvatar(1, 3);
    warmup.requestThumbnail(2, 9);

    QCOMPARE(warmup.nextBatch(), QVector<int>({2, 1}));
}

void MediaWarmupTest::nextBatchKeepsFifoWithinARank()
{
    MediaWarmup warmup;

    warmup.requestAvatar(4, 4);
    warmup.requestAvatar(2, 4);
    warmup.requestThumbnail(9, 4);
    warmup.requestThumbnail(7, 4);

    QCOMPARE(warmup.nextBatch(), QVector<int>({4, 2, 9, 7}));
}

void MediaWarmupTest::nextBatchRespectsBudget()
{
    MediaWarmup warmup;
    warmup.setBudget(2);

    for (int fileId = 1; fileId <= 5; ++fileId)
        warmup.requestThumbnail(fileId, 8);

    QCOMPARE(warmup.nextBatch(), QVector<int>({1, 2}));
    QCOMPARE(warmup.queuedCount(), 3);

    QCOMPARE(warmup.nextBatch(), QVector<int>({3, 4}));
    QCOMPARE(warmup.queuedCount(), 1);

    QCOMPARE(warmup.nextBatch(), QVector<int>({5}));
    QCOMPARE(warmup.queuedCount(), 0);

    QVERIFY(warmup.nextBatch().isEmpty());
}

void MediaWarmupTest::nextBatchRemovesOnlyReturnedIds()
{
    MediaWarmup warmup;
    warmup.setBudget(1);

    warmup.requestThumbnail(1, 8);
    warmup.requestThumbnail(2, 8);

    QCOMPARE(warmup.nextBatch(), QVector<int>({1}));

    QVERIFY(!warmup.isQueued(1));
    QVERIFY(!warmup.isReady(1));
    QVERIFY(warmup.isQueued(2));
    QCOMPARE(warmup.queuedCount(), 1);
}

void MediaWarmupTest::nextBatchIsSilentWithoutWork()
{
    MediaWarmup warmup;
    QSignalSpy changedSpy(&warmup, &MediaWarmup::changed);
    QSignalSpy downloadSpy(&warmup, &MediaWarmup::downloadRequested);

    QVERIFY(warmup.nextBatch().isEmpty());
    QCOMPARE(changedSpy.count(), 0);

    warmup.requestThumbnail(1, 8);
    QCOMPARE(changedSpy.count(), 1);

    changedSpy.clear();
    downloadSpy.clear();

    QCOMPARE(warmup.nextBatch(), QVector<int>({1}));
    QCOMPARE(changedSpy.count(), 1);
    // Handing out work is the pull side, not another request.
    QCOMPARE(downloadSpy.count(), 0);

    QVERIFY(warmup.nextBatch().isEmpty());
    QCOMPARE(changedSpy.count(), 1);
}

void MediaWarmupTest::nextBatchOrdersByUpgradedPriority()
{
    MediaWarmup warmup;

    warmup.requestThumbnail(1, 1);
    warmup.requestThumbnail(2, 1);
    warmup.requestThumbnail(3, 1);

    // Upgrades change the rank but keep the original FIFO position.
    warmup.requestThumbnail(3, 9);
    warmup.requestThumbnail(2, 5);

    QCOMPARE(warmup.nextBatch(), QVector<int>({3, 2, 1}));
}

void MediaWarmupTest::batchedIdIsInFlightNotQueued()
{
    MediaWarmup warmup;

    warmup.requestAvatar(7, 2);
    QCOMPARE(warmup.nextBatch(), QVector<int>({7}));

    // nextBatch() took the id out of the queue; it is in flight until it is
    // resolved, so a second batch has nothing to hand out.
    QVERIFY(!warmup.isQueued(7));
    QVERIFY(!warmup.isReady(7));
    QCOMPARE(warmup.queuedCount(), 0);
    QVERIFY(warmup.nextBatch().isEmpty());

    // Resolving it as ready keeps it out of the queue for good.
    warmup.markReady(7);
    QVERIFY(warmup.isReady(7));
    QVERIFY(!warmup.isQueued(7));
}

void MediaWarmupTest::requestOnInFlightIdIsNoOp()
{
    MediaWarmup warmup;
    warmup.requestThumbnail(8, 3);
    QCOMPARE(warmup.nextBatch(), QVector<int>({8}));

    QSignalSpy changedSpy(&warmup, &MediaWarmup::changed);
    QSignalSpy downloadSpy(&warmup, &MediaWarmup::downloadRequested);

    warmup.requestAvatar(8, 32);

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(downloadSpy.count(), 0);
    QCOMPARE(warmup.queuedCount(), 0);
    QVERIFY(!warmup.isReady(8));
}

void MediaWarmupTest::markReadyRemovesQueuedEntry()
{
    MediaWarmup warmup;
    warmup.requestAvatar(5, 9);

    QSignalSpy spy(&warmup, &MediaWarmup::changed);
    warmup.markReady(5);

    QCOMPARE(spy.count(), 1);
    QVERIFY(warmup.isReady(5));
    QVERIFY(!warmup.isQueued(5));
    QCOMPARE(warmup.queuedCount(), 0);

    warmup.markReady(5);
    QCOMPARE(spy.count(), 1);
}

void MediaWarmupTest::markReadyAcceptsInFlightId()
{
    MediaWarmup warmup;
    warmup.requestThumbnail(6, 2);
    QCOMPARE(warmup.nextBatch(), QVector<int>({6}));

    QSignalSpy spy(&warmup, &MediaWarmup::changed);
    warmup.markReady(6);

    QCOMPARE(spy.count(), 1);
    QVERIFY(warmup.isReady(6));
    QVERIFY(!warmup.isQueued(6));
}

void MediaWarmupTest::markReadyIsNoOpForUnknownIds()
{
    MediaWarmup warmup;
    QSignalSpy spy(&warmup, &MediaWarmup::changed);

    warmup.markReady(99);
    warmup.markReady(0);
    warmup.markReady(-2);

    QCOMPARE(spy.count(), 0);
    QVERIFY(!warmup.isReady(99));
}

void MediaWarmupTest::markFailedDropsQueuedEntry()
{
    MediaWarmup warmup;
    warmup.requestThumbnail(6, 4);

    QSignalSpy spy(&warmup, &MediaWarmup::changed);
    warmup.markFailed(6);

    QCOMPARE(spy.count(), 1);
    QVERIFY(!warmup.isQueued(6));
    QVERIFY(!warmup.isReady(6));
    QCOMPARE(warmup.queuedCount(), 0);

    warmup.markFailed(6);
    QCOMPARE(spy.count(), 1);

    // A failed id is requestable again.
    warmup.requestAvatar(6, 4);
    QVERIFY(warmup.isQueued(6));
}

void MediaWarmupTest::markFailedDropsInFlightId()
{
    MediaWarmup warmup;
    warmup.requestAvatar(6, 4);
    QCOMPARE(warmup.nextBatch(), QVector<int>({6}));

    QSignalSpy spy(&warmup, &MediaWarmup::changed);
    warmup.markFailed(6);

    QCOMPARE(spy.count(), 1);
    QVERIFY(!warmup.isQueued(6));
    QVERIFY(!warmup.isReady(6));

    warmup.requestAvatar(6, 4);
    QVERIFY(warmup.isQueued(6));
}

void MediaWarmupTest::markFailedIsNoOpForUnknownIds()
{
    MediaWarmup warmup;
    QSignalSpy spy(&warmup, &MediaWarmup::changed);

    warmup.markFailed(123);
    warmup.markFailed(0);
    warmup.markFailed(-1);

    QCOMPARE(spy.count(), 0);
}

void MediaWarmupTest::markFailedInvalidatesReady()
{
    MediaWarmup warmup;
    warmup.requestAvatar(8, 4);
    warmup.markReady(8);
    QVERIFY(warmup.isReady(8));

    QSignalSpy spy(&warmup, &MediaWarmup::changed);
    warmup.markFailed(8);

    QCOMPARE(spy.count(), 1);
    QVERIFY(!warmup.isReady(8));

    // The failure forgot the id, so it can be warmed again.
    warmup.requestAvatar(8, 4);
    QVERIFY(warmup.isQueued(8));
}

void MediaWarmupTest::requestOnReadyFileIsNoOp()
{
    MediaWarmup warmup;
    warmup.requestAvatar(3, 4);
    warmup.markReady(3);

    QSignalSpy changedSpy(&warmup, &MediaWarmup::changed);
    QSignalSpy downloadSpy(&warmup, &MediaWarmup::downloadRequested);

    warmup.requestAvatar(3, 32);
    warmup.requestThumbnail(3, 32);

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(downloadSpy.count(), 0);
    QCOMPARE(warmup.queuedCount(), 0);
    QVERIFY(warmup.isReady(3));
}

void MediaWarmupTest::clearEmptiesQueueAndReady()
{
    MediaWarmup warmup;
    warmup.setBudget(3);

    warmup.requestAvatar(1, 9);
    warmup.requestThumbnail(2, 4);
    warmup.requestThumbnail(3, 4);
    QCOMPARE(warmup.nextBatch(), QVector<int>({1, 2, 3}));
    warmup.markReady(1);
    warmup.requestAvatar(4, 1);

    QSignalSpy spy(&warmup, &MediaWarmup::changed);
    warmup.clear();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(warmup.queuedCount(), 0);
    QVERIFY(!warmup.isQueued(4));
    QVERIFY(!warmup.isReady(1));

    // The in-flight set is forgotten too, so 2 is unknown again.
    warmup.markReady(2);
    QVERIFY(!warmup.isReady(2));

    // The budget survives a clear.
    QCOMPARE(warmup.budget(), 3);
}

void MediaWarmupTest::clearIsSilentWhenEmpty()
{
    MediaWarmup warmup;
    QSignalSpy spy(&warmup, &MediaWarmup::changed);

    warmup.clear();
    QCOMPARE(spy.count(), 0);

    warmup.requestThumbnail(1, 1);
    warmup.markFailed(1);
    QCOMPARE(spy.count(), 2);

    warmup.clear();
    QCOMPARE(spy.count(), 2);
}

void MediaWarmupTest::changedTracksOnlyRealChanges()
{
    MediaWarmup warmup;
    QSignalSpy spy(&warmup, &MediaWarmup::changed);

    warmup.requestAvatar(0, 8);            // no-op: invalid id
    warmup.setBudget(12);                  // no-op: default
    warmup.requestAvatar(1, 8);            // 1
    warmup.requestThumbnail(1, 4);         // no-op: lower priority
    warmup.requestThumbnail(1, 16);        // 2
    warmup.requestAvatar(1, 16);           // no-op: already avatar
    warmup.setBudget(4);                   // 3
    warmup.setBudget(4);                   // no-op
    QCOMPARE(warmup.nextBatch(), QVector<int>({1})); // 4
    QVERIFY(warmup.nextBatch().isEmpty()); // no-op: empty queue
    warmup.requestThumbnail(2, 1);         // 5
    warmup.markFailed(2);                  // 6
    warmup.markFailed(2);                  // no-op: unknown
    warmup.markReady(1);                   // 7: 1 was in flight
    warmup.markReady(1);                   // no-op: already ready
    warmup.requestAvatar(1, 32);           // no-op: ready
    warmup.markFailed(99);                 // no-op: unknown
    warmup.clear();                        // 8
    warmup.clear();                        // no-op

    QCOMPARE(spy.count(), 8);
}

QTEST_GUILESS_MAIN(MediaWarmupTest)
#include "tst_media_warmup.moc"
