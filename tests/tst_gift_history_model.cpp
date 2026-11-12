#include "telegram/GiftHistoryModel.h"

#include <QSignalSpy>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

GiftHistoryRow row(qint64 giftId, qint64 date, qint64 senderUserId = 0,
                   qint64 receiverUserId = 0, qint64 starCount = 0,
                   bool anonymous = false)
{
    GiftHistoryRow value;
    value.giftId = giftId;
    value.senderUserId = senderUserId;
    value.receiverUserId = receiverUserId;
    value.date = date;
    value.starCount = starCount;
    value.anonymous = anonymous;
    return value;
}

QVector<qint64> idsOf(const QVector<GiftHistoryRow>& rows)
{
    QVector<qint64> ids;
    ids.reserve(rows.size());
    for (const GiftHistoryRow& value : rows)
        ids.append(value.giftId);
    return ids;
}

} // namespace

class GiftHistoryModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void dropsNonPositiveGiftIds();
    void keepsFirstRowOfDuplicateGiftId();
    void sortsNewestFirstThenByGiftIdDescending();
    void clampsNegativeStarCount();
    void emitsChangedOnlyOnRealChange();
    void sentKeepsStoredOrder();
    void receivedKeepsStoredOrder();
    void totalStarsSumsStoredRows();
    void totalStarsSaturates();
    void appendAddsOnlyNewGiftIds();
    void appendDropsInvalidAndRepeatedBatchRows();
    void appendIsNoOpWhenNothingNew();
    void clearEmptiesHistory();
    void clearIsNoOpWhenEmpty();
};

void GiftHistoryModelTest::startsEmpty()
{
    GiftHistoryModel model;
    QCOMPARE(model.count(), 0);
    QVERIFY(model.rows().isEmpty());
    QVERIFY(model.sent().isEmpty());
    QVERIFY(model.received().isEmpty());
    QCOMPARE(model.totalStars(), qint64(0));
}

void GiftHistoryModelTest::dropsNonPositiveGiftIds()
{
    GiftHistoryModel model;
    model.setRows({
        row(0, 1700000300),
        row(-5, 1700000200),
        row(7, 1700000100),
    });

    QCOMPARE(model.count(), 1);
    QCOMPARE(idsOf(model.rows()), QVector<qint64>{7});
}

void GiftHistoryModelTest::keepsFirstRowOfDuplicateGiftId()
{
    GiftHistoryModel model;
    model.setRows({
        row(11, 1700000100, 42, 7, 30),
        row(11, 1700000900, 99, 7, 80, true),
        row(12, 1700000200, 42, 7, 5),
    });

    const QVector<GiftHistoryRow> stored = model.rows();
    QCOMPARE(stored.size(), 2);

    // The later copy of gift 11 is dropped, so its newer date, other sender,
    // higher price and anonymity flag never reach the model.
    QCOMPARE(stored.at(0).giftId, qint64(12));
    QCOMPARE(stored.at(1).giftId, qint64(11));
    QCOMPARE(stored.at(1).date, qint64(1700000100));
    QCOMPARE(stored.at(1).senderUserId, qint64(42));
    QCOMPARE(stored.at(1).starCount, qint64(30));
    QVERIFY(!stored.at(1).anonymous);
}

void GiftHistoryModelTest::sortsNewestFirstThenByGiftIdDescending()
{
    GiftHistoryModel model;
    model.setRows({
        row(1, 1700000100),
        row(2, 1700000300),
        row(3, 1700000200),
        row(4, 1700000300),
        row(5, 1700000200),
    });

    QCOMPARE(idsOf(model.rows()), (QVector<qint64>{4, 2, 5, 3, 1}));
}

void GiftHistoryModelTest::clampsNegativeStarCount()
{
    GiftHistoryModel model;
    model.setRows({
        row(1, 1700000100, 42, 7, -7),
        row(2, 1700000200, 42, 7, 12),
    });

    const QVector<GiftHistoryRow> stored = model.rows();
    QCOMPARE(stored.at(0).giftId, qint64(2));
    QCOMPARE(stored.at(0).starCount, qint64(12));
    QCOMPARE(stored.at(1).giftId, qint64(1));
    QCOMPARE(stored.at(1).starCount, qint64(0));
    QCOMPARE(model.totalStars(), qint64(12));
}

void GiftHistoryModelTest::emitsChangedOnlyOnRealChange()
{
    GiftHistoryModel model;
    QSignalSpy spy(&model, &GiftHistoryModel::changed);

    model.setRows({row(1, 1700000100, 42, 7, 10), row(2, 1700000200, 42, 7, 20)});
    QCOMPARE(spy.count(), 1);

    // Same content, different input order: both normalise to the same list.
    model.setRows({row(2, 1700000200, 42, 7, 20), row(1, 1700000100, 42, 7, 10)});
    QCOMPARE(spy.count(), 1);

    // A row that is dropped leaves the stored list untouched.
    model.setRows({row(2, 1700000200, 42, 7, 20), row(1, 1700000100, 42, 7, 10),
                   row(0, 1700000300, 42, 7, 30)});
    QCOMPARE(spy.count(), 1);

    // A real field change is reported, including the anonymity flag.
    model.setRows({row(1, 1700000100, 42, 7, 10), row(2, 1700000200, 42, 7, 20, true)});
    QCOMPARE(spy.count(), 2);
}

