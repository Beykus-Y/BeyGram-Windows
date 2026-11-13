#include "ui/StoriesFacade.h"

#include <QSignalSpy>
#include <QtTest>

#include <cmath>

using namespace beygram::ui;

namespace {

bool nearly(double actual, double expected)
{
    return std::abs(actual - expected) < 1e-9;
}

} // namespace

class StoriesFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsClosed();
    void openStartsFirstStory();
    void openDropsInvalidIds();
    void openRejectsInvalidRequests();
    void openKeepsRunningSessionForSameRequest();
    void openReplacesSessionAndRestartsProgress();
    void nextMarksSeenAndStartsNextStory();
    void nextPastLastStoryCloses();
    void previousMovesBackAndRestartsProgress();
    void closeClearsState();
    void tickUsesAbsolutePosition();
    void tickAtDurationAdvancesAndMarksSeen();
    void tickAtDurationOnLastStoryCloses();
    void tickIgnoredWhilePaused();
    void setPausedTogglesPause();
    void setPausedIgnoredWhenClosed();
    void seenEmittedOnlyOncePerStory();
    void reactStoresReactionAndRequestsIt();
    void reactReplacesExistingReaction();
    void reactionFollowsCurrentStory();
    void reactionSurvivesCloseAndReopen();
    void reactIgnoresInvalidRequests();
    void clearReactionRemovesAndRequestsEmpty();
    void clearReactionWhenAbsentIsNoOp();
    void changedEmittedOnlyOnRealTransitions();
};

void StoriesFacadeTest::startsClosed()
{
    const StoriesFacade facade;

    QVERIFY(!facade.active());
    QCOMPARE(facade.currentStoryId(), QString());
    QCOMPARE(facade.chatId(), QString());
    QCOMPARE(facade.index(), 0);
    QCOMPARE(facade.count(), 0);
    QCOMPARE(facade.progress(), 0.0);
    QVERIFY(!facade.paused());
    QCOMPARE(facade.reaction(), QString());
}

void StoriesFacadeTest::openStartsFirstStory()
{
    StoriesFacade facade;

    facade.open(QStringLiteral("42"), {QStringLiteral("700"), QStringLiteral("701"),
                                       QStringLiteral("702")});

    QVERIFY(facade.active());
    QCOMPARE(facade.currentStoryId(), QStringLiteral("700"));
    QCOMPARE(facade.chatId(), QStringLiteral("42"));
    QCOMPARE(facade.index(), 0);
    QCOMPARE(facade.count(), 3);
    QCOMPARE(facade.progress(), 0.0);
    QVERIFY(!facade.paused());
    QCOMPARE(facade.reaction(), QString());
}

void StoriesFacadeTest::openDropsInvalidIds()
{
    StoriesFacade facade;

    facade.open(QStringLiteral("9"), {QStringLiteral("0"), QStringLiteral("15"),
                                      QStringLiteral("-3"), QStringLiteral("abc"),
                                      QStringLiteral("16"), QStringLiteral("  ")});

    QVERIFY(facade.active());
    QCOMPARE(facade.count(), 2);
    QCOMPARE(facade.currentStoryId(), QStringLiteral("15"));

    // Repeated ids must not be shown twice, the first position wins.
    facade.open(QStringLiteral("9"), {QStringLiteral("15"), QStringLiteral("16"),
                                      QStringLiteral("15")});
    QCOMPARE(facade.count(), 2);
    QCOMPARE(facade.currentStoryId(), QStringLiteral("15"));
}

void StoriesFacadeTest::openRejectsInvalidRequests()
{
    StoriesFacade facade;
    QSignalSpy spy(&facade, &StoriesFacade::changed);

    facade.open(QStringLiteral("0"), {QStringLiteral("10")});
    facade.open(QStringLiteral("-4"), {QStringLiteral("10")});
    facade.open(QStringLiteral("abc"), {QStringLiteral("10")});
    facade.open(QStringLiteral("5"), {});
    facade.open(QStringLiteral("5"), {QStringLiteral("0"), QStringLiteral("-1")});

    QVERIFY(!facade.active());
    QCOMPARE(facade.count(), 0);
    QCOMPARE(spy.count(), 0);

    // An invalid request must not disturb a session that is already open.
    facade.open(QStringLiteral("5"), {QStringLiteral("50")});
    QVERIFY(facade.active());
    QCOMPARE(spy.count(), 1);

    facade.open(QStringLiteral("5"), {QStringLiteral("abc")});
    QVERIFY(facade.active());
    QCOMPARE(facade.currentStoryId(), QStringLiteral("50"));
    QCOMPARE(spy.count(), 1);
}

