#include "telegram/StealthSchedule.h"

#include <QChar>
#include <QSignalSpy>
#include <QString>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

constexpr int Day = 24 * 60;

int at(int hour, int minute = 0)
{
    return hour * 60 + minute;
}

/// The separator of the window text: space, en dash, space.
QString dash()
{
    return QStringLiteral(" ") + QChar(0x2013) + QStringLiteral(" ");
}

QString windowText(int startHour, int startMinute, int endHour, int endMinute)
{
    const auto clock = [](int hour, int minute) {
        return QString::number(hour).rightJustified(2, QLatin1Char('0'))
            + QLatin1Char(':')
            + QString::number(minute).rightJustified(2, QLatin1Char('0'));
    };

    return clock(startHour, startMinute) + dash() + clock(endHour, endMinute);
}

/// Installs an enabled window from clock times, the way the settings screen does.
void setWindow(StealthSchedule& schedule, int startHour, int startMinute, int endHour, int endMinute)
{
    schedule.setWindow(at(startHour, startMinute), at(endHour, endMinute));
    schedule.setEnabled(true);
}

} // namespace

class StealthScheduleTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithNoWindow();
    void setWindowClampsBothBounds();
    void setWindowEmitsChangedOnlyForRealChange();
    void setEnabledEmitsChangedOnlyForRealChange();
    void enabledSwitchDoesNotTouchTheBounds();
    void equalBoundsStayOffWhileEnabled();
    void activeAtIncludesTheStartAndExcludesTheEnd();
    void activeAtWrapsAroundMidnight();
    void disabledScheduleIsNeverActive();
    void activeAtRejectsMinutesOutsideTheDay();
    void minutesUntilStartCountsToTheNextStart();
    void minutesUntilStartIsNegativeWhileActive();
    void minutesUntilStartIsNegativeWhenOffOrInvalid();
    void minutesUntilStartAlwaysLandsOnTheStart();
    void windowTextFormatsTheWindow();
    void windowTextIsOffWithoutAWindow();
    void resetRestoresDefaults();
    void resetIsSilentWhenAlreadyDefault();
    void resetEmitsOnceWhenOnlyTheSwitchMoved();
};

void StealthScheduleTest::startsWithNoWindow()
{
    const StealthSchedule schedule;

    QCOMPARE(schedule.startMinutes(), 0);
    QCOMPARE(schedule.endMinutes(), 0);
    QVERIFY2(!schedule.enabled(), "a fresh schedule is off");

    QVERIFY(!schedule.activeAt(at(0)));
    QVERIFY(!schedule.activeAt(at(12)));
    QVERIFY(!schedule.activeAt(at(23, 59)));
    QCOMPARE(schedule.minutesUntilStart(at(0)), -1);
    QCOMPARE(schedule.minutesUntilStart(at(12)), -1);
    QCOMPARE(schedule.windowText(), QStringLiteral("Off"));
}

void StealthScheduleTest::setWindowClampsBothBounds()
{
    StealthSchedule schedule;

    schedule.setWindow(-1, Day);
    QCOMPARE(schedule.startMinutes(), 0);
    QCOMPARE(schedule.endMinutes(), Day - 1);

    schedule.setWindow(-500, 5000);
    QCOMPARE(schedule.startMinutes(), 0);
    QCOMPARE(schedule.endMinutes(), Day - 1);

    schedule.setWindow(at(22), at(8));
    QCOMPARE(schedule.startMinutes(), at(22));
    QCOMPARE(schedule.endMinutes(), at(8));

    // The switch is the caller's separate decision, so a window write never
    // turns it on or off.
    QVERIFY(!schedule.enabled());
}

void StealthScheduleTest::setWindowEmitsChangedOnlyForRealChange()
{
    StealthSchedule schedule;
    QSignalSpy spy(&schedule, &StealthSchedule::changed);

    schedule.setWindow(at(22), at(8));
    QCOMPARE(spy.count(), 1);

    // Writing the stored bounds again is silent.
    schedule.setWindow(at(22), at(8));
    QCOMPARE(spy.count(), 1);

    // A negative start clamps to 00:00, which differs from the stored start, so
    // the clamped write is still a real change.
    schedule.setWindow(-30, at(8));
    QCOMPARE(spy.count(), 2);
    schedule.setWindow(-30, at(8));
    QCOMPARE(spy.count(), 2);

    schedule.setWindow(0, at(9));
    QCOMPARE(spy.count(), 3);
}

void StealthScheduleTest::setEnabledEmitsChangedOnlyForRealChange()
{
    StealthSchedule schedule;
    QSignalSpy spy(&schedule, &StealthSchedule::changed);

    schedule.setEnabled(true);
    QCOMPARE(spy.count(), 1);
    QVERIFY(schedule.enabled());

    schedule.setEnabled(true);
    QCOMPARE(spy.count(), 1);

    schedule.setEnabled(false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!schedule.enabled());

    schedule.setEnabled(false);
    QCOMPARE(spy.count(), 2);
}

