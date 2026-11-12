#include "telegram/BoostModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

BoostRow makeRow(qint64 userId, int boostCount = 1, const QString& name = {},
                 qint64 expirationDate = 0)
{
    BoostRow row;
    row.userId = userId;
    row.name = name.isEmpty() ? QStringLiteral("user %1").arg(userId) : name;
    row.boostCount = boostCount;
    row.expirationDate = expirationDate;
    return row;
}

QVector<qint64> userIdsOf(const QVector<BoostRow>& rows)
{
    QVector<qint64> ids;
    ids.reserve(rows.size());
    for (const BoostRow& row : rows)
        ids.append(row.userId);
    return ids;
}

} // namespace

class BoostModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setStatusStoresValues();
    void setStatusClampsNegativeValues();
    void setStatusIgnoresEquivalentValues();
    void progressIsRatioOfBoostsToTarget();
    void progressIsZeroWithoutTarget();
    void progressIsZeroWithoutBoosts();
    void progressClampsAtOne();
    void setBoostersKeepsOrder();
    void setBoostersDropsNonPositiveUserIds();
    void setBoostersDedupesKeepingFirst();
    void setBoostersClampsNegativeBoostCounts();
    void setBoostersKeepsPayload();
    void boosterCountMatchesBoosters();
    void setBoostersReplacesPreviousBoosters();
    void setBoostersIgnoresEquivalentInput();
    void clearResetsStatusAndBoosters();
    void clearIsNoOpWhenEmpty();
    void changedSignalFiresOnlyOnRealChanges();
};

void BoostModelTest::startsEmpty()
{
    BoostModel model;

    QCOMPARE(model.level(), 0);
    QCOMPARE(model.boostCount(), 0);
    QCOMPARE(model.nextLevelBoostCount(), 0);
    QCOMPARE(model.progressToNextLevel(), 0.0);
    QCOMPARE(model.boosterCount(), 0);
    QVERIFY(model.boosters().isEmpty());
}

void BoostModelTest::setStatusStoresValues()
{
    BoostModel model;
    model.setStatus(3, 7, 20);

    QCOMPARE(model.level(), 3);
    QCOMPARE(model.boostCount(), 7);
    QCOMPARE(model.nextLevelBoostCount(), 20);
}

void BoostModelTest::setStatusClampsNegativeValues()
{
    BoostModel model;
    // An unparsed answer must not reach the UI as "-2 boosts".
    model.setStatus(-2, -5, -9);

    QCOMPARE(model.level(), 0);
    QCOMPARE(model.boostCount(), 0);
    QCOMPARE(model.nextLevelBoostCount(), 0);
    QCOMPARE(model.progressToNextLevel(), 0.0);
}

void BoostModelTest::setStatusIgnoresEquivalentValues()
{
    BoostModel model;
    QSignalSpy spy(&model, &BoostModel::changed);
    QVERIFY(spy.isValid());

    model.setStatus(2, 4, 10);
    QCOMPARE(spy.count(), 1);

    model.setStatus(2, 4, 10);
    QCOMPARE(spy.count(), 1);

    model.setStatus(2, 0, 10);
    QCOMPARE(spy.count(), 2);

    // A negative value that clamps onto the stored one is no change either.
    model.setStatus(2, -1, 10);
    QCOMPARE(spy.count(), 2);

    model.setStatus(2, 4, 11);
    QCOMPARE(spy.count(), 3);
}

void BoostModelTest::progressIsRatioOfBoostsToTarget()
{
    BoostModel model;

    model.setStatus(1, 5, 10);
    QCOMPARE(model.progressToNextLevel(), 0.5);

    model.setStatus(1, 1, 4);
    QCOMPARE(model.progressToNextLevel(), 0.25);

    model.setStatus(1, 3, 4);
    QCOMPARE(model.progressToNextLevel(), 0.75);
}

void BoostModelTest::progressIsZeroWithoutTarget()
{
    BoostModel model;

    // No next level: the bar stays empty instead of dividing by zero.
    model.setStatus(4, 12, 0);
    QCOMPARE(model.progressToNextLevel(), 0.0);

    model.setStatus(0, 0, 0);
    QCOMPARE(model.progressToNextLevel(), 0.0);
}

void BoostModelTest::progressIsZeroWithoutBoosts()
{
    BoostModel model;
    // A target without any boost yet is the start of the bar, not a fraction.
    model.setStatus(2, 0, 25);

    QCOMPARE(model.boostCount(), 0);
    QCOMPARE(model.progressToNextLevel(), 0.0);
}

void BoostModelTest::progressClampsAtOne()
{
    BoostModel model;

    model.setStatus(1, 10, 10);
    QCOMPARE(model.progressToNextLevel(), 1.0);

    // A stale boost count that already passed the target never overflows.
    model.setStatus(1, 12, 10);
    QCOMPARE(model.progressToNextLevel(), 1.0);
}

void BoostModelTest::setBoostersKeepsOrder()
{
    BoostModel model;
    // Supplied out of id order: the list keeps the supplied order.
    model.setBoosters({makeRow(30), makeRow(10), makeRow(20)});

    QCOMPARE(userIdsOf(model.boosters()), (QVector<qint64>{30, 10, 20}));
}

void BoostModelTest::setBoostersDropsNonPositiveUserIds()
{
    BoostModel model;
    model.setBoosters({makeRow(0), makeRow(-4), makeRow(8)});

    QCOMPARE(model.boosterCount(), 1);
    QCOMPARE(userIdsOf(model.boosters()), (QVector<qint64>{8}));
}