void StoriesFacadeTest::openKeepsRunningSessionForSameRequest()
{
    StoriesFacade facade;
    QSignalSpy spy(&facade, &StoriesFacade::changed);

    facade.open(QStringLiteral("7"), {QStringLiteral("10"), QStringLiteral("20"),
                                      QStringLiteral("30")});
    facade.tick(2000);
    QVERIFY(nearly(facade.progress(), 0.4));
    QCOMPARE(spy.count(), 2);

    facade.open(QStringLiteral("7"), {QStringLiteral("10"), QStringLiteral("20"),
                                      QStringLiteral("30")});
    QCOMPARE(spy.count(), 2);
    QCOMPARE(facade.index(), 0);
    QVERIFY(nearly(facade.progress(), 0.4));

    // Ids that normalise to the same list are the same request too.
    facade.open(QStringLiteral("7"), {QStringLiteral("10"), QStringLiteral("0"),
                                      QStringLiteral("20"), QStringLiteral("30"),
                                      QStringLiteral("-1")});
    QCOMPARE(spy.count(), 2);
    QVERIFY(nearly(facade.progress(), 0.4));
}

void StoriesFacadeTest::openReplacesSessionAndRestartsProgress()
{
    StoriesFacade facade;
    facade.open(QStringLiteral("7"), {QStringLiteral("10"), QStringLiteral("20")});
    facade.tick(2000);

    // Same chat, different stories: the session restarts from the top.
    facade.open(QStringLiteral("7"), {QStringLiteral("99")});
    QVERIFY(facade.active());
    QCOMPARE(facade.currentStoryId(), QStringLiteral("99"));
    QCOMPARE(facade.index(), 0);
    QCOMPARE(facade.count(), 1);
    QCOMPARE(facade.progress(), 0.0);

    // A different chat also replaces the session.
    facade.open(QStringLiteral("8"), {QStringLiteral("10"), QStringLiteral("20")});
    QCOMPARE(facade.currentStoryId(), QStringLiteral("10"));
    QCOMPARE(facade.count(), 2);
    QCOMPARE(facade.progress(), 0.0);
}

void StoriesFacadeTest::nextMarksSeenAndStartsNextStory()
{
    StoriesFacade facade;
    QSignalSpy seenSpy(&facade, &StoriesFacade::seen);

    facade.open(QStringLiteral("1"), {QStringLiteral("10"), QStringLiteral("20"),
                                      QStringLiteral("30")});
    facade.tick(1000);

    facade.next();

    QVERIFY(facade.active());
    QCOMPARE(facade.index(), 1);
    QCOMPARE(facade.currentStoryId(), QStringLiteral("20"));
    QCOMPARE(facade.progress(), 0.0);
    QCOMPARE(seenSpy.count(), 1);
    QCOMPARE(seenSpy.first().at(0).toString(), QStringLiteral("10"));

    facade.next();
    QCOMPARE(facade.index(), 2);
    QCOMPARE(facade.currentStoryId(), QStringLiteral("30"));
    QCOMPARE(seenSpy.count(), 2);
    QCOMPARE(seenSpy.last().at(0).toString(), QStringLiteral("20"));
}

void StoriesFacadeTest::nextPastLastStoryCloses()
{
    StoriesFacade facade;
    QSignalSpy seenSpy(&facade, &StoriesFacade::seen);

    facade.open(QStringLiteral("1"), {QStringLiteral("10")});
    facade.tick(2000);
    facade.next();

    QVERIFY(!facade.active());
    QCOMPARE(facade.currentStoryId(), QString());
    QCOMPARE(facade.index(), 0);
    QCOMPARE(facade.count(), 0);
    QCOMPARE(facade.progress(), 0.0);
    QVERIFY(!facade.paused());
    QCOMPARE(seenSpy.count(), 1);
    QCOMPARE(seenSpy.first().at(0).toString(), QStringLiteral("10"));
}

