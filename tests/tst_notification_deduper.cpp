#include "telegram/NotificationDeduper.h"

#include <QSignalSpy>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

class NotificationDeduperTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithDefaultWindowAndNoRecords();
    void clampsWindowToSupportedRange();
    void setWindowSecondsEmitsOnlyOnChange();
    void firstMessageNotifiesAndIsTracked();
    void repeatedMessageInsideWindowIsDropped();
    void differentMessageInsideWindowNotifies();
    void perMessageDedupeAllowsAThenBThenA();
    void windowIsPerChat();
    void zeroWindowDisablesDeduplication();
    void rejectsNonPositiveIds();
    void rejectedIdsLeaveExistingRecordUntouched();
    void expiredRecordsArePrunedLazily();
    void shrinkingTheWindowKeepsRecordsUntilTouched();
    void backwardsClockDoesNotSuppress();
    void extremeTimestampsStayExpired();
    void forgetRemovesOnlyThatChat();
    void forgetIgnoresUnknownChats();
    void clearDropsEveryRecord();
    void clearOnEmptyStateIsANoOp();
    void trackedCountCountsChatsNotMessages();
    void emitsChangedOncePerObservableTransition();
};

void NotificationDeduperTest::startsWithDefaultWindowAndNoRecords()
{
    const NotificationDeduper deduper;

    QCOMPARE(deduper.windowSeconds(), 5);
    QCOMPARE(deduper.trackedCount(), 0);
}

void NotificationDeduperTest::clampsWindowToSupportedRange()
{
    NotificationDeduper deduper;

    deduper.setWindowSeconds(-1);
    QCOMPARE(deduper.windowSeconds(), 0);

    deduper.setWindowSeconds(3601);
    QCOMPARE(deduper.windowSeconds(), 3600);

    deduper.setWindowSeconds(std::numeric_limits<int>::min());
    QCOMPARE(deduper.windowSeconds(), 0);

    deduper.setWindowSeconds(std::numeric_limits<int>::max());
    QCOMPARE(deduper.windowSeconds(), 3600);

    deduper.setWindowSeconds(17);
    QCOMPARE(deduper.windowSeconds(), 17);
}

void NotificationDeduperTest::setWindowSecondsEmitsOnlyOnChange()
{
    NotificationDeduper deduper;
    QSignalSpy spy(&deduper, &NotificationDeduper::changed);

    // 5 is the default, so asking for it changes nothing.
    deduper.setWindowSeconds(5);
    QCOMPARE(spy.count(), 0);

    deduper.setWindowSeconds(6);
    QCOMPARE(spy.count(), 1);

    deduper.setWindowSeconds(6);
    QCOMPARE(spy.count(), 1);

    // Out-of-range values clamp; only the clamped result matters.
    deduper.setWindowSeconds(4000);
    QCOMPARE(deduper.windowSeconds(), 3600);
    QCOMPARE(spy.count(), 2);

    deduper.setWindowSeconds(9999);
    QCOMPARE(spy.count(), 2);

    deduper.setWindowSeconds(-5);
    QCOMPARE(deduper.windowSeconds(), 0);
    QCOMPARE(spy.count(), 3);
}

void NotificationDeduperTest::firstMessageNotifiesAndIsTracked()
{
    NotificationDeduper deduper;
    QSignalSpy spy(&deduper, &NotificationDeduper::changed);

    QVERIFY(deduper.shouldNotify(7, 42, 100));
    QCOMPARE(deduper.trackedCount(), 1);
    QCOMPARE(spy.count(), 1);
}

