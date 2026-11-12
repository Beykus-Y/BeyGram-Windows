#include "ui/CallsFacade.h"

#include <QList>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtTest>

using beygram::ui::CallsFacade;

namespace {

/// "name — mm:ss — outcome" with a real em dash, kept ASCII in this source.
QString line(const QString& name, const QString& clock, const QString& outcome)
{
    const QChar dash(0x2014);
    return name + QLatin1Char(' ') + dash + QLatin1Char(' ') + clock + QLatin1Char(' ')
        + dash + QLatin1Char(' ') + outcome;
}

} // namespace

class CallsFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsIdle();
    void startOutgoingConnects();
    void startOutgoingRefusesMalformedIds();
    void incomingRings();
    void incomingRenameRepaints();
    void incomingReplacesPreviousCall();
    void acceptAnswersIncomingCall();
    void acceptIsSilentWithoutACall();
    void declineEndsRingingCall();
    void endEndsActiveCall();
    void endIsSilentWithoutACall();
    void muteAndSpeakerFollowTheCall();
    void togglesAreIgnoredWhileIdle();
    void elapsedTracksTheCall();
    void elapsedClampsNegativeAndResetsForNewCall();
    void changedFollowsVisibleState();
    void historyFormatsEntries();
    void historyNewestFirst();
    void historyFallsBackToUnknownName();
    void clearHistoryEmptiesTheList();
};

void CallsFacadeTest::startsIdle()
{
    const CallsFacade facade;

    QVERIFY(!facade.active());
    QCOMPARE(facade.phase(), QStringLiteral("idle"));
    QVERIFY(facade.peerName().isEmpty());
    QVERIFY(!facade.video());
    QVERIFY(!facade.muted());
    QVERIFY(!facade.speaker());
    QCOMPARE(facade.elapsed(), 0);
    QVERIFY(facade.history().isEmpty());
}

void CallsFacadeTest::startOutgoingConnects()
{
    CallsFacade facade;
    QSignalSpy outgoing(&facade, &CallsFacade::outgoingRequested);
    QSignalSpy changed(&facade, &CallsFacade::changed);

    facade.startOutgoing(QStringLiteral("7"), QStringLiteral("42"), QStringLiteral("Ada"), true);

    // Outgoing means ringing for the peer, so the local side is connecting.
    QVERIFY(facade.active());
    QCOMPARE(facade.phase(), QStringLiteral("connecting"));
    QCOMPARE(facade.peerName(), QStringLiteral("Ada"));
    QVERIFY(facade.video());
    QVERIFY(!facade.muted());
    QVERIFY(!facade.speaker());
    QCOMPARE(facade.elapsed(), 0);

    QCOMPARE(outgoing.count(), 1);
    QCOMPARE(outgoing.at(0).at(0).toString(), QStringLiteral("7"));
    QCOMPARE(outgoing.at(0).at(1).toString(), QStringLiteral("42"));
    QCOMPARE(outgoing.at(0).at(2).toBool(), true);
    QVERIFY(changed.count() >= 1);
}

void CallsFacadeTest::startOutgoingRefusesMalformedIds()
{
    CallsFacade facade;
    QSignalSpy outgoing(&facade, &CallsFacade::outgoingRequested);
    QSignalSpy changed(&facade, &CallsFacade::changed);

    struct Ids
    {
        QString callId;
        QString userId;
    };

    const QList<Ids> refused = {
        { QString(), QStringLiteral("42") },
        { QStringLiteral("0"), QStringLiteral("42") },
        { QStringLiteral("-3"), QStringLiteral("42") },
        { QStringLiteral("abc"), QStringLiteral("42") },
        { QStringLiteral("7"), QString() },
        { QStringLiteral("7"), QStringLiteral("0") },
        { QStringLiteral("7"), QStringLiteral("not-a-number") },
    };

    for (const Ids& ids : refused) {
        facade.startOutgoing(ids.callId, ids.userId, QStringLiteral("Ada"), true);
        QCOMPARE(facade.phase(), QStringLiteral("idle"));
        QVERIFY(!facade.active());
        QVERIFY(facade.peerName().isEmpty());
        QVERIFY(!facade.video());
    }

    QCOMPARE(outgoing.count(), 0);
    QCOMPARE(changed.count(), 0);
}