void StoriesFacadeTest::previousMovesBackAndRestartsProgress()
{
    StoriesFacade facade;
    QSignalSpy seenSpy(&facade, &StoriesFacade::seen);

    facade.open(QStringLiteral("1"), {QStringLiteral("10"), QStringLiteral("20"),
                                      QStringLiteral("30")});

    // Already on the first story: no wrap around and no progress restart.
    facade.tick(1000);
    facade.previous();
    QCOMPARE(facade.index(), 0);
    QVERIFY(nearly(facade.progress(), 0.2));

    facade.next();
    facade.tick(2500);
    QCOMPARE(facade.index(), 1);
    QVERIFY(nearly(facade.progress(), 0.5));

    facade.previous();
    QCOMPARE(facade.index(), 0);
    QCOMPARE(facade.currentStoryId(), QStringLiteral("10"));
    QCOMPARE(facade.progress(), 0.0);

    // Going back never marks anything seen.
    QCOMPARE(seenSpy.count(), 1);
}

void StoriesFacadeTest::closeClearsState()
{
    StoriesFacade facade;
    facade.open(QStringLiteral("3"), {QStringLiteral("30"), QStringLiteral("31")});
    facade.tick(1000);
    facade.setPaused(true);
    facade.react(QStringLiteral("👍"));

    QSignalSpy seenSpy(&facade, &StoriesFacade::seen);
    QSignalSpy requestSpy(&facade, &StoriesFacade::reactionRequested);

    facade.close();

    QVERIFY(!facade.active());
    QCOMPARE(facade.currentStoryId(), QString());
    QCOMPARE(facade.index(), 0);
    QCOMPARE(facade.count(), 0);
    QCOMPARE(facade.progress(), 0.0);
    QVERIFY(!facade.paused());
    QCOMPARE(facade.reaction(), QString());

    // A closed viewer ignores every command.
    facade.close();
    facade.next();
    facade.previous();
    facade.tick(1000);
    facade.setPaused(true);
    facade.react(QStringLiteral("🔥"));
    facade.clearReaction();

    QVERIFY(!facade.active());
    QCOMPARE(seenSpy.count(), 0);
    QCOMPARE(requestSpy.count(), 0);
}

void StoriesFacadeTest::tickUsesAbsolutePosition()
{
    StoriesFacade facade;
    facade.open(QStringLiteral("1"), {QStringLiteral("10")});

    facade.tick(2500);
    QVERIFY(nearly(facade.progress(), 0.5));

    // Positions are absolute, not deltas: a smaller offset moves the bar back.
    facade.tick(1000);
    QVERIFY(nearly(facade.progress(), 0.2));

    facade.tick(-50);
    QCOMPARE(facade.progress(), 0.0);
}

void StoriesFacadeTest::tickAtDurationAdvancesAndMarksSeen()
{
    StoriesFacade facade;
    QSignalSpy seenSpy(&facade, &StoriesFacade::seen);

    facade.open(QStringLiteral("1"), {QStringLiteral("10"), QStringLiteral("20")});
    facade.tick(StoriesFacade::StoryDurationMs);

    QVERIFY(facade.active());
    QCOMPARE(facade.index(), 1);
    QCOMPARE(facade.currentStoryId(), QStringLiteral("20"));
    QCOMPARE(facade.progress(), 0.0);
    QCOMPARE(seenSpy.count(), 1);
    QCOMPARE(seenSpy.first().at(0).toString(), QStringLiteral("10"));
}

void StoriesFacadeTest::tickAtDurationOnLastStoryCloses()
{
    StoriesFacade facade;
    QSignalSpy seenSpy(&facade, &StoriesFacade::seen);

    facade.open(QStringLiteral("1"), {QStringLiteral("10")});
    facade.tick(StoriesFacade::StoryDurationMs);

    QVERIFY(!facade.active());
    QCOMPARE(facade.progress(), 0.0);
    QCOMPARE(seenSpy.count(), 1);
}

