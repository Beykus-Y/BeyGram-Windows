#include "telegram/QrLoginController.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class QrLoginControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsIdleWithoutLink();
    void beginAcceptsSupportedLinks();
    void beginTrimsTheStoredLink();
    void beginRejectsEmptyAndForeignLinks();
    void beginKeepsTheCurrentAttemptOnInvalidInput();
    void beginReplacesThePreviousAttempt();
    void beginWithTheSameLinkKeepsTheLink();
    void markScannedOnlyMovesWaitingForward();
    void markConfirmedFromWaitingOrScanned();
    void markConfirmedIsIgnoredInOtherStates();
    void markExpiredOnlyWhileActive();
    void resetReturnsToIdle();
    void resetIsSilentWhenIdle();
    void reportsEveryRealTransitionOnce();
};

void QrLoginControllerTest::startsIdleWithoutLink()
{
    const QrLoginController controller;

    QCOMPARE(controller.state(), QStringLiteral("idle"));
    QVERIFY(controller.link().isEmpty());
    QVERIFY(!controller.active());
    QVERIFY(!controller.expired());
}

void QrLoginControllerTest::beginAcceptsSupportedLinks()
{
    QrLoginController controller;

    const QString token = QStringLiteral("tg://login?token=abcdef123456");
    controller.begin(token);
    QCOMPARE(controller.state(), QStringLiteral("waiting"));
    QCOMPARE(controller.link(), token);
    QVERIFY(controller.active());
    QVERIFY(!controller.expired());

    const QString https = QStringLiteral("https://example.org/login?token=abcdef123456");
    controller.begin(https);
    QCOMPARE(controller.state(), QStringLiteral("waiting"));
    QCOMPARE(controller.link(), https);
    QVERIFY(controller.active());
}

void QrLoginControllerTest::beginTrimsTheStoredLink()
{
    QrLoginController controller;

    controller.begin(QStringLiteral("  tg://login?token=abcdef123456  "));
    QCOMPARE(controller.link(), QStringLiteral("tg://login?token=abcdef123456"));

    controller.begin(QStringLiteral("\n\thttps://example.org/login?token=abcdef123456\t"));
    QCOMPARE(controller.link(), QStringLiteral("https://example.org/login?token=abcdef123456"));
}

void QrLoginControllerTest::beginRejectsEmptyAndForeignLinks()
{
    QrLoginController controller;
    QSignalSpy changedSpy(&controller, &QrLoginController::changed);
    QSignalSpy linkSpy(&controller, &QrLoginController::linkChanged);

    controller.begin(QString());
    controller.begin(QStringLiteral("   "));
    controller.begin(QStringLiteral("tg://login?token"));
    controller.begin(QStringLiteral("http://example.org/login"));
    controller.begin(QStringLiteral("example.org/login?token=abcdef"));

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(linkSpy.count(), 0);
    QCOMPARE(controller.state(), QStringLiteral("idle"));
    QVERIFY(controller.link().isEmpty());

    // A valid link still starts the flow after the rejected ones.
    controller.begin(QStringLiteral("tg://login?token=abcdef123456"));
    QCOMPARE(controller.state(), QStringLiteral("waiting"));
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(linkSpy.count(), 1);
}

void QrLoginControllerTest::beginKeepsTheCurrentAttemptOnInvalidInput()
{
    QrLoginController controller;
    const QString link = QStringLiteral("tg://login?token=abcdef123456");
    controller.begin(link);
    controller.markScanned();

    QSignalSpy changedSpy(&controller, &QrLoginController::changed);
    QSignalSpy linkSpy(&controller, &QrLoginController::linkChanged);

    controller.begin(QString());
    controller.begin(QStringLiteral("http://example.org/login"));
    controller.begin(QStringLiteral("not a link"));

    QCOMPARE(controller.state(), QStringLiteral("scanned"));
    QCOMPARE(controller.link(), link);
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(linkSpy.count(), 0);
}

void QrLoginControllerTest::beginReplacesThePreviousAttempt()
{
    QrLoginController controller;
    const QString first = QStringLiteral("tg://login?token=first");
    const QString second = QStringLiteral("https://example.org/login?token=second");

    controller.begin(first);
    controller.markScanned();

    QSignalSpy changedSpy(&controller, &QrLoginController::changed);
    QSignalSpy linkSpy(&controller, &QrLoginController::linkChanged);

    controller.begin(second);

    QCOMPARE(controller.state(), QStringLiteral("waiting"));
    QCOMPARE(controller.link(), second);
    QVERIFY(controller.active());
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(linkSpy.count(), 1);
    QCOMPARE(linkSpy.at(0).at(0).toString(), second);
}

