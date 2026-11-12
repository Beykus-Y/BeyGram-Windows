#include "telegram/CallController.h"

#include <QSignalSpy>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

class CallControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsIdle();
    void startOutgoingConnects();
    void incomingRings();
    void startOutgoingSameCallIsSilent();
    void rejectsNonPositiveIds();
    void acceptFromConnectingActivates();
    void acceptFromRingingActivates();
    void acceptIgnoredOutsideSetup();
    void declineEndsRingingCall();
    void endEndsActiveCall();
    void declineAndEndAreNoOpsWhenIdle();
    void declineAndEndAreNoOpsWhenEnded();
    void endedRetainsCallIdentity();
    void newCallResetsTransientState();
    void newCallReplacesPreviousCall();
    void muteAndSpeakerToggle();
    void settersIgnoredWhenIdle();
    void elapsedTracksDuration();
    void elapsedClampsNegative();
    void elapsedIgnoredWhenIdle();
    void elapsedIsPerCall();
    void phaseNames();
    void signalsOnlyOnRealChange();
    void videoFlagFollowsCall();
};

void CallControllerTest::startsIdle()
{
    const CallController controller;

    QCOMPARE(controller.phase(), CallPhase::Idle);
    QCOMPARE(controller.phaseName(), QStringLiteral("idle"));
    QCOMPARE(controller.callId(), qint64(0));
    QCOMPARE(controller.userId(), qint64(0));
    QVERIFY(!controller.video());
    QVERIFY(!controller.muted());
    QVERIFY(!controller.speaker());
    QCOMPARE(controller.elapsedSeconds(), 0);
}

void CallControllerTest::startOutgoingConnects()
{
    CallController controller;

    controller.startOutgoing(7, 42, true);

    // Outgoing means ringing for the peer, so the local side is connecting.
    QCOMPARE(controller.phase(), CallPhase::Connecting);
    QCOMPARE(controller.phaseName(), QStringLiteral("connecting"));
    QCOMPARE(controller.callId(), qint64(7));
    QCOMPARE(controller.userId(), qint64(42));
    QVERIFY(controller.video());
}

void CallControllerTest::incomingRings()
{
    CallController controller;

    controller.incoming(9, 43, false);

    QCOMPARE(controller.phase(), CallPhase::Ringing);
    QCOMPARE(controller.phaseName(), QStringLiteral("ringing"));
    QCOMPARE(controller.callId(), qint64(9));
    QCOMPARE(controller.userId(), qint64(43));
    QVERIFY(!controller.video());
}

void CallControllerTest::startOutgoingSameCallIsSilent()
{
    CallController controller;
    controller.startOutgoing(7, 42, false);

    QSignalSpy spy(&controller, &CallController::changed);
    controller.startOutgoing(7, 42, false);

    // Same call, same state: nothing observable moved.
    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.phase(), CallPhase::Connecting);
}

void CallControllerTest::rejectsNonPositiveIds()
{
    CallController controller;
    QSignalSpy spy(&controller, &CallController::changed);

    controller.startOutgoing(0, 42, false);
    controller.startOutgoing(7, 0, false);
    controller.startOutgoing(-1, -2, false);
    controller.incoming(0, 42, false);
    controller.incoming(9, -42, false);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.phase(), CallPhase::Idle);
    QCOMPARE(controller.callId(), qint64(0));
    QCOMPARE(controller.userId(), qint64(0));

    // A rejected start must not break a call already in progress either.
    controller.startOutgoing(7, 42, false);
    controller.startOutgoing(0, 42, true);

    QCOMPARE(controller.phase(), CallPhase::Connecting);
    QCOMPARE(controller.callId(), qint64(7));
    QCOMPARE(controller.userId(), qint64(42));
    QVERIFY(!controller.video());
}

void CallControllerTest::acceptFromConnectingActivates()
{
    CallController controller;
    controller.startOutgoing(7, 42, false);

    controller.accept();

    QCOMPARE(controller.phase(), CallPhase::Active);
    QCOMPARE(controller.phaseName(), QStringLiteral("active"));
}

void CallControllerTest::acceptFromRingingActivates()
{
    CallController controller;
    controller.incoming(9, 43, true);

    controller.accept();

    QCOMPARE(controller.phase(), CallPhase::Active);
    QCOMPARE(controller.callId(), qint64(9));
    QCOMPARE(controller.userId(), qint64(43));
    QVERIFY(controller.video());
}

