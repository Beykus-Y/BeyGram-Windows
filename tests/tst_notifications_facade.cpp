#include "ui/NotificationsFacade.h"

#include <QChar>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QtTest>

using namespace beygram::ui;

namespace {

int at(int hour, int minute = 0)
{
    return hour * 60 + minute;
}

/// The separator of the quiet-hours text: space, en dash, space.
QString dash()
{
    return QStringLiteral(" ") + QChar(0x2013) + QStringLiteral(" ");
}

QString quietWindow(int startHour, int startMinute, int endHour, int endMinute)
{
    const auto clock = [](int hour, int minute) {
        return QString::number(hour).rightJustified(2, QLatin1Char('0'))
            + QLatin1Char(':')
            + QString::number(minute).rightJustified(2, QLatin1Char('0'));
    };

    return clock(startHour, startMinute) + dash() + clock(endHour, endMinute);
}

/// Three sounds, so a row index is never also a sound id.
void loadSampleSounds(NotificationsFacade& facade)
{
    facade.setSounds({QStringLiteral("10"), QStringLiteral("20"), QStringLiteral("30")},
                     {QStringLiteral("Ping"), QStringLiteral("Chime"), QStringLiteral("Bell")});
}

} // namespace

class NotificationsFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithDefaults();
    void togglesForwardToTheSettingsModel();
    void redundantTogglesAreSilent();

    void quietTextIsOffWhenTheWindowIsDisabled();
    void quietHoursStoreMinutesAndFormatTheText();
    void quietTextZeroPadsTheClock();
    void quietTextKeepsTheWrappedOrder();
    void quietHoursClampToTheDay();
    void quietHoursChangedOnlyOnRealChange();

    void setSoundsBuildsTitles();
    void setSoundsDropsMalformedIds();
    void setSoundsShortTitlesReadEmpty();
    void selectSoundSelectsTheRow();
    void selectSoundNegativeSelectsSilent();
    void selectSoundIgnoresOutOfRange();
    void selectionSurvivesANewSoundList();

    void loadChatLoadsTheOverrides();
    void loadChatIsSilentWhenNothingChanges();
    void chatOverridesEditOneFlagAtATime();
    void chatOverridesAreIgnoredWithoutAChat();
    void loadChatWithMalformedIdLoadsNoChat();

    void saveChatIsSilentWhenClean();
    void saveChatEmitsThePayload();
    void saveChatKeepsANegativeChatId();
    void saveChatMarksTheOverridesSaved();
    void saveChatWithoutAChatEmitsNothing();
};

void NotificationsFacadeTest::startsWithDefaults()
{
    const NotificationsFacade facade;

    QVERIFY(facade.enabled());
    QVERIFY(facade.sound());
    QVERIFY(facade.preview());
    QVERIFY(!facade.dnd());

    QCOMPARE(facade.quietStart(), 0);
    QCOMPARE(facade.quietEnd(), 0);
    QCOMPARE(facade.quietText(), QStringLiteral("Off"));

    QVERIFY(facade.soundTitles().isEmpty());
    QCOMPARE(facade.soundIndex(), -1);

    QVERIFY(!facade.chatMuted());
    QVERIFY(facade.chatSound());
    QVERIFY(facade.chatPreview());
}

void NotificationsFacadeTest::togglesForwardToTheSettingsModel()
{
    NotificationsFacade facade;
    QSignalSpy spy(&facade, &NotificationsFacade::changed);

    facade.setEnabled(false);
    QCOMPARE(spy.count(), 1);
    QVERIFY(!facade.enabled());

    facade.setSound(false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!facade.sound());

    facade.setPreview(false);
    QCOMPARE(spy.count(), 3);
    QVERIFY(!facade.preview());

    facade.setDnd(true);
    QCOMPARE(spy.count(), 4);
    QVERIFY(facade.dnd());

    // The switches are independent: turning one off leaves the others alone.
    QVERIFY(!facade.enabled());
    QVERIFY(!facade.sound());
}

void NotificationsFacadeTest::redundantTogglesAreSilent()
{
    NotificationsFacade facade;
    QSignalSpy spy(&facade, &NotificationsFacade::changed);

    // Writing the stored defaults again must not repaint the screen.
    facade.setEnabled(true);
    facade.setSound(true);
    facade.setPreview(true);
    facade.setDnd(false);

    QCOMPARE(spy.count(), 0);
}

void NotificationsFacadeTest::quietTextIsOffWhenTheWindowIsDisabled()
{
    NotificationsFacade facade;
    QSignalSpy spy(&facade, &NotificationsFacade::changed);

    // Equal bounds are how the model spells "quiet hours off".
    facade.setQuietHours(at(12), at(12));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.quietStart(), at(12));
    QCOMPARE(facade.quietEnd(), at(12));
    QCOMPARE(facade.quietText(), QStringLiteral("Off"));

    facade.setQuietHours(0, 0);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(facade.quietText(), QStringLiteral("Off"));
}

