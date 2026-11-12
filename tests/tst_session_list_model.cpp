#include "telegram/SessionListModel.h"

#include <QSignalSpy>
#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

SessionInfo makeSession(qint64 id, const QString& device, bool current = false,
                        const QString& location = QStringLiteral("Berlin, Germany"),
                        qint64 lastActive = 1700000000)
{
    SessionInfo session;
    session.id = id;
    session.device = device;
    session.location = location;
    session.lastActive = lastActive;
    session.current = current;
    return session;
}

} // namespace

class SessionListModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setSessionsKeepsOrder();
    void setSessionsDropsNonPositiveIds();
    void setSessionsKeepsFirstDuplicate();
    void setSessionsEmitsChangedOnce();
    void setSessionsSameSessionsIsSilent();
    void setSessionsReorderedIsAChange();
    void setSessionsChangedFieldIsAChange();
    void setSessionsReplacesPrevious();
    void setSessionsDemotesLaterCurrent();
    void setSessionsKeepsFirstCurrentWhenDuplicated();
    void sessionsReturnsCopy();
    void countMatchesSessions();
    void othersExcludesCurrent();
    void othersEmptyWhenOnlyCurrent();
    void othersKeepsOrder();
    void removeDropsSession();
    void removeAbsentIsNoOp();
    void clearForgetsSessions();
    void clearOnEmptyModelIsSilent();
    void modelIsUsableAfterClear();
};

void SessionListModelTest::startsEmpty()
{
    SessionListModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.sessions().isEmpty());
    QVERIFY(model.others().isEmpty());
}

void SessionListModelTest::setSessionsKeepsOrder()
{
    SessionListModel model;
    model.setSessions({makeSession(30, QStringLiteral("Pixel 8")),
                       makeSession(10, QStringLiteral("Desktop"), true),
                       makeSession(20, QStringLiteral("iPad"))});

    const QVector<SessionInfo> sessions = model.sessions();
    QCOMPARE(int(sessions.size()), 3);
    // Telegram's order is the screen's order: nothing here sorts by id or date.
    QCOMPARE(sessions.at(0).id, qint64(30));
    QCOMPARE(sessions.at(1).id, qint64(10));
    QCOMPARE(sessions.at(2).id, qint64(20));
}

void SessionListModelTest::setSessionsDropsNonPositiveIds()
{
    SessionListModel model;
    model.setSessions({makeSession(0, QStringLiteral("No id")),
                       makeSession(7, QStringLiteral("Pixel 8")),
                       makeSession(-3, QStringLiteral("Bad id"))});

    // A session without a positive id can be neither shown nor terminated.
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.sessions().at(0).id, qint64(7));
    QCOMPARE(model.sessions().at(0).device, QStringLiteral("Pixel 8"));
}

void SessionListModelTest::setSessionsKeepsFirstDuplicate()
{
    SessionListModel model;
    model.setSessions({makeSession(5, QStringLiteral("First")),
                       makeSession(5, QStringLiteral("Second")),
                       makeSession(6, QStringLiteral("Other"))});

    QCOMPARE(model.count(), 2);
    QCOMPARE(model.sessions().at(0).device, QStringLiteral("First"));
    QCOMPARE(model.sessions().at(1).id, qint64(6));
}

void SessionListModelTest::setSessionsEmitsChangedOnce()
{
    SessionListModel model;
    QSignalSpy spy(&model, &SessionListModel::changed);

    model.setSessions({makeSession(1, QStringLiteral("Pixel 8"), true)});

    QCOMPARE(spy.count(), 1);
}

void SessionListModelTest::setSessionsSameSessionsIsSilent()
{
    SessionListModel model;
    const QVector<SessionInfo> sessions{
        makeSession(1, QStringLiteral("Pixel 8"), true),
        makeSession(2, QStringLiteral("Desktop"))};
    model.setSessions(sessions);

    QSignalSpy spy(&model, &SessionListModel::changed);
    model.setSessions(sessions);

    // A refresh that brings the same data must not repaint the list.
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 2);
}