void StealthScheduleTest::enabledSwitchDoesNotTouchTheBounds()
{
    StealthSchedule schedule;
    schedule.setWindow(at(22), at(8));

    schedule.setEnabled(true);
    QCOMPARE(schedule.startMinutes(), at(22));
    QCOMPARE(schedule.endMinutes(), at(8));

    schedule.setEnabled(false);
    QCOMPARE(schedule.startMinutes(), at(22));
    QCOMPARE(schedule.endMinutes(), at(8));
}

void StealthScheduleTest::equalBoundsStayOffWhileEnabled()
{
    StealthSchedule schedule;
    schedule.setWindow(at(9), at(9));
    schedule.setEnabled(true);

    // The switch is on, but a window that starts and ends at the same minute is
    // off: there is no interval to cover.
    QVERIFY(schedule.enabled());
    QVERIFY(!schedule.activeAt(at(0)));
    QVERIFY(!schedule.activeAt(at(9)));
    QVERIFY(!schedule.activeAt(at(23, 59)));
    QCOMPARE(schedule.minutesUntilStart(at(8)), -1);
    QCOMPARE(schedule.windowText(), QStringLiteral("Off"));
}

void StealthScheduleTest::activeAtIncludesTheStartAndExcludesTheEnd()
{
    StealthSchedule schedule;
    setWindow(schedule, 9, 0, 17, 0);

    QVERIFY(!schedule.activeAt(at(8, 59)));
    QVERIFY2(schedule.activeAt(at(9, 0)), "the start minute is inside the window");
    QVERIFY(schedule.activeAt(at(12, 0)));
    QVERIFY2(schedule.activeAt(at(16, 59)), "the minute before the end is still inside");
    QVERIFY2(!schedule.activeAt(at(17, 0)), "the end minute is outside the window");
    QVERIFY(!schedule.activeAt(at(23, 59)));
    QVERIFY(!schedule.activeAt(at(0)));
}

void StealthScheduleTest::activeAtWrapsAroundMidnight()
{
    StealthSchedule schedule;
    setWindow(schedule, 22, 0, 8, 0);

    QVERIFY(!schedule.activeAt(at(21, 59)));
    QVERIFY(schedule.activeAt(at(22, 0)));
    QVERIFY(schedule.activeAt(at(23, 59)));
    QVERIFY2(schedule.activeAt(at(0, 0)), "midnight is inside a wrapping window");
    QVERIFY(schedule.activeAt(at(7, 59)));
    QVERIFY2(!schedule.activeAt(at(8, 0)), "the end minute is outside the window");
    QVERIFY(!schedule.activeAt(at(12, 0)));
    QVERIFY(!schedule.activeAt(at(21, 0)));
}

void StealthScheduleTest::disabledScheduleIsNeverActive()
{
    StealthSchedule schedule;
    schedule.setWindow(at(22), at(8));
    QVERIFY(!schedule.enabled());

    for (int minute = 0; minute < Day; ++minute) {
        QVERIFY(!schedule.activeAt(minute));
        QCOMPARE(schedule.minutesUntilStart(minute), -1);
    }
}

void StealthScheduleTest::activeAtRejectsMinutesOutsideTheDay()
{
    StealthSchedule schedule;
    setWindow(schedule, 22, 0, 8, 0);

    QVERIFY(!schedule.activeAt(-1));
    QVERIFY(!schedule.activeAt(Day));
    QVERIFY(!schedule.activeAt(std::numeric_limits<int>::min()));
    QVERIFY(!schedule.activeAt(std::numeric_limits<int>::max()));
}

void StealthScheduleTest::minutesUntilStartCountsToTheNextStart()
{
    StealthSchedule schedule;
    setWindow(schedule, 22, 0, 8, 0);

    QCOMPARE(schedule.minutesUntilStart(at(21, 59)), 1);
    QCOMPARE(schedule.minutesUntilStart(at(21, 0)), 60);
    QCOMPARE(schedule.minutesUntilStart(at(12, 0)), 10 * 60);
    QCOMPARE(schedule.minutesUntilStart(at(8, 0)), 14 * 60);

    // An ordinary window counts within the same day.
    schedule.setWindow(at(9), at(17));
    QCOMPARE(schedule.minutesUntilStart(at(8, 59)), 1);
    QCOMPARE(schedule.minutesUntilStart(at(8, 0)), 60);
    QCOMPARE(schedule.minutesUntilStart(at(17, 0)), 16 * 60);
}

void StealthScheduleTest::minutesUntilStartIsNegativeWhileActive()
{
    StealthSchedule schedule;
    setWindow(schedule, 22, 0, 8, 0);

    QCOMPARE(schedule.minutesUntilStart(at(22, 0)), -1);
    QCOMPARE(schedule.minutesUntilStart(at(23, 30)), -1);
    QCOMPARE(schedule.minutesUntilStart(at(0, 0)), -1);
    QCOMPARE(schedule.minutesUntilStart(at(7, 59)), -1);
}

