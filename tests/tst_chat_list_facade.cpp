#include "ui/ChatListFacade.h"

#include <QSignalSpy>
#include <QtTest>

using beygram::ui::ChatListFacade;

class ChatListFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithTelegramDefaults();
    void setSortModeAcceptsEveryName();
    void setSortModeIgnoresUnknownNames();
    void setSortModeIgnoresCaseAndWhitespace();
    void sortFlagsRoundTrip();
    void flagsNotifyOnlyOnRealTransitions();
    void setArchivedIdsParsesChatIds();
    void setArchivedIdsDropsInvalidAndDuplicateIds();
    void archivedCountSurvivesArchiveToggles();
    void openAndCloseArchive();
    void archiveOpenChangedCarriesTheNewState();
    void archiveIdsAndOpenShareChanged();
    void setChatUnreadUpdatesTotals();
    void setChatUnreadClampsNegativeCounts();
    void setChatUnreadIgnoresInvalidChatId();
    void badgeTextFollowsTotal();
    void removeChatDropsItsUnread();
    void removeUnknownChatIsSilent();
    void unreadChangesNotifyOncePerTransition();
    void totalUnreadSumsMutedChats();
    void propertiesAreReachableThroughTheMetaObject();
};

void ChatListFacadeTest::startsWithTelegramDefaults()
{
    const ChatListFacade facade;

    // Position is TDLib's own order, descending is newest-first, and pinned
    // chats stay on top until the user says otherwise.
    QCOMPARE(facade.sortMode(), QStringLiteral("position"));
    QVERIFY(!facade.ascending());
    QVERIFY(facade.pinnedFirst());

    QVERIFY(!facade.archiveOpen());
    QCOMPARE(facade.archivedCount(), 0);

    QCOMPARE(facade.badgeText(), QString());
    QCOMPARE(facade.totalUnread(), 0);
}

void ChatListFacadeTest::setSortModeAcceptsEveryName()
{
    ChatListFacade facade;
    QSignalSpy spy(&facade, &ChatListFacade::changed);

    facade.setSortMode(QStringLiteral("date"));
    QCOMPARE(facade.sortMode(), QStringLiteral("date"));
    QCOMPARE(spy.count(), 1);

    facade.setSortMode(QStringLiteral("unread"));
    QCOMPARE(facade.sortMode(), QStringLiteral("unread"));
    QCOMPARE(spy.count(), 2);

    facade.setSortMode(QStringLiteral("name"));
    QCOMPARE(facade.sortMode(), QStringLiteral("name"));
    QCOMPARE(spy.count(), 3);

    facade.setSortMode(QStringLiteral("position"));
    QCOMPARE(facade.sortMode(), QStringLiteral("position"));
    QCOMPARE(spy.count(), 4);
}

void ChatListFacadeTest::setSortModeIgnoresUnknownNames()
{
    ChatListFacade facade;
    facade.setSortMode(QStringLiteral("date"));

    QSignalSpy spy(&facade, &ChatListFacade::changed);

    // An unknown name must not silently reset the selection to position.
    facade.setSortMode(QString());
    facade.setSortMode(QStringLiteral("   "));
    facade.setSortMode(QStringLiteral("recent"));
    facade.setSortMode(QStringLiteral("pinned"));

    QCOMPARE(facade.sortMode(), QStringLiteral("date"));
    QCOMPARE(spy.count(), 0);
}

void ChatListFacadeTest::setSortModeIgnoresCaseAndWhitespace()
{
    ChatListFacade facade;
    QSignalSpy spy(&facade, &ChatListFacade::changed);

    facade.setSortMode(QStringLiteral(" DATE "));
    QCOMPARE(facade.sortMode(), QStringLiteral("date"));
    QCOMPARE(spy.count(), 1);

    // The same mode in another casing is not a transition.
    facade.setSortMode(QStringLiteral("Date"));
    facade.setSortMode(QStringLiteral("date"));
    QCOMPARE(spy.count(), 1);
}

void ChatListFacadeTest::sortFlagsRoundTrip()
{
    ChatListFacade facade;

    facade.setAscending(true);
    QVERIFY(facade.ascending());

    facade.setPinnedFirst(false);
    QVERIFY(!facade.pinnedFirst());

    facade.setAscending(false);
    QVERIFY(!facade.ascending());

    facade.setPinnedFirst(true);
    QVERIFY(facade.pinnedFirst());
}

void ChatListFacadeTest::flagsNotifyOnlyOnRealTransitions()
{
    ChatListFacade facade;
    QSignalSpy spy(&facade, &ChatListFacade::changed);

    // Both setters start at their defaults, so repeating them is silent.
    facade.setAscending(false);
    facade.setPinnedFirst(true);
    QCOMPARE(spy.count(), 0);

    facade.setAscending(true);
    QCOMPARE(spy.count(), 1);
    facade.setAscending(true);
    QCOMPARE(spy.count(), 1);

    facade.setPinnedFirst(false);
    QCOMPARE(spy.count(), 2);
    facade.setPinnedFirst(false);
    QCOMPARE(spy.count(), 2);
}