void SessionListModelTest::setSessionsReorderedIsAChange()
{
    SessionListModel model;
    model.setSessions({makeSession(1, QStringLiteral("Pixel 8")),
                       makeSession(2, QStringLiteral("Desktop"))});

    QSignalSpy spy(&model, &SessionListModel::changed);
    model.setSessions({makeSession(2, QStringLiteral("Desktop")),
                       makeSession(1, QStringLiteral("Pixel 8"))});

    // Same sessions, different display order: the screen would look different.
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.sessions().at(0).id, qint64(2));
}

void SessionListModelTest::setSessionsChangedFieldIsAChange()
{
    SessionListModel model;
    model.setSessions({makeSession(1, QStringLiteral("Pixel 8"), true,
                                   QStringLiteral("Berlin, Germany"), 1700000000)});

    // lastActive moves when the session is used again, so the row's "last
    // active" label is stale until changed() fires.
    QSignalSpy spy(&model, &SessionListModel::changed);
    model.setSessions({makeSession(1, QStringLiteral("Pixel 8"), true,
                                   QStringLiteral("Berlin, Germany"), 1700009999)});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.sessions().at(0).lastActive, qint64(1700009999));
}

void SessionListModelTest::setSessionsReplacesPrevious()
{
    SessionListModel model;
    model.setSessions({makeSession(1, QStringLiteral("Pixel 8"))});
    model.setSessions({makeSession(2, QStringLiteral("Desktop"))});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.sessions().at(0).id, qint64(2));
}

void SessionListModelTest::setSessionsDemotesLaterCurrent()
{
    SessionListModel model;
    // A merged or stale answer can flag more than one session as this device.
    model.setSessions({makeSession(1, QStringLiteral("Pixel 8")),
                       makeSession(2, QStringLiteral("Desktop"), true),
                       makeSession(3, QStringLiteral("iPad"), true)});

    QCOMPARE(model.count(), 3);
    QVERIFY(!model.sessions().at(0).current);
    QVERIFY(model.sessions().at(1).current);
    // The extra row is kept, not dropped: it is still a real session to end.
    QVERIFY(!model.sessions().at(2).current);

    const QVector<SessionInfo> others = model.others();
    QCOMPARE(int(others.size()), 2);
    QCOMPARE(others.at(0).id, qint64(1));
    QCOMPARE(others.at(1).id, qint64(3));
}

void SessionListModelTest::setSessionsKeepsFirstCurrentWhenDuplicated()
{
    SessionListModel model;
    // The duplicate id is dropped before the flag matters, so the surviving
    // row keeps whatever flag it arrived with.
    model.setSessions({makeSession(1, QStringLiteral("Pixel 8")),
                       makeSession(1, QStringLiteral("Pixel 8 (dup)"), true),
                       makeSession(2, QStringLiteral("Desktop"), true)});

    QCOMPARE(model.count(), 2);
    QVERIFY(!model.sessions().at(0).current);
    QVERIFY(model.sessions().at(1).current);
    QCOMPARE(int(model.others().size()), 1);
    QCOMPARE(model.others().at(0).id, qint64(1));
}

void SessionListModelTest::sessionsReturnsCopy()
{
    SessionListModel model;
    model.setSessions({makeSession(1, QStringLiteral("Pixel 8"))});

    QVector<SessionInfo> copy = model.sessions();
    copy[0].device = QStringLiteral("Mutated");

    // Handing out a copy keeps the model's own state private.
    QCOMPARE(model.sessions().at(0).device, QStringLiteral("Pixel 8"));
}

void SessionListModelTest::countMatchesSessions()
{
    SessionListModel model;
    QCOMPARE(model.count(), 0);

    model.setSessions({makeSession(1, QStringLiteral("Pixel 8"), true),
                       makeSession(2, QStringLiteral("Desktop"))});
    QCOMPARE(model.count(), int(model.sessions().size()));
    QCOMPARE(model.count(), 2);
}

