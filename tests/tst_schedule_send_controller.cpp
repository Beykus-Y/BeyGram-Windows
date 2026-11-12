#include "telegram/ScheduleSendController.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

QDateTime utc(int year, int month, int day, int hour = 0, int minute = 0)
{
    return QDateTime(QDate(year, month, day), QTime(hour, minute), Qt::UTC);
}

} // namespace

class ScheduleSendControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsUnset();
    void setChatStoresAnyNonZeroId();
    void setChatZeroClearsTheTarget();
    void setChatEmitsOnlyOnChange();
    void setDateTimeStoresTheGivenMoment();
    void setDateTimeAcceptsInvalidAsCleared();
    void setDateTimeEmitsOnlyOnChange();
    void validRequiresChatAndTime();
    void validRejectsPastDates();
    void validAcceptsFarFutureDates();
    void sendDateConvertsToUnixSeconds();
    void sendDateIgnoresTheTimeZoneOfTheMoment();
    void sendDateSaturatesAtInt32Max();
    void sendDateIsZeroWhenInvalid();
    void resetClearsChatAndTime();
    void resetKeepsOnlineOnly();
    void resetEmitsOnlyOnChange();
    void setOnlineOnlyTogglesTheFlag();
    void setOnlineOnlyEmitsOnlyOnChange();
    void emitsChangedForEveryRealChange();
};

void ScheduleSendControllerTest::startsUnset()
{
    const ScheduleSendController controller;

    QCOMPARE(controller.chatId(), qint64(0));
    QVERIFY(!controller.dateTime().isValid());
    QVERIFY(!controller.valid());
    QCOMPARE(controller.sendDate(), 0);
    QVERIFY(!controller.onlineOnly());
}

void ScheduleSendControllerTest::setChatStoresAnyNonZeroId()
{
    ScheduleSendController controller;

    controller.setChat(42);
    QCOMPARE(controller.chatId(), qint64(42));

    // Group and channel ids are negative in TDLib, so they are valid targets.
    controller.setChat(-1001234567890LL);
    QCOMPARE(controller.chatId(), qint64(-1001234567890LL));

    // A large private-chat id must not be truncated.
    controller.setChat(4242424242LL);
    QCOMPARE(controller.chatId(), qint64(4242424242LL));
}

void ScheduleSendControllerTest::setChatZeroClearsTheTarget()
{
    ScheduleSendController controller;
    controller.setChat(42);
    controller.setDateTime(utc(2100, 1, 1, 12, 0));
    QVERIFY(controller.valid());

    controller.setChat(0);
    QCOMPARE(controller.chatId(), qint64(0));
    QVERIFY(!controller.valid());
    QCOMPARE(controller.sendDate(), 0);

    // Clearing the target must not forget the chosen time.
    QVERIFY(controller.dateTime().isValid());
}

void ScheduleSendControllerTest::setChatEmitsOnlyOnChange()
{
    ScheduleSendController controller;
    QSignalSpy spy(&controller, &ScheduleSendController::changed);

    controller.setChat(42);   // 1
    controller.setChat(42);   // no-op
    controller.setChat(-7);   // 2
    controller.setChat(-7);   // no-op
    controller.setChat(0);    // 3
    controller.setChat(0);    // no-op

    QCOMPARE(spy.count(), 3);
}

void ScheduleSendControllerTest::setDateTimeStoresTheGivenMoment()
{
    ScheduleSendController controller;
    const QDateTime when = utc(2100, 1, 1, 12, 30);

    controller.setDateTime(when);

    QVERIFY(controller.dateTime().isValid());
    QCOMPARE(controller.dateTime(), when);
    QCOMPARE(controller.dateTime().timeSpec(), Qt::UTC);
    QCOMPARE(controller.dateTime().date(), QDate(2100, 1, 1));
    QCOMPARE(controller.dateTime().time(), QTime(12, 30));

    // The time can be set before the chat exists.
    QCOMPARE(controller.chatId(), qint64(0));
    QVERIFY(!controller.valid());
}

void ScheduleSendControllerTest::setDateTimeAcceptsInvalidAsCleared()
{
    ScheduleSendController controller;
    controller.setChat(42);
    controller.setDateTime(utc(2100, 1, 1, 12, 0));
    QVERIFY(controller.valid());

    controller.setDateTime(QDateTime());
    QVERIFY(!controller.dateTime().isValid());
    QVERIFY(!controller.valid());
    QCOMPARE(controller.sendDate(), 0);

    // The chat is an independent field and survives the cleared time.
    QCOMPARE(controller.chatId(), qint64(42));
}

