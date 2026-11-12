#include "telegram/StoryViewerController.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class StoryViewerControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsClosed();
    void openStartsAtFirstStory();
    void openDropsInvalidIds();
    void openRejectsInvalidRequests();
    void openKeepsExistingSessionForSameRequest();
    void openReplacesSession();
    void nextAdvancesThroughStories();
    void nextPastLastStoryCloses();
    void previousStopsAtFirstStory();
    void navigationRequiresOpenViewer();
    void markSeenTracksSeenStories();
    void markSeenIgnoresUnknownDuplicateAndInvalidIds();
    void markSeenRequiresOpenViewer();
    void unseenListsUnseenStoriesInOrder();
    void setPausedTogglesPause();
    void setPausedRequiresOpenViewer();
    void closeClearsViewer();
    void closeWhenClosedIsNoOp();
    void closedIsEmittedBeforeChanged();
    void emitsChangedOnlyOnRealTransitions();
};

void StoryViewerControllerTest::startsClosed()
{
    const StoryViewerController controller;

    QVERIFY(!controller.active());
    QCOMPARE(controller.chatId(), qint64(0));
    QCOMPARE(controller.storyIds(), QVector<qint64>{});
    QCOMPARE(controller.currentIndex(), 0);
    QCOMPARE(controller.currentStoryId(), qint64(0));
    QCOMPARE(controller.unseen(), QVector<qint64>{});
    QVERIFY(!controller.paused());
}

void StoryViewerControllerTest::openStartsAtFirstStory()
{
    StoryViewerController controller;

    controller.open(42, {700, 701, 702});

    QVERIFY(controller.active());
    QCOMPARE(controller.chatId(), qint64(42));
    QCOMPARE(controller.storyIds(), (QVector<qint64>{700, 701, 702}));
    QCOMPARE(controller.currentIndex(), 0);
    QCOMPARE(controller.currentStoryId(), qint64(700));
    QCOMPARE(controller.unseen(), (QVector<qint64>{700, 701, 702}));
    QVERIFY(!controller.paused());
}

void StoryViewerControllerTest::openDropsInvalidIds()
{
    StoryViewerController controller;

    controller.open(9, {0, 15, -3, 16, 0});

    QVERIFY(controller.active());
    QCOMPARE(controller.chatId(), qint64(9));
    QCOMPARE(controller.storyIds(), (QVector<qint64>{15, 16}));
    QCOMPARE(controller.currentStoryId(), qint64(15));

    // Repeated ids must not be shown twice, the first position wins.
    controller.open(9, {15, 16, 15});
    QCOMPARE(controller.storyIds(), (QVector<qint64>{15, 16}));
}

void StoryViewerControllerTest::openRejectsInvalidRequests()
{
    StoryViewerController controller;
    QSignalSpy spy(&controller, &StoryViewerController::changed);

    controller.open(0, {10, 20});
    controller.open(-4, {10, 20});
    controller.open(1, {});
    controller.open(1, {0, -1});

    QVERIFY(!controller.active());
    QCOMPARE(controller.chatId(), qint64(0));
    QCOMPARE(controller.storyIds(), QVector<qint64>{});
    QCOMPARE(spy.count(), 0);

    // An invalid request must not disturb a session that is already open.
    controller.open(5, {50, 51});
    controller.next();
    controller.markSeen(51);
    QCOMPARE(spy.count(), 3);

    controller.open(0, {60});
    controller.open(-1, {60});
    controller.open(5, {});
    controller.open(5, {0, -60});

    QVERIFY(controller.active());
    QCOMPARE(controller.chatId(), qint64(5));
    QCOMPARE(controller.storyIds(), (QVector<qint64>{50, 51}));
    QCOMPARE(controller.currentIndex(), 1);
    QCOMPARE(controller.currentStoryId(), qint64(51));
    QCOMPARE(spy.count(), 3);
}