void ChatListFacadeTest::setArchivedIdsParsesChatIds()
{
    ChatListFacade facade;
    QSignalSpy spy(&facade, &ChatListFacade::changed);

    facade.setArchivedIds({QStringLiteral("90"), QStringLiteral("12"), QStringLiteral("500")});

    QCOMPARE(facade.archivedCount(), 3);
    QCOMPARE(spy.count(), 1);

    // Telegram ids exceed the 32-bit range, so a full 64-bit value must survive.
    facade.setArchivedIds({QStringLiteral("4242424242")});
    QCOMPARE(facade.archivedCount(), 1);
    QCOMPARE(spy.count(), 2);
}

void ChatListFacadeTest::setArchivedIdsDropsInvalidAndDuplicateIds()
{
    ChatListFacade facade;

    // Zero, negative and non-numeric entries are rejected by the archive
    // controller; repeated ids keep their first position.
    facade.setArchivedIds({QStringLiteral("0"), QStringLiteral("-3"), QStringLiteral("abc"),
                           QStringLiteral("7"), QStringLiteral("7"), QStringLiteral("12")});
    QCOMPARE(facade.archivedCount(), 2);

    // A list that parses to nothing empties the archive.
    facade.setArchivedIds({QStringLiteral("oops")});
    QCOMPARE(facade.archivedCount(), 0);
}

void ChatListFacadeTest::archivedCountSurvivesArchiveToggles()
{
    ChatListFacade facade;

    facade.setArchivedIds({QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3")});

    facade.openArchive();
    QCOMPARE(facade.archivedCount(), 3);

    // Closing the folder must not forget what it contains.
    facade.closeArchive();
    QCOMPARE(facade.archivedCount(), 3);
}

void ChatListFacadeTest::openAndCloseArchive()
{
    ChatListFacade facade;

    facade.openArchive();
    QVERIFY(facade.archiveOpen());

    facade.closeArchive();
    QVERIFY(!facade.archiveOpen());

    facade.openArchive();
    QVERIFY(facade.archiveOpen());
}

void ChatListFacadeTest::archiveOpenChangedCarriesTheNewState()
{
    ChatListFacade facade;
    QSignalSpy spy(&facade, &ChatListFacade::archiveOpenChanged);

    facade.openArchive();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);

    // Reopening an open archive is not a transition.
    facade.openArchive();
    QCOMPARE(spy.count(), 1);

    facade.closeArchive();
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toBool(), false);
}

void ChatListFacadeTest::archiveIdsAndOpenShareChanged()
{
    ChatListFacade facade;
    QSignalSpy spy(&facade, &ChatListFacade::changed);
    QSignalSpy openSpy(&facade, &ChatListFacade::archiveOpenChanged);

    facade.openArchive();                                                    // 1
    facade.setArchivedIds({QStringLiteral("4"), QStringLiteral("8")});       // 2
    facade.openArchive();                                                    // no-op
    facade.closeArchive();                                                   // 3
    facade.setArchivedIds({QStringLiteral("8"), QStringLiteral("4")});       // 4
    facade.setArchivedIds({QStringLiteral("4"), QStringLiteral("8")});       // 5

    QCOMPARE(spy.count(), 5);

    // Only the two real open/close transitions reach the narrower signal.
    QCOMPARE(openSpy.count(), 2);
}

void ChatListFacadeTest::setChatUnreadUpdatesTotals()
{
    ChatListFacade facade;
    QSignalSpy spy(&facade, &ChatListFacade::changed);

    facade.setChatUnread(QStringLiteral("1"), 4, false);
    QCOMPARE(facade.totalUnread(), 4);
    QCOMPARE(facade.badgeText(), QStringLiteral("4"));
    QCOMPARE(spy.count(), 1);

    facade.setChatUnread(QStringLiteral("2"), 6, true);
    QCOMPARE(facade.totalUnread(), 10);
    QCOMPARE(facade.badgeText(), QStringLiteral("10"));
    QCOMPARE(spy.count(), 2);
}

void ChatListFacadeTest::setChatUnreadClampsNegativeCounts()
{
    ChatListFacade facade;

    facade.setChatUnread(QStringLiteral("5"), -3, false);

    QCOMPARE(facade.totalUnread(), 0);
    QCOMPARE(facade.badgeText(), QString());

    // The stored value is the clamped zero, so repeating it changes nothing.
    QSignalSpy spy(&facade, &ChatListFacade::changed);
    facade.setChatUnread(QStringLiteral("5"), 0, false);
    QCOMPARE(spy.count(), 0);
}

