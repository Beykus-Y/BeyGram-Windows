#include "telegram/VoiceRecordingController.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class VoiceRecordingControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsIdle();
    void startBeginsRecording();
    void startIgnoredWhileRecording();
    void startResetsPausedLockedAndElapsed();
    void cancelStopsRecording();
    void cancelIgnoredWhenNotRecording();
    void finishRequiresRecording();
    void finishRejectedWhenTooShort();
    void finishAcceptedAtMinimumDuration();
    void finishRejectedWhenTooLong();
    void finishAcceptedJustBelowMaximum();
    void finishClearsAllState();
    void setPausedTogglesPause();
    void setPausedIgnoredWhenNotRecording();
    void setElapsedClampsToRange();
    void setElapsedIgnoredWhenNotRecording();
    void setLockedTogglesLock();
    void setLockedIgnoredWhenNotRecording();
    void tooShortAndTooLongBoundaries();
    void emitsChangedOnlyOnRealTransitions();
};

void VoiceRecordingControllerTest::startsIdle()
{
    const VoiceRecordingController controller;

    QVERIFY(!controller.recording());
    QVERIFY(!controller.paused());
    QVERIFY(!controller.locked());
    QCOMPARE(controller.elapsedSeconds(), 0);
}

void VoiceRecordingControllerTest::startBeginsRecording()
{
    VoiceRecordingController controller;

    controller.start();

    QVERIFY(controller.recording());
    QVERIFY(!controller.paused());
    QVERIFY(!controller.locked());
    QCOMPARE(controller.elapsedSeconds(), 0);
}

void VoiceRecordingControllerTest::startIgnoredWhileRecording()
{
    VoiceRecordingController controller;

    controller.start();
    controller.setPaused(true);
    controller.setLocked(true);
    controller.setElapsedSeconds(12);

    // A second start must not restart the recording in flight.
    controller.start();

    QVERIFY(controller.recording());
    QVERIFY(controller.paused());
    QVERIFY(controller.locked());
    QCOMPARE(controller.elapsedSeconds(), 12);
}

void VoiceRecordingControllerTest::startResetsPausedLockedAndElapsed()
{
    VoiceRecordingController controller;

    controller.start();
    controller.setPaused(true);
    controller.setLocked(true);
    controller.setElapsedSeconds(30);
    controller.cancel();

    controller.start();

    QVERIFY(controller.recording());
    QVERIFY(!controller.paused());
    QVERIFY(!controller.locked());
    QCOMPARE(controller.elapsedSeconds(), 0);
}

void VoiceRecordingControllerTest::cancelStopsRecording()
{
    VoiceRecordingController controller;

    controller.start();
    controller.setPaused(true);
    controller.setLocked(true);
    controller.setElapsedSeconds(5);
    controller.cancel();

    QVERIFY(!controller.recording());
    QVERIFY(!controller.paused());
    QVERIFY(!controller.locked());
    QCOMPARE(controller.elapsedSeconds(), 0);

    // A fresh recording can start after cancelling.
    controller.start();
    QVERIFY(controller.recording());
    QVERIFY(!controller.paused());
    QVERIFY(!controller.locked());
}

void VoiceRecordingControllerTest::cancelIgnoredWhenNotRecording()
{
    VoiceRecordingController controller;

    controller.cancel();

    QVERIFY(!controller.recording());
    QVERIFY(!controller.paused());
    QVERIFY(!controller.locked());
    QCOMPARE(controller.elapsedSeconds(), 0);
}

void VoiceRecordingControllerTest::finishRequiresRecording()
{
    VoiceRecordingController controller;

    // Too short, but finish is refused anyway because nothing is recording.
    controller.finish();

    QVERIFY(!controller.recording());
    QCOMPARE(controller.elapsedSeconds(), 0);
}

void VoiceRecordingControllerTest::finishRejectedWhenTooShort()
{
    VoiceRecordingController controller;

    controller.start();
    controller.setElapsedSeconds(0);
    QVERIFY(controller.tooShort());

    controller.finish();

    // Still recording, so the caller may keep going or cancel.
    QVERIFY(controller.recording());
    QCOMPARE(controller.elapsedSeconds(), 0);
}

void VoiceRecordingControllerTest::finishAcceptedAtMinimumDuration()
{
    VoiceRecordingController controller;

    controller.start();
    controller.setElapsedSeconds(1);
    QVERIFY(!controller.tooShort());

    controller.finish();

    QVERIFY(!controller.recording());
    QCOMPARE(controller.elapsedSeconds(), 0);
}

void VoiceRecordingControllerTest::finishRejectedWhenTooLong()
{
    VoiceRecordingController controller;

    controller.start();
    controller.setElapsedSeconds(60);
    QVERIFY(controller.tooLong());

    controller.finish();

    QVERIFY(controller.recording());
    QCOMPARE(controller.elapsedSeconds(), 60);
}

void VoiceRecordingControllerTest::finishAcceptedJustBelowMaximum()
{
    VoiceRecordingController controller;

    controller.start();
    controller.setElapsedSeconds(59);
    QVERIFY(!controller.tooLong());

    controller.finish();

    QVERIFY(!controller.recording());
    QCOMPARE(controller.elapsedSeconds(), 0);
}

void VoiceRecordingControllerTest::finishClearsAllState()
{
    VoiceRecordingController controller;

    controller.start();
    controller.setPaused(true);
    controller.setLocked(true);
    controller.setElapsedSeconds(10);
    controller.finish();

    QVERIFY(!controller.recording());
    QVERIFY(!controller.paused());
    QVERIFY(!controller.locked());
    QCOMPARE(controller.elapsedSeconds(), 0);
}

