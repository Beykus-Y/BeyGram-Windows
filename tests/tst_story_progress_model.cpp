#include "telegram/StoryProgressModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class StoryProgressModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsIdle();
    void beginStartsStory();
    void beginRejectsInvalidInput();
    void beginKeepsRunningStoryOnInvalidInput();
    void beginRestartsFromZero();
    void tickAdvancesPosition();
    void tickIgnoredWhenIdle();
    void tickIgnoredWhenPaused();
    void tickClampsAtDuration();
    void tickFinishesOnlyOnce();
    void tickClampsNegativeToZero();
    void pauseResumeTogglesFlag();
    void pauseResumeNoOpWhenIdle();
    void pauseResumeNoOpWhenUnchanged();
    void progressTracksPosition();
    void progressIsZeroWhenIdle();
    void progressAtEndIsOne();
    void resetClearsState();
    void resetOnIdleIsSilent();
    void resetFromFinishedReportsUnfinished();
    void beginFromFinishedClearsFinishedFlag();
    void changedOnlyOnRealChanges();
};

void StoryProgressModelTest::startsIdle()
{
    const StoryProgressModel model;

    QVERIFY(!model.active());
    QCOMPARE(model.storyId(), 0);
    QCOMPARE(model.durationMs(), 0);
    QCOMPARE(model.elapsedMs(), 0);
    QCOMPARE(model.progress(), 0.0);
    QVERIFY(!model.paused());
    QVERIFY(!model.finished());
}

void StoryProgressModelTest::beginStartsStory()
{
    StoryProgressModel model;
    QSignalSpy spy(&model, &StoryProgressModel::changed);

    model.begin(9001, 15000);

    QVERIFY(model.active());
    QCOMPARE(model.storyId(), 9001);
    QCOMPARE(model.durationMs(), 15000);
    QCOMPARE(model.elapsedMs(), 0);
    QVERIFY(!model.paused());
    QVERIFY(!model.finished());
    QCOMPARE(spy.count(), 1);
}

void StoryProgressModelTest::beginRejectsInvalidInput()
{
    StoryProgressModel model;
    QSignalSpy spy(&model, &StoryProgressModel::changed);

    model.begin(0, 5000);
    model.begin(-3, 5000);
    model.begin(7, 0);
    model.begin(7, -1);

    QVERIFY(!model.active());
    QCOMPARE(model.storyId(), 0);
    QCOMPARE(model.durationMs(), 0);
    QCOMPARE(spy.count(), 0);
}

void StoryProgressModelTest::beginKeepsRunningStoryOnInvalidInput()
{
    StoryProgressModel model;
    model.begin(42, 4000);
    model.tick(1000);

    QSignalSpy spy(&model, &StoryProgressModel::changed);

    model.begin(0, 1000);
    model.begin(43, 0);

    QVERIFY(model.active());
    QCOMPARE(model.storyId(), 42);
    QCOMPARE(model.durationMs(), 4000);
    QCOMPARE(model.elapsedMs(), 1000);
    QCOMPARE(spy.count(), 0);
}

void StoryProgressModelTest::beginRestartsFromZero()
{
    StoryProgressModel model;
    model.begin(42, 4000);
    model.tick(2500);
    QCOMPARE(model.elapsedMs(), 2500);

    model.begin(42, 4000);

    QCOMPARE(model.elapsedMs(), 0);
    QCOMPARE(model.storyId(), 42);
    QCOMPARE(model.durationMs(), 4000);
    QVERIFY(!model.finished());
}

void StoryProgressModelTest::tickAdvancesPosition()
{
    StoryProgressModel model;
    model.begin(1, 10000);

    QSignalSpy spy(&model, &StoryProgressModel::changed);

    model.tick(250);
    QCOMPARE(model.elapsedMs(), 250);
    QCOMPARE(spy.count(), 1);

    model.tick(5000);
    QCOMPARE(model.elapsedMs(), 5000);
    QCOMPARE(spy.count(), 2);

    // Position is absolute, not additive.
    model.tick(3000);
    QCOMPARE(model.elapsedMs(), 3000);
    QCOMPARE(spy.count(), 3);
}

