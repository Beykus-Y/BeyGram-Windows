#include "telegram/MediaDownloadQueue.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class MediaDownloadQueueTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmptyWithDefaultCap();
    void setMaxConcurrentClampsToRange();
    void setMaxConcurrentEmitsOnlyOnRealChange();
    void setMaxConcurrentBelowActiveKeepsTransfersRunning();
    void enqueueRejectsNonPositiveIds();
    void enqueueRejectsDuplicates();
    void enqueueStartsHighestPriorityFirst();
    void enqueueKeepsFifoWithinSamePriority();
    void enqueueInsertsBeforeLowerPriorityOnly();
    void enqueueAcceptsIdAgainAfterItLeftTheQueue();
    void pendingReportsStartOrder();
    void activeReportsStartOrder();
    void startNextRespectsConcurrencyCap();
    void startNextRefusesEmptyQueue();
    void startNextMovesFrontOfPending();
    void cancelRemovesPendingEntry();
    void cancelRemovesActiveEntry();
    void cancelRefusesUnknownIds();
    void cancelActiveFreesASlot();
    void finishOnlyAppliesToActiveIds();
    void failRequeuesAtFrontOfPending();
    void failKeepsOriginalPriority();
    void clearEmptiesBothLists();
    void clearIsNoOpWhenEmpty();
    void changedTracksOnlyRealChanges();
};

void MediaDownloadQueueTest::startsEmptyWithDefaultCap()
{
    const MediaDownloadQueue queue;

    QCOMPARE(queue.maxConcurrent(), 3);
    QCOMPARE(queue.pendingCount(), 0);
    QCOMPARE(queue.activeCount(), 0);
    QVERIFY(queue.pending().isEmpty());
    QVERIFY(queue.active().isEmpty());
    QVERIFY(!queue.contains(1));
}

void MediaDownloadQueueTest::setMaxConcurrentClampsToRange()
{
    MediaDownloadQueue queue;

    queue.setMaxConcurrent(-5);
    QCOMPARE(queue.maxConcurrent(), 1);

    queue.setMaxConcurrent(0);
    QCOMPARE(queue.maxConcurrent(), 1);

    queue.setMaxConcurrent(1);
    QCOMPARE(queue.maxConcurrent(), 1);

    queue.setMaxConcurrent(5);
    QCOMPARE(queue.maxConcurrent(), 5);

    // The upper bound is inclusive and everything above it is cut down.
    queue.setMaxConcurrent(8);
    QCOMPARE(queue.maxConcurrent(), 8);

    queue.setMaxConcurrent(9);
    QCOMPARE(queue.maxConcurrent(), 8);

    queue.setMaxConcurrent(1000);
    QCOMPARE(queue.maxConcurrent(), 8);
}

void MediaDownloadQueueTest::setMaxConcurrentEmitsOnlyOnRealChange()
{
    MediaDownloadQueue queue;
    QSignalSpy spy(&queue, &MediaDownloadQueue::changed);

    queue.setMaxConcurrent(3);
    QCOMPARE(spy.count(), 0);

    queue.setMaxConcurrent(4);
    QCOMPARE(spy.count(), 1);

    queue.setMaxConcurrent(4);
    QCOMPARE(spy.count(), 1);

    // Both values clamp to 8, so the second call changes nothing.
    queue.setMaxConcurrent(8);
    QCOMPARE(spy.count(), 2);
    queue.setMaxConcurrent(50);
    QCOMPARE(spy.count(), 2);
}

void MediaDownloadQueueTest::setMaxConcurrentBelowActiveKeepsTransfersRunning()
{
    MediaDownloadQueue queue;
    queue.setMaxConcurrent(3);

    QVERIFY(queue.enqueue(1, 8));
    QVERIFY(queue.enqueue(2, 8));
    QVERIFY(queue.enqueue(3, 8));
    QVERIFY(queue.startNext());
    QVERIFY(queue.startNext());
    QCOMPARE(queue.activeCount(), 2);

    queue.setMaxConcurrent(1);
    QCOMPARE(queue.active(), QVector<int>({1, 2}));
    QCOMPARE(queue.pending(), QVector<int>({3}));

    // Full until the running transfers drain down to the new cap.
    QVERIFY(!queue.startNext());
    QVERIFY(queue.finish(1));
    QVERIFY(!queue.startNext());
    QVERIFY(queue.finish(2));
    QVERIFY(queue.startNext());
    QCOMPARE(queue.active(), QVector<int>({3}));
}