void VoiceRecordingControllerTest::setPausedTogglesPause()
{
    VoiceRecordingController controller;

    controller.start();
    controller.setPaused(true);
    QVERIFY(controller.paused());
    QVERIFY(controller.recording());

    controller.setPaused(false);
    QVERIFY(!controller.paused());
    QVERIFY(controller.recording());
}

void VoiceRecordingControllerTest::setPausedIgnoredWhenNotRecording()
{
    VoiceRecordingController controller;

    controller.setPaused(true);
    QVERIFY(!controller.paused());

    controller.start();
    controller.setPaused(true);
    controller.cancel();
    controller.setPaused(true);
    QVERIFY(!controller.paused());
}

void VoiceRecordingControllerTest::setElapsedClampsToRange()
{
    VoiceRecordingController controller;

    controller.start();

    controller.setElapsedSeconds(-5);
    QCOMPARE(controller.elapsedSeconds(), 0);

    controller.setElapsedSeconds(120);
    QCOMPARE(controller.elapsedSeconds(), 120);

    // The stored value saturates rather than overflowing.
    controller.setElapsedSeconds(1000000);
    QCOMPARE(controller.elapsedSeconds(), 3600);

    controller.setElapsedSeconds(-1);
    QCOMPARE(controller.elapsedSeconds(), 0);
}

void VoiceRecordingControllerTest::setElapsedIgnoredWhenNotRecording()
{
    VoiceRecordingController controller;

    controller.setElapsedSeconds(25);
    QCOMPARE(controller.elapsedSeconds(), 0);

    controller.start();
    controller.setElapsedSeconds(25);
    controller.cancel();
    controller.setElapsedSeconds(40);
    QCOMPARE(controller.elapsedSeconds(), 0);
}

void VoiceRecordingControllerTest::setLockedTogglesLock()
{
    VoiceRecordingController controller;

    controller.start();
    controller.setLocked(true);
    QVERIFY(controller.locked());

    controller.setLocked(false);
    QVERIFY(!controller.locked());

    // Locking is independent of pausing.
    controller.setPaused(true);
    controller.setLocked(true);
    QVERIFY(controller.locked());
    QVERIFY(controller.paused());
}

void VoiceRecordingControllerTest::setLockedIgnoredWhenNotRecording()
{
    VoiceRecordingController controller;

    controller.setLocked(true);
    QVERIFY(!controller.locked());

    controller.start();
    controller.setLocked(true);
    controller.cancel();
    controller.setLocked(true);
    QVERIFY(!controller.locked());
}

void VoiceRecordingControllerTest::tooShortAndTooLongBoundaries()
{
    VoiceRecordingController controller;

    // Elapsed is zero outside a recording, so the clip is too short by
    // definition and nowhere near too long.
    QVERIFY(controller.tooShort());
    QVERIFY(!controller.tooLong());

    controller.start();

    controller.setElapsedSeconds(0);
    QVERIFY(controller.tooShort());
    QVERIFY(!controller.tooLong());

    controller.setElapsedSeconds(1);
    QVERIFY(!controller.tooShort());
    QVERIFY(!controller.tooLong());

    controller.setElapsedSeconds(59);
    QVERIFY(!controller.tooShort());
    QVERIFY(!controller.tooLong());

    controller.setElapsedSeconds(60);
    QVERIFY(!controller.tooShort());
    QVERIFY(controller.tooLong());

    controller.setElapsedSeconds(3600);
    QVERIFY(!controller.tooShort());
    QVERIFY(controller.tooLong());
}

void VoiceRecordingControllerTest::emitsChangedOnlyOnRealTransitions()
{
    VoiceRecordingController controller;
    QSignalSpy spy(&controller, &VoiceRecordingController::changed);

    // Nothing changes while idle.
    controller.cancel();
    controller.finish();
    controller.setPaused(true);
    controller.setElapsedSeconds(10);
    controller.setLocked(true);
    QCOMPARE(spy.count(), 0);

    controller.start();
    QCOMPARE(spy.count(), 1);

    // Repeating start while recording is a no-op.
    controller.start();
    QCOMPARE(spy.count(), 1);

    controller.setPaused(true);
    QCOMPARE(spy.count(), 2);
    controller.setPaused(true);
    QCOMPARE(spy.count(), 2);

    controller.setLocked(true);
    QCOMPARE(spy.count(), 3);
    controller.setLocked(true);
    QCOMPARE(spy.count(), 3);

    controller.setElapsedSeconds(5);
    QCOMPARE(spy.count(), 4);
    controller.setElapsedSeconds(5);
    QCOMPARE(spy.count(), 4);

    // Clamping to the value already stored is not a change.
    controller.setElapsedSeconds(3600);
    QCOMPARE(spy.count(), 5);
    controller.setElapsedSeconds(9999);
    QCOMPARE(spy.count(), 5);

    controller.setElapsedSeconds(60);
    QCOMPARE(spy.count(), 6);

    // A refused finish (too long) leaves the state untouched and stays silent.
    controller.finish();
    QCOMPARE(spy.count(), 6);

    controller.setElapsedSeconds(1);
    QCOMPARE(spy.count(), 7);

    controller.finish();
    QCOMPARE(spy.count(), 8);

    // Finishing or cancelling an idle controller is silent.
    controller.finish();
    controller.cancel();
    QCOMPARE(spy.count(), 8);

    controller.start();
    QCOMPARE(spy.count(), 9);

    controller.cancel();
    QCOMPARE(spy.count(), 10);
}

QTEST_GUILESS_MAIN(VoiceRecordingControllerTest)
#include "tst_voice_recording_controller.moc"