void NotificationDeduperTest::repeatedMessageInsideWindowIsDropped()
{
    NotificationDeduper deduper;
    QSignalSpy spy(&deduper, &NotificationDeduper::changed);

    QVERIFY(deduper.shouldNotify(7, 42, 100));
    QCOMPARE(spy.count(), 1);

    // Replays at 101..104 stay inside the 5 second window and change nothing.
    QVERIFY(!deduper.shouldNotify(7, 42, 101));
    QVERIFY(!deduper.shouldNotify(7, 42, 104));
    QCOMPARE(deduper.trackedCount(), 1);
    QCOMPARE(spy.count(), 1);

    // The window runs from the first delivery, not the last replay: 105 is
    // exactly five seconds after 100, so the record has expired.
    QVERIFY(deduper.shouldNotify(7, 42, 105));
    QCOMPARE(deduper.trackedCount(), 1);
    QCOMPARE(spy.count(), 1);

    // The re-delivery started a fresh window at 105.
    QVERIFY(!deduper.shouldNotify(7, 42, 109));
    QVERIFY(deduper.shouldNotify(7, 42, 110));
}

void NotificationDeduperTest::differentMessageInsideWindowNotifies()
{
    NotificationDeduper deduper;

    QVERIFY(deduper.shouldNotify(7, 1, 100));
    QVERIFY(deduper.shouldNotify(7, 2, 100));
    QVERIFY(deduper.shouldNotify(7, 3, 101));

    QCOMPARE(deduper.trackedCount(), 1);
}

void NotificationDeduperTest::perMessageDedupeAllowsAThenBThenA()
{
    NotificationDeduper deduper;

    // Only the last id is remembered, so A returning after B is a new arrival
    // for the deduper rather than a repeat.
    QVERIFY(deduper.shouldNotify(7, 1, 100));
    QVERIFY(deduper.shouldNotify(7, 2, 101));
    QVERIFY(deduper.shouldNotify(7, 1, 102));

    QCOMPARE(deduper.trackedCount(), 1);
}

void NotificationDeduperTest::windowIsPerChat()
{
    NotificationDeduper deduper;

    // The same message id in two chats is two independent records.
    QVERIFY(deduper.shouldNotify(7, 42, 100));
    QVERIFY(deduper.shouldNotify(8, 42, 100));
    QCOMPARE(deduper.trackedCount(), 2);

    QVERIFY(!deduper.shouldNotify(7, 42, 101));
    QVERIFY(!deduper.shouldNotify(8, 42, 101));
}

void NotificationDeduperTest::zeroWindowDisablesDeduplication()
{
    NotificationDeduper deduper;
    deduper.setWindowSeconds(0);
    QSignalSpy spy(&deduper, &NotificationDeduper::changed);

    QVERIFY(deduper.shouldNotify(7, 42, 100));
    QCOMPARE(spy.count(), 1);

    // Nothing is ever inside a zero-length window, even at the same second.
    QVERIFY(deduper.shouldNotify(7, 42, 100));
    QVERIFY(deduper.shouldNotify(7, 42, 99));
    QCOMPARE(deduper.trackedCount(), 1);
    QCOMPARE(spy.count(), 1);
}

void NotificationDeduperTest::rejectsNonPositiveIds()
{
    NotificationDeduper deduper;
    QSignalSpy spy(&deduper, &NotificationDeduper::changed);

    QVERIFY(!deduper.shouldNotify(0, 42, 100));
    QVERIFY(!deduper.shouldNotify(-3, 42, 100));
    QVERIFY(!deduper.shouldNotify(7, 0, 100));
    QVERIFY(!deduper.shouldNotify(7, -9, 100));

    QCOMPARE(deduper.trackedCount(), 0);
    QCOMPARE(spy.count(), 0);
}

void NotificationDeduperTest::rejectedIdsLeaveExistingRecordUntouched()
{
    NotificationDeduper deduper;
    QSignalSpy spy(&deduper, &NotificationDeduper::changed);

    QVERIFY(deduper.shouldNotify(7, 42, 100));
    QCOMPARE(spy.count(), 1);

    QVERIFY(!deduper.shouldNotify(7, 0, 200));
    QVERIFY(!deduper.shouldNotify(0, 42, 200));
    QVERIFY(!deduper.shouldNotify(-7, 42, 200));

    QCOMPARE(deduper.trackedCount(), 1);
    QCOMPARE(spy.count(), 1);

    // The original record still suppresses its message inside the window.
    QVERIFY(!deduper.shouldNotify(7, 42, 101));
}

