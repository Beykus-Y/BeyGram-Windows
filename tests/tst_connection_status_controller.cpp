#include "telegram/ConnectionStatusController.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class ConnectionStatusControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsUnknownOfflineWithoutRetry();
    void setStateStoresEveryState();
    void setStateEmitsOnlyOnRealChange();
    void setRetryAfterClampsToRange();
    void setRetryAfterIsIgnoredOutsideOfflineStates();
    void setStateLeavingOfflineResetsRetry();
    void setStateBetweenOfflineStatesKeepsRetry();
    void tickCountsDownToZero();
    void tickEmitsOncePerSecond();
    void tickIsSilentAtZero();
    void onlineIsTrueOnlyInReady();
    void degradedCoversWaitingAndConnecting();
    void labelNamesEveryState();
    void labelFallsBackForUnknownValues();
    void changedTracksOnlyRealTransitions();
};

void ConnectionStatusControllerTest::startsUnknownOfflineWithoutRetry()
{
    const ConnectionStatusController controller;

    QCOMPARE(controller.state(), ConnectionState::Unknown);
    QCOMPARE(controller.retryAfter(), 0);
    QVERIFY(!controller.online());
    QVERIFY(!controller.degraded());
    QCOMPARE(controller.label(), QStringLiteral("Offline"));
}

void ConnectionStatusControllerTest::setStateStoresEveryState()
{
    ConnectionStatusController controller;

    controller.setState(ConnectionState::WaitingForNetwork);
    QCOMPARE(controller.state(), ConnectionState::WaitingForNetwork);

    controller.setState(ConnectionState::Connecting);
    QCOMPARE(controller.state(), ConnectionState::Connecting);

    controller.setState(ConnectionState::Updating);
    QCOMPARE(controller.state(), ConnectionState::Updating);

    controller.setState(ConnectionState::Ready);
    QCOMPARE(controller.state(), ConnectionState::Ready);

    controller.setState(ConnectionState::Unknown);
    QCOMPARE(controller.state(), ConnectionState::Unknown);
}

void ConnectionStatusControllerTest::setStateEmitsOnlyOnRealChange()
{
    ConnectionStatusController controller;
    QSignalSpy spy(&controller, &ConnectionStatusController::changed);

    // The initial state is Unknown, so asking for Unknown changes nothing.
    controller.setState(ConnectionState::Unknown);
    QCOMPARE(spy.count(), 0);

    controller.setState(ConnectionState::Connecting);
    QCOMPARE(spy.count(), 1);

    controller.setState(ConnectionState::Connecting);
    QCOMPARE(spy.count(), 1);

    controller.setState(ConnectionState::Ready);
    QCOMPARE(spy.count(), 2);

    controller.setState(ConnectionState::Ready);
    QCOMPARE(spy.count(), 2);
}

void ConnectionStatusControllerTest::setRetryAfterClampsToRange()
{
    ConnectionStatusController controller;
    controller.setState(ConnectionState::WaitingForNetwork);

    // Negative delays collapse to zero instead of counting up forever.
    controller.setRetryAfter(-1);
    QCOMPARE(controller.retryAfter(), 0);

    controller.setRetryAfter(0);
    QCOMPARE(controller.retryAfter(), 0);

    controller.setRetryAfter(60);
    QCOMPARE(controller.retryAfter(), 60);

    // The upper bound is inclusive and everything above it is cut down.
    controller.setRetryAfter(3600);
    QCOMPARE(controller.retryAfter(), 3600);

    controller.setRetryAfter(3601);
    QCOMPARE(controller.retryAfter(), 3600);

    controller.setRetryAfter(1000000);
    QCOMPARE(controller.retryAfter(), 3600);
}

void ConnectionStatusControllerTest::setRetryAfterIsIgnoredOutsideOfflineStates()
{
    ConnectionStatusController controller;

    const ConnectionState states[] = {
        ConnectionState::Unknown,
        ConnectionState::Updating,
        ConnectionState::Ready,
    };

    for (const ConnectionState state : states) {
        controller.setState(state);
        QSignalSpy spy(&controller, &ConnectionStatusController::changed);

        controller.setRetryAfter(120);
        QCOMPARE(controller.retryAfter(), 0);
        // Nothing was stored, so nothing changed and nothing was announced.
        QCOMPARE(spy.count(), 0);
    }
}

void ConnectionStatusControllerTest::setStateLeavingOfflineResetsRetry()
{
    ConnectionStatusController controller;

    controller.setState(ConnectionState::WaitingForNetwork);
    controller.setRetryAfter(90);
    QCOMPARE(controller.retryAfter(), 90);

    controller.setState(ConnectionState::Ready);
    QCOMPARE(controller.retryAfter(), 0);
    QCOMPARE(controller.state(), ConnectionState::Ready);

    // Coming back online must not resurrect the old delay.
    controller.setState(ConnectionState::WaitingForNetwork);
    QCOMPARE(controller.retryAfter(), 0);

    controller.setRetryAfter(45);
    controller.setState(ConnectionState::Updating);
    QCOMPARE(controller.retryAfter(), 0);

    // A malformed state value also cannot carry a retry delay.
    controller.setState(ConnectionState::Connecting);
    controller.setRetryAfter(10);
    controller.setState(static_cast<ConnectionState>(42));
    QCOMPARE(controller.retryAfter(), 0);
}