void StoriesFacadeTest::tickIgnoredWhilePaused()
{
    StoriesFacade facade;
    facade.open(QStringLiteral("1"), {QStringLiteral("10")});
    facade.tick(1000);
    facade.setPaused(true);

    facade.tick(4000);
    QVERIFY(nearly(facade.progress(), 0.2));
    QVERIFY(facade.active());

    facade.setPaused(false);
    facade.tick(3000);
    QVERIFY(nearly(facade.progress(), 0.6));
}

void StoriesFacadeTest::setPausedTogglesPause()
{
    StoriesFacade facade;
    facade.open(QStringLiteral("1"), {QStringLiteral("10"), QStringLiteral("20")});
    QSignalSpy spy(&facade, &StoriesFacade::changed);

    QVERIFY(!facade.paused());

    facade.setPaused(true);
    QVERIFY(facade.paused());
    QCOMPARE(spy.count(), 1);

    facade.setPaused(true);
    QCOMPARE(spy.count(), 1);

    facade.setPaused(false);
    QVERIFY(!facade.paused());
    QCOMPARE(spy.count(), 2);

    facade.setPaused(false);
    QCOMPARE(spy.count(), 2);

    // Pausing does not move the story on.
    QCOMPARE(facade.currentStoryId(), QStringLiteral("10"));
}

void StoriesFacadeTest::setPausedIgnoredWhenClosed()
{
    StoriesFacade facade;
    QSignalSpy spy(&facade, &StoriesFacade::changed);

    facade.setPaused(true);

    QVERIFY(!facade.paused());
    QCOMPARE(spy.count(), 0);
}

void StoriesFacadeTest::seenEmittedOnlyOncePerStory()
{
    StoriesFacade facade;
    QSignalSpy seenSpy(&facade, &StoriesFacade::seen);

    facade.open(QStringLiteral("1"), {QStringLiteral("10"), QStringLiteral("20")});

    facade.next();
    QCOMPARE(seenSpy.count(), 1);

    // Revisiting a story that was already marked seen must not report it again.
    facade.previous();
    facade.next();
    QCOMPARE(seenSpy.count(), 1);
    QCOMPARE(facade.currentStoryId(), QStringLiteral("20"));

    facade.next();
    QCOMPARE(seenSpy.count(), 2);
    QCOMPARE(seenSpy.last().at(0).toString(), QStringLiteral("20"));
    QVERIFY(!facade.active());
}

void StoriesFacadeTest::reactStoresReactionAndRequestsIt()
{
    StoriesFacade facade;
    facade.open(QStringLiteral("7"), {QStringLiteral("70"), QStringLiteral("71")});

    QSignalSpy requestSpy(&facade, &StoriesFacade::reactionRequested);
    QSignalSpy changedSpy(&facade, &StoriesFacade::changed);

    facade.react(QStringLiteral("👍"));

    QCOMPARE(facade.reaction(), QStringLiteral("👍"));
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(requestSpy.first().at(0).toString(), QStringLiteral("70"));
    QCOMPARE(requestSpy.first().at(1).toString(), QStringLiteral("👍"));

    // Repeating the stored emoji neither changes the state nor requests it.
    facade.react(QStringLiteral("👍"));
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(requestSpy.count(), 1);

    // The emoji is trimmed on the way in.
    facade.react(QStringLiteral("  🔥  "));
    QCOMPARE(facade.reaction(), QStringLiteral("🔥"));
    QCOMPARE(requestSpy.count(), 2);
    QCOMPARE(requestSpy.last().at(1).toString(), QStringLiteral("🔥"));
}

void StoriesFacadeTest::reactReplacesExistingReaction()
{
    StoriesFacade facade;
    facade.open(QStringLiteral("7"), {QStringLiteral("70")});
    QSignalSpy requestSpy(&facade, &StoriesFacade::reactionRequested);

    facade.react(QStringLiteral("👍"));
    facade.react(QStringLiteral("🔥"));

    QCOMPARE(facade.reaction(), QStringLiteral("🔥"));
    QCOMPARE(requestSpy.count(), 2);
    QCOMPARE(requestSpy.last().at(0).toString(), QStringLiteral("70"));
    QCOMPARE(requestSpy.last().at(1).toString(), QStringLiteral("🔥"));
}

