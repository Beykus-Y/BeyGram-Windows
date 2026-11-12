#include "ui/SessionsFacade.h"

#include <QChar>
#include <QLatin1Char>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtTest>

using beygram::ui::SessionsFacade;

namespace {

// Same "device - location" shape the facade builds, with a real em dash. The
// source stays pure ASCII, like the facade itself.
QString label(const QString& device, const QString& location)
{
    return device + QLatin1Char(' ') + QChar(0x2014) + QLatin1Char(' ') + location;
}

// Two terminable sessions in Telegram's order: a phone and a laptop.
void loadSample(SessionsFacade& facade)
{
    facade.setSessions(QStringList{QStringLiteral("7"), QStringLiteral("8")},
                       QStringList{QStringLiteral("Pixel 8"), QStringLiteral("Surface Laptop")},
                       QStringList{QStringLiteral("Berlin, Germany"),
                                   QStringLiteral("Lisbon, Portugal")});
}

} // namespace

class SessionsFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void loadsSessionsInOrder();
    void deviceTextOmitsMissingParts();
    void dropsInvalidIds();
    void missingLabelsFallBackToUnknownDevice();
    void requestOpensConfirmation();
    void requestUnknownIdIsIgnored();
    void requestWhileRunningIsIgnored();
    void cancelClosesConfirmation();
    void confirmForOneSession();
    void confirmForAllOthers();
    void confirmWithoutConfirmationIsRefused();
    void confirmTwiceIsRefused();
    void runningFollowsController();
    void setRunningOutsideConfirmationIsIgnored();
    void terminateAllWithoutOthersIsIgnored();
    void reloadDropsStaleTarget();
    void reloadKeepsAllTarget();
    void reloadSameSessionsIsSilent();
};

void SessionsFacadeTest::startsEmpty()
{
    SessionsFacade facade;
    QSignalSpy changed(&facade, &SessionsFacade::changed);

    QCOMPARE(facade.devices(), QStringList{});
    QCOMPARE(facade.sessionIds(), QStringList{});
    QVERIFY(!facade.confirming());
    QCOMPARE(facade.pendingTarget(), QString());
    QVERIFY(!facade.running());
    QCOMPARE(changed.count(), 0);
}

void SessionsFacadeTest::loadsSessionsInOrder()
{
    SessionsFacade facade;
    QSignalSpy changed(&facade, &SessionsFacade::changed);

    loadSample(facade);

    QCOMPARE(facade.sessionIds(), (QStringList{QStringLiteral("7"), QStringLiteral("8")}));
    QCOMPARE(facade.devices(),
             (QStringList{label(QStringLiteral("Pixel 8"), QStringLiteral("Berlin, Germany")),
                          label(QStringLiteral("Surface Laptop"),
                                QStringLiteral("Lisbon, Portugal"))}));
    QCOMPARE(changed.count(), 1);
}

void SessionsFacadeTest::deviceTextOmitsMissingParts()
{
    SessionsFacade facade;

    // A row may arrive without a location (privacy) or without a device name.
    facade.setSessions(QStringList{QStringLiteral("1"), QStringLiteral("2")},
                       QStringList{QStringLiteral(" Phone "), QString()},
                       QStringList{QString(), QStringLiteral("Paris, France")});

    QCOMPARE(facade.devices(), (QStringList{QStringLiteral("Phone"),
                                           QStringLiteral("Paris, France")}));
}

void SessionsFacadeTest::dropsInvalidIds()
{
    SessionsFacade facade;

    // Only the fourth entry is a usable session; the labels still belong to it.
    facade.setSessions(QStringList{QStringLiteral("0"), QStringLiteral("-4"),
                                   QStringLiteral("abc"), QStringLiteral("12"),
                                   QString()},
                       QStringList{QStringLiteral("zero"), QStringLiteral("negative"),
                                   QStringLiteral("text"), QStringLiteral("Real device"),
                                   QStringLiteral("empty id")},
                       QStringList{QStringLiteral("A"), QStringLiteral("B"),
                                   QStringLiteral("C"), QStringLiteral("Oslo"),
                                   QStringLiteral("E")});

    QCOMPARE(facade.sessionIds(), QStringList{QStringLiteral("12")});
    QCOMPARE(facade.devices(),
             QStringList{label(QStringLiteral("Real device"), QStringLiteral("Oslo"))});
}