void NotificationDeduperTest::expiredRecordsArePrunedLazily()
{
    NotificationDeduper deduper;

    QVERIFY(deduper.shouldNotify(7, 100, 1000));
    QVERIFY(deduper.shouldNotify(8, 200, 1000));
    QCOMPARE(deduper.trackedCount(), 2);

    // Both records are now stale, but nothing sweeps them until their chat is
    // seen again, so the count does not drop on its own.
    QCOMPARE(deduper.trackedCount(), 2);

    // Chat 7 is touched: its stale record is replaced in place, not appended.
    QVERIFY(deduper.shouldNotify(7, 101, 2000));
    QCOMPARE(deduper.trackedCount(), 2);

    // The replacement started a fresh window for 101.
    QVERIFY(!deduper.shouldNotify(7, 101, 2004));

    // The old id is no longer the chat's record, so it notifies again too.
    QVERIFY(deduper.shouldNotify(7, 100, 2005));
    QCOMPARE(deduper.trackedCount(), 2);
}

void NotificationDeduperTest::shrinkingTheWindowKeepsRecordsUntilTouched()
{
    NotificationDeduper deduper;
    deduper.setWindowSeconds(3600);

    QVERIFY(deduper.shouldNotify(7, 42, 1000));
    QCOMPARE(deduper.trackedCount(), 1);

    // The record is now far outside a 5 second window, but the setter does not
    // prune: the next call for the chat decides.
    deduper.setWindowSeconds(5);
    QCOMPARE(deduper.trackedCount(), 1);

    QVERIFY(deduper.shouldNotify(7, 43, 2000));
    QVERIFY(!deduper.shouldNotify(7, 43, 2004));
    QVERIFY(deduper.shouldNotify(7, 43, 2005));
}

void NotificationDeduperTest::backwardsClockDoesNotSuppress()
{
    NotificationDeduper deduper;

    QVERIFY(deduper.shouldNotify(7, 42, 1000));

    // A timestamp earlier than the record means the clock moved backwards. The
    // record is treated as stale instead of suppressing until the clock
    // catches up.
    QVERIFY(deduper.shouldNotify(7, 42, 999));

    // The new observation starts the window.
    QVERIFY(!deduper.shouldNotify(7, 42, 1000));
    QVERIFY(!deduper.shouldNotify(7, 42, 1003));
    QVERIFY(deduper.shouldNotify(7, 42, 1004));
}

void NotificationDeduperTest::extremeTimestampsStayExpired()
{
    NotificationDeduper deduper;
    constexpr qint64 lowest = std::numeric_limits<qint64>::min();
    constexpr qint64 highest = std::numeric_limits<qint64>::max();

    QVERIFY(deduper.shouldNotify(7, 42, lowest));

    // The distance between the two extremes overflows signed 64-bit math; the
    // record is still ancient, so it must not be mistaken for a live one.
    QVERIFY(deduper.shouldNotify(7, 42, highest));
    QVERIFY(!deduper.shouldNotify(7, 42, highest));
}

void NotificationDeduperTest::forgetRemovesOnlyThatChat()
{
    NotificationDeduper deduper;

    QVERIFY(deduper.shouldNotify(7, 42, 100));
    QVERIFY(deduper.shouldNotify(8, 42, 100));
    QVERIFY(deduper.shouldNotify(9, 42, 100));

    QSignalSpy spy(&deduper, &NotificationDeduper::changed);
    deduper.forget(8);

    QCOMPARE(deduper.trackedCount(), 2);
    QCOMPARE(spy.count(), 1);

    // The same message for the forgotten chat is a first arrival again.
    QVERIFY(deduper.shouldNotify(8, 42, 100));
    QCOMPARE(deduper.trackedCount(), 3);
    QCOMPARE(spy.count(), 2);

    // The neighbours were untouched and still suppress their duplicates.
    QVERIFY(!deduper.shouldNotify(7, 42, 101));
    QVERIFY(!deduper.shouldNotify(9, 42, 101));
}

