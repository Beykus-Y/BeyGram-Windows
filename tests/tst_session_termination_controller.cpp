#include "telegram/SessionTerminationController.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class SessionTerminationControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsIdle();
    void requestTerminateOpensConfirmation();
    void requestTerminateRejectsNonPositive();
    void requestTerminateSameTargetIsSilent();
    void requestTerminateRetargetsPending();
    void requestTerminateAllOthersUsesZeroPending();
    void requestTerminateAllOthersReplacesSessionTarget();
    void requestTerminateAllOthersRepeatIsSilent();
    void requestsWhileRunningAreIgnored();
    void cancelClosesConfirmation();
    void cancelResetsRunning();
    void cancelWithoutConfirmationIsSilent();
    void setRunningOnlyInsideConfirmation();
    void setRunningEmitsOnlyOnRealChange();
    void canConfirmRequiresConfirmation();
    void canConfirmIsFalseWhileRunning();
    void clearResetsEverything();
    void clearWhenIdleIsSilent();
    void controllerIsUsableAfterClear();
};

void SessionTerminationControllerTest::startsIdle()
{
    SessionTerminationController controller;
    QSignalSpy spy(&controller, &SessionTerminationController::changed);

    QVERIFY(!controller.confirming());
    QCOMPARE(controller.pendingSessionId(), qint64(0));
    QVERIFY(!controller.running());
    QVERIFY(!controller.canConfirm());
    QCOMPARE(spy.count(), 0);
}

void SessionTerminationControllerTest::requestTerminateOpensConfirmation()
{
    SessionTerminationController controller;
    QSignalSpy spy(&controller, &SessionTerminationController::changed);

    controller.requestTerminate(42);

    QVERIFY(controller.confirming());
    QCOMPARE(controller.pendingSessionId(), qint64(42));
    QVERIFY(!controller.running());
    QVERIFY(controller.canConfirm());
    QCOMPARE(spy.count(), 1);
}

void SessionTerminationControllerTest::requestTerminateRejectsNonPositive()
{
    SessionTerminationController controller;
    QSignalSpy spy(&controller, &SessionTerminationController::changed);

    controller.requestTerminate(0);
    controller.requestTerminate(-7);

    QVERIFY(!controller.confirming());
    QCOMPARE(controller.pendingSessionId(), qint64(0));
    QVERIFY(!controller.canConfirm());
    QCOMPARE(spy.count(), 0);
}

void SessionTerminationControllerTest::requestTerminateSameTargetIsSilent()
{
    SessionTerminationController controller;
    controller.requestTerminate(42);
    QSignalSpy spy(&controller, &SessionTerminationController::changed);

    controller.requestTerminate(42);

    QVERIFY(controller.confirming());
    QCOMPARE(controller.pendingSessionId(), qint64(42));
    QCOMPARE(spy.count(), 0);
}

void SessionTerminationControllerTest::requestTerminateRetargetsPending()
{
    SessionTerminationController controller;
    controller.requestTerminate(42);
    QSignalSpy spy(&controller, &SessionTerminationController::changed);

    controller.requestTerminate(43);

    QVERIFY(controller.confirming());
    QCOMPARE(controller.pendingSessionId(), qint64(43));
    QCOMPARE(spy.count(), 1);
}

void SessionTerminationControllerTest::requestTerminateAllOthersUsesZeroPending()
{
    SessionTerminationController controller;
    QSignalSpy spy(&controller, &SessionTerminationController::changed);

    controller.requestTerminateAllOthers();

    QVERIFY(controller.confirming());
    QCOMPARE(controller.pendingSessionId(), qint64(0));
    QVERIFY(controller.canConfirm());
    QCOMPARE(spy.count(), 1);
}

void SessionTerminationControllerTest::requestTerminateAllOthersReplacesSessionTarget()
{
    SessionTerminationController controller;
    controller.requestTerminate(42);
    QSignalSpy spy(&controller, &SessionTerminationController::changed);

    controller.requestTerminateAllOthers();

    QVERIFY(controller.confirming());
    QCOMPARE(controller.pendingSessionId(), qint64(0));
    QCOMPARE(spy.count(), 1);
}

void SessionTerminationControllerTest::requestTerminateAllOthersRepeatIsSilent()
{
    SessionTerminationController controller;
    controller.requestTerminateAllOthers();
    QSignalSpy spy(&controller, &SessionTerminationController::changed);

    controller.requestTerminateAllOthers();

    QVERIFY(controller.confirming());
    QCOMPARE(controller.pendingSessionId(), qint64(0));
    QCOMPARE(spy.count(), 0);
}

void SessionTerminationControllerTest::requestsWhileRunningAreIgnored()
{
    SessionTerminationController controller;
    controller.requestTerminate(42);
    controller.setRunning(true);
    QSignalSpy spy(&controller, &SessionTerminationController::changed);

    // The action in flight owns the target until it finishes or is cancelled.
    controller.requestTerminate(43);
    controller.requestTerminateAllOthers();

    QVERIFY(controller.confirming());
    QCOMPARE(controller.pendingSessionId(), qint64(42));
    QVERIFY(controller.running());
    QCOMPARE(spy.count(), 0);
}