void QrLoginControllerTest::beginWithTheSameLinkKeepsTheLink()
{
    QrLoginController controller;
    const QString link = QStringLiteral("tg://login?token=abcdef123456");
    controller.begin(link);

    QSignalSpy changedSpy(&controller, &QrLoginController::changed);
    QSignalSpy linkSpy(&controller, &QrLoginController::linkChanged);

    // Already waiting for exactly this link: nothing to report.
    controller.begin(link);
    controller.begin(QStringLiteral("  ") + link + QStringLiteral("  "));
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(linkSpy.count(), 0);

    // Restarting a scanned attempt is a state change, not a link change.
    controller.markScanned();
    QCOMPARE(changedSpy.count(), 1);
    controller.begin(link);
    QCOMPARE(controller.state(), QStringLiteral("waiting"));
    QCOMPARE(changedSpy.count(), 2);
    QCOMPARE(linkSpy.count(), 0);

    // The same holds after the attempt expired.
    controller.markExpired();
    controller.begin(link);
    QCOMPARE(controller.state(), QStringLiteral("waiting"));
    QCOMPARE(changedSpy.count(), 4);
    QCOMPARE(linkSpy.count(), 0);
}

void QrLoginControllerTest::markScannedOnlyMovesWaitingForward()
{
    QrLoginController controller;
    QSignalSpy changedSpy(&controller, &QrLoginController::changed);

    // Nothing to scan while idle.
    controller.markScanned();
    QCOMPARE(controller.state(), QStringLiteral("idle"));
    QCOMPARE(changedSpy.count(), 0);

    controller.begin(QStringLiteral("tg://login?token=abcdef123456"));
    QCOMPARE(changedSpy.count(), 1);

    controller.markScanned();
    QCOMPARE(controller.state(), QStringLiteral("scanned"));
    QVERIFY(controller.active());
    QCOMPARE(changedSpy.count(), 2);

    // A repeated scan update must not move the attempt.
    controller.markScanned();
    QCOMPARE(controller.state(), QStringLiteral("scanned"));
    QCOMPARE(changedSpy.count(), 2);
}

void QrLoginControllerTest::markConfirmedFromWaitingOrScanned()
{
    const QString link = QStringLiteral("tg://login?token=abcdef123456");

    QrLoginController direct;
    direct.begin(link);
    direct.markConfirmed();
    QCOMPARE(direct.state(), QStringLiteral("confirmed"));
    QVERIFY(!direct.active());
    QVERIFY(!direct.expired());
    QCOMPARE(direct.link(), link);

    QrLoginController scanned;
    scanned.begin(link);
    scanned.markScanned();
    scanned.markConfirmed();
    QCOMPARE(scanned.state(), QStringLiteral("confirmed"));
    QVERIFY(!scanned.active());
    QCOMPARE(scanned.link(), link);
}

void QrLoginControllerTest::markConfirmedIsIgnoredInOtherStates()
{
    QrLoginController idle;
    QSignalSpy idleSpy(&idle, &QrLoginController::changed);
    idle.markConfirmed();
    QCOMPARE(idle.state(), QStringLiteral("idle"));
    QCOMPARE(idleSpy.count(), 0);

    QrLoginController confirmed;
    confirmed.begin(QStringLiteral("tg://login?token=abcdef123456"));
    confirmed.markConfirmed();
    QSignalSpy confirmedSpy(&confirmed, &QrLoginController::changed);
    confirmed.markConfirmed();
    QCOMPARE(confirmed.state(), QStringLiteral("confirmed"));
    QCOMPARE(confirmedSpy.count(), 0);

    QrLoginController expired;
    expired.begin(QStringLiteral("tg://login?token=abcdef123456"));
    expired.markExpired();
    QSignalSpy expiredSpy(&expired, &QrLoginController::changed);
    expired.markConfirmed();
    QCOMPARE(expired.state(), QStringLiteral("expired"));
    QVERIFY(expired.expired());
    QCOMPARE(expiredSpy.count(), 0);
}