void NotificationDeduperTest::forgetIgnoresUnknownChats()
{
    NotificationDeduper deduper;
    QSignalSpy spy(&deduper, &NotificationDeduper::changed);

    deduper.forget(42);
    deduper.forget(0);
    deduper.forget(-1);

    QCOMPARE(deduper.trackedCount(), 0);
    QCOMPARE(spy.count(), 0);

    // Repeating a removal of a chat that is gone is still a no-op.
    QVERIFY(deduper.shouldNotify(7, 42, 100));
    deduper.forget(7);
    QCOMPARE(spy.count(), 2);

    deduper.forget(7);
    QCOMPARE(spy.count(), 2);
}

void NotificationDeduperTest::clearDropsEveryRecord()
{
    NotificationDeduper deduper;

    QVERIFY(deduper.shouldNotify(7, 42, 100));
    QVERIFY(deduper.shouldNotify(8, 43, 100));
    QCOMPARE(deduper.trackedCount(), 2);

    QSignalSpy spy(&deduper, &NotificationDeduper::changed);
    deduper.clear();

    QCOMPARE(deduper.trackedCount(), 0);
    QCOMPARE(spy.count(), 1);

    // Nothing is remembered, so the same message notifies again.
    QVERIFY(deduper.shouldNotify(7, 42, 100));
    QCOMPARE(deduper.trackedCount(), 1);
    QCOMPARE(spy.count(), 2);
}

void NotificationDeduperTest::clearOnEmptyStateIsANoOp()
{
    NotificationDeduper deduper;
    QSignalSpy spy(&deduper, &NotificationDeduper::changed);

    deduper.clear();
    deduper.clear();

    QCOMPARE(deduper.trackedCount(), 0);
    QCOMPARE(spy.count(), 0);
}

void NotificationDeduperTest::trackedCountCountsChatsNotMessages()
{
    NotificationDeduper deduper;

    QVERIFY(deduper.shouldNotify(7, 1, 100));
    QVERIFY(deduper.shouldNotify(7, 2, 101));
    QVERIFY(deduper.shouldNotify(7, 3, 102));
    QCOMPARE(deduper.trackedCount(), 1);

    QVERIFY(deduper.shouldNotify(8, 4, 102));
    QCOMPARE(deduper.trackedCount(), 2);
}

void NotificationDeduperTest::emitsChangedOncePerObservableTransition()
{
    NotificationDeduper deduper;
    QSignalSpy spy(&deduper, &NotificationDeduper::changed);

    // Rejected ids record nothing.
    QVERIFY(!deduper.shouldNotify(0, 1, 100));
    QCOMPARE(spy.count(), 0);

    // A first chat grows the tracked set.
    QVERIFY(deduper.shouldNotify(7, 42, 100));
    QCOMPARE(spy.count(), 1);

    // A duplicate and a different message in the same chat leave the set size
    // alone, so neither repaints an observer.
    QVERIFY(!deduper.shouldNotify(7, 42, 101));
    QVERIFY(deduper.shouldNotify(7, 43, 102));
    QCOMPARE(spy.count(), 1);

    // A second chat grows the set again.
    QVERIFY(deduper.shouldNotify(8, 42, 102));
    QCOMPARE(spy.count(), 2);

    // Forgetting an unknown chat is a no-op; forgetting a tracked one shrinks
    // the set.
    deduper.forget(9);
    QCOMPARE(spy.count(), 2);
    deduper.forget(7);
    QCOMPARE(spy.count(), 3);

    // Clearing a non-empty set shrinks it; clearing an empty one is a no-op.
    deduper.clear();
    QCOMPARE(spy.count(), 4);
    deduper.clear();
    QCOMPARE(spy.count(), 4);

    // Only a real window move emits.
    deduper.setWindowSeconds(10);
    QCOMPARE(spy.count(), 5);
    deduper.setWindowSeconds(10);
    QCOMPARE(spy.count(), 5);
}

QTEST_GUILESS_MAIN(NotificationDeduperTest)
#include "tst_notification_deduper.moc"