void StoryViewerControllerTest::openKeepsExistingSessionForSameRequest()
{
    StoryViewerController controller;
    controller.open(7, {10, 20, 30});
    controller.next();
    controller.markSeen(20);
    controller.setPaused(true);

    QSignalSpy spy(&controller, &StoryViewerController::changed);

    controller.open(7, {10, 20, 30});
    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.currentIndex(), 1);
    QCOMPARE(controller.currentStoryId(), qint64(20));
    QVERIFY(controller.paused());
    QCOMPARE(controller.unseen(), (QVector<qint64>{10, 30}));

    // Ids that normalise to the same list are the same request too.
    controller.open(7, {10, 0, 20, 30, -1});
    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.currentIndex(), 1);
    QCOMPARE(controller.unseen(), (QVector<qint64>{10, 30}));
}

void StoryViewerControllerTest::openReplacesSession()
{
    StoryViewerController controller;
    QSignalSpy spy(&controller, &StoryViewerController::changed);

    controller.open(7, {10, 20});
    controller.next();
    controller.markSeen(10);
    controller.setPaused(true);
    QCOMPARE(spy.count(), 4);

    // Same chat, different stories: the session restarts from the top.
    controller.open(7, {99});
    QCOMPARE(spy.count(), 5);
    QCOMPARE(controller.chatId(), qint64(7));
    QCOMPARE(controller.storyIds(), (QVector<qint64>{99}));
    QCOMPARE(controller.currentIndex(), 0);
    QCOMPARE(controller.currentStoryId(), qint64(99));
    QCOMPARE(controller.unseen(), (QVector<qint64>{99}));
    QVERIFY(!controller.paused());

    // A different chat also replaces the session.
    controller.next();
    QVERIFY(!controller.active());

    controller.open(8, {10, 20});
    controller.next();
    controller.markSeen(20);
    QCOMPARE(controller.unseen(), (QVector<qint64>{10}));

    controller.open(9, {10, 20});
    QCOMPARE(controller.chatId(), qint64(9));
    QCOMPARE(controller.currentIndex(), 0);
    QCOMPARE(controller.unseen(), (QVector<qint64>{10, 20}));
}

void StoryViewerControllerTest::nextAdvancesThroughStories()
{
    StoryViewerController controller;
    controller.open(1, {10, 20, 30});

    controller.next();
    QVERIFY(controller.active());
    QCOMPARE(controller.currentIndex(), 1);
    QCOMPARE(controller.currentStoryId(), qint64(20));

    controller.next();
    QVERIFY(controller.active());
    QCOMPARE(controller.currentIndex(), 2);
    QCOMPARE(controller.currentStoryId(), qint64(30));
}

void StoryViewerControllerTest::nextPastLastStoryCloses()
{
    StoryViewerController controller;
    QSignalSpy closedSpy(&controller, &StoryViewerController::closed);
    QSignalSpy changedSpy(&controller, &StoryViewerController::changed);

    controller.open(1, {10, 20});
    controller.next();
    QCOMPARE(closedSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 2);

    controller.next();

    QCOMPARE(closedSpy.count(), 1);
    QCOMPARE(changedSpy.count(), 3);
    QVERIFY(!controller.active());
    QCOMPARE(controller.chatId(), qint64(0));
    QCOMPARE(controller.storyIds(), QVector<qint64>{});
    QCOMPARE(controller.currentIndex(), 0);
    QCOMPARE(controller.currentStoryId(), qint64(0));
    QCOMPARE(controller.unseen(), QVector<qint64>{});
    QVERIFY(!controller.paused());

    // A single story session closes on the very first advance.
    controller.open(2, {40});
    controller.next();
    QVERIFY(!controller.active());
    QCOMPARE(closedSpy.count(), 2);
    QCOMPARE(changedSpy.count(), 5);
}