void NotificationsFacadeTest::quietHoursStoreMinutesAndFormatTheText()
{
    NotificationsFacade facade;

    facade.setQuietHours(at(22), at(8));

    QCOMPARE(facade.quietStart(), at(22));
    QCOMPARE(facade.quietEnd(), at(8));
    QCOMPARE(facade.quietText(), quietWindow(22, 0, 8, 0));
    QCOMPARE(facade.quietText(), QStringLiteral("22:00") + dash() + QStringLiteral("08:00"));
}

void NotificationsFacadeTest::quietTextZeroPadsTheClock()
{
    NotificationsFacade facade;

    facade.setQuietHours(at(9, 5), at(17, 30));
    QCOMPARE(facade.quietText(), quietWindow(9, 5, 17, 30));

    facade.setQuietHours(at(0), at(0, 1));
    QCOMPARE(facade.quietText(), QStringLiteral("00:00") + dash() + QStringLiteral("00:01"));

    facade.setQuietHours(at(0, 0), at(23, 59));
    QCOMPARE(facade.quietText(), quietWindow(0, 0, 23, 59));
}

void NotificationsFacadeTest::quietTextKeepsTheWrappedOrder()
{
    NotificationsFacade facade;

    // A wrapped window is printed as entered, start first: the user reads it as
    // "quiet from 23:45 until 06:05", not sorted by time of day.
    facade.setQuietHours(at(23, 45), at(6, 5));

    QVERIFY(facade.quietStart() > facade.quietEnd());
    QCOMPARE(facade.quietText(), quietWindow(23, 45, 6, 5));
}

void NotificationsFacadeTest::quietHoursClampToTheDay()
{
    NotificationsFacade facade;

    facade.setQuietHours(-30, 24 * 60 + 30);

    QCOMPARE(facade.quietStart(), 0);
    QCOMPARE(facade.quietEnd(), 24 * 60 - 1);
    QCOMPARE(facade.quietText(), quietWindow(0, 0, 23, 59));
}

void NotificationsFacadeTest::quietHoursChangedOnlyOnRealChange()
{
    NotificationsFacade facade;
    QSignalSpy spy(&facade, &NotificationsFacade::changed);

    facade.setQuietHours(at(22), at(7));
    QCOMPARE(spy.count(), 1);

    facade.setQuietHours(at(22), at(7));
    QCOMPARE(spy.count(), 1);

    // Moving one bound is a change even while the window stays active.
    facade.setQuietHours(at(22), at(8));
    QCOMPARE(spy.count(), 2);

    // Bounds that clamp back to the stored ones are not a change.
    facade.setQuietHours(-5, 24 * 60 + 5);
    QCOMPARE(spy.count(), 3);
    QCOMPARE(facade.quietStart(), 0);
    QCOMPARE(facade.quietEnd(), 24 * 60 - 1);

    facade.setQuietHours(-1, 24 * 60 + 1);
    QCOMPARE(spy.count(), 3);
}

void NotificationsFacadeTest::setSoundsBuildsTitles()
{
    NotificationsFacade facade;
    QSignalSpy spy(&facade, &NotificationsFacade::changed);

    loadSampleSounds(facade);

    const QStringList expected{QStringLiteral("Ping"), QStringLiteral("Chime"),
                               QStringLiteral("Bell")};
    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.soundTitles(), expected);

    // Loading a list does not pick a sound; the silent selection stays.
    QCOMPARE(facade.soundIndex(), -1);
}

void NotificationsFacadeTest::setSoundsDropsMalformedIds()
{
    NotificationsFacade facade;

    // 0 is silent, a negative id is not a sound, "abc" is not a number and the
    // repeated "1" loses to its first copy. All four are dropped by the model.
    facade.setSounds({QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("0"),
                      QStringLiteral("-4"), QStringLiteral("abc"), QStringLiteral("1"),
                      QStringLiteral("12x")},
                     {QStringLiteral("Ping"), QStringLiteral("Chime"), QStringLiteral("Zero"),
                      QStringLiteral("Negative"), QStringLiteral("Bad"),
                      QStringLiteral("Ping again"), QStringLiteral("Partial")});

    const QStringList expected{QStringLiteral("Ping"), QStringLiteral("Chime")};
    QCOMPARE(facade.soundTitles(), expected);
}