void SessionTerminationControllerTest::cancelClosesConfirmation()
{
    SessionTerminationController controller;
    controller.requestTerminate(42);
    QSignalSpy spy(&controller, &SessionTerminationController::changed);

    controller.cancel();

    QVERIFY(!controller.confirming());
    QCOMPARE(controller.pendingSessionId(), qint64(0));
    QVERIFY(!controller.running());
    QVERIFY(!controller.canConfirm());
    QCOMPARE(spy.count(), 1);
}

void SessionTerminationControllerTest::cancelResetsRunning()
{
    SessionTerminationController controller;
    controller.requestTerminateAllOthers();
    controller.setRunning(true);
    QSignalSpy spy(&controller, &SessionTerminationController::changed);

    controller.cancel();

    QVERIFY(!controller.confirming());
    QCOMPARE(controller.pendingSessionId(), qint64(0));
    QVERIFY(!controller.running());
    QCOMPARE(spy.count(), 1);
}

void SessionTerminationControllerTest::cancelWithoutConfirmationIsSilent()
{
    SessionTerminationController controller;
    QSignalSpy spy(&controller, &SessionTerminationController::changed);

    controller.cancel();

    QVERIFY(!controller.confirming());
    QCOMPARE(controller.pendingSessionId(), qint64(0));
    QVERIFY(!controller.running());
    QCOMPARE(spy.count(), 0);
}

void SessionTerminationControllerTest::setRunningOnlyInsideConfirmation()
{
    SessionTerminationController controller;
    QSignalSpy spy(&controller, &SessionTerminationController::changed);

    // An idle controller cannot be marked busy: there is no target to finish.
    controller.setRunning(true);

    QVERIFY(!controller.confirming());
    QVERIFY(!controller.running());
    QVERIFY(!controller.canConfirm());
    QCOMPARE(spy.count(), 0);
}

void SessionTerminationControllerTest::setRunningEmitsOnlyOnRealChange()
{
    SessionTerminationController controller;
    controller.requestTerminate(42);
    QSignalSpy spy(&controller, &SessionTerminationController::changed);

    controller.setRunning(true);
    QCOMPARE(spy.count(), 1);
    QVERIFY(controller.running());

    controller.setRunning(true);
    QCOMPARE(spy.count(), 1);

    controller.setRunning(false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!controller.running());
    QVERIFY(controller.canConfirm());
}

void SessionTerminationControllerTest::canConfirmRequiresConfirmation()
{
    SessionTerminationController controller;

    QVERIFY(!controller.canConfirm());

    controller.requestTerminate(42);
    QVERIFY(controller.canConfirm());

    controller.cancel();
    QVERIFY(!controller.canConfirm());

    // "All other sessions" is a real target, so it can be confirmed too.
    controller.requestTerminateAllOthers();
    QVERIFY(controller.canConfirm());
}

void SessionTerminationControllerTest::canConfirmIsFalseWhileRunning()
{
    SessionTerminationController controller;
    controller.requestTerminate(42);

    controller.setRunning(true);

    QVERIFY(controller.confirming());
    QVERIFY(!controller.canConfirm());
}

void SessionTerminationControllerTest::clearResetsEverything()
{
    SessionTerminationController controller;
    controller.requestTerminate(42);
    controller.setRunning(true);
    QSignalSpy spy(&controller, &SessionTerminationController::changed);

    controller.clear();

    QVERIFY(!controller.confirming());
    QCOMPARE(controller.pendingSessionId(), qint64(0));
    QVERIFY(!controller.running());
    QVERIFY(!controller.canConfirm());
    QCOMPARE(spy.count(), 1);
}

void SessionTerminationControllerTest::clearWhenIdleIsSilent()
{
    SessionTerminationController controller;
    QSignalSpy spy(&controller, &SessionTerminationController::changed);

    controller.clear();

    QVERIFY(!controller.confirming());
    QCOMPARE(controller.pendingSessionId(), qint64(0));
    QVERIFY(!controller.running());
    QCOMPARE(spy.count(), 0);
}

void SessionTerminationControllerTest::controllerIsUsableAfterClear()
{
    SessionTerminationController controller;
    controller.requestTerminate(42);
    controller.setRunning(true);
    controller.clear();

    // A fresh dialog after sign-out behaves like a first confirmation.
    controller.requestTerminateAllOthers();
    QVERIFY(controller.confirming());
    QCOMPARE(controller.pendingSessionId(), qint64(0));
    QVERIFY(controller.canConfirm());

    controller.setRunning(true);
    QVERIFY(!controller.canConfirm());
    controller.setRunning(false);
    controller.cancel();
    QVERIFY(!controller.confirming());
    QVERIFY(!controller.running());
}

QTEST_GUILESS_MAIN(SessionTerminationControllerTest)
#include "tst_session_termination_controller.moc"
