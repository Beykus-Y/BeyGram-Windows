#include "telegram/GroupCallModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

GroupCallParticipant participant(qint64 userId, const QString& name, bool muted = false,
                                 bool speaking = false, bool video = false)
{
    GroupCallParticipant value;
    value.userId = userId;
    value.name = name;
    value.muted = muted;
    value.speaking = speaking;
    value.video = video;
    return value;
}

QVector<qint64> idsOf(const QVector<GroupCallParticipant>& participants)
{
    QVector<qint64> ids;
    ids.reserve(participants.size());
    for (const GroupCallParticipant& value : participants)
        ids.append(value.userId);
    return ids;
}

} // namespace

class GroupCallModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsInactive();
    void setCallActivatesCall();
    void setCallIgnoresNonPositiveId();
    void setCallClampsNegativeCount();
    void setCallIsSilentWhenNothingChanges();
    void setCallToAnotherCallDropsStaleParticipants();
    void setParticipantsDropsNonPositiveIds();
    void setParticipantsKeepsFirstDuplicate();
    void setParticipantsPreservesOrder();
    void setParticipantsIsSilentWhenNothingChanges();
    void updateParticipantChangesExistingRow();
    void updateParticipantRefusesUnknownId();
    void updateParticipantIsSilentWhenNothingChanges();
    void speakingCountCountsSpeakingParticipants();
    void leaveEndsCallAndDropsParticipants();
    void leaveIsNoOpWhenAlreadyEmpty();
    void clearEmptiesModel();
    void clearIsNoOpWhenEmpty();
};

void GroupCallModelTest::startsInactive()
{
    GroupCallModel model;
    QVERIFY(!model.active());
    QCOMPARE(model.callId(), qint64(0));
    QCOMPARE(model.participantCount(), 0);
    QVERIFY(model.participants().isEmpty());
    QCOMPARE(model.speakingCount(), 0);
}

void GroupCallModelTest::setCallActivatesCall()
{
    GroupCallModel model;
    QSignalSpy spy(&model, &GroupCallModel::changed);

    model.setCall(700, 5);

    QCOMPARE(spy.count(), 1);
    QVERIFY(model.active());
    QCOMPARE(model.callId(), qint64(700));
    QCOMPARE(model.participantCount(), 5);
    QVERIFY(model.participants().isEmpty());
}

void GroupCallModelTest::setCallIgnoresNonPositiveId()
{
    GroupCallModel model;
    QSignalSpy spy(&model, &GroupCallModel::changed);

    // No call id means no call, so the model stays inactive rather than
    // holding an id that active() would have to special-case.
    model.setCall(0, 4);
    model.setCall(-3, 4);

    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.active());
    QCOMPARE(model.callId(), qint64(0));
    QCOMPARE(model.participantCount(), 0);

    // A refused id also leaves an already active call alone.
    model.setCall(700, 5);
    QCOMPARE(spy.count(), 1);
    model.setCall(0, 99);
    model.setCall(-700, 99);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.callId(), qint64(700));
    QCOMPARE(model.participantCount(), 5);
}

void GroupCallModelTest::setCallClampsNegativeCount()
{
    GroupCallModel model;
    model.setCall(700, -12);

    QVERIFY(model.active());
    QCOMPARE(model.participantCount(), 0);

    // A later negative count does not reopen a count that was already 0.
    QSignalSpy spy(&model, &GroupCallModel::changed);
    model.setCall(700, -1);
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.participantCount(), 0);
}

void GroupCallModelTest::setCallIsSilentWhenNothingChanges()
{
    GroupCallModel model;
    QSignalSpy spy(&model, &GroupCallModel::changed);

    model.setCall(700, 5);
    QCOMPARE(spy.count(), 1);

    model.setCall(700, 5);
    QCOMPARE(spy.count(), 1);

    // A real count change is reported once.
    model.setCall(700, 6);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.participantCount(), 6);
}

void GroupCallModelTest::setCallToAnotherCallDropsStaleParticipants()
{
    GroupCallModel model;
    model.setCall(700, 3);
    model.setParticipants({participant(1, QStringLiteral("Ana")),
                           participant(2, QStringLiteral("Bo"))});

    QSignalSpy spy(&model, &GroupCallModel::changed);

    // Same count, different call: the roster belonged to call 700.
    model.setCall(701, 3);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.callId(), qint64(701));
    QVERIFY(model.participants().isEmpty());
    QCOMPARE(model.speakingCount(), 0);
}

