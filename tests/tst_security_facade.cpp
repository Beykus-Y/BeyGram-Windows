#include "ui/SecurityFacade.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::ui;

class SecurityFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithoutChat();
    void loadsSecretChat();
    void loadsRegularChat();
    void keepsNegativeChatId();
    void unloadDropsEverything();

    void protectedContentRoundTrips();
    void protectedContentBlocksCopy();
    void protectedContentOnlyChangeEmitsChanged();
    void protectedContentDroppedOnUnload();

    void ttlTextIsOffWithoutTimer();
    void ttlTextFormatsLargestWholeUnit();
    void ttlRequestCarriesChatAndSeconds();
    void ttlRequestClampsToWeek();
    void ttlRequestAnnouncesEveryPick();
    void ttlIgnoredWithoutChat();

    void screenshotWarnFollowsFlag();

    void revealsAndHidesSpoilers();
    void ignoresInvalidMessageIds();
    void trimsMessageIds();
    void loadingAnotherChatCoversSpoilers();

    void changedOnlyOnRealChanges();
};

void SecurityFacadeTest::startsWithoutChat()
{
    const SecurityFacade facade;

    QVERIFY(!facade.secret());
    QCOMPARE(facade.ttlSeconds(), 0);
    QCOMPARE(facade.ttlText(), QStringLiteral("Off"));
    QVERIFY(facade.notificationsAllowed());
    QVERIFY(facade.copyAllowed());
    QVERIFY(!facade.screenshotWarn());
    QVERIFY(!facade.protectedContent());
    QVERIFY(!facade.isRevealed(QStringLiteral("1")));
}

void SecurityFacadeTest::loadsSecretChat()
{
    SecurityFacade facade;

    facade.setChat(QStringLiteral("42"), true, false);

    QVERIFY(facade.secret());
    QCOMPARE(facade.ttlSeconds(), 0);
    QCOMPARE(facade.ttlText(), QStringLiteral("Off"));
    // Secret chats never notify and never copy.
    QVERIFY(!facade.notificationsAllowed());
    QVERIFY(!facade.copyAllowed());
    // Screenshots are still allowed, so the other side would be told about one.
    QVERIFY(facade.screenshotWarn());
    QVERIFY(!facade.protectedContent());
}

void SecurityFacadeTest::loadsRegularChat()
{
    SecurityFacade facade;

    facade.setChat(QStringLiteral("7"), false, false);

    QVERIFY(!facade.secret());
    QVERIFY(facade.notificationsAllowed());
    QVERIFY(facade.copyAllowed());
    QVERIFY(!facade.screenshotWarn());
    QVERIFY(!facade.protectedContent());
}

void SecurityFacadeTest::keepsNegativeChatId()
{
    // Groups and channels have negative ids and must survive the string round
    // trip into the request.
    const QString chatId = QStringLiteral("-1001234567890");

    SecurityFacade facade;
    facade.setChat(chatId, false, false);

    QSignalSpy spy(&facade, &SecurityFacade::ttlRequested);
    facade.setTtl(3600);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), chatId);
    QCOMPARE(spy.at(0).at(1).toInt(), 3600);
}

void SecurityFacadeTest::unloadDropsEverything()
{
    SecurityFacade facade;
    facade.setChat(QStringLiteral("42"), true, true);
    facade.setTtl(3600);
    facade.setScreenshotAllowed(false);
    facade.reveal(QStringLiteral("5"));

    // The protected-content flag is passed as true again, but the chat is
    // unloaded, so it must not survive on its own.
    facade.setChat(QStringLiteral("0"), true, true);

    QVERIFY(!facade.secret());
    QCOMPARE(facade.ttlSeconds(), 0);
    QCOMPARE(facade.ttlText(), QStringLiteral("Off"));
    QVERIFY(facade.notificationsAllowed());
    QVERIFY(facade.copyAllowed());
    QVERIFY(!facade.screenshotWarn());
    QVERIFY(!facade.protectedContent());
    QVERIFY(!facade.isRevealed(QStringLiteral("5")));
}