void CallControllerTest::acceptIgnoredOutsideSetup()
{
    CallController controller;
    QSignalSpy spy(&controller, &CallController::changed);

    // Idle has nothing to accept.
    controller.accept();
    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.phase(), CallPhase::Idle);

    controller.startOutgoing(7, 42, false);
    controller.accept();
    QCOMPARE(spy.count(), 2);
    QCOMPARE(controller.phase(), CallPhase::Active);

    // Already active: accepting again is a no-op.
    controller.accept();
    QCOMPARE(spy.count(), 2);
    QCOMPARE(controller.phase(), CallPhase::Active);

    // An ended call must not come back to life.
    controller.end();
    controller.accept();
    QCOMPARE(spy.count(), 3);
    QCOMPARE(controller.phase(), CallPhase::Ended);
}

void CallControllerTest::declineEndsRingingCall()
{
    CallController controller;
    controller.incoming(9, 43, false);

    controller.decline();

    QCOMPARE(controller.phase(), CallPhase::Ended);
    QCOMPARE(controller.phaseName(), QStringLiteral("ended"));
}

void CallControllerTest::endEndsActiveCall()
{
    CallController controller;
    controller.startOutgoing(7, 42, false);
    controller.accept();
    QCOMPARE(controller.phase(), CallPhase::Active);

    controller.end();

    QCOMPARE(controller.phase(), CallPhase::Ended);
}

void CallControllerTest::declineAndEndAreNoOpsWhenIdle()
{
    CallController controller;
    QSignalSpy spy(&controller, &CallController::changed);

    controller.decline();
    controller.end();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.phase(), CallPhase::Idle);
}

void CallControllerTest::declineAndEndAreNoOpsWhenEnded()
{
    CallController controller;
    controller.incoming(9, 43, false);
    controller.decline();
    QCOMPARE(controller.phase(), CallPhase::Ended);

    QSignalSpy spy(&controller, &CallController::changed);

    // A duplicate "call ended" update is not a second ending.
    controller.decline();
    controller.end();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.phase(), CallPhase::Ended);
}

void CallControllerTest::endedRetainsCallIdentity()
{
    CallController controller;
    controller.startOutgoing(7, 42, true);
    controller.accept();
    controller.setElapsedSeconds(12);
    controller.end();

    // The ended screen still needs to show who the call was with.
    QCOMPARE(controller.phase(), CallPhase::Ended);
    QCOMPARE(controller.callId(), qint64(7));
    QCOMPARE(controller.userId(), qint64(42));
    QVERIFY(controller.video());
    QCOMPARE(controller.elapsedSeconds(), 12);
}

void CallControllerTest::newCallResetsTransientState()
{
    CallController controller;
    controller.startOutgoing(7, 42, false);
    controller.accept();
    controller.setMuted(true);
    controller.setSpeaker(true);
    controller.setElapsedSeconds(30);
    QVERIFY(controller.muted());
    QVERIFY(controller.speaker());
    QCOMPARE(controller.elapsedSeconds(), 30);

    // The next call starts clean: mute, speaker and the clock are per call.
    controller.startOutgoing(8, 43, false);

    QCOMPARE(controller.phase(), CallPhase::Connecting);
    QVERIFY(!controller.muted());
    QVERIFY(!controller.speaker());
    QCOMPARE(controller.elapsedSeconds(), 0);
}

void CallControllerTest::newCallReplacesPreviousCall()
{
    CallController controller;
    controller.incoming(9, 43, false);

    // A second incoming call replaces the ring instead of being ignored.
    controller.incoming(10, 44, true);

    QCOMPARE(controller.phase(), CallPhase::Ringing);
    QCOMPARE(controller.callId(), qint64(10));
    QCOMPARE(controller.userId(), qint64(44));
    QVERIFY(controller.video());
}

void CallControllerTest::muteAndSpeakerToggle()
{
    CallController controller;
    controller.startOutgoing(7, 42, false);
    controller.accept();

    controller.setMuted(true);
    QVERIFY(controller.muted());
    controller.setSpeaker(true);
    QVERIFY(controller.speaker());
    QVERIFY(controller.muted());

    controller.setMuted(false);
    QVERIFY(!controller.muted());
    QVERIFY(controller.speaker());
}