void StoryProgressModelTest::tickIgnoredWhenIdle()
{
    StoryProgressModel model;
    QSignalSpy spy(&model, &StoryProgressModel::changed);
    QSignalSpy finishedSpy(&model, &StoryProgressModel::finishedChanged);

    model.tick(100);
    model.tick(100000);

    QCOMPARE(model.elapsedMs(), 0);
    QVERIFY(!model.finished());
    QCOMPARE(spy.count(), 0);
    QCOMPARE(finishedSpy.count(), 0);
}

void StoryProgressModelTest::tickIgnoredWhenPaused()
{
    StoryProgressModel model;
    model.begin(5, 2000);
    model.tick(500);
    model.pause();

    QSignalSpy spy(&model, &StoryProgressModel::changed);

    model.tick(900);
    model.tick(2000);

    QCOMPARE(model.elapsedMs(), 500);
    QVERIFY(!model.finished());
    QCOMPARE(spy.count(), 0);
}

void StoryProgressModelTest::tickClampsAtDuration()
{
    StoryProgressModel model;
    model.begin(3, 1000);
    QSignalSpy finishedSpy(&model, &StoryProgressModel::finishedChanged);

    model.tick(1500);

    QCOMPARE(model.elapsedMs(), 1000);
    QCOMPARE(model.durationMs(), 1000);
    QVERIFY(model.finished());
    QCOMPARE(finishedSpy.count(), 1);

    // A later, smaller position is still clamped inside the story.
    model.tick(2000);
    QCOMPARE(model.elapsedMs(), 1000);
    QCOMPARE(finishedSpy.count(), 1);
}

void StoryProgressModelTest::tickFinishesOnlyOnce()
{
    StoryProgressModel model;
    model.begin(8, 600);
    QSignalSpy finishedSpy(&model, &StoryProgressModel::finishedChanged);

    model.tick(599);
    QCOMPARE(finishedSpy.count(), 0);
    QVERIFY(!model.finished());

    model.tick(600);
    QCOMPARE(finishedSpy.count(), 1);
    QVERIFY(model.finished());

    model.tick(600);
    model.tick(601);
    model.tick(9999);
    QCOMPARE(finishedSpy.count(), 1);
}

void StoryProgressModelTest::tickClampsNegativeToZero()
{
    StoryProgressModel model;
    model.begin(11, 5000);
    model.tick(1200);

    model.tick(-400);

    QCOMPARE(model.elapsedMs(), 0);
    QVERIFY(!model.finished());
    QCOMPARE(model.progress(), 0.0);
}

void StoryProgressModelTest::pauseResumeTogglesFlag()
{
    StoryProgressModel model;
    model.begin(1, 3000);
    model.tick(1000);
    QSignalSpy spy(&model, &StoryProgressModel::changed);

    model.pause();
    QVERIFY(model.paused());
    QCOMPARE(spy.count(), 1);

    model.resume();
    QVERIFY(!model.paused());
    QCOMPARE(spy.count(), 2);

    // Position survived the pause round trip.
    QCOMPARE(model.elapsedMs(), 1000);
    QVERIFY(model.active());
}

void StoryProgressModelTest::pauseResumeNoOpWhenIdle()
{
    StoryProgressModel model;
    QSignalSpy spy(&model, &StoryProgressModel::changed);

    model.pause();
    QVERIFY(!model.paused());
    QCOMPARE(spy.count(), 0);

    model.resume();
    QVERIFY(!model.paused());
    QCOMPARE(spy.count(), 0);
}

void StoryProgressModelTest::pauseResumeNoOpWhenUnchanged()
{
    StoryProgressModel model;
    model.begin(1, 3000);

    QSignalSpy spy(&model, &StoryProgressModel::changed);

    model.resume();
    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.paused());

    model.pause();
    QCOMPARE(spy.count(), 1);

    model.pause();
    QCOMPARE(spy.count(), 1);
    QVERIFY(model.paused());
}

void StoryProgressModelTest::progressTracksPosition()
{
    StoryProgressModel model;
    model.begin(1, 4000);

    QCOMPARE(model.progress(), 0.0);

    model.tick(1000);
    QCOMPARE(model.progress(), 0.25);

    model.tick(2000);
    QCOMPARE(model.progress(), 0.5);

    model.tick(3000);
    QCOMPARE(model.progress(), 0.75);

    // Thirds are not exact in binary floating point.
    StoryProgressModel thirds;
    thirds.begin(2, 3000);
    thirds.tick(1000);
    QVERIFY(qFuzzyCompare(thirds.progress(), 1.0 / 3.0));
}