void SecurityFacadeTest::protectedContentRoundTrips()
{
    SecurityFacade facade;

    facade.setChat(QStringLiteral("7"), false, true);
    QVERIFY(facade.protectedContent());

    facade.setChat(QStringLiteral("7"), false, false);
    QVERIFY(!facade.protectedContent());
}

void SecurityFacadeTest::protectedContentBlocksCopy()
{
    SecurityFacade facade;

    // A regular chat allows copying until content protection is set.
    facade.setChat(QStringLiteral("7"), false, false);
    QVERIFY(facade.copyAllowed());

    facade.setChat(QStringLiteral("7"), false, true);
    QVERIFY(!facade.copyAllowed());
    // Protection has nothing to do with notifications.
    QVERIFY(facade.notificationsAllowed());

    // A secret chat denies copying either way.
    facade.setChat(QStringLiteral("42"), true, true);
    QVERIFY(!facade.copyAllowed());
    QVERIFY(!facade.notificationsAllowed());

    facade.setChat(QStringLiteral("42"), true, false);
    QVERIFY(!facade.copyAllowed());
}

void SecurityFacadeTest::protectedContentOnlyChangeEmitsChanged()
{
    SecurityFacade facade;
    facade.setChat(QStringLiteral("7"), false, false);

    QSignalSpy spy(&facade, &SecurityFacade::changed);

    // Same chat and same secret flag: the flag change is the only news.
    facade.setChat(QStringLiteral("7"), false, true);
    QCOMPARE(spy.count(), 1);
    QVERIFY(facade.protectedContent());

    facade.setChat(QStringLiteral("7"), false, true);
    QCOMPARE(spy.count(), 1);

    facade.setChat(QStringLiteral("7"), false, false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!facade.protectedContent());

    // A new chat announces itself through the safety controller instead, so the
    // protection change must not add a second emission.
    facade.setChat(QStringLiteral("8"), false, true);
    QCOMPARE(spy.count(), 3);
    QVERIFY(facade.protectedContent());
}

void SecurityFacadeTest::protectedContentDroppedOnUnload()
{
    SecurityFacade facade;
    facade.setChat(QStringLiteral("7"), false, true);

    QSignalSpy spy(&facade, &SecurityFacade::changed);
    facade.setChat(QString(), false, true);

    QCOMPARE(spy.count(), 1);
    QVERIFY(!facade.protectedContent());
    QVERIFY(facade.copyAllowed());
}

void SecurityFacadeTest::ttlTextIsOffWithoutTimer()
{
    SecurityFacade facade;

    QCOMPARE(facade.ttlText(), QStringLiteral("Off"));

    facade.setChat(QStringLiteral("42"), true, false);
    QCOMPARE(facade.ttlText(), QStringLiteral("Off"));
}

void SecurityFacadeTest::ttlTextFormatsLargestWholeUnit()
{
    struct Case {
        int seconds;
        const char* text;
    };

    const Case cases[] = {
        {0, "Off"},
        {1, "1 second"},
        {30, "30 seconds"},
        {59, "59 seconds"},
        {60, "1 minute"},
        {90, "1 minute"},
        {300, "5 minutes"},
        {3599, "59 minutes"},
        {3600, "1 hour"},
        {5400, "1 hour"},
        {7200, "2 hours"},
        {86399, "23 hours"},
        {86400, "1 day"},
        {172800, "2 days"},
        {604800, "7 days"},
    };

    SecurityFacade facade;
    facade.setChat(QStringLiteral("42"), true, false);

    for (const Case& c : cases) {
        facade.setTtl(c.seconds);
        QCOMPARE(facade.ttlSeconds(), c.seconds);
        QCOMPARE(facade.ttlText(), QString::fromLatin1(c.text));
    }
}

