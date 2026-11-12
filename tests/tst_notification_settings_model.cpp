#include "telegram/NotificationSettingsModel.h"

#include <QSignalSpy>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

constexpr int MinutesPerDay = 24 * 60;

int at(int hour, int minute = 0)
{
    return hour * 60 + minute;
}

} // namespace

class NotificationSettingsModelTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultsAreEnabledWithSoundAndPreview();
    void defaultsHaveQuietHoursOff();
    void setEnabledEmitsChangedOnlyForRealChange();
    void setSoundEnabledEmitsChangedOnlyForRealChange();
    void setPreviewEnabledEmitsChangedOnlyForRealChange();
    void setDndEmitsChangedOnlyForRealChange();
    void setQuietHoursClampsBoundsToTheDay();
    void setQuietHoursEmitsChangedOnlyForRealChange();
    void quietHoursActiveTracksStartEqualsEnd();
    void inQuietHoursHandlesSameDayWindow();
    void inQuietHoursHandlesWindowAcrossMidnight();
    void inQuietHoursExcludesTheEndMinute();
    void inQuietHoursIgnoresTimeOutsideTheDay();
    void inQuietHoursIsOffWhenStartEqualsEnd();
    void shouldNotifyAllowsByDefault();
    void shouldNotifyRefusesWhenNotificationsAreOff();
    void shouldNotifyRefusesDuringDnd();
    void shouldNotifyRefusesForMutedChat();
    void shouldNotifyRefusesInsideQuietHours();
    void shouldNotifyAllowsOutsideTheWindow();
};

void NotificationSettingsModelTest::defaultsAreEnabledWithSoundAndPreview()
{
    const NotificationSettingsModel model;

    QVERIFY2(model.enabled(), "notifications are on by default");
    QVERIFY2(model.soundEnabled(), "the sound is on by default");
    QVERIFY2(model.previewEnabled(), "previews are on by default");
    QVERIFY2(!model.dnd(), "do-not-disturb is off by default");
}

void NotificationSettingsModelTest::defaultsHaveQuietHoursOff()
{
    const NotificationSettingsModel model;

    QCOMPARE(model.quietStartMinutes(), 0);
    QCOMPARE(model.quietEndMinutes(), 0);
    QVERIFY2(!model.quietHoursActive(), "equal bounds mean the window is disabled");
    QVERIFY(!model.inQuietHours(0));
    QVERIFY(!model.inQuietHours(at(3)));
    QVERIFY(!model.inQuietHours(at(23, 59)));
}

void NotificationSettingsModelTest::setEnabledEmitsChangedOnlyForRealChange()
{
    NotificationSettingsModel model;
    QSignalSpy spy(&model, &NotificationSettingsModel::changed);

    model.setEnabled(false);
    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.enabled());

    // Writing the stored value again is silent.
    model.setEnabled(false);
    QCOMPARE(spy.count(), 1);

    model.setEnabled(true);
    QCOMPARE(spy.count(), 2);
    QVERIFY(model.enabled());

    model.setEnabled(true);
    QCOMPARE(spy.count(), 2);
}

void NotificationSettingsModelTest::setSoundEnabledEmitsChangedOnlyForRealChange()
{
    NotificationSettingsModel model;
    QSignalSpy spy(&model, &NotificationSettingsModel::changed);

    model.setSoundEnabled(false);
    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.soundEnabled());

    model.setSoundEnabled(false);
    QCOMPARE(spy.count(), 1);

    model.setSoundEnabled(true);
    QCOMPARE(spy.count(), 2);
    QVERIFY(model.soundEnabled());

    // The sound switch is independent of the master switch.
    QVERIFY(model.enabled());
}

void NotificationSettingsModelTest::setPreviewEnabledEmitsChangedOnlyForRealChange()
{
    NotificationSettingsModel model;
    QSignalSpy spy(&model, &NotificationSettingsModel::changed);

    model.setPreviewEnabled(false);
    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.previewEnabled());

    model.setPreviewEnabled(false);
    QCOMPARE(spy.count(), 1);

    model.setPreviewEnabled(true);
    QCOMPARE(spy.count(), 2);
    QVERIFY(model.previewEnabled());
}

void NotificationSettingsModelTest::setDndEmitsChangedOnlyForRealChange()
{
    NotificationSettingsModel model;
    QSignalSpy spy(&model, &NotificationSettingsModel::changed);

    model.setDnd(true);
    QCOMPARE(spy.count(), 1);
    QVERIFY(model.dnd());

    model.setDnd(true);
    QCOMPARE(spy.count(), 1);

    model.setDnd(false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!model.dnd());

    // Do-not-disturb does not touch the master switch.
    QVERIFY(model.enabled());
}