void StoriesFacadeTest::reactionFollowsCurrentStory()
{
    StoriesFacade facade;
    facade.open(QStringLiteral("7"), {QStringLiteral("70"), QStringLiteral("71")});

    facade.react(QStringLiteral("👍"));
    QCOMPARE(facade.reaction(), QStringLiteral("👍"));

    facade.next();
    QCOMPARE(facade.currentStoryId(), QStringLiteral("71"));
    QCOMPARE(facade.reaction(), QString());

    facade.react(QStringLiteral("🔥"));
    QCOMPARE(facade.reaction(), QStringLiteral("🔥"));

    facade.previous();
    QCOMPARE(facade.currentStoryId(), QStringLiteral("70"));
    QCOMPARE(facade.reaction(), QStringLiteral("👍"));
}

void StoriesFacadeTest::reactionSurvivesCloseAndReopen()
{
    StoriesFacade facade;
    facade.open(QStringLiteral("7"), {QStringLiteral("70")});
    facade.react(QStringLiteral("👍"));

    facade.close();
    QCOMPARE(facade.reaction(), QString());

    facade.open(QStringLiteral("7"), {QStringLiteral("70")});
    QCOMPARE(facade.reaction(), QStringLiteral("👍"));
}

void StoriesFacadeTest::reactIgnoresInvalidRequests()
{
    StoriesFacade facade;
    QSignalSpy requestSpy(&facade, &StoriesFacade::reactionRequested);

    // Closed viewer.
    facade.react(QStringLiteral("👍"));
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(facade.reaction(), QString());

    // Empty emoji, with and without whitespace.
    facade.open(QStringLiteral("7"), {QStringLiteral("70")});
    facade.react(QString());
    facade.react(QStringLiteral("   "));
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(facade.reaction(), QString());
}

void StoriesFacadeTest::clearReactionRemovesAndRequestsEmpty()
{
    StoriesFacade facade;
    facade.open(QStringLiteral("7"), {QStringLiteral("70")});
    facade.react(QStringLiteral("👍"));

    QSignalSpy requestSpy(&facade, &StoriesFacade::reactionRequested);
    QSignalSpy changedSpy(&facade, &StoriesFacade::changed);

    facade.clearReaction();

    QCOMPARE(facade.reaction(), QString());
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(requestSpy.first().at(0).toString(), QStringLiteral("70"));
    QCOMPARE(requestSpy.first().at(1).toString(), QString());

    // Clearing again is a no-op.
    facade.clearReaction();
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(requestSpy.count(), 1);
}

void StoriesFacadeTest::clearReactionWhenAbsentIsNoOp()
{
    StoriesFacade facade;
    facade.open(QStringLiteral("7"), {QStringLiteral("70")});
    QSignalSpy requestSpy(&facade, &StoriesFacade::reactionRequested);

    facade.clearReaction();

    QCOMPARE(facade.reaction(), QString());
    QCOMPARE(requestSpy.count(), 0);
}

void StoriesFacadeTest::changedEmittedOnlyOnRealTransitions()
{
    StoriesFacade facade;
    QSignalSpy spy(&facade, &StoriesFacade::changed);

    facade.open(QStringLiteral("3"), {QStringLiteral("30"), QStringLiteral("31"),
                                      QStringLiteral("32")});
    QCOMPARE(spy.count(), 1);

    // Reopening the same session changes nothing.
    facade.open(QStringLiteral("3"), {QStringLiteral("30"), QStringLiteral("31"),
                                      QStringLiteral("32")});
    QCOMPARE(spy.count(), 1);

    facade.tick(1000);
    QCOMPARE(spy.count(), 2);

    // The same absolute offset is not a new position.
    facade.tick(1000);
    QCOMPARE(spy.count(), 2);

    facade.setPaused(true);
    QCOMPARE(spy.count(), 3);

    facade.setPaused(true);
    QCOMPARE(spy.count(), 3);

    facade.react(QStringLiteral("👍"));
    QCOMPARE(spy.count(), 4);

    facade.react(QStringLiteral("👍"));
    QCOMPARE(spy.count(), 4);

    facade.close();
    QCOMPARE(spy.count(), 5);

    facade.close();
    QCOMPARE(spy.count(), 5);
}

QTEST_GUILESS_MAIN(StoriesFacadeTest)
#include "tst_stories_facade.moc"