void StealthScheduleTest::minutesUntilStartIsNegativeWhenOffOrInvalid()
{
    StealthSchedule schedule;

    // A fresh schedule has no start to count to.
    QCOMPARE(schedule.minutesUntilStart(at(12)), -1);

    // A window that is written but not switched on is off.
    schedule.setWindow(at(22), at(8));
    QCOMPARE(schedule.minutesUntilStart(at(12)), -1);

    // Equal bounds are off even while the switch is on.
    schedule.setEnabled(true);
    schedule.setWindow(at(9), at(9));
    QCOMPARE(schedule.minutesUntilStart(at(8)), -1);

    // A value that does not name a minute of the day cannot be counted from.
    schedule.setWindow(at(22), at(8));
    QCOMPARE(schedule.minutesUntilStart(-1), -1);
    QCOMPARE(schedule.minutesUntilStart(Day), -1);
    QCOMPARE(schedule.minutesUntilStart(std::numeric_limits<int>::min()), -1);
    QCOMPARE(schedule.minutesUntilStart(std::numeric_limits<int>::max()), -1);
}

void StealthScheduleTest::minutesUntilStartAlwaysLandsOnTheStart()
{
    StealthSchedule schedule;
    setWindow(schedule, 22, 0, 8, 0);

    const auto check = [](const StealthSchedule& candidate, int start) {
        for (int minute = 0; minute < Day; ++minute) {
            const int wait = candidate.minutesUntilStart(minute);
            if (candidate.activeAt(minute)) {
                QCOMPARE(wait, -1);
                continue;
            }
            QVERIFY2(wait >= 1 && wait <= Day - 1, "a waiting time is inside one day");
            QCOMPARE((minute + wait) % Day, start);
        }
    };

    check(schedule, at(22));

    schedule.setWindow(at(9), at(17));
    check(schedule, at(9));
}

void StealthScheduleTest::windowTextFormatsTheWindow()
{
    StealthSchedule schedule;
    setWindow(schedule, 22, 0, 8, 0);

    QCOMPARE(schedule.windowText(), windowText(22, 0, 8, 0));
    QCOMPARE(schedule.windowText(), QStringLiteral("22:00") + dash() + QStringLiteral("08:00"));

    // Both bounds are zero-padded to two digits, so the text lines up.
    schedule.setWindow(at(9, 5), at(17, 30));
    QCOMPARE(schedule.windowText(), windowText(9, 5, 17, 30));
    QCOMPARE(schedule.windowText(), QStringLiteral("09:05") + dash() + QStringLiteral("17:30"));

    schedule.setWindow(0, Day - 1);
    QCOMPARE(schedule.windowText(), QStringLiteral("00:00") + dash() + QStringLiteral("23:59"));
}

void StealthScheduleTest::windowTextIsOffWithoutAWindow()
{
    StealthSchedule schedule;
    QCOMPARE(schedule.windowText(), QStringLiteral("Off"));

    // A configured window is still off while the switch is off.
    schedule.setWindow(at(22), at(8));
    QCOMPARE(schedule.windowText(), QStringLiteral("Off"));

    // Equal bounds have no interval to print, even while the switch is on.
    schedule.setEnabled(true);
    schedule.setWindow(at(9), at(9));
    QCOMPARE(schedule.windowText(), QStringLiteral("Off"));
}

void StealthScheduleTest::resetRestoresDefaults()
{
    StealthSchedule schedule;
    setWindow(schedule, 22, 0, 8, 0);

    QSignalSpy spy(&schedule, &StealthSchedule::changed);
    schedule.reset();
    QCOMPARE(spy.count(), 1);

    QCOMPARE(schedule.startMinutes(), 0);
    QCOMPARE(schedule.endMinutes(), 0);
    QVERIFY(!schedule.enabled());
    QVERIFY(!schedule.activeAt(at(0)));
    QCOMPARE(schedule.minutesUntilStart(at(12)), -1);
    QCOMPARE(schedule.windowText(), QStringLiteral("Off"));

    // The restored schedule behaves like a fresh one.
    schedule.setWindow(0, 0);
    QCOMPARE(spy.count(), 1);
}

void StealthScheduleTest::resetIsSilentWhenAlreadyDefault()
{
    StealthSchedule schedule;
    QSignalSpy spy(&schedule, &StealthSchedule::changed);

    // A fresh schedule is already at the defaults.
    schedule.reset();
    QCOMPARE(spy.count(), 0);

    // Writing the default values explicitly does not change anything either.
    schedule.setWindow(0, 0);
    schedule.setEnabled(false);
    QCOMPARE(spy.count(), 0);

    schedule.reset();
    QCOMPARE(spy.count(), 0);
}

void StealthScheduleTest::resetEmitsOnceWhenOnlyTheSwitchMoved()
{
    StealthSchedule schedule;
    QSignalSpy spy(&schedule, &StealthSchedule::changed);

    schedule.setEnabled(true);
    QCOMPARE(spy.count(), 1);

    schedule.reset();
    QCOMPARE(spy.count(), 2);
    QVERIFY(!schedule.enabled());
    QCOMPARE(schedule.startMinutes(), 0);
    QCOMPARE(schedule.endMinutes(), 0);
}

QTEST_GUILESS_MAIN(StealthScheduleTest)
#include "tst_stealth_schedule.moc"