void StoryViewerControllerTest::previousStopsAtFirstStory()
{
    StoryViewerController controller;
    controller.open(1, {10, 20, 30});

    QSignalSpy spy(&controller, &StoryViewerController::changed);

    controller.previous();
    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.currentIndex(), 0);

    controller.next();
    controller.next();
    QCOMPARE(controller.currentIndex(), 2);
    QCOMPARE(spy.count(), 2);

    controller.previous();
    QCOMPARE(controller.currentIndex(), 1);
    QCOMPARE(controller.currentStoryId(), qint64(20));

    controller.previous();
    QCOMPARE(controller.currentIndex(), 0);
    QCOMPARE(controller.currentStoryId(), qint64(10));

    // Already on the first story: no wrap around to the last one.
    controller.previous();
    QCOMPARE(spy.count(), 4);
    QCOMPARE(controller.currentIndex(), 0);
    QCOMPARE(controller.currentStoryId(), qint64(10));
    QVERIFY(controller.active());
}

void StoryViewerControllerTest::navigationRequiresOpenViewer()
{
    StoryViewerController controller;
    QSignalSpy spy(&controller, &StoryViewerController::changed);

    controller.next();
    controller.previous();
    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.currentIndex(), 0);

    controller.open(1, {10, 20});
    controller.close();
    QCOMPARE(spy.count(), 2);

    controller.next();
    controller.previous();
    QCOMPARE(spy.count(), 2);
    QVERIFY(!controller.active());
    QCOMPARE(controller.currentIndex(), 0);
}

void StoryViewerControllerTest::markSeenTracksSeenStories()
{
    StoryViewerController controller;
    controller.open(1, {10, 20, 30});

    controller.markSeen(20);
    QCOMPARE(controller.unseen(), (QVector<qint64>{10, 30}));
    QCOMPARE(controller.currentStoryId(), qint64(10));

    controller.markSeen(10);
    QCOMPARE(controller.unseen(), (QVector<qint64>{30}));

    // Seen stories stay remembered while the session moves on.
    controller.next();
    controller.next();
    QCOMPARE(controller.currentStoryId(), qint64(30));
    QCOMPARE(controller.unseen(), (QVector<qint64>{30}));
}

void StoryViewerControllerTest::markSeenIgnoresUnknownDuplicateAndInvalidIds()
{
    StoryViewerController controller;
    controller.open(1, {10, 20});
    QSignalSpy spy(&controller, &StoryViewerController::changed);

    controller.markSeen(20);
    QCOMPARE(spy.count(), 1);

    controller.markSeen(20);
    QCOMPARE(spy.count(), 1);

    controller.markSeen(99);
    controller.markSeen(0);
    controller.markSeen(-5);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.unseen(), (QVector<qint64>{10}));
}

void StoryViewerControllerTest::markSeenRequiresOpenViewer()
{
    StoryViewerController controller;
    QSignalSpy spy(&controller, &StoryViewerController::changed);

    controller.markSeen(10);
    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.unseen(), QVector<qint64>{});

    controller.open(1, {10});
    controller.close();
    QCOMPARE(spy.count(), 2);

    controller.markSeen(10);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(controller.unseen(), QVector<qint64>{});
}

void StoryViewerControllerTest::unseenListsUnseenStoriesInOrder()
{
    StoryViewerController controller;
    controller.open(1, {30, 10, 20});

    QCOMPARE(controller.unseen(), (QVector<qint64>{30, 10, 20}));

    controller.markSeen(10);
    QCOMPARE(controller.unseen(), (QVector<qint64>{30, 20}));

    controller.markSeen(30);
    QCOMPARE(controller.unseen(), (QVector<qint64>{20}));

    controller.markSeen(20);
    QCOMPARE(controller.unseen(), QVector<qint64>{});
}

void StoryViewerControllerTest::setPausedTogglesPause()
{
    StoryViewerController controller;
    controller.open(1, {10, 20});
    QSignalSpy spy(&controller, &StoryViewerController::changed);

    QVERIFY(!controller.paused());

    controller.setPaused(true);
    QVERIFY(controller.paused());
    QCOMPARE(spy.count(), 1);

    controller.setPaused(true);
    QCOMPARE(spy.count(), 1);

    controller.setPaused(false);
    QVERIFY(!controller.paused());
    QCOMPARE(spy.count(), 2);

    controller.setPaused(false);
    QCOMPARE(spy.count(), 2);
}