void GiftHistoryModelTest::sentKeepsStoredOrder()
{
    GiftHistoryModel model;
    model.setRows({
        row(1, 1700000100, 7, 0),
        row(2, 1700000200, 0, 5),
        row(3, 1700000300, 9, 5, 25, true),
        row(4, 1700000400),
    });

    // Stored newest first, and sent() keeps that order rather than re-sorting.
    // The anonymous row still counts as sent: the flag hides the sender from
    // other users, it does not remove the sender id.
    QCOMPARE(idsOf(model.sent()), (QVector<qint64>{3, 1}));
}

void GiftHistoryModelTest::receivedKeepsStoredOrder()
{
    GiftHistoryModel model;
    model.setRows({
        row(1, 1700000100, 7, 0),
        row(2, 1700000200, 0, 5),
        row(3, 1700000300, 9, 5, 25, true),
        row(4, 1700000400),
    });

    QCOMPARE(idsOf(model.received()), (QVector<qint64>{3, 2}));
}

void GiftHistoryModelTest::totalStarsSumsStoredRows()
{
    GiftHistoryModel model;
    QCOMPARE(model.totalStars(), qint64(0));

    model.setRows({
        row(1, 1700000100, 42, 7, 10),
        row(2, 1700000200, 42, 7, 0),
        row(3, 1700000300, 42, 7, -4),
        row(4, 1700000400, 42, 7, 25),
    });

    // The clamped row contributes nothing instead of subtracting.
    QCOMPARE(model.totalStars(), qint64(35));
}

void GiftHistoryModelTest::totalStarsSaturates()
{
    GiftHistoryModel model;
    const qint64 maximum = std::numeric_limits<qint64>::max();

    model.setRows({
        row(1, 1700000100, 42, 7, maximum),
        row(2, 1700000200, 42, 7, 5),
    });

    QCOMPARE(model.totalStars(), maximum);
}

void GiftHistoryModelTest::appendAddsOnlyNewGiftIds()
{
    GiftHistoryModel model;
    model.setRows({row(1, 1700000100, 7, 5, 10), row(2, 1700000200, 7, 5, 20)});

    QSignalSpy spy(&model, &GiftHistoryModel::changed);
    model.append({
        row(2, 1700000900, 8, 8, 99, true),
        row(3, 1700000150, 7, 5, 30),
    });

    QCOMPARE(spy.count(), 1);
    QCOMPARE(idsOf(model.rows()), (QVector<qint64>{2, 3, 1}));

    // The stored copy of gift 2 is untouched by the incoming one.
    const QVector<GiftHistoryRow> stored = model.rows();
    QCOMPARE(stored.at(0).date, qint64(1700000200));
    QCOMPARE(stored.at(0).senderUserId, qint64(7));
    QCOMPARE(stored.at(0).starCount, qint64(20));
    QVERIFY(!stored.at(0).anonymous);
    QCOMPARE(stored.at(1).starCount, qint64(30));
    QCOMPARE(model.totalStars(), qint64(60));
}

void GiftHistoryModelTest::appendDropsInvalidAndRepeatedBatchRows()
{
    GiftHistoryModel model;
    QSignalSpy spy(&model, &GiftHistoryModel::changed);

    model.append({
        row(0, 1700000100),
        row(-3, 1700000200),
        row(4, 1700000500, 7, 5, 15),
        row(4, 1700000600, 7, 5, 90),
    });

    // One signal for the whole batch, and the first copy of gift 4 wins.
    QCOMPARE(spy.count(), 1);
    QCOMPARE(idsOf(model.rows()), QVector<qint64>{4});
    QCOMPARE(model.rows().at(0).date, qint64(1700000500));
    QCOMPARE(model.rows().at(0).starCount, qint64(15));
}

void GiftHistoryModelTest::appendIsNoOpWhenNothingNew()
{
    GiftHistoryModel model;
    model.setRows({row(1, 1700000100, 7, 5, 10)});

    QSignalSpy spy(&model, &GiftHistoryModel::changed);

    model.append({row(0, 1700000200), row(1, 1700000300, 7, 5, 80)});
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.rows().at(0).starCount, qint64(10));

    model.append({});
    QCOMPARE(spy.count(), 0);
}

void GiftHistoryModelTest::clearEmptiesHistory()
{
    GiftHistoryModel model;
    model.setRows({row(1, 1700000100, 7, 5, 10), row(2, 1700000200, 7, 5, 20)});

    QSignalSpy spy(&model, &GiftHistoryModel::changed);
    model.clear();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.count(), 0);
    QVERIFY(model.rows().isEmpty());
    QVERIFY(model.sent().isEmpty());
    QVERIFY(model.received().isEmpty());
    QCOMPARE(model.totalStars(), qint64(0));
}

void GiftHistoryModelTest::clearIsNoOpWhenEmpty()
{
    GiftHistoryModel model;
    QSignalSpy spy(&model, &GiftHistoryModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);

    // Clearing twice in a row reports the drop once.
    model.setRows({row(1, 1700000100, 7, 5, 10)});
    QCOMPARE(spy.count(), 1);
    model.clear();
    QCOMPARE(spy.count(), 2);
    model.clear();
    QCOMPARE(spy.count(), 2);
}

QTEST_GUILESS_MAIN(GiftHistoryModelTest)
#include "tst_gift_history_model.moc"