void StoryProgressModelTest::progressIsZeroWhenIdle()
{
    StoryProgressModel model;
    QCOMPARE(model.progress(), 0.0);

    model.begin(1, 10);
    model.reset();
    QCOMPARE(model.progress(), 0.0);
}

void StoryProgressModelTest::progressAtEndIsOne()
{
    StoryProgressModel model;
    model.begin(1, 1000);
    model.tick(1000);
    QCOMPARE(model.progress(), 1.0);

    model.tick(5000);
    QCOMPARE(model.progress(), 1.0);
}

void StoryProgressModelTest::resetClearsState()
{
    StoryProgressModel model;
    model.begin(77, 2000);
    model.tick(500);
    model.pause();

    QSignalSpy spy(&model, &StoryProgressModel::changed);

    model.reset();

    QVERIFY(!model.active());
    QCOMPARE(model.storyId(), 0);
    QCOMPARE(model.durationMs(), 0);
    QCOMPARE(model.elapsedMs(), 0);
    QVERIFY(!model.paused());
    QVERIFY(!model.finished());
    QCOMPARE(model.progress(), 0.0);
    QCOMPARE(spy.count(), 1);

    // Ticks after a reset are ignored.
    model.tick(100);
    QCOMPARE(model.elapsedMs(), 0);
}

void StoryProgressModelTest::resetOnIdleIsSilent()
{
    StoryProgressModel model;
    QSignalSpy spy(&model, &StoryProgressModel::changed);
    QSignalSpy finishedSpy(&model, &StoryProgressModel::finishedChanged);

    model.reset();
    model.reset();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(finishedSpy.count(), 0);

    model.begin(1, 100);
    QCOMPARE(spy.count(), 1);
    model.reset();
    QCOMPARE(spy.count(), 2);
    model.reset();
    QCOMPARE(spy.count(), 2);
}

void StoryProgressModelTest::resetFromFinishedReportsUnfinished()
{
    StoryProgressModel model;
    model.begin(4, 500);
    model.tick(500);
    QVERIFY(model.finished());

    QSignalSpy finishedSpy(&model, &StoryProgressModel::finishedChanged);

    model.reset();

    QVERIFY(!model.finished());
    QCOMPARE(finishedSpy.count(), 1);
}

void StoryProgressModelTest::beginFromFinishedClearsFinishedFlag()
{
    StoryProgressModel model;
    model.begin(4, 500);
    model.tick(500);

    QSignalSpy finishedSpy(&model, &StoryProgressModel::finishedChanged);

    model.begin(5, 800);

    QCOMPARE(model.storyId(), 5);
    QCOMPARE(model.durationMs(), 800);
    QCOMPARE(model.elapsedMs(), 0);
    QVERIFY(!model.finished());
    QCOMPARE(finishedSpy.count(), 1);
}

void StoryProgressModelTest::changedOnlyOnRealChanges()
{
    StoryProgressModel model;
    QSignalSpy spy(&model, &StoryProgressModel::changed);

    // Idle no-ops.
    model.pause();
    model.resume();
    model.reset();
    model.tick(10);
    QCOMPARE(spy.count(), 0);

    model.begin(1, 1000);
    QCOMPARE(spy.count(), 1);

    // Same position is not a change.
    model.tick(0);
    QCOMPARE(spy.count(), 1);

    model.tick(100);
    QCOMPARE(spy.count(), 2);

    // Duplicate pause is silent.
    model.pause();
    model.pause();
    QCOMPARE(spy.count(), 3);

    // Ticks while paused are silent.
    model.tick(200);
    QCOMPARE(spy.count(), 3);

    model.resume();
    QCOMPARE(spy.count(), 4);

    // Reaching the end is a change once, then silence.
    model.tick(1000);
    QCOMPARE(spy.count(), 5);
    model.tick(1000);
    QCOMPARE(spy.count(), 5);
}

QTEST_GUILESS_MAIN(StoryProgressModelTest)
#include "tst_story_progress_model.moc"
