#include "telegram/CallHistoryModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

CallRecord record(qint64 messageId, qint64 chatId, qint64 date,
                  const QString& status = QStringLiteral("ended"), int duration = 0,
                  bool outgoing = false, bool video = false)
{
    CallRecord value;
    value.messageId = messageId;
    value.chatId = chatId;
    value.date = date;
    value.status = status;
    value.duration = duration;
    value.outgoing = outgoing;
    value.video = video;
    return value;
}

QVector<qint64> idsOf(const QVector<CallRecord>& records)
{
    QVector<qint64> ids;
    ids.reserve(records.size());
    for (const CallRecord& value : records)
        ids.append(value.messageId);
    return ids;
}

} // namespace

class CallHistoryModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void dropsNonPositiveIds();
    void keepsFirstRecordOfDuplicateId();
    void sortsNewestFirstThenByIdDescending();
    void clampsNegativeDuration();
    void emitsChangedOnlyOnRealChange();
    void missedKeepsStoredOrder();
    void countsMissedCallsOnly();
    void filtersByUser();
    void clearEmptiesHistory();
    void clearIsNoOpWhenEmpty();
};

void CallHistoryModelTest::startsEmpty()
{
    CallHistoryModel model;
    QCOMPARE(model.count(), 0);
    QVERIFY(model.records().isEmpty());
    QVERIFY(model.missed().isEmpty());
    QCOMPARE(model.missedCount(), 0);
    QVERIFY(model.withUser(42).isEmpty());
}

void CallHistoryModelTest::dropsNonPositiveIds()
{
    CallHistoryModel model;
    model.setRecords({
        record(0, 42, 1700000300),
        record(-5, 42, 1700000200),
        record(7, 42, 1700000100),
    });

    QCOMPARE(model.count(), 1);
    QCOMPARE(idsOf(model.records()), QVector<qint64>{7});
}

void CallHistoryModelTest::keepsFirstRecordOfDuplicateId()
{
    CallHistoryModel model;
    model.setRecords({
        record(11, 42, 1700000100, QStringLiteral("ended"), 30),
        record(11, 99, 1700000900, QStringLiteral("missed"), 0),
        record(12, 42, 1700000200, QStringLiteral("ended"), 5),
    });

    const QVector<CallRecord> stored = model.records();
    QCOMPARE(stored.size(), 2);

    // The later record for id 11 is dropped, so its newer date, other peer and
    // missed status never reach the model.
    QCOMPARE(stored.at(0).messageId, qint64(12));
    QCOMPARE(stored.at(1).messageId, qint64(11));
    QCOMPARE(stored.at(1).chatId, qint64(42));
    QCOMPARE(stored.at(1).duration, 30);
    QCOMPARE(stored.at(1).status, QStringLiteral("ended"));
    QCOMPARE(model.missedCount(), 0);
}

void CallHistoryModelTest::sortsNewestFirstThenByIdDescending()
{
    CallHistoryModel model;
    model.setRecords({
        record(1, 42, 1700000100),
        record(2, 42, 1700000300),
        record(3, 42, 1700000200),
        record(4, 42, 1700000300),
        record(5, 42, 1700000200),
    });

    QCOMPARE(idsOf(model.records()), (QVector<qint64>{4, 2, 5, 3, 1}));
}

void CallHistoryModelTest::clampsNegativeDuration()
{
    CallHistoryModel model;
    model.setRecords({
        record(1, 42, 1700000100, QStringLiteral("ended"), -7),
        record(2, 42, 1700000200, QStringLiteral("ended"), 12),
    });

    const QVector<CallRecord> stored = model.records();
    QCOMPARE(stored.at(0).messageId, qint64(2));
    QCOMPARE(stored.at(0).duration, 12);
    QCOMPARE(stored.at(1).messageId, qint64(1));
    QCOMPARE(stored.at(1).duration, 0);
}