void MediaDownloadQueueTest::enqueueRejectsNonPositiveIds()
{
    MediaDownloadQueue queue;
    QSignalSpy spy(&queue, &MediaDownloadQueue::changed);

    QVERIFY(!queue.enqueue(0, 32));
    QVERIFY(!queue.enqueue(-1, 32));
    QVERIFY(!queue.enqueue(-1000, 8));

    QCOMPARE(queue.pendingCount(), 0);
    QCOMPARE(spy.count(), 0);
}

void MediaDownloadQueueTest::enqueueRejectsDuplicates()
{
    MediaDownloadQueue queue;
    queue.setMaxConcurrent(1);

    QVERIFY(queue.enqueue(7, 8));
    QVERIFY(!queue.enqueue(7, 32));

    // A duplicate must not move the entry or add a second copy.
    QCOMPARE(queue.pending(), QVector<int>({7}));
    QCOMPARE(queue.pendingCount(), 1);

    QVERIFY(queue.startNext());
    QVERIFY(!queue.enqueue(7, 8));
    QCOMPARE(queue.active(), QVector<int>({7}));
    QCOMPARE(queue.pendingCount(), 0);
}

void MediaDownloadQueueTest::enqueueStartsHighestPriorityFirst()
{
    MediaDownloadQueue queue;

    QVERIFY(queue.enqueue(10, 8));
    QVERIFY(queue.enqueue(11, 32));
    QVERIFY(queue.enqueue(12, 16));

    QCOMPARE(queue.pending(), QVector<int>({11, 12, 10}));
}

void MediaDownloadQueueTest::enqueueKeepsFifoWithinSamePriority()
{
    MediaDownloadQueue queue;

    QVERIFY(queue.enqueue(1, 16));
    QVERIFY(queue.enqueue(2, 16));
    QVERIFY(queue.enqueue(3, 16));

    QCOMPARE(queue.pending(), QVector<int>({1, 2, 3}));
}

void MediaDownloadQueueTest::enqueueInsertsBeforeLowerPriorityOnly()
{
    MediaDownloadQueue queue;

    QVERIFY(queue.enqueue(1, 8));
    QVERIFY(queue.enqueue(2, 32));
    QCOMPARE(queue.pending(), QVector<int>({2, 1}));

    // Equal priority goes behind the existing entry, not in front of it.
    QVERIFY(queue.enqueue(3, 32));
    QCOMPARE(queue.pending(), QVector<int>({2, 3, 1}));

    // A middle priority lands between the high and the low block.
    QVERIFY(queue.enqueue(4, 16));
    QCOMPARE(queue.pending(), QVector<int>({2, 3, 4, 1}));

    // The lowest priority lands at the very end.
    QVERIFY(queue.enqueue(5, 1));
    QCOMPARE(queue.pending(), QVector<int>({2, 3, 4, 1, 5}));
}

void MediaDownloadQueueTest::enqueueAcceptsIdAgainAfterItLeftTheQueue()
{
    MediaDownloadQueue queue;
    queue.setMaxConcurrent(1);

    QVERIFY(queue.enqueue(9, 8));
    QVERIFY(queue.startNext());
    QVERIFY(queue.finish(9));

    QVERIFY(queue.enqueue(9, 16));
    QCOMPARE(queue.pending(), QVector<int>({9}));
}

void MediaDownloadQueueTest::pendingReportsStartOrder()
{
    MediaDownloadQueue queue;
    queue.setMaxConcurrent(1);

    QVERIFY(queue.enqueue(1, 8));
    QVERIFY(queue.enqueue(2, 32));
    QVERIFY(queue.enqueue(3, 16));

    QCOMPARE(queue.pendingCount(), 3);

    QVERIFY(queue.startNext());
    QCOMPARE(queue.pending(), QVector<int>({3, 1}));
    QVERIFY(queue.finish(2));

    QVERIFY(queue.startNext());
    QCOMPARE(queue.pending(), QVector<int>({1}));
    QVERIFY(queue.finish(3));

    QVERIFY(queue.startNext());
    QVERIFY(queue.pending().isEmpty());
    QCOMPARE(queue.active(), QVector<int>({1}));
}

void MediaDownloadQueueTest::activeReportsStartOrder()
{
    MediaDownloadQueue queue;
    queue.setMaxConcurrent(3);

    QVERIFY(queue.enqueue(4, 1));
    QVERIFY(queue.enqueue(5, 32));
    QVERIFY(queue.enqueue(6, 16));

    QVERIFY(queue.startNext());
    QVERIFY(queue.startNext());
    QVERIFY(queue.startNext());

    QCOMPARE(queue.active(), QVector<int>({5, 6, 4}));
    QCOMPARE(queue.activeCount(), 3);
}