void GroupCallModelTest::setParticipantsDropsNonPositiveIds()
{
    GroupCallModel model;
    model.setCall(700, 4);
    model.setParticipants({
        participant(0, QStringLiteral("Nobody")),
        participant(-5, QStringLiteral("Ghost")),
        participant(7, QStringLiteral("Ana")),
    });

    QCOMPARE(model.participants().size(), 1);
    QCOMPARE(idsOf(model.participants()), QVector<qint64>{7});
    QCOMPARE(model.participants().at(0).name, QStringLiteral("Ana"));
}

void GroupCallModelTest::setParticipantsKeepsFirstDuplicate()
{
    GroupCallModel model;
    model.setCall(700, 3);
    model.setParticipants({
        participant(11, QStringLiteral("Ana"), false, true, true),
        participant(11, QStringLiteral("Ana Renamed"), true, false, false),
        participant(12, QStringLiteral("Bo")),
    });

    const QVector<GroupCallParticipant> stored = model.participants();
    QCOMPARE(idsOf(stored), (QVector<qint64>{11, 12}));

    // The later row for id 11 is dropped, so its rename, mute and lost video
    // never reach the model.
    QCOMPARE(stored.at(0).name, QStringLiteral("Ana"));
    QVERIFY(!stored.at(0).muted);
    QVERIFY(stored.at(0).speaking);
    QVERIFY(stored.at(0).video);
    QCOMPARE(model.speakingCount(), 1);
}

void GroupCallModelTest::setParticipantsPreservesOrder()
{
    GroupCallModel model;
    model.setCall(700, 4);
    model.setParticipants({
        participant(30, QStringLiteral("Zoe")),
        participant(10, QStringLiteral("Ana")),
        participant(20, QStringLiteral("Bo")),
    });

    // Insertion order, not sorted by id or name.
    QCOMPARE(idsOf(model.participants()), (QVector<qint64>{30, 10, 20}));
}

void GroupCallModelTest::setParticipantsIsSilentWhenNothingChanges()
{
    GroupCallModel model;
    model.setCall(700, 3);
    QSignalSpy spy(&model, &GroupCallModel::changed);

    const QVector<GroupCallParticipant> roster{participant(1, QStringLiteral("Ana")),
                                               participant(2, QStringLiteral("Bo"))};
    model.setParticipants(roster);
    QCOMPARE(spy.count(), 1);

    model.setParticipants(roster);
    QCOMPARE(spy.count(), 1);

    // Input that normalises to the same roster is not a change.
    model.setParticipants({participant(1, QStringLiteral("Ana")),
                           participant(0, QStringLiteral("Nobody")),
                           participant(2, QStringLiteral("Bo")),
                           participant(2, QStringLiteral("Bo Again"))});
    QCOMPARE(spy.count(), 1);

    // A field change on a stored row is a change.
    model.setParticipants({participant(1, QStringLiteral("Ana"), true),
                           participant(2, QStringLiteral("Bo"))});
    QCOMPARE(spy.count(), 2);
}

void GroupCallModelTest::updateParticipantChangesExistingRow()
{
    GroupCallModel model;
    model.setCall(700, 2);
    model.setParticipants({participant(1, QStringLiteral("Ana")),
                           participant(2, QStringLiteral("Bo"))});

    QSignalSpy spy(&model, &GroupCallModel::changed);

    QVERIFY(model.updateParticipant(participant(2, QStringLiteral("Bo"), true, false, true)));
    QCOMPARE(spy.count(), 1);

    const QVector<GroupCallParticipant> stored = model.participants();
    QCOMPARE(stored.size(), 2);
    QCOMPARE(stored.at(1).userId, qint64(2));
    QVERIFY(stored.at(1).muted);
    QVERIFY(stored.at(1).video);

    // The other row and the order are untouched.
    QCOMPARE(stored.at(0).userId, qint64(1));
    QVERIFY(!stored.at(0).muted);
}

void GroupCallModelTest::updateParticipantRefusesUnknownId()
{
    GroupCallModel model;
    model.setCall(700, 2);
    model.setParticipants({participant(1, QStringLiteral("Ana"))});

    QSignalSpy spy(&model, &GroupCallModel::changed);

    // An update cannot add a row; only setParticipants() fills the roster.
    QVERIFY(!model.updateParticipant(participant(2, QStringLiteral("Bo"))));
    QVERIFY(!model.updateParticipant(participant(0, QStringLiteral("Nobody"))));
    QVERIFY(!model.updateParticipant(participant(-1, QStringLiteral("Ghost"))));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(idsOf(model.participants()), QVector<qint64>{1});
}