void CallControllerTest::settersIgnoredWhenIdle()
{
    CallController controller;
    QSignalSpy spy(&controller, &CallController::changed);

    controller.setMuted(true);
    controller.setSpeaker(true);

    QVERIFY(!controller.muted());
    QVERIFY(!controller.speaker());
    QCOMPARE(spy.count(), 0);
}

void CallControllerTest::elapsedTracksDuration()
{
    CallController controller;
    controller.startOutgoing(7, 42, false);
    controller.accept();

    controller.setElapsedSeconds(5);
    QCOMPARE(controller.elapsedSeconds(), 5);

    controller.setElapsedSeconds(6);
    QCOMPARE(controller.elapsedSeconds(), 6);

    // A repeated report of the same second is not a change.
    QSignalSpy spy(&controller, &CallController::changed);
    controller.setElapsedSeconds(6);
    QCOMPARE(spy.count(), 0);
}

void CallControllerTest::elapsedClampsNegative()
{
    CallController controller;
    controller.startOutgoing(7, 42, false);
    controller.accept();

    // A broken duration must not render as a negative clock.
    controller.setElapsedSeconds(10);
    QCOMPARE(controller.elapsedSeconds(), 10);

    controller.setElapsedSeconds(-5);
    QCOMPARE(controller.elapsedSeconds(), 0);

    // Already at zero, so the clamped value changes nothing.
    QSignalSpy spy(&controller, &CallController::changed);
    controller.setElapsedSeconds(-1);
    QCOMPARE(controller.elapsedSeconds(), 0);
    QCOMPARE(spy.count(), 0);
}

void CallControllerTest::elapsedIgnoredWhenIdle()
{
    CallController controller;
    QSignalSpy spy(&controller, &CallController::changed);

    controller.setElapsedSeconds(9);

    QCOMPARE(controller.elapsedSeconds(), 0);
    QCOMPARE(spy.count(), 0);
}

void CallControllerTest::elapsedIsPerCall()
{
    CallController controller;
    controller.startOutgoing(7, 42, false);
    controller.accept();
    controller.setElapsedSeconds(40);

    controller.end();
    // The duration stays with the ended call so the ended screen can show it.
    QCOMPARE(controller.elapsedSeconds(), 40);

    controller.incoming(11, 45, false);
    QCOMPARE(controller.elapsedSeconds(), 0);
}

void CallControllerTest::phaseNames()
{
    CallController controller;
    QCOMPARE(controller.phaseName(), QStringLiteral("idle"));

    controller.incoming(9, 43, false);
    QCOMPARE(controller.phaseName(), QStringLiteral("ringing"));

    controller.startOutgoing(7, 42, false);
    QCOMPARE(controller.phaseName(), QStringLiteral("connecting"));

    controller.accept();
    QCOMPARE(controller.phaseName(), QStringLiteral("active"));

    controller.end();
    QCOMPARE(controller.phaseName(), QStringLiteral("ended"));
}

void CallControllerTest::signalsOnlyOnRealChange()
{
    CallController controller;
    QSignalSpy spy(&controller, &CallController::changed);

    controller.startOutgoing(7, 42, false);
    QCOMPARE(spy.count(), 1);

    controller.accept();
    QCOMPARE(spy.count(), 2);

    controller.setMuted(true);
    QCOMPARE(spy.count(), 3);

    controller.setMuted(true);
    QCOMPARE(spy.count(), 3);

    controller.setSpeaker(true);
    QCOMPARE(spy.count(), 4);

    controller.setSpeaker(true);
    QCOMPARE(spy.count(), 4);

    controller.setElapsedSeconds(1);
    QCOMPARE(spy.count(), 5);

    controller.setElapsedSeconds(1);
    QCOMPARE(spy.count(), 5);

    controller.end();
    QCOMPARE(spy.count(), 6);

    controller.end();
    QCOMPARE(spy.count(), 6);

    // A fresh call always changes the phase, so it always notifies.
    controller.incoming(8, 43, true);
    QCOMPARE(spy.count(), 7);
}

void CallControllerTest::videoFlagFollowsCall()
{
    CallController controller;

    controller.startOutgoing(7, 42, true);
    QVERIFY(controller.video());

    // The video flag belongs to the call, not to the controller.
    controller.startOutgoing(8, 43, false);
    QVERIFY(!controller.video());

    controller.incoming(9, 44, true);
    QVERIFY(controller.video());
}

QTEST_GUILESS_MAIN(CallControllerTest)
#include "tst_call_controller.moc"