void ConnectionStatusControllerTest::setStateBetweenOfflineStatesKeepsRetry()
{
    ConnectionStatusController controller;
    QSignalSpy spy(&controller, &ConnectionStatusController::changed);

    controller.setState(ConnectionState::WaitingForNetwork); // 1
    controller.setRetryAfter(120);                          // 2
    controller.setState(ConnectionState::Connecting);       // 3

    // Both states can be retried, so the pending delay survives the switch.
    QCOMPARE(controller.retryAfter(), 120);
    QCOMPARE(spy.count(), 3);

    controller.setState(ConnectionState::Connecting);
    QCOMPARE(spy.count(), 3);
}

void ConnectionStatusControllerTest::tickCountsDownToZero()
{
    ConnectionStatusController controller;
    controller.setState(ConnectionState::Connecting);
    controller.setRetryAfter(3);

    controller.tick();
    QCOMPARE(controller.retryAfter(), 2);

    controller.tick();
    QCOMPARE(controller.retryAfter(), 1);

    controller.tick();
    QCOMPARE(controller.retryAfter(), 0);

    // The counter stops at zero instead of wrapping around.
    controller.tick();
    controller.tick();
    QCOMPARE(controller.retryAfter(), 0);
    QCOMPARE(controller.state(), ConnectionState::Connecting);
}

void ConnectionStatusControllerTest::tickEmitsOncePerSecond()
{
    ConnectionStatusController controller;
    controller.setState(ConnectionState::WaitingForNetwork);
    controller.setRetryAfter(2);

    QSignalSpy spy(&controller, &ConnectionStatusController::changed);

    controller.tick();
    QCOMPARE(spy.count(), 1);
    controller.tick();
    QCOMPARE(spy.count(), 2);
    controller.tick();
    QCOMPARE(spy.count(), 2);
}

void ConnectionStatusControllerTest::tickIsSilentAtZero()
{
    ConnectionStatusController controller;
    controller.setState(ConnectionState::WaitingForNetwork);

    QSignalSpy spy(&controller, &ConnectionStatusController::changed);
    controller.tick();
    controller.tick();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.retryAfter(), 0);
}

void ConnectionStatusControllerTest::onlineIsTrueOnlyInReady()
{
    ConnectionStatusController controller;

    controller.setState(ConnectionState::Unknown);
    QVERIFY(!controller.online());

    controller.setState(ConnectionState::WaitingForNetwork);
    QVERIFY(!controller.online());

    controller.setState(ConnectionState::Connecting);
    QVERIFY(!controller.online());

    controller.setState(ConnectionState::Updating);
    QVERIFY(!controller.online());

    controller.setState(ConnectionState::Ready);
    QVERIFY(controller.online());
}

void ConnectionStatusControllerTest::degradedCoversWaitingAndConnecting()
{
    ConnectionStatusController controller;

    controller.setState(ConnectionState::Unknown);
    QVERIFY(!controller.degraded());

    controller.setState(ConnectionState::WaitingForNetwork);
    QVERIFY(controller.degraded());

    controller.setState(ConnectionState::Connecting);
    QVERIFY(controller.degraded());

    // Updating is already online work, not a degraded link.
    controller.setState(ConnectionState::Updating);
    QVERIFY(!controller.degraded());

    controller.setState(ConnectionState::Ready);
    QVERIFY(!controller.degraded());
}

void ConnectionStatusControllerTest::labelNamesEveryState()
{
    ConnectionStatusController controller;

    controller.setState(ConnectionState::Unknown);
    QCOMPARE(controller.label(), QStringLiteral("Offline"));

    controller.setState(ConnectionState::WaitingForNetwork);
    QCOMPARE(controller.label(), QStringLiteral("Waiting for network"));

    controller.setState(ConnectionState::Connecting);
    QCOMPARE(controller.label(), QStringLiteral("Connecting…"));

    controller.setState(ConnectionState::Updating);
    QCOMPARE(controller.label(), QStringLiteral("Updating…"));

    controller.setState(ConnectionState::Ready);
    QCOMPARE(controller.label(), QStringLiteral("Connected"));
}

void ConnectionStatusControllerTest::labelFallsBackForUnknownValues()
{
    ConnectionStatusController controller;
    controller.setState(static_cast<ConnectionState>(99));

    QCOMPARE(controller.label(), QStringLiteral("Unknown"));
    QVERIFY(!controller.online());
    QVERIFY(!controller.degraded());
    QCOMPARE(controller.retryAfter(), 0);
}

void ConnectionStatusControllerTest::changedTracksOnlyRealTransitions()
{
    ConnectionStatusController controller;
    QSignalSpy spy(&controller, &ConnectionStatusController::changed);

    controller.setState(ConnectionState::WaitingForNetwork); // 1
    controller.setState(ConnectionState::WaitingForNetwork); // no-op
    controller.setRetryAfter(30);                           // 2
    controller.setRetryAfter(30);                           // no-op
    controller.tick();                                      // 3
    controller.setState(ConnectionState::Connecting);       // 4
    controller.tick();                                      // 5
    controller.setState(ConnectionState::Updating);         // 6
    controller.tick();                                      // no-op
    controller.setState(ConnectionState::Ready);            // 7
    controller.setState(ConnectionState::Ready);            // no-op
    controller.setRetryAfter(15);                           // no-op
    controller.setState(ConnectionState::Unknown);          // 8

    QCOMPARE(spy.count(), 8);
    QCOMPARE(controller.state(), ConnectionState::Unknown);
    QCOMPARE(controller.retryAfter(), 0);
}

QTEST_GUILESS_MAIN(ConnectionStatusControllerTest)
#include "tst_connection_status_controller.moc"