void SessionListModelTest::othersExcludesCurrent()
{
    SessionListModel model;
    model.setSessions({makeSession(1, QStringLiteral("Pixel 8"), true),
                       makeSession(2, QStringLiteral("Desktop")),
                       makeSession(3, QStringLiteral("iPad"))});

    const QVector<SessionInfo> others = model.others();
    QCOMPARE(int(others.size()), 2);
    QCOMPARE(others.at(0).id, qint64(2));
    QCOMPARE(others.at(1).id, qint64(3));
}

void SessionListModelTest::othersEmptyWhenOnlyCurrent()
{
    SessionListModel model;
    model.setSessions({makeSession(1, QStringLiteral("Pixel 8"), true)});

    // With one device signed in, "terminate all other sessions" has no target.
    QVERIFY(model.others().isEmpty());
    QCOMPARE(model.count(), 1);
}

void SessionListModelTest::othersKeepsOrder()
{
    SessionListModel model;
    model.setSessions({makeSession(9, QStringLiteral("iPad")),
                       makeSession(4, QStringLiteral("Pixel 8"), true),
                       makeSession(5, QStringLiteral("Desktop"))});

    const QVector<SessionInfo> others = model.others();
    QCOMPARE(int(others.size()), 2);
    QCOMPARE(others.at(0).id, qint64(9));
    QCOMPARE(others.at(1).id, qint64(5));
}

void SessionListModelTest::removeDropsSession()
{
    SessionListModel model;
    model.setSessions({makeSession(1, QStringLiteral("Pixel 8"), true),
                       makeSession(2, QStringLiteral("Desktop")),
                       makeSession(3, QStringLiteral("iPad"))});

    QSignalSpy spy(&model, &SessionListModel::changed);
    QVERIFY(model.remove(2));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.sessions().at(0).id, qint64(1));
    QCOMPARE(model.sessions().at(1).id, qint64(3));
    QCOMPARE(int(model.others().size()), 1);
    QCOMPARE(model.others().at(0).id, qint64(3));
}

void SessionListModelTest::removeAbsentIsNoOp()
{
    SessionListModel model;
    model.setSessions({makeSession(1, QStringLiteral("Pixel 8"), true)});

    QSignalSpy spy(&model, &SessionListModel::changed);
    QVERIFY(!model.remove(42));

    // A second termination answer for the same session must not repaint.
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 1);
}

void SessionListModelTest::clearForgetsSessions()
{
    SessionListModel model;
    model.setSessions({makeSession(1, QStringLiteral("Pixel 8"), true),
                       makeSession(2, QStringLiteral("Desktop"))});

    QSignalSpy spy(&model, &SessionListModel::changed);
    model.clear();

    QCOMPARE(spy.count(), 1);
    QVERIFY(model.sessions().isEmpty());
    QVERIFY(model.others().isEmpty());
    QCOMPARE(model.count(), 0);
}

void SessionListModelTest::clearOnEmptyModelIsSilent()
{
    SessionListModel model;
    QSignalSpy spy(&model, &SessionListModel::changed);

    model.clear();

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.sessions().isEmpty());
}

void SessionListModelTest::modelIsUsableAfterClear()
{
    SessionListModel model;
    model.setSessions({makeSession(1, QStringLiteral("Pixel 8"), true)});
    model.clear();

    // A fresh answer after sign-out behaves like a first load.
    model.setSessions({makeSession(4, QStringLiteral("Desktop"), true),
                       makeSession(5, QStringLiteral("iPad"))});
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.others().at(0).id, qint64(5));
    QVERIFY(model.remove(5));
    QVERIFY(model.others().isEmpty());
    QCOMPARE(model.count(), 1);
}

QTEST_GUILESS_MAIN(SessionListModelTest)
#include "tst_session_list_model.moc"