void StoryViewerControllerTest::setPausedRequiresOpenViewer()
{
    StoryViewerController controller;
    QSignalSpy spy(&controller, &StoryViewerController::changed);

    controller.setPaused(true);
    QVERIFY(!controller.paused());
    QCOMPARE(spy.count(), 0);
}

void StoryViewerControllerTest::closeClearsViewer()
{
    StoryViewerController controller;
    controller.open(3, {30, 31, 32});
    controller.next();
    controller.markSeen(30);
    controller.setPaused(true);

    QSignalSpy closedSpy(&controller, &StoryViewerController::closed);
    QSignalSpy changedSpy(&controller, &StoryViewerController::changed);

    controller.close();

    QCOMPARE(closedSpy.count(), 1);
    QCOMPARE(changedSpy.count(), 1);
    QVERIFY(!controller.active());
    QCOMPARE(controller.chatId(), qint64(0));
    QCOMPARE(controller.storyIds(), QVector<qint64>{});
    QCOMPARE(controller.currentIndex(), 0);
    QCOMPARE(controller.currentStoryId(), qint64(0));
    QCOMPARE(controller.unseen(), QVector<qint64>{});
    QVERIFY(!controller.paused());

    // The viewer can be opened again after closing.
    controller.open(4, {40});
    QVERIFY(controller.active());
    QCOMPARE(controller.chatId(), qint64(4));
    QCOMPARE(controller.currentStoryId(), qint64(40));
}

void StoryViewerControllerTest::closeWhenClosedIsNoOp()
{
    StoryViewerController controller;
    QSignalSpy closedSpy(&controller, &StoryViewerController::closed);
    QSignalSpy changedSpy(&controller, &StoryViewerController::changed);

    controller.close();
    controller.close();

    QCOMPARE(closedSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 0);
}

void StoryViewerControllerTest::closedIsEmittedBeforeChanged()
{
    StoryViewerController controller;
    controller.open(1, {10});
    QSignalSpy changedSpy(&controller, &StoryViewerController::changed);

    int changedWhenClosed = -1;
    connect(&controller, &StoryViewerController::closed, &controller, [&changedSpy, &changedWhenClosed] {
        changedWhenClosed = changedSpy.count();
    });

    controller.close();
    QCOMPARE(changedWhenClosed, 0);
    QCOMPARE(changedSpy.count(), 1);

    // Advancing past the last story goes through the same path.
    controller.open(1, {10});
    changedWhenClosed = -1;
    controller.next();
    QCOMPARE(changedWhenClosed, 2);
    QCOMPARE(changedSpy.count(), 3);
}

void StoryViewerControllerTest::emitsChangedOnlyOnRealTransitions()
{
    StoryViewerController controller;
    QSignalSpy spy(&controller, &StoryViewerController::changed);

    controller.open(3, {30, 31, 32});
    QCOMPARE(spy.count(), 1);

    // Reopening the same session changes nothing.
    controller.open(3, {30, 31, 32});
    QCOMPARE(spy.count(), 1);

    controller.next();
    QCOMPARE(spy.count(), 2);

    controller.previous();
    QCOMPARE(spy.count(), 3);

    // Staying on the first story changes nothing.
    controller.previous();
    QCOMPARE(spy.count(), 3);

    controller.markSeen(30);
    QCOMPARE(spy.count(), 4);

    controller.markSeen(30);
    QCOMPARE(spy.count(), 4);

    controller.setPaused(true);
    QCOMPARE(spy.count(), 5);

    controller.setPaused(true);
    QCOMPARE(spy.count(), 5);

    controller.close();
    QCOMPARE(spy.count(), 6);

    // Closed viewer ignores everything.
    controller.next();
    controller.previous();
    controller.markSeen(30);
    controller.setPaused(true);
    controller.close();
    QCOMPARE(spy.count(), 6);

    controller.open(4, {40, 41});
    QCOMPARE(spy.count(), 7);
}

QTEST_GUILESS_MAIN(StoryViewerControllerTest)
#include "tst_story_viewer_controller.moc"
