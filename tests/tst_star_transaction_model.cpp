#include "telegram/StarTransactionModel.h"

#include <QSignalSpy>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

StarTransaction transaction(qint64 id, qint64 date, qint64 amount,
                            const QString& title = QStringLiteral("stars"), bool incoming = false)
{
    StarTransaction value;
    value.id = id;
    value.date = date;
    value.amount = amount;
    value.title = title;
    value.incoming = incoming;
    return value;
}

QVector<qint64> idsOf(const QVector<StarTransaction>& transactions)
{
    QVector<qint64> ids;
    ids.reserve(transactions.size());
    for (const StarTransaction& value : transactions)
        ids.append(value.id);
    return ids;
}

} // namespace

class StarTransactionModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void dropsNonPositiveIds();
    void keepsFirstTransactionOfDuplicateId();
    void sortsNewestFirstThenByIdDescending();
    void storesAbsoluteAmounts();
    void totalsFollowIncomingFlag();
    void totalsSaturate();
    void netCombinesTotals();
    void emitsChangedOnlyOnRealChange();
    void appendAddsOnlyNewIds();
    void appendKeepsStoredCopy();
    void appendSortsAndReportsChange();
    void appendIsNoOpWithoutNewIds();
    void appendDropsInvalidAndDuplicateEntries();
    void clearEmptiesTransactions();
    void clearIsNoOpWhenEmpty();
};

void StarTransactionModelTest::startsEmpty()
{
    StarTransactionModel model;
    QCOMPARE(model.count(), 0);
    QVERIFY(model.transactions().isEmpty());
    QCOMPARE(model.totalIncoming(), qint64(0));
    QCOMPARE(model.totalOutgoing(), qint64(0));
    QCOMPARE(model.net(), qint64(0));
}

void StarTransactionModelTest::dropsNonPositiveIds()
{
    StarTransactionModel model;
    model.setTransactions({
        transaction(0, 1700000300, 10),
        transaction(-5, 1700000200, 20),
        transaction(7, 1700000100, 30),
    });

    QCOMPARE(model.count(), 1);
    QCOMPARE(idsOf(model.transactions()), QVector<qint64>{7});
}

void StarTransactionModelTest::keepsFirstTransactionOfDuplicateId()
{
    StarTransactionModel model;
    model.setTransactions({
        transaction(11, 1700000100, 10, QStringLiteral("first")),
        transaction(11, 1700000900, 99, QStringLiteral("second"), true),
        transaction(12, 1700000200, 5),
    });

    const QVector<StarTransaction> stored = model.transactions();
    QCOMPARE(stored.size(), 2);
    QCOMPARE(stored.at(0).id, qint64(12));
    QCOMPARE(stored.at(1).id, qint64(11));
    QCOMPARE(stored.at(1).title, QStringLiteral("first"));
    QCOMPARE(stored.at(1).amount, qint64(10));
    QCOMPARE(stored.at(1).incoming, false);
}

void StarTransactionModelTest::sortsNewestFirstThenByIdDescending()
{
    StarTransactionModel model;
    model.setTransactions({
        transaction(1, 1700000100, 1),
        transaction(2, 1700000300, 1),
        transaction(3, 1700000200, 1),
        transaction(4, 1700000300, 1),
        transaction(5, 1700000200, 1),
    });

    QCOMPARE(idsOf(model.transactions()), (QVector<qint64>{4, 2, 5, 3, 1}));
}

void StarTransactionModelTest::storesAbsoluteAmounts()
{
    StarTransactionModel model;
    model.setTransactions({
        transaction(1, 1700000100, -25),
        transaction(2, 1700000200, 40),
        transaction(3, 1700000300, std::numeric_limits<qint64>::min()),
    });

    const QVector<StarTransaction> stored = model.transactions();
    QCOMPARE(stored.at(0).id, qint64(3));
    QCOMPARE(stored.at(0).amount, std::numeric_limits<qint64>::max());
    QCOMPARE(stored.at(1).amount, qint64(40));
    QCOMPARE(stored.at(2).amount, qint64(25));
}

void StarTransactionModelTest::totalsFollowIncomingFlag()
{
    StarTransactionModel model;
    model.setTransactions({
        transaction(1, 1700000100, 100, QStringLiteral("received"), true),
        transaction(2, 1700000200, -30, QStringLiteral("sent")),
        transaction(3, 1700000300, 20, QStringLiteral("received"), true),
        transaction(4, 1700000400, 7, QStringLiteral("sent")),
    });

    QCOMPARE(model.totalIncoming(), qint64(120));
    QCOMPARE(model.totalOutgoing(), qint64(37));
    QCOMPARE(model.net(), qint64(83));
}

void StarTransactionModelTest::totalsSaturate()
{
    constexpr qint64 maximum = std::numeric_limits<qint64>::max();

    StarTransactionModel model;
    model.setTransactions({
        transaction(1, 1700000100, maximum, QStringLiteral("in"), true),
        transaction(2, 1700000200, 1, QStringLiteral("in"), true),
        transaction(3, 1700000300, maximum, QStringLiteral("out")),
        transaction(4, 1700000400, 1, QStringLiteral("out")),
    });

    QCOMPARE(model.totalIncoming(), maximum);
    QCOMPARE(model.totalOutgoing(), maximum);
    QCOMPARE(model.net(), qint64(0));
}