void GroupCallModelTest::updateParticipantIsSilentWhenNothingChanges()
{
    GroupCallModel model;
    model.setCall(700, 1);
    const GroupCallParticipant ana = participant(1, QStringLiteral("Ana"), true, false, true);
    model.setParticipants({ana});

    QSignalSpy spy(&model, &GroupCallModel::changed);

    // A heartbeat repeating the current state changes nothing.
    QVERIFY(!model.updateParticipant(ana));
    QCOMPARE(spy.count(), 0);

    QVERIFY(model.updateParticipant(participant(1, QStringLiteral("Ana"), true, true, true)));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.speakingCount(), 1);
}

void GroupCallModelTest::speakingCountCountsSpeakingParticipants()
{
    GroupCallModel model;
    model.setCall(700, 4);

    QCOMPARE(model.speakingCount(), 0);

    model.setParticipants({participant(1, QStringLiteral("Ana"), false, true),
                           participant(2, QStringLiteral("Bo"), true, false),
                           participant(3, QStringLiteral("Ca"), false, true),
                           participant(4, QStringLiteral("Di"))});

    QCOMPARE(model.speakingCount(), 2);
    QCOMPARE(model.participants().size(), 4);

    // Speaking is read as delivered, so a muted entry that still claims to
    // speak is counted rather than silently rewritten.
    model.setParticipants({participant(1, QStringLiteral("Ana"), true, true)});
    QCOMPARE(model.speakingCount(), 1);

    model.setParticipants({});
    QCOMPARE(model.speakingCount(), 0);
}

void GroupCallModelTest::leaveEndsCallAndDropsParticipants()
{
    GroupCallModel model;
    model.setCall(700, 3);
    model.setParticipants({participant(1, QStringLiteral("Ana"), false, true),
                           participant(2, QStringLiteral("Bo"))});

    QSignalSpy spy(&model, &GroupCallModel::changed);
    model.leave();

    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.active());
    QCOMPARE(model.callId(), qint64(0));
    QCOMPARE(model.participantCount(), 0);
    QVERIFY(model.participants().isEmpty());
    QCOMPARE(model.speakingCount(), 0);

    // The model is reusable after leaving.
    model.setCall(800, 2);
    QCOMPARE(spy.count(), 2);
    QVERIFY(model.active());
    QCOMPARE(model.callId(), qint64(800));
    QCOMPARE(model.participantCount(), 2);
}

void GroupCallModelTest::leaveIsNoOpWhenAlreadyEmpty()
{
    GroupCallModel model;
    QSignalSpy spy(&model, &GroupCallModel::changed);

    model.leave();
    QCOMPARE(spy.count(), 0);

    model.setCall(700, 1);
    model.leave();
    QCOMPARE(spy.count(), 2);

    // Leaving twice reports the drop once.
    model.leave();
    QCOMPARE(spy.count(), 2);
}

void GroupCallModelTest::clearEmptiesModel()
{
    GroupCallModel model;
    model.setCall(700, 3);
    model.setParticipants({participant(1, QStringLiteral("Ana"), false, true),
                           participant(2, QStringLiteral("Bo"))});

    QSignalSpy spy(&model, &GroupCallModel::changed);
    model.clear();

    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.active());
    QCOMPARE(model.callId(), qint64(0));
    QCOMPARE(model.participantCount(), 0);
    QVERIFY(model.participants().isEmpty());
    QCOMPARE(model.speakingCount(), 0);
}

void GroupCallModelTest::clearIsNoOpWhenEmpty()
{
    GroupCallModel model;
    QSignalSpy spy(&model, &GroupCallModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);

    // Clearing twice in a row reports the drop once.
    model.setCall(700, 1);
    model.setParticipants({participant(1, QStringLiteral("Ana"))});
    QCOMPARE(spy.count(), 2);

    model.clear();
    QCOMPARE(spy.count(), 3);
    model.clear();
    QCOMPARE(spy.count(), 3);

    // A participant-only roster with no call still counts as state to drop.
    model.setParticipants({participant(5, QStringLiteral("Eve"))});
    QCOMPARE(spy.count(), 4);
    model.clear();
    QCOMPARE(spy.count(), 5);
    QVERIFY(model.participants().isEmpty());
}

QTEST_GUILESS_MAIN(GroupCallModelTest)
#include "tst_group_call_model.moc"