void NotificationsFacadeTest::setSoundsShortTitlesReadEmpty()
{
    NotificationsFacade facade;

    facade.setSounds({QStringLiteral("7"), QStringLiteral("8")}, {QStringLiteral("Ping")});

    // Titles are read positionally, so a missing one is empty rather than a
    // shifted label.
    const QStringList expected{QStringLiteral("Ping"), QString()};
    QCOMPARE(facade.soundTitles(), expected);
}

void NotificationsFacadeTest::selectSoundSelectsTheRow()
{
    NotificationsFacade facade;
    loadSampleSounds(facade);

    QSignalSpy spy(&facade, &NotificationsFacade::changed);

    facade.selectSound(1);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.soundIndex(), 1);

    // Reselecting the stored row is silent.
    facade.selectSound(1);
    QCOMPARE(spy.count(), 1);

    facade.selectSound(2);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(facade.soundIndex(), 2);
}

void NotificationsFacadeTest::selectSoundNegativeSelectsSilent()
{
    NotificationsFacade facade;
    loadSampleSounds(facade);

    facade.selectSound(0);
    QCOMPARE(facade.soundIndex(), 0);

    QSignalSpy spy(&facade, &NotificationsFacade::changed);

    // A negative row is the silent choice, not an invalid one.
    facade.selectSound(-1);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.soundIndex(), -1);

    facade.selectSound(-1);
    QCOMPARE(spy.count(), 1);

    facade.selectSound(-5);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.soundIndex(), -1);
}

void NotificationsFacadeTest::selectSoundIgnoresOutOfRange()
{
    NotificationsFacade facade;
    loadSampleSounds(facade);

    facade.selectSound(1);

    QSignalSpy spy(&facade, &NotificationsFacade::changed);

    // Row 3 does not exist (0..2 do), so a stale row cannot select a sound that
    // is not offered.
    facade.selectSound(3);
    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.soundIndex(), 1);

    facade.selectSound(100);
    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.soundIndex(), 1);

    // Nor can anything be selected while no sounds are stored.
    NotificationsFacade empty;
    QSignalSpy emptySpy(&empty, &NotificationsFacade::changed);
    empty.selectSound(0);
    QCOMPARE(emptySpy.count(), 0);
    QCOMPARE(empty.soundIndex(), -1);
}

void NotificationsFacadeTest::selectionSurvivesANewSoundList()
{
    NotificationsFacade facade;
    loadSampleSounds(facade);

    facade.selectSound(2); // sound id 30
    QCOMPARE(facade.soundIndex(), 2);

    // A longer list keeps the same row.
    facade.setSounds({QStringLiteral("10"), QStringLiteral("20"), QStringLiteral("30"),
                      QStringLiteral("40")},
                     {QStringLiteral("Ping"), QStringLiteral("Chime"),
                      QStringLiteral("Bell"), QStringLiteral("Gong")});
    QCOMPARE(facade.soundIndex(), 2);

    // The id is kept even while the list does not store it, so no row matches.
    facade.setSounds({QStringLiteral("40")}, {QStringLiteral("Gong")});
    QCOMPARE(facade.soundIndex(), -1);

    // And the same selection comes back when the sound returns.
    facade.setSounds({QStringLiteral("30"), QStringLiteral("40")},
                     {QStringLiteral("Bell"), QStringLiteral("Gong")});
    QCOMPARE(facade.soundIndex(), 0);
}

void NotificationsFacadeTest::loadChatLoadsTheOverrides()
{
    NotificationsFacade facade;
    QSignalSpy spy(&facade, &NotificationsFacade::changed);

    facade.loadChat(QStringLiteral("42"), true, false, false);

    QCOMPARE(spy.count(), 1);
    QVERIFY(facade.chatMuted());
    QVERIFY(!facade.chatSound());
    QVERIFY(!facade.chatPreview());
}

void NotificationsFacadeTest::loadChatIsSilentWhenNothingChanges()
{
    NotificationsFacade facade;
    facade.loadChat(QStringLiteral("42"), true, false, false);

    QSignalSpy spy(&facade, &NotificationsFacade::changed);

    // Reloading the same chat with the same flags is not a change.
    facade.loadChat(QStringLiteral("42"), true, false, false);
    QCOMPARE(spy.count(), 0);

    // Switching chats is.
    facade.loadChat(QStringLiteral("43"), true, false, false);
    QCOMPARE(spy.count(), 1);
}

void NotificationsFacadeTest::chatOverridesEditOneFlagAtATime()
{
    NotificationsFacade facade;
    facade.loadChat(QStringLiteral("42"), false, true, true);

    QSignalSpy spy(&facade, &NotificationsFacade::changed);

    facade.setChatMuted(true);
    QCOMPARE(spy.count(), 1);
    QVERIFY(facade.chatMuted());

    facade.setChatMuted(true);
    QCOMPARE(spy.count(), 1);

    facade.setChatSound(false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!facade.chatSound());

    facade.setChatPreview(false);
    QCOMPARE(spy.count(), 3);
    QVERIFY(!facade.chatPreview());

    // Editing one override leaves the other two alone.
    QVERIFY(facade.chatMuted());
    QVERIFY(!facade.chatSound());
}