void NotificationSettingsModelTest::setQuietHoursClampsBoundsToTheDay()
{
    NotificationSettingsModel model;

    model.setQuietHours(-1, MinutesPerDay + 60);
    QCOMPARE(model.quietStartMinutes(), 0);
    QCOMPARE(model.quietEndMinutes(), MinutesPerDay - 1);
    QVERIFY(model.quietHoursActive());

    model.setQuietHours(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    QCOMPARE(model.quietStartMinutes(), 0);
    QCOMPARE(model.quietEndMinutes(), MinutesPerDay - 1);

    // A clamped window is a real window, not an accident that wraps.
    QVERIFY(model.inQuietHours(0));
    QVERIFY(model.inQuietHours(MinutesPerDay - 2));
}

void NotificationSettingsModelTest::setQuietHoursEmitsChangedOnlyForRealChange()
{
    NotificationSettingsModel model;
    QSignalSpy spy(&model, &NotificationSettingsModel::changed);

    model.setQuietHours(at(22), at(7));
    QCOMPARE(spy.count(), 1);

    model.setQuietHours(at(22), at(7));
    QCOMPARE(spy.count(), 1);

    // Moving one bound is a change, even when the window stays active.
    model.setQuietHours(at(23), at(7));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.quietStartMinutes(), at(23));

    // Closing the window is a change as well.
    model.setQuietHours(0, 0);
    QCOMPARE(spy.count(), 3);
    QVERIFY(!model.quietHoursActive());

    // Bounds that clamp back to the stored ones are not a change.
    model.setQuietHours(-5, MinutesPerDay + 5);
    QCOMPARE(spy.count(), 4);
    QCOMPARE(model.quietStartMinutes(), 0);
    QCOMPARE(model.quietEndMinutes(), MinutesPerDay - 1);

    model.setQuietHours(-1, MinutesPerDay + 1);
    QCOMPARE(spy.count(), 4);
}

void NotificationSettingsModelTest::quietHoursActiveTracksStartEqualsEnd()
{
    NotificationSettingsModel model;
    QVERIFY(!model.quietHoursActive());

    model.setQuietHours(at(9), at(17));
    QVERIFY(model.quietHoursActive());

    model.setQuietHours(at(9), at(9));
    QVERIFY2(!model.quietHoursActive(), "equal bounds disable the window");

    // A wrapped window is active too.
    model.setQuietHours(at(23), at(6));
    QVERIFY(model.quietHoursActive());
}

void NotificationSettingsModelTest::inQuietHoursHandlesSameDayWindow()
{
    NotificationSettingsModel model;
    model.setQuietHours(at(9), at(17));

    QVERIFY(!model.inQuietHours(at(8, 59)));
    QVERIFY2(model.inQuietHours(at(9)), "the start minute is inside the window");
    QVERIFY(model.inQuietHours(at(12)));
    QVERIFY(model.inQuietHours(at(16, 59)));
    QVERIFY2(!model.inQuietHours(at(17)), "the end minute is outside the window");
    QVERIFY(!model.inQuietHours(at(23, 59)));
    QVERIFY(!model.inQuietHours(0));
}

void NotificationSettingsModelTest::inQuietHoursHandlesWindowAcrossMidnight()
{
    NotificationSettingsModel model;
    model.setQuietHours(at(22), at(7));

    QVERIFY(!model.inQuietHours(at(21, 59)));
    QVERIFY2(model.inQuietHours(at(22)), "the evening part starts at the start minute");
    QVERIFY(model.inQuietHours(at(23, 59)));
    QVERIFY2(model.inQuietHours(0), "midnight is inside a wrapped window");
    QVERIFY(model.inQuietHours(at(6, 59)));
    QVERIFY2(!model.inQuietHours(at(7)), "the morning part ends at the end minute");
    QVERIFY(!model.inQuietHours(at(12)));
    QVERIFY(!model.inQuietHours(at(21, 59)));
}

void NotificationSettingsModelTest::inQuietHoursExcludesTheEndMinute()
{
    NotificationSettingsModel model;

    // A one-minute same-day window holds exactly its start minute.
    model.setQuietHours(at(12), at(12) + 1);
    QVERIFY(model.inQuietHours(at(12)));
    QVERIFY(!model.inQuietHours(at(12) + 1));

    // A one-minute wrapped window covers the last minute of the day only.
    model.setQuietHours(MinutesPerDay - 1, 0);
    QVERIFY(model.inQuietHours(MinutesPerDay - 1));
    QVERIFY(!model.inQuietHours(0));
    QVERIFY(!model.inQuietHours(MinutesPerDay - 2));
}