void CallsFacadeTest::incomingRings()
{
    CallsFacade facade;
    QSignalSpy outgoing(&facade, &CallsFacade::outgoingRequested);

    facade.incoming(QStringLiteral("9"), QStringLiteral("43"), QStringLiteral("Bob"), false);

    QVERIFY(facade.active());
    QCOMPARE(facade.phase(), QStringLiteral("ringing"));
    QCOMPARE(facade.peerName(), QStringLiteral("Bob"));
    QVERIFY(!facade.video());
    QCOMPARE(outgoing.count(), 0);
}

void CallsFacadeTest::incomingRenameRepaints()
{
    CallsFacade facade;

    facade.incoming(QStringLiteral("9"), QStringLiteral("43"), QStringLiteral("Unknown"), false);

    QSignalSpy changed(&facade, &CallsFacade::changed);
    facade.incoming(QStringLiteral("9"), QStringLiteral("43"), QStringLiteral("Ada"), false);

    // The same ring reported again is silent in the controller, but the name
    // that arrived late still has to reach the screen.
    QCOMPARE(facade.phase(), QStringLiteral("ringing"));
    QCOMPARE(facade.peerName(), QStringLiteral("Ada"));
    QCOMPARE(changed.count(), 1);
}

void CallsFacadeTest::incomingReplacesPreviousCall()
{
    CallsFacade facade;

    facade.startOutgoing(QStringLiteral("7"), QStringLiteral("42"), QStringLiteral("Ada"), true);
    facade.accept();
    facade.setMuted(true);
    facade.setSpeaker(true);
    facade.setElapsed(30);
    QCOMPARE(facade.phase(), QStringLiteral("active"));

    facade.incoming(QStringLiteral("10"), QStringLiteral("44"), QStringLiteral("Cleo"), false);

    // The flags and the clock belong to the call that just ended.
    QCOMPARE(facade.phase(), QStringLiteral("ringing"));
    QCOMPARE(facade.peerName(), QStringLiteral("Cleo"));
    QVERIFY(!facade.video());
    QVERIFY(!facade.muted());
    QVERIFY(!facade.speaker());
    QCOMPARE(facade.elapsed(), 0);
}

void CallsFacadeTest::acceptAnswersIncomingCall()
{
    CallsFacade facade;
    QSignalSpy answer(&facade, &CallsFacade::answerRequested);

    facade.incoming(QStringLiteral("9"), QStringLiteral("43"), QStringLiteral("Bob"), false);
    facade.accept();

    QCOMPARE(facade.phase(), QStringLiteral("active"));
    QVERIFY(facade.active());
    QCOMPARE(answer.count(), 1);
    QCOMPARE(answer.at(0).at(0).toString(), QStringLiteral("9"));
}

void CallsFacadeTest::acceptIsSilentWithoutACall()
{
    CallsFacade facade;
    QSignalSpy answer(&facade, &CallsFacade::answerRequested);

    facade.accept();

    QCOMPARE(facade.phase(), QStringLiteral("idle"));
    QVERIFY(!facade.active());
    QCOMPARE(answer.count(), 0);
}