void ChatListFacadeTest::setChatUnreadIgnoresInvalidChatId()
{
    ChatListFacade facade;
    QSignalSpy spy(&facade, &ChatListFacade::changed);

    facade.setChatUnread(QStringLiteral("abc"), 5, false);
    facade.setChatUnread(QStringLiteral("0"), 5, false);
    facade.setChatUnread(QString(), 5, false);

    QCOMPARE(facade.totalUnread(), 0);
    QCOMPARE(spy.count(), 0);
}

void ChatListFacadeTest::badgeTextFollowsTotal()
{
    ChatListFacade facade;

    facade.setChatUnread(QStringLiteral("1"), 1, false);
    QCOMPARE(facade.badgeText(), QStringLiteral("1"));

    facade.setChatUnread(QStringLiteral("1"), 99, false);
    QCOMPARE(facade.badgeText(), QStringLiteral("99"));

    // Above the cap the label is "99+", not the exact number.
    facade.setChatUnread(QStringLiteral("1"), 100, false);
    QCOMPARE(facade.badgeText(), QStringLiteral("99+"));

    facade.setChatUnread(QStringLiteral("1"), 0, false);
    QCOMPARE(facade.badgeText(), QString());
}

void ChatListFacadeTest::removeChatDropsItsUnread()
{
    ChatListFacade facade;

    facade.setChatUnread(QStringLiteral("1"), 3, false);
    facade.setChatUnread(QStringLiteral("2"), 4, false);
    QCOMPARE(facade.totalUnread(), 7);

    facade.removeChat(QStringLiteral("1"));
    QCOMPARE(facade.totalUnread(), 4);

    facade.removeChat(QStringLiteral("2"));
    QCOMPARE(facade.totalUnread(), 0);
    QCOMPARE(facade.badgeText(), QString());
}

void ChatListFacadeTest::removeUnknownChatIsSilent()
{
    ChatListFacade facade;
    facade.setChatUnread(QStringLiteral("1"), 2, false);

    QSignalSpy spy(&facade, &ChatListFacade::changed);

    facade.removeChat(QStringLiteral("999"));
    facade.removeChat(QStringLiteral("not-an-id"));
    facade.removeChat(QString());

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.totalUnread(), 2);
}

void ChatListFacadeTest::unreadChangesNotifyOncePerTransition()
{
    ChatListFacade facade;
    QSignalSpy spy(&facade, &ChatListFacade::changed);

    facade.setChatUnread(QStringLiteral("1"), 5, false);   // 1
    facade.setChatUnread(QStringLiteral("1"), 5, false);   // no-op
    facade.setChatUnread(QStringLiteral("1"), 5, true);    // 2: muted flag changed
    facade.setChatUnread(QStringLiteral("1"), 5, true);    // no-op
    facade.removeChat(QStringLiteral("1"));                // 3
    facade.removeChat(QStringLiteral("1"));                // no-op

    QCOMPARE(spy.count(), 3);
}

void ChatListFacadeTest::totalUnreadSumsMutedChats()
{
    ChatListFacade facade;

    facade.setChatUnread(QStringLiteral("1"), 5, true);
    facade.setChatUnread(QStringLiteral("2"), 2, false);

    // Muted chats still count towards the total; only notifications differ.
    QCOMPARE(facade.totalUnread(), 7);
    QCOMPARE(facade.badgeText(), QStringLiteral("7"));
}

void ChatListFacadeTest::propertiesAreReachableThroughTheMetaObject()
{
    ChatListFacade facade;

    QCOMPARE(facade.property("sortMode").toString(), QStringLiteral("position"));
    QCOMPARE(facade.property("ascending").toBool(), false);
    QCOMPARE(facade.property("pinnedFirst").toBool(), true);
    QCOMPARE(facade.property("archiveOpen").toBool(), false);
    QCOMPARE(facade.property("archivedCount").toInt(), 0);
    QCOMPARE(facade.property("badgeText").toString(), QString());
    QCOMPARE(facade.property("totalUnread").toInt(), 0);

    // The WRITE accessors QML uses must land in the same place as the setters.
    QVERIFY(facade.setProperty("sortMode", QStringLiteral("name")));
    QCOMPARE(facade.sortMode(), QStringLiteral("name"));

    QVERIFY(facade.setProperty("ascending", true));
    QVERIFY(facade.ascending());

    QVERIFY(facade.setProperty("pinnedFirst", false));
    QVERIFY(!facade.pinnedFirst());

    facade.setArchivedIds({QStringLiteral("11"), QStringLiteral("22")});
    QCOMPARE(facade.property("archivedCount").toInt(), 2);

    facade.setChatUnread(QStringLiteral("11"), 3, false);
    QCOMPARE(facade.property("totalUnread").toInt(), 3);
    QCOMPARE(facade.property("badgeText").toString(), QStringLiteral("3"));
}

QTEST_GUILESS_MAIN(ChatListFacadeTest)
#include "tst_chat_list_facade.moc"