void SecurityFacadeTest::ttlRequestCarriesChatAndSeconds()
{
    SecurityFacade facade;
    facade.setChat(QStringLiteral("42"), true, false);

    QSignalSpy spy(&facade, &SecurityFacade::ttlRequested);
    facade.setTtl(3600);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("42"));
    QCOMPARE(spy.at(0).at(1).toInt(), 3600);
    // The facade already holds what it announced.
    QCOMPARE(facade.ttlSeconds(), 3600);
    QCOMPARE(facade.ttlText(), QStringLiteral("1 hour"));
}

void SecurityFacadeTest::ttlRequestClampsToWeek()
{
    SecurityFacade facade;
    facade.setChat(QStringLiteral("42"), true, false);

    QSignalSpy spy(&facade, &SecurityFacade::ttlRequested);

    facade.setTtl(999999);
    QCOMPARE(facade.ttlSeconds(), 604800);
    QCOMPARE(facade.ttlText(), QStringLiteral("7 days"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toInt(), 604800);

    facade.setTtl(-10);
    QCOMPARE(facade.ttlSeconds(), 0);
    QCOMPARE(facade.ttlText(), QStringLiteral("Off"));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(1).toInt(), 0);
}

void SecurityFacadeTest::ttlRequestAnnouncesEveryPick()
{
    SecurityFacade facade;
    facade.setChat(QStringLiteral("42"), true, false);

    QSignalSpy spy(&facade, &SecurityFacade::ttlRequested);

    facade.setTtl(3600);
    QCOMPARE(spy.count(), 1);

    // Picking the value already in effect is still an explicit action, so it is
    // announced, but the state did not move and changed() stays silent.
    QSignalSpy changedSpy(&facade, &SecurityFacade::changed);
    facade.setTtl(3600);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(1).toInt(), 3600);
    QCOMPARE(changedSpy.count(), 0);

    facade.setTtl(7200);
    QCOMPARE(spy.count(), 3);
    QCOMPARE(spy.at(2).at(1).toInt(), 7200);
    QCOMPARE(changedSpy.count(), 1);
}

void SecurityFacadeTest::ttlIgnoredWithoutChat()
{
    SecurityFacade facade;

    QSignalSpy spy(&facade, &SecurityFacade::ttlRequested);

    facade.setTtl(3600);
    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.ttlSeconds(), 0);
    QCOMPARE(facade.ttlText(), QStringLiteral("Off"));

    // Unloading the chat also takes the timer with it, so a late pick has
    // nowhere to go.
    facade.setChat(QStringLiteral("42"), true, false);
    facade.setTtl(3600);
    facade.setChat(QStringLiteral("0"), true, false);

    facade.setTtl(7200);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.ttlSeconds(), 0);
}

void SecurityFacadeTest::screenshotWarnFollowsFlag()
{
    SecurityFacade facade;
    facade.setChat(QStringLiteral("42"), true, false);

    QVERIFY(facade.screenshotWarn());
    facade.setScreenshotAllowed(false);
    QVERIFY(!facade.screenshotWarn());

    facade.setScreenshotAllowed(true);
    QVERIFY(facade.screenshotWarn());

    // A regular chat never warns, whatever the flag holds.
    facade.setChat(QStringLiteral("7"), false, false);
    QVERIFY(!facade.screenshotWarn());
    facade.setScreenshotAllowed(false);
    QVERIFY(!facade.screenshotWarn());
    QVERIFY(facade.notificationsAllowed());
    QVERIFY(facade.copyAllowed());
}

