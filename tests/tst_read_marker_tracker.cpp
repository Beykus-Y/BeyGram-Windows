#include "telegram/ReadMarkerTracker.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class ReadMarkerTrackerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsAtZero();
    void applyInboxIsMonotonic();
    void applyOutboxIsMonotonic();
    void ignoresNonPositiveIds();
    void shouldMarkComparesAgainstCommittedInbox();
    void commitAdvancesInbox();
    void commitIsMonotonic();
    void resetZerosBothMarkers();
    void emitsChangedOnlyOnRealChanges();
};

void ReadMarkerTrackerTest::startsAtZero()
{
    const ReadMarkerTracker tracker;

    QCOMPARE(tracker.inbox(), qint64(0));
    QCOMPARE(tracker.outbox(), qint64(0));
    QVERIFY(!tracker.shouldMark(0));
}

void ReadMarkerTrackerTest::applyInboxIsMonotonic()
{
    ReadMarkerTracker tracker;

    tracker.applyInbox(10);
    QCOMPARE(tracker.inbox(), qint64(10));

    tracker.applyInbox(25);
    QCOMPARE(tracker.inbox(), qint64(25));

    // Older markers never move the value backwards.
    tracker.applyInbox(24);
    tracker.applyInbox(1);
    QCOMPARE(tracker.inbox(), qint64(25));

    // The outbox marker is independent of the inbox marker.
    QCOMPARE(tracker.outbox(), qint64(0));
}

void ReadMarkerTrackerTest::applyOutboxIsMonotonic()
{
    ReadMarkerTracker tracker;

    tracker.applyOutbox(7);
    QCOMPARE(tracker.outbox(), qint64(7));

    tracker.applyOutbox(9);
    QCOMPARE(tracker.outbox(), qint64(9));

    tracker.applyOutbox(8);
    QCOMPARE(tracker.outbox(), qint64(9));

    // The inbox marker is independent of the outbox marker.
    QCOMPARE(tracker.inbox(), qint64(0));
}

void ReadMarkerTrackerTest::ignoresNonPositiveIds()
{
    ReadMarkerTracker tracker;

    tracker.applyInbox(0);
    tracker.applyInbox(-1);
    tracker.applyOutbox(0);
    tracker.applyOutbox(-42);
    tracker.commit(0);
    tracker.commit(-5);

    QCOMPARE(tracker.inbox(), qint64(0));
    QCOMPARE(tracker.outbox(), qint64(0));

    // A non-positive id must not reset a marker that already advanced.
    tracker.applyInbox(3);
    tracker.commit(5);
    tracker.applyInbox(0);
    tracker.applyOutbox(-1);
    tracker.commit(-1);
    QCOMPARE(tracker.inbox(), qint64(5));
    QCOMPARE(tracker.outbox(), qint64(0));
}

void ReadMarkerTrackerTest::shouldMarkComparesAgainstCommittedInbox()
{
    ReadMarkerTracker tracker;

    // Nothing read yet: any positive visible id needs a mark.
    QVERIFY(tracker.shouldMark(1));
    QVERIFY(tracker.shouldMark(500));
    QVERIFY(!tracker.shouldMark(0));
    QVERIFY(!tracker.shouldMark(-3));

    tracker.applyInbox(500);
    QVERIFY(!tracker.shouldMark(500));
    QVERIFY(!tracker.shouldMark(499));
    QVERIFY(tracker.shouldMark(501));

    // The outbox marker must not satisfy shouldMark.
    tracker.applyOutbox(900);
    QVERIFY(tracker.shouldMark(501));
    QVERIFY(!tracker.shouldMark(500));
}

void ReadMarkerTrackerTest::commitAdvancesInbox()
{
    ReadMarkerTracker tracker;

    tracker.commit(12);
    QCOMPARE(tracker.inbox(), qint64(12));
    QVERIFY(!tracker.shouldMark(12));
    QVERIFY(tracker.shouldMark(13));

    tracker.commit(30);
    QCOMPARE(tracker.inbox(), qint64(30));
    QVERIFY(!tracker.shouldMark(30));
    QVERIFY(tracker.shouldMark(31));
}

void ReadMarkerTrackerTest::commitIsMonotonic()
{
    ReadMarkerTracker tracker;

    tracker.commit(30);
    tracker.commit(20);
    tracker.commit(29);
    QCOMPARE(tracker.inbox(), qint64(30));
    QVERIFY(tracker.shouldMark(31));
    QVERIFY(!tracker.shouldMark(30));
}

void ReadMarkerTrackerTest::resetZerosBothMarkers()
{
    ReadMarkerTracker tracker;

    tracker.applyInbox(15);
    tracker.applyOutbox(22);
    tracker.commit(18);

    tracker.reset();

    QCOMPARE(tracker.inbox(), qint64(0));
    QCOMPARE(tracker.outbox(), qint64(0));
    QVERIFY(tracker.shouldMark(1));

    // Both markers accept new values after a reset.
    tracker.applyInbox(4);
    tracker.applyOutbox(6);
    QCOMPARE(tracker.inbox(), qint64(4));
    QCOMPARE(tracker.outbox(), qint64(6));
}

void ReadMarkerTrackerTest::emitsChangedOnlyOnRealChanges()
{
    ReadMarkerTracker tracker;
    QSignalSpy spy(&tracker, &ReadMarkerTracker::changed);

    tracker.applyInbox(5);
    QCOMPARE(spy.count(), 1);

    // Re-applying the same id, an older id, or a non-positive id changes
    // nothing and must stay silent.
    tracker.applyInbox(5);
    tracker.applyInbox(4);
    tracker.applyInbox(0);
    QCOMPARE(spy.count(), 1);

    tracker.applyOutbox(2);
    QCOMPARE(spy.count(), 2);
    tracker.applyOutbox(1);
    QCOMPARE(spy.count(), 2);

    tracker.commit(6);
    QCOMPARE(spy.count(), 3);
    tracker.commit(6);
    QCOMPARE(spy.count(), 3);

    tracker.reset();
    QCOMPARE(spy.count(), 4);
    // Already zero: reset is a no-op.
    tracker.reset();
    QCOMPARE(spy.count(), 4);
}

QTEST_GUILESS_MAIN(ReadMarkerTrackerTest)
#include "tst_read_marker_tracker.moc"