void QrLoginControllerTest::markExpiredOnlyWhileActive()
{
    QrLoginController controller;
    QSignalSpy changedSpy(&controller, &QrLoginController::changed);

    // Idle cannot expire.
    controller.markExpired();
    QCOMPARE(controller.state(), QStringLiteral("idle"));
    QVERIFY(!controller.expired());
    QCOMPARE(changedSpy.count(), 0);

    const QString link = QStringLiteral("tg://login?token=abcdef123456");
    controller.begin(link);
    controller.markExpired();
    QCOMPARE(controller.state(), QStringLiteral("expired"));
    QVERIFY(controller.expired());
    QVERIFY(!controller.active());
    QCOMPARE(controller.link(), link);

    // Expiry is final: repeating it or confirming afterwards changes nothing.
    QCOMPARE(changedSpy.count(), 2);
    controller.markExpired();
    controller.markConfirmed();
    QCOMPARE(controller.state(), QStringLiteral("expired"));
    QCOMPARE(changedSpy.count(), 2);

    // A scanned attempt can expire too.
    QrLoginController scanned;
    scanned.begin(link);
    scanned.markScanned();
    scanned.markExpired();
    QCOMPARE(scanned.state(), QStringLiteral("expired"));

    // A confirmed attempt is no longer active and cannot expire.
    QrLoginController confirmed;
    confirmed.begin(link);
    confirmed.markConfirmed();
    confirmed.markExpired();
    QCOMPARE(confirmed.state(), QStringLiteral("confirmed"));
    QVERIFY(!confirmed.expired());
}

void QrLoginControllerTest::resetReturnsToIdle()
{
    const QString link = QStringLiteral("tg://login?token=abcdef123456");

    QrLoginController waiting;
    waiting.begin(link);
    QSignalSpy waitingChanged(&waiting, &QrLoginController::changed);
    QSignalSpy waitingLink(&waiting, &QrLoginController::linkChanged);
    waiting.reset();
    QCOMPARE(waiting.state(), QStringLiteral("idle"));
    QVERIFY(waiting.link().isEmpty());
    QVERIFY(!waiting.active());
    QVERIFY(!waiting.expired());
    QCOMPARE(waitingChanged.count(), 1);
    QCOMPARE(waitingLink.count(), 1);
    QVERIFY(waitingLink.at(0).at(0).toString().isEmpty());

    QrLoginController scanned;
    scanned.begin(link);
    scanned.markScanned();
    scanned.reset();
    QCOMPARE(scanned.state(), QStringLiteral("idle"));
    QVERIFY(scanned.link().isEmpty());

    QrLoginController confirmed;
    confirmed.begin(link);
    confirmed.markConfirmed();
    QSignalSpy confirmedLink(&confirmed, &QrLoginController::linkChanged);
    confirmed.reset();
    QCOMPARE(confirmed.state(), QStringLiteral("idle"));
    QVERIFY(confirmed.link().isEmpty());
    QCOMPARE(confirmedLink.count(), 1);

    QrLoginController expired;
    expired.begin(link);
    expired.markExpired();
    expired.reset();
    QCOMPARE(expired.state(), QStringLiteral("idle"));
    QVERIFY(expired.link().isEmpty());
    QVERIFY(!expired.expired());
}

void QrLoginControllerTest::resetIsSilentWhenIdle()
{
    QrLoginController controller;
    QSignalSpy changedSpy(&controller, &QrLoginController::changed);
    QSignalSpy linkSpy(&controller, &QrLoginController::linkChanged);

    controller.reset();
    QCOMPARE(controller.state(), QStringLiteral("idle"));
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(linkSpy.count(), 0);

    // A second reset after a real one is silent again.
    controller.begin(QStringLiteral("tg://login?token=abcdef123456"));
    controller.reset();
    QCOMPARE(changedSpy.count(), 2);
    QCOMPARE(linkSpy.count(), 2);

    controller.reset();
    QCOMPARE(changedSpy.count(), 2);
    QCOMPARE(linkSpy.count(), 2);
}

void QrLoginControllerTest::reportsEveryRealTransitionOnce()
{
    QrLoginController controller;
    const QString first = QStringLiteral("tg://login?token=first");
    const QString second = QStringLiteral("https://example.org/login?token=second");

    QSignalSpy changedSpy(&controller, &QrLoginController::changed);
    QSignalSpy linkSpy(&controller, &QrLoginController::linkChanged);

    controller.begin(first);       // changed 1, link 1
    controller.begin(first);       // no-op
    controller.markScanned();      // changed 2
    controller.markScanned();      // no-op
    controller.markConfirmed();    // changed 3
    controller.markConfirmed();    // no-op
    controller.markExpired();      // no-op, already confirmed
    controller.reset();            // changed 4, link 2
    controller.reset();            // no-op
    controller.begin(second);      // changed 5, link 3
    controller.markExpired();      // changed 6
    controller.begin(second);      // changed 7, link unchanged

    QCOMPARE(changedSpy.count(), 7);
    QCOMPARE(linkSpy.count(), 3);
    QCOMPARE(linkSpy.at(0).at(0).toString(), first);
    QVERIFY(linkSpy.at(1).at(0).toString().isEmpty());
    QCOMPARE(linkSpy.at(2).at(0).toString(), second);
}

QTEST_GUILESS_MAIN(QrLoginControllerTest)
#include "tst_qr_login_controller.moc"