void CallsFacadeTest::declineEndsRingingCall()
{
    CallsFacade facade;
    QSignalSpy end(&facade, &CallsFacade::endRequested);
    QSignalSpy answer(&facade, &CallsFacade::answerRequested);

    facade.incoming(QStringLiteral("9"), QStringLiteral("43"), QStringLiteral("Bob"), false);
    facade.decline();

    // Ended is not active, so an overlay bound to active() closes; the name and
    // the id stay readable for an ended screen.
    QCOMPARE(facade.phase(), QStringLiteral("ended"));
    QVERIFY(!facade.active());
    QCOMPARE(facade.peerName(), QStringLiteral("Bob"));
    QCOMPARE(end.count(), 1);
    QCOMPARE(end.at(0).at(0).toString(), QStringLiteral("9"));
    QCOMPARE(answer.count(), 0);
}

void CallsFacadeTest::endEndsActiveCall()
{
    CallsFacade facade;
    QSignalSpy end(&facade, &CallsFacade::endRequested);

    facade.startOutgoing(QStringLiteral("7"), QStringLiteral("42"), QStringLiteral("Ada"), false);
    facade.accept();
    facade.end();

    QCOMPARE(facade.phase(), QStringLiteral("ended"));
    QVERIFY(!facade.active());
    QCOMPARE(end.count(), 1);
    QCOMPARE(end.at(0).at(0).toString(), QStringLiteral("7"));

    // A duplicate hang-up is silent: the call is already over.
    facade.end();
    facade.decline();
    QCOMPARE(end.count(), 1);
}

void CallsFacadeTest::endIsSilentWithoutACall()
{
    CallsFacade facade;
    QSignalSpy end(&facade, &CallsFacade::endRequested);

    facade.end();
    facade.decline();

    QCOMPARE(facade.phase(), QStringLiteral("idle"));
    QCOMPARE(end.count(), 0);
}

void CallsFacadeTest::muteAndSpeakerFollowTheCall()
{
    CallsFacade facade;

    facade.startOutgoing(QStringLiteral("7"), QStringLiteral("42"), QStringLiteral("Ada"), false);
    facade.accept();

    QSignalSpy changed(&facade, &CallsFacade::changed);

    facade.setMuted(true);
    QVERIFY(facade.muted());
    QCOMPARE(changed.count(), 1);

    // Repeating the current value is not a change.
    facade.setMuted(true);
    QCOMPARE(changed.count(), 1);

    facade.setMuted(false);
    QVERIFY(!facade.muted());
    QCOMPARE(changed.count(), 2);

    facade.setSpeaker(true);
    QVERIFY(facade.speaker());
    QCOMPARE(changed.count(), 3);
}

void CallsFacadeTest::togglesAreIgnoredWhileIdle()
{
    CallsFacade facade;
    QSignalSpy changed(&facade, &CallsFacade::changed);

    facade.setMuted(true);
    facade.setSpeaker(true);
    facade.setElapsed(15);

    QVERIFY(!facade.muted());
    QVERIFY(!facade.speaker());
    QCOMPARE(facade.elapsed(), 0);
    QCOMPARE(changed.count(), 0);
}

void CallsFacadeTest::elapsedTracksTheCall()
{
    CallsFacade facade;

    facade.startOutgoing(QStringLiteral("7"), QStringLiteral("42"), QStringLiteral("Ada"), false);
    facade.accept();

    facade.setElapsed(75);
    QCOMPARE(facade.elapsed(), 75);

    facade.setElapsed(76);
    QCOMPARE(facade.elapsed(), 76);
}

void CallsFacadeTest::elapsedClampsNegativeAndResetsForNewCall()
{
    CallsFacade facade;

    facade.startOutgoing(QStringLiteral("7"), QStringLiteral("42"), QStringLiteral("Ada"), false);
    facade.accept();

    facade.setElapsed(-5);
    QCOMPARE(facade.elapsed(), 0);

    facade.setElapsed(30);
    QCOMPARE(facade.elapsed(), 30);

    facade.incoming(QStringLiteral("10"), QStringLiteral("44"), QStringLiteral("Cleo"), false);
    QCOMPARE(facade.elapsed(), 0);
}