void NotificationSettingsModelTest::inQuietHoursIgnoresTimeOutsideTheDay()
{
    NotificationSettingsModel model;
    model.setQuietHours(at(22), at(7));

    QVERIFY2(!model.inQuietHours(-1), "a negative minute is not a time of day");
    QVERIFY(!model.inQuietHours(MinutesPerDay));
    QVERIFY(!model.inQuietHours(std::numeric_limits<int>::max()));
    QVERIFY(!model.inQuietHours(std::numeric_limits<int>::min()));
}

void NotificationSettingsModelTest::inQuietHoursIsOffWhenStartEqualsEnd()
{
    NotificationSettingsModel model;
    model.setQuietHours(at(12), at(12));

    QVERIFY2(!model.inQuietHours(at(12)), "a disabled window never matches");
    QVERIFY(!model.inQuietHours(0));
    QVERIFY(!model.inQuietHours(MinutesPerDay - 1));
}

void NotificationSettingsModelTest::shouldNotifyAllowsByDefault()
{
    const NotificationSettingsModel model;

    QVERIFY(model.shouldNotify(0, false));
    QVERIFY(model.shouldNotify(at(12), false));
    QVERIFY(model.shouldNotify(MinutesPerDay - 1, false));
}

void NotificationSettingsModelTest::shouldNotifyRefusesWhenNotificationsAreOff()
{
    NotificationSettingsModel model;
    model.setEnabled(false);

    QVERIFY(!model.shouldNotify(at(12), false));
    QVERIFY(!model.shouldNotify(at(3), false));

    // The other switches cannot bring notifications back.
    model.setSoundEnabled(true);
    model.setPreviewEnabled(true);
    model.setDnd(false);
    QVERIFY(!model.shouldNotify(at(12), false));

    model.setEnabled(true);
    QVERIFY(model.shouldNotify(at(12), false));
}

void NotificationSettingsModelTest::shouldNotifyRefusesDuringDnd()
{
    NotificationSettingsModel model;
    model.setDnd(true);

    QVERIFY(!model.shouldNotify(at(12), false));
    QVERIFY(!model.shouldNotify(0, false));

    // Do-not-disturb is independent of the quiet-hours window.
    model.setQuietHours(0, 0);
    QVERIFY(!model.shouldNotify(at(12), false));

    model.setDnd(false);
    QVERIFY(model.shouldNotify(at(12), false));
}

void NotificationSettingsModelTest::shouldNotifyRefusesForMutedChat()
{
    NotificationSettingsModel model;

    QVERIFY(!model.shouldNotify(at(12), true));

    // A muted chat stays muted even when quiet hours are off and DND is off.
    model.setQuietHours(0, 0);
    model.setDnd(false);
    QVERIFY(model.enabled());
    QVERIFY(!model.shouldNotify(at(12), true));

    // And it does not poison an unmuted chat.
    QVERIFY(model.shouldNotify(at(12), false));
}

void NotificationSettingsModelTest::shouldNotifyRefusesInsideQuietHours()
{
    NotificationSettingsModel model;
    model.setQuietHours(at(22), at(7));

    QVERIFY2(!model.shouldNotify(at(23), false), "the evening part is quiet");
    QVERIFY(!model.shouldNotify(at(3), false));
    QVERIFY2(!model.shouldNotify(at(6, 59), false), "the last quiet minute is still quiet");

    QVERIFY2(model.shouldNotify(at(7), false), "the window ends at the end minute");
    QVERIFY(model.shouldNotify(at(12), false));
    QVERIFY(model.shouldNotify(at(21, 59), false));
}

void NotificationSettingsModelTest::shouldNotifyAllowsOutsideTheWindow()
{
    NotificationSettingsModel model;
    model.setQuietHours(at(13), at(14));

    QVERIFY(!model.shouldNotify(at(13, 30), false));
    QVERIFY(model.shouldNotify(at(14), false));
    QVERIFY(model.shouldNotify(0, false));
    QVERIFY(model.shouldNotify(MinutesPerDay - 1, false));

    // Turning the sound off does not change whether a notification is shown.
    model.setSoundEnabled(false);
    QVERIFY(model.shouldNotify(at(12), false));
    QVERIFY(!model.shouldNotify(at(13, 30), false));

    // Turning previews off does not change it either.
    model.setPreviewEnabled(false);
    QVERIFY(model.shouldNotify(at(12), false));
}

QTEST_GUILESS_MAIN(NotificationSettingsModelTest)
#include "tst_notification_settings_model.moc"