void MediaDownloadQueueTest::startNextRespectsConcurrencyCap()
{
    MediaDownloadQueue queue;
    queue.setMaxConcurrent(2);

    QVERIFY(queue.enqueue(1, 8));
    QVERIFY(queue.enqueue(2, 8));
    QVERIFY(queue.enqueue(3, 8));

    QVERIFY(queue.startNext());
    QVERIFY(queue.startNext());

    // Two transfers are already running, so the third has to wait.
    QVERIFY(!queue.startNext());
    QCOMPARE(queue.activeCount(), 2);
    QCOMPARE(queue.pendingCount(), 1);

    QVERIFY(queue.finish(1));
    QVERIFY(queue.startNext());
    QCOMPARE(queue.active(), QVector<int>({2, 3}));
}

void MediaDownloadQueueTest::startNextRefusesEmptyQueue()
{
    MediaDownloadQueue queue;
    QSignalSpy spy(&queue, &MediaDownloadQueue::changed);

    QVERIFY(!queue.startNext());
    QVERIFY(!queue.startNext());

    QCOMPARE(queue.activeCount(), 0);
    QCOMPARE(spy.count(), 0);
}

void MediaDownloadQueueTest::startNextMovesFrontOfPending()
{
    MediaDownloadQueue queue;
    QVERIFY(queue.enqueue(1, 8));
    QVERIFY(queue.enqueue(2, 32));

    QSignalSpy spy(&queue, &MediaDownloadQueue::changed);
    QVERIFY(queue.startNext());

    QCOMPARE(spy.count(), 1);
    QCOMPARE(queue.active(), QVector<int>({2}));
    QCOMPARE(queue.pending(), QVector<int>({1}));
    QVERIFY(queue.contains(2));
}

void MediaDownloadQueueTest::cancelRemovesPendingEntry()
{
    MediaDownloadQueue queue;
    QVERIFY(queue.enqueue(1, 8));
    QVERIFY(queue.enqueue(2, 32));
    QVERIFY(queue.enqueue(3, 16));

    QSignalSpy spy(&queue, &MediaDownloadQueue::changed);
    QVERIFY(queue.cancel(3));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(queue.pending(), QVector<int>({2, 1}));
    QVERIFY(!queue.contains(3));
}

void MediaDownloadQueueTest::cancelRemovesActiveEntry()
{
    MediaDownloadQueue queue;
    QVERIFY(queue.enqueue(1, 16));
    QVERIFY(queue.enqueue(2, 8));
    QVERIFY(queue.startNext());

    QSignalSpy spy(&queue, &MediaDownloadQueue::changed);
    QVERIFY(queue.cancel(1));

    QCOMPARE(spy.count(), 1);
    QVERIFY(queue.active().isEmpty());
    QCOMPARE(queue.pending(), QVector<int>({2}));
    QVERIFY(!queue.contains(1));
}

void MediaDownloadQueueTest::cancelRefusesUnknownIds()
{
    MediaDownloadQueue queue;
    QVERIFY(queue.enqueue(1, 8));

    QSignalSpy spy(&queue, &MediaDownloadQueue::changed);

    QVERIFY(!queue.cancel(2));
    QVERIFY(!queue.cancel(0));
    QVERIFY(!queue.cancel(-3));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(queue.pending(), QVector<int>({1}));
}

void MediaDownloadQueueTest::cancelActiveFreesASlot()
{
    MediaDownloadQueue queue;
    queue.setMaxConcurrent(1);

    QVERIFY(queue.enqueue(1, 8));
    QVERIFY(queue.enqueue(2, 8));
    QVERIFY(queue.startNext());
    QVERIFY(!queue.startNext());

    QVERIFY(queue.cancel(1));
    QVERIFY(queue.startNext());
    QCOMPARE(queue.active(), QVector<int>({2}));
}

void MediaDownloadQueueTest::finishOnlyAppliesToActiveIds()
{
    MediaDownloadQueue queue;
    queue.setMaxConcurrent(2);

    QVERIFY(queue.enqueue(1, 8));
    QVERIFY(queue.enqueue(2, 8));
    QVERIFY(queue.startNext());

    // Pending ids are not finished, they are cancelled.
    QVERIFY(!queue.finish(2));
    QVERIFY(!queue.finish(99));
    QVERIFY(!queue.finish(0));

    QCOMPARE(queue.pendingCount(), 1);

    QVERIFY(queue.finish(1));
    QVERIFY(!queue.finish(1));
    QVERIFY(!queue.contains(1));
    QCOMPARE(queue.activeCount(), 0);
}