void NotificationsFacadeTest::chatOverridesAreIgnoredWithoutAChat()
{
    NotificationsFacade facade;
    QSignalSpy spy(&facade, &NotificationsFacade::changed);

    facade.setChatMuted(true);
    facade.setChatSound(false);
    facade.setChatPreview(false);

    QCOMPARE(spy.count(), 0);
    QVERIFY(!facade.chatMuted());
    QVERIFY(facade.chatSound());
    QVERIFY(facade.chatPreview());
}

void NotificationsFacadeTest::loadChatWithMalformedIdLoadsNoChat()
{
    NotificationsFacade facade;
    facade.loadChat(QStringLiteral("42"), true, false, false);

    QSignalSpy spy(&facade, &NotificationsFacade::changed);

    // A value that is not a number names no chat, so the overrides are cleared
    // instead of showing flags that could never be saved.
    facade.loadChat(QStringLiteral("not-a-chat"), true, false, false);
    QCOMPARE(spy.count(), 1);
    QVERIFY(!facade.chatMuted());
    QVERIFY(facade.chatSound());
    QVERIFY(facade.chatPreview());

    // Chat id 0 is not a chat either: Telegram never assigns it.
    facade.loadChat(QStringLiteral("0"), true, false, false);
    QVERIFY(!facade.chatMuted());

    // The setters stay inert and a save sends nothing.
    QSignalSpy saveSpy(&facade, &NotificationsFacade::chatSettingsRequested);
    facade.setChatMuted(true);
    facade.saveChat();
    QCOMPARE(saveSpy.count(), 0);
    QVERIFY(!facade.chatMuted());
}

void NotificationsFacadeTest::saveChatIsSilentWhenClean()
{
    NotificationsFacade facade;
    facade.loadChat(QStringLiteral("42"), false, true, true);

    QSignalSpy spy(&facade, &NotificationsFacade::chatSettingsRequested);
    facade.saveChat();

    QCOMPARE(spy.count(), 0);
}

void NotificationsFacadeTest::saveChatEmitsThePayload()
{
    NotificationsFacade facade;
    facade.loadChat(QStringLiteral("42"), false, true, true);
    facade.setChatMuted(true);
    facade.setChatSound(false);

    QSignalSpy spy(&facade, &NotificationsFacade::chatSettingsRequested);
    facade.saveChat();

    QCOMPARE(spy.count(), 1);

    const QList<QVariant> payload = spy.at(0);
    QCOMPARE(payload.at(0).toString(), QStringLiteral("42"));
    QVERIFY(payload.at(1).toBool());
    QVERIFY(!payload.at(2).toBool());
    QVERIFY(payload.at(3).toBool());
}

void NotificationsFacadeTest::saveChatKeepsANegativeChatId()
{
    // Supergroup and channel ids are negative and must not be truncated or
    // turned into a JavaScript number on the way back.
    const QString chatId = QStringLiteral("-1001234567890");

    NotificationsFacade facade;
    facade.loadChat(chatId, false, true, true);
    facade.setChatPreview(false);

    QSignalSpy spy(&facade, &NotificationsFacade::chatSettingsRequested);
    facade.saveChat();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), chatId);
    QVERIFY(!spy.at(0).at(3).toBool());
}

void NotificationsFacadeTest::saveChatMarksTheOverridesSaved()
{
    NotificationsFacade facade;
    facade.loadChat(QStringLiteral("42"), false, true, true);
    facade.setChatMuted(true);

    QSignalSpy spy(&facade, &NotificationsFacade::chatSettingsRequested);

    facade.saveChat();
    QCOMPARE(spy.count(), 1);

    // The current flags are now the saved baseline, so a second click sends
    // nothing.
    facade.saveChat();
    QCOMPARE(spy.count(), 1);

    // A new edit is dirty again and is sent with the saved mute.
    facade.setChatPreview(false);
    facade.saveChat();
    QCOMPARE(spy.count(), 2);
    QVERIFY(spy.at(1).at(1).toBool());
    QVERIFY(!spy.at(1).at(3).toBool());
}

void NotificationsFacadeTest::saveChatWithoutAChatEmitsNothing()
{
    NotificationsFacade facade;

    QSignalSpy spy(&facade, &NotificationsFacade::chatSettingsRequested);
    facade.saveChat();

    QCOMPARE(spy.count(), 0);
}

QTEST_GUILESS_MAIN(NotificationsFacadeTest)

#include "tst_notifications_facade.moc"