void StarTransactionModelTest::netCombinesTotals()
{
    StarTransactionModel model;
    model.setTransactions({
        transaction(1, 1700000100, 15, QStringLiteral("out")),
        transaction(2, 1700000200, 50, QStringLiteral("in"), true),
    });

    QCOMPARE(model.totalIncoming(), qint64(50));
    QCOMPARE(model.totalOutgoing(), qint64(15));
    QCOMPARE(model.net(), qint64(35));

    // Removing the incoming entry flips the net negative.
    model.setTransactions({transaction(1, 1700000100, 15, QStringLiteral("out"))});
    QCOMPARE(model.net(), qint64(-15));
}

void StarTransactionModelTest::emitsChangedOnlyOnRealChange()
{
    StarTransactionModel model;
    QSignalSpy spy(&model, &StarTransactionModel::changed);

    model.setTransactions({transaction(1, 1700000100, 10),
                           transaction(2, 1700000200, 20, QStringLiteral("in"), true)});
    QCOMPARE(spy.count(), 1);

    // Same content in a different input order normalises to the same list.
    model.setTransactions({transaction(2, 1700000200, 20, QStringLiteral("in"), true),
                           transaction(1, 1700000100, 10)});
    QCOMPARE(spy.count(), 1);

    // A dropped entry leaves the stored list untouched.
    model.setTransactions({transaction(1, 1700000100, 10),
                           transaction(2, 1700000200, 20, QStringLiteral("in"), true),
                           transaction(0, 1700000300, 30)});
    QCOMPARE(spy.count(), 1);

    // A real field change is reported.
    model.setTransactions({transaction(1, 1700000100, 10),
                           transaction(2, 1700000200, 21, QStringLiteral("in"), true)});
    QCOMPARE(spy.count(), 2);
}

void StarTransactionModelTest::appendAddsOnlyNewIds()
{
    StarTransactionModel model;
    model.setTransactions({transaction(1, 1700000200, 10),
                           transaction(2, 1700000100, 20, QStringLiteral("in"), true)});

    model.append({transaction(2, 1700000100, 20, QStringLiteral("in"), true),
                  transaction(3, 1700000300, 30),
                  transaction(0, 1700000400, 40)});

    QCOMPARE(model.count(), 3);
    QCOMPARE(idsOf(model.transactions()), (QVector<qint64>{3, 1, 2}));
}

void StarTransactionModelTest::appendKeepsStoredCopy()
{
    StarTransactionModel model;
    model.setTransactions({transaction(1, 1700000100, 10, QStringLiteral("stored"))});

    model.append({transaction(1, 1700000900, -99, QStringLiteral("ignored"), true)});

    QCOMPARE(model.count(), 1);
    const QVector<StarTransaction> stored = model.transactions();
    QCOMPARE(stored.at(0).id, qint64(1));
    QCOMPARE(stored.at(0).date, qint64(1700000100));
    QCOMPARE(stored.at(0).amount, qint64(10));
    QCOMPARE(stored.at(0).title, QStringLiteral("stored"));
    QCOMPARE(stored.at(0).incoming, false);
}

void StarTransactionModelTest::appendSortsAndReportsChange()
{
    StarTransactionModel model;
    model.setTransactions({transaction(1, 1700000100, 10)});

    QSignalSpy spy(&model, &StarTransactionModel::changed);
    model.append({transaction(3, 1700000300, -5), transaction(2, 1700000200, 15)});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(idsOf(model.transactions()), (QVector<qint64>{3, 2, 1}));
    QCOMPARE(model.transactions().at(0).amount, qint64(5));
    QCOMPARE(model.totalOutgoing(), qint64(30));
}

void StarTransactionModelTest::appendIsNoOpWithoutNewIds()
{
    StarTransactionModel model;
    model.setTransactions({transaction(1, 1700000100, 10)});

    QSignalSpy spy(&model, &StarTransactionModel::changed);
    model.append({transaction(1, 1700000100, 10)});
    model.append({transaction(0, 1700000200, 5), transaction(-3, 1700000300, 5)});
    model.append({});

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 1);
}

void StarTransactionModelTest::appendDropsInvalidAndDuplicateEntries()
{
    StarTransactionModel model;

    model.append({transaction(4, 1700000100, 10),
                  transaction(4, 1700000900, 99),
                  transaction(5, 1700000200, -20)});

    QCOMPARE(model.count(), 2);
    const QVector<StarTransaction> stored = model.transactions();
    QCOMPARE(idsOf(stored), (QVector<qint64>{5, 4}));
    QCOMPARE(stored.at(1).amount, qint64(10));
    QCOMPARE(stored.at(1).date, qint64(1700000100));
    QCOMPARE(stored.at(0).amount, qint64(20));
}

void StarTransactionModelTest::clearEmptiesTransactions()
{
    StarTransactionModel model;
    model.setTransactions({transaction(1, 1700000100, 10, QStringLiteral("in"), true),
                           transaction(2, 1700000200, 20)});

    QSignalSpy spy(&model, &StarTransactionModel::changed);
    model.clear();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.count(), 0);
    QVERIFY(model.transactions().isEmpty());
    QCOMPARE(model.totalIncoming(), qint64(0));
    QCOMPARE(model.totalOutgoing(), qint64(0));
    QCOMPARE(model.net(), qint64(0));
}

void StarTransactionModelTest::clearIsNoOpWhenEmpty()
{
    StarTransactionModel model;
    QSignalSpy spy(&model, &StarTransactionModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);

    model.setTransactions({transaction(1, 1700000100, 10)});
    QCOMPARE(spy.count(), 1);
    model.clear();
    QCOMPARE(spy.count(), 2);
    model.clear();
    QCOMPARE(spy.count(), 2);
}

QTEST_GUILESS_MAIN(StarTransactionModelTest)
#include "tst_star_transaction_model.moc"