void BoostModelTest::setBoostersDedupesKeepingFirst()
{
    BoostModel model;
    // The first row of user 8 wins, so the later copy's name and count are lost.
    model.setBoosters({makeRow(8, 3, QStringLiteral("first")), makeRow(8, 9, QStringLiteral("second")),
                       makeRow(9, 1, QStringLiteral("other"))});

    QCOMPARE(model.boosterCount(), 2);
    QCOMPARE(userIdsOf(model.boosters()), (QVector<qint64>{8, 9}));
    QCOMPARE(model.boosters().constFirst().name, QStringLiteral("first"));
    QCOMPARE(model.boosters().constFirst().boostCount, 3);
}

void BoostModelTest::setBoostersClampsNegativeBoostCounts()
{
    BoostModel model;
    model.setBoosters({makeRow(1, -6), makeRow(2, 0), makeRow(3, 4)});

    const QVector<BoostRow> rows = model.boosters();
    QCOMPARE(rows.size(), 3);
    QCOMPARE(rows.at(0).boostCount, 0);
    QCOMPARE(rows.at(1).boostCount, 0);
    QCOMPARE(rows.at(2).boostCount, 4);
}

void BoostModelTest::setBoostersKeepsPayload()
{
    BoostModel model;
    model.setBoosters({makeRow(42, 2, QStringLiteral("Ada"), 1700000000)});

    const QVector<BoostRow> rows = model.boosters();
    QCOMPARE(rows.size(), 1);

    const BoostRow row = rows.constFirst();
    QCOMPARE(row.userId, qint64(42));
    QCOMPARE(row.name, QStringLiteral("Ada"));
    QCOMPARE(row.boostCount, 2);
    QCOMPARE(row.expirationDate, qint64(1700000000));
}

void BoostModelTest::boosterCountMatchesBoosters()
{
    BoostModel model;
    model.setBoosters({makeRow(1), makeRow(2), makeRow(1), makeRow(3)});

    QCOMPARE(model.boosterCount(), model.boosters().size());
    QCOMPARE(model.boosterCount(), 3);
}

void BoostModelTest::setBoostersReplacesPreviousBoosters()
{
    BoostModel model;
    model.setBoosters({makeRow(1), makeRow(2)});
    QCOMPARE(model.boosterCount(), 2);

    model.setBoosters({makeRow(3)});

    QCOMPARE(model.boosterCount(), 1);
    QCOMPARE(userIdsOf(model.boosters()), (QVector<qint64>{3}));
}

void BoostModelTest::setBoostersIgnoresEquivalentInput()
{
    BoostModel model;
    QSignalSpy spy(&model, &BoostModel::changed);
    QVERIFY(spy.isValid());

    model.setBoosters({makeRow(1, 0), makeRow(2, 0)});
    QCOMPARE(spy.count(), 1);

    // Same list again, then a duplicate and a negative count that sanitize back
    // to the stored rows: neither is a real change.
    model.setBoosters({makeRow(1, 0), makeRow(2, 0)});
    QCOMPARE(spy.count(), 1);
    model.setBoosters({makeRow(1, 0), makeRow(1, 0), makeRow(2, 0)});
    QCOMPARE(spy.count(), 1);
    model.setBoosters({makeRow(1, -2), makeRow(2, 0)});
    QCOMPARE(spy.count(), 1);

    // A payload edit is a real change.
    model.setBoosters({makeRow(1, 5), makeRow(2)});
    QCOMPARE(spy.count(), 2);
}

void BoostModelTest::clearResetsStatusAndBoosters()
{
    BoostModel model;
    model.setStatus(3, 7, 20);
    model.setBoosters({makeRow(1), makeRow(2)});

    model.clear();

    QCOMPARE(model.level(), 0);
    QCOMPARE(model.boostCount(), 0);
    QCOMPARE(model.nextLevelBoostCount(), 0);
    QCOMPARE(model.progressToNextLevel(), 0.0);
    QCOMPARE(model.boosterCount(), 0);
    QVERIFY(model.boosters().isEmpty());
}

void BoostModelTest::clearIsNoOpWhenEmpty()
{
    BoostModel model;
    QSignalSpy spy(&model, &BoostModel::changed);
    QVERIFY(spy.isValid());

    model.clear();
    QCOMPARE(spy.count(), 0);

    // A status without boosters is still something to forget.
    model.setStatus(1, 2, 3);
    QCOMPARE(spy.count(), 1);

    model.clear();
    QCOMPARE(spy.count(), 2);

    model.clear();
    QCOMPARE(spy.count(), 2);
}

void BoostModelTest::changedSignalFiresOnlyOnRealChanges()
{
    BoostModel model;
    QSignalSpy spy(&model, &BoostModel::changed);
    QVERIFY(spy.isValid());

    model.setStatus(2, 4, 10);
    QCOMPARE(spy.count(), 1);
    model.setStatus(2, 4, 10);
    QCOMPARE(spy.count(), 1);

    model.setBoosters({makeRow(1)});
    QCOMPARE(spy.count(), 2);
    model.setBoosters({makeRow(1)});
    QCOMPARE(spy.count(), 2);

    // The status and the booster list share the same change signal.
    model.setStatus(3, 4, 10);
    QCOMPARE(spy.count(), 3);
    QCOMPARE(model.level(), 3);
    QCOMPARE(model.boosterCount(), 1);

    model.clear();
    QCOMPARE(spy.count(), 4);
    model.clear();
    QCOMPARE(spy.count(), 4);
}

QTEST_GUILESS_MAIN(BoostModelTest)
#include "tst_boost_model.moc"