void CallsFacadeTest::changedFollowsVisibleState()
{
    CallsFacade facade;
    QSignalSpy changed(&facade, &CallsFacade::changed);

    facade.startOutgoing(QStringLiteral("7"), QStringLiteral("42"), QStringLiteral("Ada"), false);
    QCOMPARE(changed.count(), 1);

    facade.accept();
    QCOMPARE(changed.count(), 2);

    facade.setMuted(true);
    QCOMPARE(changed.count(), 3);

    facade.addHistory(QStringLiteral("Ada"), 12, false, true, false);
    QCOMPARE(changed.count(), 4);

    facade.clearHistory();
    QCOMPARE(changed.count(), 5);

    // Clearing an empty history changes nothing.
    facade.clearHistory();
    QCOMPARE(changed.count(), 5);
}

void CallsFacadeTest::historyFormatsEntries()
{
    CallsFacade facade;

    facade.addHistory(QStringLiteral("Ada"), 75, false, false, false);
    facade.addHistory(QStringLiteral("Bob"), 5, true, false, false);
    facade.addHistory(QStringLiteral("Cara"), 0, true, true, true);
    facade.addHistory(QStringLiteral("Dan"), 3725, false, true, true);

    const QStringList lines = facade.history();
    QCOMPARE(lines.size(), 4);

    // Newest first: Dan, Cara, Bob, Ada.
    QCOMPARE(lines.at(0), line(QStringLiteral("Dan"), QStringLiteral("62:05"),
                              QStringLiteral("outgoing")));
    QCOMPARE(lines.at(1), line(QStringLiteral("Cara"), QStringLiteral("00:00"),
                              QStringLiteral("cancelled")));
    QCOMPARE(lines.at(2), line(QStringLiteral("Bob"), QStringLiteral("00:05"),
                              QStringLiteral("missed")));
    QCOMPARE(lines.at(3), line(QStringLiteral("Ada"), QStringLiteral("01:15"),
                              QStringLiteral("incoming")));
}

void CallsFacadeTest::historyNewestFirst()
{
    CallsFacade facade;

    facade.addHistory(QStringLiteral("First"), 10, false, false, false);
    facade.addHistory(QStringLiteral("Second"), 20, false, false, false);
    facade.addHistory(QStringLiteral("Third"), 30, false, false, false);

    const QStringList lines = facade.history();
    QCOMPARE(lines.size(), 3);
    QVERIFY(lines.at(0).startsWith(QStringLiteral("Third ")));
    QVERIFY(lines.at(1).startsWith(QStringLiteral("Second ")));
    QVERIFY(lines.at(2).startsWith(QStringLiteral("First ")));
}

void CallsFacadeTest::historyFallsBackToUnknownName()
{
    CallsFacade facade;

    facade.addHistory(QString(), -30, false, false, false);

    QCOMPARE(facade.history().size(), 1);
    QCOMPARE(facade.history().first(),
             line(QStringLiteral("Unknown"), QStringLiteral("00:00"),
                  QStringLiteral("incoming")));
}

void CallsFacadeTest::clearHistoryEmptiesTheList()
{
    CallsFacade facade;

    facade.addHistory(QStringLiteral("Ada"), 10, false, false, false);
    facade.addHistory(QStringLiteral("Bob"), 20, true, false, false);
    QCOMPARE(facade.history().size(), 2);

    QSignalSpy changed(&facade, &CallsFacade::changed);
    facade.clearHistory();

    QVERIFY(facade.history().isEmpty());
    QCOMPARE(changed.count(), 1);

    // A new call after a clear starts a fresh list.
    facade.addHistory(QStringLiteral("Cleo"), 40, false, true, false);
    QCOMPARE(facade.history().size(), 1);
    QVERIFY(facade.history().first().startsWith(QStringLiteral("Cleo ")));
}

QTEST_GUILESS_MAIN(CallsFacadeTest)
#include "tst_calls_facade.moc"