void ScheduleSendControllerTest::setDateTimeEmitsOnlyOnChange()
{
    ScheduleSendController controller;
    QSignalSpy spy(&controller, &ScheduleSendController::changed);

    const QDateTime when = utc(2100, 1, 1, 12, 0);
    controller.setDateTime(when);        // 1
    controller.setDateTime(when);        // no-op

    // The same instant in another time spec is not a new delivery time.
    controller.setDateTime(when.toOffsetFromUtc(5 * 3600));  // no-op
    QCOMPARE(spy.count(), 1);

    controller.setDateTime(when.addSecs(60));  // 2
    QCOMPARE(spy.count(), 2);

    controller.setDateTime(QDateTime());  // 3
    controller.setDateTime(QDateTime());  // no-op, still unset
    QCOMPARE(spy.count(), 3);
}

void ScheduleSendControllerTest::validRequiresChatAndTime()
{
    ScheduleSendController controller;
    const QDateTime future = utc(2100, 1, 1, 12, 0);

    QVERIFY(!controller.valid());

    controller.setChat(42);
    QVERIFY(!controller.valid());

    controller.setDateTime(future);
    QVERIFY(controller.valid());

    controller.setChat(0);
    QVERIFY(!controller.valid());

    controller.setChat(42);
    QVERIFY(controller.valid());

    controller.setDateTime(QDateTime());
    QVERIFY(!controller.valid());
}

void ScheduleSendControllerTest::validRejectsPastDates()
{
    ScheduleSendController controller;
    controller.setChat(42);

    controller.setDateTime(utc(2000, 1, 1, 0, 0));
    QVERIFY(!controller.valid());
    QCOMPARE(controller.sendDate(), 0);

    // The stored moment is kept even though it can no longer be sent.
    QCOMPARE(controller.dateTime(), utc(2000, 1, 1, 0, 0));

    controller.setDateTime(utc(2100, 1, 1, 0, 0));
    QVERIFY(controller.valid());
}

void ScheduleSendControllerTest::validAcceptsFarFutureDates()
{
    ScheduleSendController controller;
    controller.setChat(42);

    // Far enough ahead that no plausible clock skew can invalidate the check.
    controller.setDateTime(QDateTime::currentDateTimeUtc().addYears(10));
    QVERIFY(controller.valid());

    controller.setDateTime(QDateTime::currentDateTimeUtc().addSecs(3600));
    QVERIFY(controller.valid());
}

void ScheduleSendControllerTest::sendDateConvertsToUnixSeconds()
{
    ScheduleSendController controller;
    controller.setChat(42);

    // The moment is relative to the clock so the date stays schedulable, but
    // the expectation comes from the very same QDateTime: the value must be
    // Unix seconds, not milliseconds and not shifted by the time zone.
    const QDateTime when = QDateTime::currentDateTimeUtc().addDays(60);
    controller.setDateTime(when);
    QCOMPARE(controller.sendDate(), static_cast<qint32>(when.toSecsSinceEpoch()));

    // Sub-second precision is dropped, not rounded up.
    const QDateTime withMillis = when.addMSecs(900);
    controller.setDateTime(withMillis);
    QCOMPARE(controller.sendDate(),
             static_cast<qint32>(withMillis.toSecsSinceEpoch()));
}

void ScheduleSendControllerTest::sendDateIgnoresTheTimeZoneOfTheMoment()
{
    ScheduleSendController controller;
    controller.setChat(42);

    const QDateTime when = QDateTime::currentDateTimeUtc().addDays(60);
    controller.setDateTime(when);
    const qint32 utcSeconds = controller.sendDate();

    controller.setDateTime(when.toOffsetFromUtc(5 * 3600));
    QCOMPARE(controller.sendDate(), utcSeconds);

    controller.setDateTime(when.toLocalTime());
    QCOMPARE(controller.sendDate(), utcSeconds);
}

void ScheduleSendControllerTest::sendDateSaturatesAtInt32Max()
{
    ScheduleSendController controller;
    controller.setChat(42);

    // 2200 is far beyond the int32 range TDLib uses for send_date; the value
    // saturates instead of wrapping into a negative date.
    controller.setDateTime(utc(2200, 1, 1, 0, 0));
    QVERIFY(controller.valid());
    QCOMPARE(controller.sendDate(), std::numeric_limits<qint32>::max());
}