void SecurityFacadeTest::revealsAndHidesSpoilers()
{
    SecurityFacade facade;
    facade.setChat(QStringLiteral("42"), true, false);

    QVERIFY(!facade.isRevealed(QStringLiteral("5")));

    facade.reveal(QStringLiteral("5"));
    QVERIFY(facade.isRevealed(QStringLiteral("5")));

    facade.reveal(QStringLiteral("6"));
    QVERIFY(facade.isRevealed(QStringLiteral("5")));
    QVERIFY(facade.isRevealed(QStringLiteral("6")));
    QVERIFY(!facade.isRevealed(QStringLiteral("7")));

    facade.hideAllSpoilers();
    QVERIFY(!facade.isRevealed(QStringLiteral("5")));
    QVERIFY(!facade.isRevealed(QStringLiteral("6")));

    // Hiding is not final: the message can be revealed again.
    facade.reveal(QStringLiteral("5"));
    QVERIFY(facade.isRevealed(QStringLiteral("5")));
}

void SecurityFacadeTest::ignoresInvalidMessageIds()
{
    SecurityFacade facade;
    facade.setChat(QStringLiteral("42"), true, false);

    const QString invalid[] = {
        QString(),
        QStringLiteral("abc"),
        QStringLiteral("0"),
        QStringLiteral("-3"),
        QStringLiteral("1.5"),
    };

    for (const QString& id : invalid) {
        facade.reveal(id);
        QVERIFY(!facade.isRevealed(id));
    }
}

void SecurityFacadeTest::trimsMessageIds()
{
    SecurityFacade facade;
    facade.setChat(QStringLiteral("42"), true, false);

    facade.reveal(QStringLiteral(" 7 "));
    QVERIFY(facade.isRevealed(QStringLiteral("7")));
    QVERIFY(facade.isRevealed(QStringLiteral("  7")));
}

void SecurityFacadeTest::loadingAnotherChatCoversSpoilers()
{
    SecurityFacade facade;
    facade.setChat(QStringLiteral("42"), true, false);
    facade.reveal(QStringLiteral("5"));

    facade.setChat(QStringLiteral("43"), true, false);
    QVERIFY(!facade.isRevealed(QStringLiteral("5")));

    // Reloading the same chat is not a switch, so its reveals stay.
    facade.reveal(QStringLiteral("5"));
    facade.setChat(QStringLiteral("43"), true, false);
    QVERIFY(facade.isRevealed(QStringLiteral("5")));
}

void SecurityFacadeTest::changedOnlyOnRealChanges()
{
    SecurityFacade facade;
    QSignalSpy spy(&facade, &SecurityFacade::changed);

    facade.setChat(QStringLiteral("42"), true, false);
    QCOMPARE(spy.count(), 1);

    facade.setChat(QStringLiteral("42"), true, false);
    QCOMPARE(spy.count(), 1);

    facade.setChat(QStringLiteral("42"), false, false);
    QCOMPARE(spy.count(), 2);

    facade.setTtl(3600);
    QCOMPARE(spy.count(), 3);

    facade.setTtl(3600);
    QCOMPARE(spy.count(), 3);

    facade.setScreenshotAllowed(false);
    QCOMPARE(spy.count(), 4);

    facade.setScreenshotAllowed(false);
    QCOMPARE(spy.count(), 4);

    facade.reveal(QStringLiteral("5"));
    QCOMPARE(spy.count(), 5);

    facade.reveal(QStringLiteral("5"));
    QCOMPARE(spy.count(), 5);

    facade.hideAllSpoilers();
    QCOMPARE(spy.count(), 6);

    facade.hideAllSpoilers();
    QCOMPARE(spy.count(), 6);

    // A new chat covers the (empty) reveal set and changes the identity: one
    // emission in total.
    facade.setChat(QStringLiteral("43"), false, false);
    QCOMPARE(spy.count(), 7);

    facade.setChat(QStringLiteral("0"), false, false);
    QCOMPARE(spy.count(), 8);

    facade.setChat(QStringLiteral("0"), false, false);
    QCOMPARE(spy.count(), 8);
}

QTEST_GUILESS_MAIN(SecurityFacadeTest)
#include "tst_security_facade.moc"