void CallHistoryModelTest::emitsChangedOnlyOnRealChange()
{
    CallHistoryModel model;
    QSignalSpy spy(&model, &CallHistoryModel::changed);

    model.setRecords({record(1, 42, 1700000100), record(2, 42, 1700000200)});
    QCOMPARE(spy.count(), 1);

    // Same content, different input order: the model normalises both to the
    // same list, so nothing changed.
    model.setRecords({record(2, 42, 1700000200), record(1, 42, 1700000100)});
    QCOMPARE(spy.count(), 1);

    // A record that is dropped leaves the stored list untouched.
    model.setRecords({record(2, 42, 1700000200), record(1, 42, 1700000100),
                      record(0, 42, 1700000300)});
    QCOMPARE(spy.count(), 1);

    // A real field change is reported.
    model.setRecords({record(1, 42, 1700000100), record(2, 42, 1700000200,
                                                      QStringLiteral("ended"), 60)});
    QCOMPARE(spy.count(), 2);
}

void CallHistoryModelTest::missedKeepsStoredOrder()
{
    CallHistoryModel model;
    model.setRecords({
        record(1, 42, 1700000100, QStringLiteral("missed")),
        record(2, 42, 1700000200, QStringLiteral("ended"), 30),
        record(3, 42, 1700000300, QStringLiteral("missed")),
        record(4, 42, 1700000400, QStringLiteral("cancelled")),
    });

    // Stored newest first, and missed() keeps that order rather than re-sorting.
    QCOMPARE(idsOf(model.missed()), (QVector<qint64>{3, 1}));
}

void CallHistoryModelTest::countsMissedCallsOnly()
{
    CallHistoryModel model;
    model.setRecords({
        record(1, 42, 1700000100, QStringLiteral("missed")),
        record(2, 42, 1700000200, QStringLiteral("missed")),
        record(3, 42, 1700000300, QStringLiteral("cancelled")),
        record(4, 42, 1700000400, QStringLiteral("declined")),
        record(5, 42, 1700000500, QStringLiteral("ended"), 10),
    });

    QCOMPARE(model.missedCount(), 2);
    QCOMPARE(model.count(), 5);
}

void CallHistoryModelTest::filtersByUser()
{
    CallHistoryModel model;
    model.setRecords({
        record(1, 42, 1700000100),
        record(2, 43, 1700000200),
        record(3, 42, 1700000300),
        record(4, 0, 1700000400),
        record(5, -1001234567890LL, 1700000500),
    });

    QCOMPARE(idsOf(model.withUser(42)), (QVector<qint64>{3, 1}));
    QCOMPARE(idsOf(model.withUser(43)), QVector<qint64>{2});

    // A non-positive id names no peer, and unknown peers match nothing.
    QVERIFY(model.withUser(0).isEmpty());
    QVERIFY(model.withUser(-42).isEmpty());
    QVERIFY(model.withUser(7).isEmpty());
}

void CallHistoryModelTest::clearEmptiesHistory()
{
    CallHistoryModel model;
    model.setRecords({record(1, 42, 1700000100, QStringLiteral("missed")),
                      record(2, 42, 1700000200)});

    QSignalSpy spy(&model, &CallHistoryModel::changed);
    model.clear();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.count(), 0);
    QVERIFY(model.records().isEmpty());
    QVERIFY(model.missed().isEmpty());
    QCOMPARE(model.missedCount(), 0);
    QVERIFY(model.withUser(42).isEmpty());
}

void CallHistoryModelTest::clearIsNoOpWhenEmpty()
{
    CallHistoryModel model;
    QSignalSpy spy(&model, &CallHistoryModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);

    // Clearing twice in a row reports the drop once.
    model.setRecords({record(1, 42, 1700000100)});
    QCOMPARE(spy.count(), 1);
    model.clear();
    QCOMPARE(spy.count(), 2);
    model.clear();
    QCOMPARE(spy.count(), 2);
}

QTEST_GUILESS_MAIN(CallHistoryModelTest)
#include "tst_call_history_model.moc"