void MediaDownloadQueueTest::failRequeuesAtFrontOfPending()
{
    MediaDownloadQueue queue;
    queue.setMaxConcurrent(2);

    QVERIFY(queue.enqueue(1, 8));
    QVERIFY(queue.enqueue(2, 8));
    QVERIFY(queue.enqueue(3, 8));
    QVERIFY(queue.startNext());
    QVERIFY(queue.startNext());
    QCOMPARE(queue.active(), QVector<int>({1, 2}));

    QSignalSpy spy(&queue, &MediaDownloadQueue::changed);
    QVERIFY(queue.fail(1));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(queue.active(), QVector<int>({2}));
    // The retry is in front of the entry that was already waiting.
    QCOMPARE(queue.pending(), QVector<int>({1, 3}));
    QVERIFY(queue.contains(1));

    // A failed id is no longer active, so it cannot fail twice.
    QVERIFY(!queue.fail(1));

    QVERIFY(queue.startNext());
    QCOMPARE(queue.active(), QVector<int>({2, 1}));
    QCOMPARE(queue.pending(), QVector<int>({3}));
}

void MediaDownloadQueueTest::failKeepsOriginalPriority()
{
    MediaDownloadQueue queue;
    queue.setMaxConcurrent(1);

    QVERIFY(queue.enqueue(7, 5));
    QVERIFY(queue.startNext());
    QVERIFY(queue.fail(7));

    // A more urgent arrival still goes before the retry.
    QVERIFY(queue.enqueue(8, 9));
    QCOMPARE(queue.pending(), QVector<int>({8, 7}));

    // A less urgent one still stays behind it.
    QVERIFY(queue.enqueue(9, 1));
    QCOMPARE(queue.pending(), QVector<int>({8, 7, 9}));
}

void MediaDownloadQueueTest::clearEmptiesBothLists()
{
    MediaDownloadQueue queue;
    queue.setMaxConcurrent(2);

    QVERIFY(queue.enqueue(1, 8));
    QVERIFY(queue.enqueue(2, 16));
    QVERIFY(queue.enqueue(3, 32));
    QVERIFY(queue.startNext());

    QSignalSpy spy(&queue, &MediaDownloadQueue::changed);
    queue.clear();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(queue.pendingCount(), 0);
    QCOMPARE(queue.activeCount(), 0);
    QVERIFY(!queue.contains(1));
    QVERIFY(!queue.contains(2));

    // The concurrency cap survives a clear.
    QCOMPARE(queue.maxConcurrent(), 2);
}

void MediaDownloadQueueTest::clearIsNoOpWhenEmpty()
{
    MediaDownloadQueue queue;
    QSignalSpy spy(&queue, &MediaDownloadQueue::changed);

    queue.clear();
    QCOMPARE(spy.count(), 0);

    QVERIFY(queue.enqueue(1, 8));
    QVERIFY(queue.cancel(1));
    queue.clear();
    QCOMPARE(spy.count(), 2);
}

void MediaDownloadQueueTest::changedTracksOnlyRealChanges()
{
    MediaDownloadQueue queue;
    QSignalSpy spy(&queue, &MediaDownloadQueue::changed);

    QVERIFY(!queue.enqueue(0, 8));          // no-op
    QVERIFY(queue.enqueue(1, 8));           // 1
    QVERIFY(!queue.enqueue(1, 32));         // no-op
    queue.setMaxConcurrent(3);              // no-op
    queue.setMaxConcurrent(4);              // 2
    QVERIFY(queue.startNext());             // 3
    QVERIFY(!queue.startNext());            // no-op
    QVERIFY(queue.finish(1));               // 4
    QVERIFY(!queue.finish(1));              // no-op
    QVERIFY(queue.enqueue(2, 16));          // 5
    QVERIFY(queue.cancel(2));               // 6
    QVERIFY(!queue.cancel(2));              // no-op
    queue.clear();                          // no-op
    QVERIFY(queue.enqueue(3, 1));           // 7
    queue.clear();                          // 8

    QCOMPARE(spy.count(), 8);
}

QTEST_GUILESS_MAIN(MediaDownloadQueueTest)
#include "tst_media_download_queue.moc"