void SessionsFacadeTest::missingLabelsFallBackToUnknownDevice()
{
    SessionsFacade facade;

    // Shorter label lists must not shift ids or leave a blank row.
    facade.setSessions(QStringList{QStringLiteral("5")}, QStringList{}, QStringList{});

    QCOMPARE(facade.sessionIds(), QStringList{QStringLiteral("5")});
    QCOMPARE(facade.devices(), QStringList{QStringLiteral("Unknown device")});
}

void SessionsFacadeTest::requestOpensConfirmation()
{
    SessionsFacade facade;
    loadSample(facade);
    QSignalSpy changed(&facade, &SessionsFacade::changed);

    facade.requestTerminate(QStringLiteral("8"));

    QVERIFY(facade.confirming());
    QCOMPARE(facade.pendingTarget(), QStringLiteral("8"));
    QVERIFY(!facade.running());
    QCOMPARE(changed.count(), 1);
}

void SessionsFacadeTest::requestUnknownIdIsIgnored()
{
    SessionsFacade facade;
    loadSample(facade);
    QSignalSpy changed(&facade, &SessionsFacade::changed);

    // Not listed, not a number, not positive: none of these can be terminated.
    facade.requestTerminate(QStringLiteral("99"));
    facade.requestTerminate(QStringLiteral("abc"));
    facade.requestTerminate(QString());
    facade.requestTerminate(QStringLiteral("0"));
    facade.requestTerminate(QStringLiteral("-3"));

    QVERIFY(!facade.confirming());
    QCOMPARE(facade.pendingTarget(), QString());
    QCOMPARE(changed.count(), 0);
}

void SessionsFacadeTest::requestWhileRunningIsIgnored()
{
    SessionsFacade facade;
    loadSample(facade);
    facade.requestTerminate(QStringLiteral("7"));
    facade.setRunning(true);
    QSignalSpy changed(&facade, &SessionsFacade::changed);

    facade.requestTerminate(QStringLiteral("8"));
    facade.requestTerminateAllOthers();

    // The action in flight owns the target until it finishes or is cancelled.
    QCOMPARE(facade.pendingTarget(), QStringLiteral("7"));
    QVERIFY(facade.running());
    QCOMPARE(changed.count(), 0);
}

void SessionsFacadeTest::cancelClosesConfirmation()
{
    SessionsFacade facade;
    loadSample(facade);
    facade.requestTerminate(QStringLiteral("8"));
    QSignalSpy changed(&facade, &SessionsFacade::changed);

    facade.cancel();

    QVERIFY(!facade.confirming());
    QCOMPARE(facade.pendingTarget(), QString());
    QVERIFY(!facade.running());
    QCOMPARE(changed.count(), 1);
}

void SessionsFacadeTest::confirmForOneSession()
{
    SessionsFacade facade;
    loadSample(facade);
    facade.requestTerminate(QStringLiteral("8"));
    QSignalSpy requests(&facade, &SessionsFacade::terminateRequested);

    QVERIFY(facade.confirm());

    QCOMPARE(requests.count(), 1);
    QCOMPARE(requests.at(0).at(0).toString(), QStringLiteral("8"));
    QVERIFY(facade.running());
    QVERIFY(facade.confirming());
    QCOMPARE(facade.pendingTarget(), QStringLiteral("8"));
}

void SessionsFacadeTest::confirmForAllOthers()
{
    SessionsFacade facade;
    loadSample(facade);
    facade.requestTerminateAllOthers();
    QCOMPARE(facade.pendingTarget(), QStringLiteral("all"));
    QSignalSpy requests(&facade, &SessionsFacade::terminateRequested);

    QVERIFY(facade.confirm());

    QCOMPARE(requests.count(), 1);
    // An empty payload is terminateAllOtherSessions.
    QCOMPARE(requests.at(0).at(0).toString(), QString());
    QVERIFY(facade.running());
}