void ScheduleSendControllerTest::sendDateIsZeroWhenInvalid()
{
    ScheduleSendController controller;

    // No chat, no time.
    QCOMPARE(controller.sendDate(), 0);

    // Time only.
    controller.setDateTime(utc(2100, 1, 1, 0, 0));
    QCOMPARE(controller.sendDate(), 0);

    // Chat only.
    controller.setDateTime(QDateTime());
    controller.setChat(42);
    QCOMPARE(controller.sendDate(), 0);

    // Chat and a past time.
    controller.setDateTime(utc(2000, 1, 1, 0, 0));
    QCOMPARE(controller.sendDate(), 0);
}

void ScheduleSendControllerTest::resetClearsChatAndTime()
{
    ScheduleSendController controller;
    controller.setChat(-1001234567890LL);
    controller.setDateTime(utc(2100, 1, 1, 12, 0));
    QVERIFY(controller.valid());

    controller.reset();

    QCOMPARE(controller.chatId(), qint64(0));
    QVERIFY(!controller.dateTime().isValid());
    QVERIFY(!controller.valid());
    QCOMPARE(controller.sendDate(), 0);
}

void ScheduleSendControllerTest::resetKeepsOnlineOnly()
{
    ScheduleSendController controller;
    controller.setOnlineOnly(true);
    controller.setChat(42);
    controller.setDateTime(utc(2100, 1, 1, 12, 0));

    controller.reset();

    QVERIFY(controller.onlineOnly());
    QCOMPARE(controller.chatId(), qint64(0));
    QVERIFY(!controller.dateTime().isValid());

    controller.setOnlineOnly(false);
    QVERIFY(!controller.onlineOnly());
}

void ScheduleSendControllerTest::resetEmitsOnlyOnChange()
{
    ScheduleSendController controller;
    QSignalSpy spy(&controller, &ScheduleSendController::changed);

    // Already empty: nothing to clear.
    controller.reset();
    QCOMPARE(spy.count(), 0);

    controller.setChat(42);   // 1
    controller.reset();       // 2
    controller.reset();       // no-op
    QCOMPARE(spy.count(), 2);

    controller.setDateTime(utc(2100, 1, 1, 12, 0));  // 3
    controller.reset();                              // 4
    QCOMPARE(spy.count(), 4);
}

void ScheduleSendControllerTest::setOnlineOnlyTogglesTheFlag()
{
    ScheduleSendController controller;
    QVERIFY(!controller.onlineOnly());

    controller.setOnlineOnly(true);
    QVERIFY(controller.onlineOnly());

    controller.setOnlineOnly(false);
    QVERIFY(!controller.onlineOnly());
}

void ScheduleSendControllerTest::setOnlineOnlyEmitsOnlyOnChange()
{
    ScheduleSendController controller;
    QSignalSpy spy(&controller, &ScheduleSendController::changed);

    controller.setOnlineOnly(false);  // no-op, default is off
    QCOMPARE(spy.count(), 0);

    controller.setOnlineOnly(true);   // 1
    controller.setOnlineOnly(true);   // no-op
    QCOMPARE(spy.count(), 1);

    controller.setOnlineOnly(false);  // 2
    QCOMPARE(spy.count(), 2);
}

void ScheduleSendControllerTest::emitsChangedForEveryRealChange()
{
    ScheduleSendController controller;
    QSignalSpy spy(&controller, &ScheduleSendController::changed);

    const QDateTime when = utc(2100, 1, 1, 12, 0);

    controller.setChat(42);           // 1
    controller.setDateTime(when);     // 2
    controller.setOnlineOnly(true);   // 3
    controller.setChat(42);           // no-op
    controller.setDateTime(when);     // no-op
    controller.setOnlineOnly(true);   // no-op
    controller.setDateTime(when.addSecs(1));  // 4
    controller.reset();               // 5
    controller.setChat(0);            // no-op, already cleared
    controller.setOnlineOnly(false);  // 6

    QCOMPARE(spy.count(), 6);
    QVERIFY(!controller.valid());
    QVERIFY(!controller.onlineOnly());
}

QTEST_GUILESS_MAIN(ScheduleSendControllerTest)
#include "tst_schedule_send_controller.moc"