void SessionsFacadeTest::confirmWithoutConfirmationIsRefused()
{
    SessionsFacade facade;
    QSignalSpy requests(&facade, &SessionsFacade::terminateRequested);

    QVERIFY(!facade.confirm());

    QCOMPARE(requests.count(), 0);
    QVERIFY(!facade.running());
}

void SessionsFacadeTest::confirmTwiceIsRefused()
{
    SessionsFacade facade;
    loadSample(facade);
    facade.requestTerminate(QStringLiteral("7"));
    QSignalSpy requests(&facade, &SessionsFacade::terminateRequested);

    QVERIFY(facade.confirm());
    // The first request is still in flight, so a second click sends nothing.
    QVERIFY(!facade.confirm());

    QCOMPARE(requests.count(), 1);
    QCOMPARE(requests.at(0).at(0).toString(), QStringLiteral("7"));
}

void SessionsFacadeTest::runningFollowsController()
{
    SessionsFacade facade;
    loadSample(facade);
    facade.requestTerminate(QStringLiteral("7"));

    facade.setRunning(true);
    QVERIFY(facade.running());
    QVERIFY(facade.confirming());

    facade.setRunning(false);
    QVERIFY(!facade.running());
    QVERIFY(facade.confirming());
}

void SessionsFacadeTest::setRunningOutsideConfirmationIsIgnored()
{
    SessionsFacade facade;
    loadSample(facade);
    QSignalSpy changed(&facade, &SessionsFacade::changed);

    // No target to finish, so an idle facade cannot be marked busy.
    facade.setRunning(true);

    QVERIFY(!facade.running());
    QVERIFY(!facade.confirming());
    QCOMPARE(changed.count(), 0);
}

void SessionsFacadeTest::terminateAllWithoutOthersIsIgnored()
{
    SessionsFacade facade;
    QSignalSpy changed(&facade, &SessionsFacade::changed);

    facade.requestTerminateAllOthers();

    QVERIFY(!facade.confirming());
    QCOMPARE(changed.count(), 0);
}

void SessionsFacadeTest::reloadDropsStaleTarget()
{
    SessionsFacade facade;
    loadSample(facade);
    facade.requestTerminate(QStringLiteral("8"));

    // The laptop signed out elsewhere before the user confirmed.
    facade.setSessions(QStringList{QStringLiteral("7")}, QStringList{QStringLiteral("Pixel 8")},
                       QStringList{QStringLiteral("Berlin, Germany")});

    QVERIFY(!facade.confirming());
    QCOMPARE(facade.pendingTarget(), QString());

    QSignalSpy requests(&facade, &SessionsFacade::terminateRequested);
    QVERIFY(!facade.confirm());
    QCOMPARE(requests.count(), 0);
}

void SessionsFacadeTest::reloadKeepsAllTarget()
{
    SessionsFacade facade;
    loadSample(facade);
    facade.requestTerminateAllOthers();

    // "All other sessions" describes the list as it is now, so it survives.
    facade.setSessions(QStringList{QStringLiteral("9")}, QStringList{QStringLiteral("iPad")},
                       QStringList{QStringLiteral("Rome, Italy")});

    QVERIFY(facade.confirming());
    QCOMPARE(facade.pendingTarget(), QStringLiteral("all"));
}

void SessionsFacadeTest::reloadSameSessionsIsSilent()
{
    SessionsFacade facade;
    loadSample(facade);
    QSignalSpy changed(&facade, &SessionsFacade::changed);

    loadSample(facade);

    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.sessionIds(), (QStringList{QStringLiteral("7"), QStringLiteral("8")}));
}

QTEST_GUILESS_MAIN(SessionsFacadeTest)
#include "tst_sessions_facade.moc"
