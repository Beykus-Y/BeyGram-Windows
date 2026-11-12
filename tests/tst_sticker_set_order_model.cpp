#include "telegram/StickerSetOrderModel.h"

#include <QSignalSpy>
#include <QString>
#include <QVector>
#include <QtTest>

#include <initializer_list>

using namespace beygram::telegram;

namespace {

// Every field differs per id, so a test that compares ids is still sensitive to
// a model that stores the wrong entry or drops metadata.
StickerSetInfo makeSet(qint64 setId)
{
    StickerSetInfo set;
    set.id = setId;
    set.title = QStringLiteral("Set %1").arg(setId);
    set.name = QStringLiteral("set_%1").arg(setId);
    set.stickerCount = int(setId % 50);
    set.thumbnailFileId = int(setId % 1000) + 1;
    set.installed = true;
    return set;
}

QVector<StickerSetInfo> setsOf(std::initializer_list<qint64> setIds)
{
    QVector<StickerSetInfo> sets;
    sets.reserve(setIds.size());
    for (qint64 setId : setIds)
        sets.append(makeSet(setId));
    return sets;
}

QVector<qint64> idsOf(const QVector<StickerSetInfo>& sets)
{
    QVector<qint64> ids;
    ids.reserve(sets.size());
    for (const StickerSetInfo& set : sets)
        ids.append(set.id);
    return ids;
}

// A vector literal keeps the QCOMPARE expectations readable; set ids are
// qint64, so the initializer list needs the explicit element type.
QVector<qint64> ids(std::initializer_list<qint64> values)
{
    return QVector<qint64>(values);
}

} // namespace

class StickerSetOrderModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();

    void setSetsStoresOrderAndMetadata();
    void setSetsDropsNonPositiveIds();
    void setSetsDedupesKeepingFirst();
    void setSetsReplacesPreviousSets();
    void setSetsWithEmptyListClears();
    void setSetsKeepsSurvivingPins();
    void setSetsForgetsPinsOfRemovedSets();

    void moveReordersStoredOrder();
    void moveToSameIndexIsNoOp();
    void moveIgnoresUnknownId();
    void moveIgnoresOutOfRangeIndex();
    void moveKeepsOtherSetsInOrder();
    void moveOfPinnedSetKeepsItFirstInOrdered();

    void pinPutsSetFirst();
    void pinIsIdempotent();
    void pinMovesPinnedSetToFront();
    void pinIgnoresUnknownId();

    void orderedWithoutPinsFollowsStoredOrder();
    void orderedPutsPinnedFirst();
    void orderedKeepsRemainingStoredOrder();

    void clearDropsSetsAndPins();
    void clearOnEmptyIsNoOp();

    void emitsChangedOnlyOnRealSetSetsChanges();
    void emitsChangedWhenASetLeavesTheCatalog();
    void emitsChangedForMove();
    void emitsChangedForPin();
    void emitsChangedOncePerClear();

    void returnsIndependentCopies();
};

void StickerSetOrderModelTest::startsEmpty()
{
    StickerSetOrderModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.sets().isEmpty());
    QVERIFY(model.pinned().isEmpty());
    QVERIFY(model.ordered().isEmpty());
}

void StickerSetOrderModelTest::setSetsStoresOrderAndMetadata()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({30, 10, 20}));

    QCOMPARE(model.count(), 3);
    QCOMPARE(idsOf(model.sets()), ids({30, 10, 20}));

    // The entry itself is kept, not just the id, so the strip can render it.
    const QVector<StickerSetInfo> stored = model.sets();
    const StickerSetInfo& first = stored.at(0);
    QCOMPARE(first.title, QStringLiteral("Set 30"));
    QCOMPARE(first.name, QStringLiteral("set_30"));
    QCOMPARE(first.stickerCount, 30);
    QCOMPARE(first.thumbnailFileId, 31);
    QVERIFY(first.installed);
    QVERIFY(!first.archived);

    // Without pins the display order is the stored order.
    QCOMPARE(idsOf(model.ordered()), ids({30, 10, 20}));
}

void StickerSetOrderModelTest::setSetsDropsNonPositiveIds()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({0, -1, 5, -7, 6}));

    // Zero and negative ids are not sets, so they never reach the list.
    QCOMPARE(model.count(), 2);
    QCOMPARE(idsOf(model.sets()), ids({5, 6}));
}

void StickerSetOrderModelTest::setSetsDedupesKeepingFirst()
{
    StickerSetOrderModel model;

    StickerSetInfo first = makeSet(5);
    first.title = QStringLiteral("First");
    StickerSetInfo second = makeSet(5);
    second.title = QStringLiteral("Second");

    model.setSets({first, second, makeSet(3), makeSet(5)});

    // The first position of an id wins, including its metadata.
    QCOMPARE(model.count(), 2);
    QCOMPARE(idsOf(model.sets()), ids({5, 3}));
    QCOMPARE(model.sets().first().title, QStringLiteral("First"));
}

void StickerSetOrderModelTest::setSetsReplacesPreviousSets()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2, 3}));
    model.setSets(setsOf({4, 5}));

    QCOMPARE(model.count(), 2);
    QCOMPARE(idsOf(model.sets()), ids({4, 5}));
    QCOMPARE(idsOf(model.ordered()), ids({4, 5}));
}

void StickerSetOrderModelTest::setSetsWithEmptyListClears()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2}));
    model.pin(2);

    model.setSets({});

    QCOMPARE(model.count(), 0);
    QVERIFY(model.sets().isEmpty());
    QVERIFY(model.pinned().isEmpty());
    QVERIFY(model.ordered().isEmpty());
}

void StickerSetOrderModelTest::setSetsKeepsSurvivingPins()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2, 3}));
    model.pin(2);
    model.pin(3);
    QCOMPARE(model.pinned(), ids({3, 2}));

    // A catalog refresh that still contains the pinned ids keeps their order.
    model.setSets(setsOf({1, 2, 3, 4}));
    QCOMPARE(model.pinned(), ids({3, 2}));
    QCOMPARE(idsOf(model.ordered()), ids({3, 2, 1, 4}));
}

void StickerSetOrderModelTest::setSetsForgetsPinsOfRemovedSets()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2, 3}));
    model.pin(2);
    model.pin(1);
    QCOMPARE(model.pinned(), ids({1, 2}));

    // Set 1 left the catalog, so its pin cannot survive; set 2 stays first.
    model.setSets(setsOf({2, 3}));
    QCOMPARE(model.pinned(), ids({2}));
    QCOMPARE(idsOf(model.ordered()), ids({2, 3}));
}

void StickerSetOrderModelTest::moveReordersStoredOrder()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2, 3, 4}));

    QVERIFY(model.move(1, 2));
    QCOMPARE(idsOf(model.sets()), ids({2, 3, 1, 4}));

    QVERIFY(model.move(4, 0));
    QCOMPARE(idsOf(model.sets()), ids({4, 2, 3, 1}));

    QVERIFY(model.move(4, 3));
    QCOMPARE(idsOf(model.sets()), ids({2, 3, 1, 4}));
    QCOMPARE(model.count(), 4);
}

void StickerSetOrderModelTest::moveToSameIndexIsNoOp()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2, 3}));

    QVERIFY(!model.move(2, 1));
    QCOMPARE(idsOf(model.sets()), ids({1, 2, 3}));
}

void StickerSetOrderModelTest::moveIgnoresUnknownId()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2, 3}));

    QVERIFY(!model.move(99, 0));
    QVERIFY(!model.move(0, 1));
    QVERIFY(!model.move(-5, 2));
    QCOMPARE(idsOf(model.sets()), ids({1, 2, 3}));
}

void StickerSetOrderModelTest::moveIgnoresOutOfRangeIndex()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2, 3}));

    QVERIFY(!model.move(1, -1));
    QVERIFY(!model.move(1, 3));
    QVERIFY(!model.move(1, 50));
    QCOMPARE(idsOf(model.sets()), ids({1, 2, 3}));
}

void StickerSetOrderModelTest::moveKeepsOtherSetsInOrder()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2, 3, 4, 5}));

    // Moving to the back shifts only the sets that were after the old spot.
    QVERIFY(model.move(2, 4));
    QCOMPARE(idsOf(model.sets()), ids({1, 3, 4, 5, 2}));
}

void StickerSetOrderModelTest::moveOfPinnedSetKeepsItFirstInOrdered()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2, 3}));
    model.pin(2);

    // The stored order moves, but the pin still leads the display order.
    QVERIFY(model.move(2, 2));
    QCOMPARE(idsOf(model.sets()), ids({1, 3, 2}));
    QCOMPARE(idsOf(model.ordered()), ids({2, 1, 3}));

    // Moving an unpinned set does change the tail of the display order.
    QVERIFY(model.move(3, 0));
    QCOMPARE(idsOf(model.ordered()), ids({2, 3, 1}));
}

void StickerSetOrderModelTest::pinPutsSetFirst()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2, 3}));

    QVERIFY(model.pin(2));
    QCOMPARE(model.pinned(), ids({2}));
    QCOMPARE(idsOf(model.ordered()), ids({2, 1, 3}));

    // The newest pin goes in front of the older one.
    QVERIFY(model.pin(3));
    QCOMPARE(model.pinned(), ids({3, 2}));
    QCOMPARE(idsOf(model.ordered()), ids({3, 2, 1}));
    QCOMPARE(model.count(), 3);
}

void StickerSetOrderModelTest::pinIsIdempotent()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2}));

    QVERIFY(model.pin(1));
    // Pinning the set that is already first is not a change and never
    // duplicates or unpins it.
    QVERIFY(!model.pin(1));
    QCOMPARE(model.pinned(), ids({1}));
    QCOMPARE(idsOf(model.ordered()), ids({1, 2}));
}

void StickerSetOrderModelTest::pinMovesPinnedSetToFront()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2, 3}));
    model.pin(1);
    model.pin(2);
    QCOMPARE(model.pinned(), ids({2, 1}));

    // Re-pinning an older pin reorders it instead of listing it twice.
    QVERIFY(model.pin(1));
    QCOMPARE(model.pinned(), ids({1, 2}));
    QCOMPARE(idsOf(model.ordered()), ids({1, 2, 3}));
    QCOMPARE(model.count(), 3);
}

void StickerSetOrderModelTest::pinIgnoresUnknownId()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2}));

    QVERIFY(!model.pin(9));
    QVERIFY(!model.pin(0));
    QVERIFY(!model.pin(-1));
    QVERIFY(model.pinned().isEmpty());
    QCOMPARE(idsOf(model.ordered()), ids({1, 2}));
}

void StickerSetOrderModelTest::orderedWithoutPinsFollowsStoredOrder()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2, 3, 4}));
    model.move(4, 0);

    QCOMPARE(idsOf(model.ordered()), ids({4, 1, 2, 3}));
}

void StickerSetOrderModelTest::orderedPutsPinnedFirst()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2, 3}));
    model.pin(3);

    QCOMPARE(idsOf(model.ordered()), ids({3, 1, 2}));
    // Every set is shown exactly once, pinned or not.
    QCOMPARE(int(model.ordered().size()), model.count());
}

void StickerSetOrderModelTest::orderedKeepsRemainingStoredOrder()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2, 3, 4, 5}));
    model.pin(4);
    model.pin(2);

    // Pinned order first (2 was pinned last), then the rest as stored.
    QCOMPARE(model.pinned(), ids({2, 4}));
    QCOMPARE(idsOf(model.ordered()), ids({2, 4, 1, 3, 5}));
}

void StickerSetOrderModelTest::clearDropsSetsAndPins()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2}));
    model.pin(2);

    model.clear();

    QCOMPARE(model.count(), 0);
    QVERIFY(model.sets().isEmpty());
    QVERIFY(model.pinned().isEmpty());
    QVERIFY(model.ordered().isEmpty());
}

void StickerSetOrderModelTest::clearOnEmptyIsNoOp()
{
    StickerSetOrderModel model;
    QSignalSpy spy(&model, &StickerSetOrderModel::changed);

    model.clear();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 0);
}

void StickerSetOrderModelTest::emitsChangedOnlyOnRealSetSetsChanges()
{
    StickerSetOrderModel model;
    QSignalSpy spy(&model, &StickerSetOrderModel::changed);

    model.setSets(setsOf({1, 2}));
    QCOMPARE(spy.count(), 1);

    // The same list again is not a change.
    model.setSets(setsOf({1, 2}));
    QCOMPARE(spy.count(), 1);

    // Ids that normalize away leave the stored list untouched.
    model.setSets(setsOf({1, 2, 2, 0, -3}));
    QCOMPARE(spy.count(), 1);

    model.setSets(setsOf({2, 1}));
    QCOMPARE(spy.count(), 2);

    model.setSets({});
    QCOMPARE(spy.count(), 3);

    // Already empty stays silent.
    model.setSets({});
    QCOMPARE(spy.count(), 3);

    // The same ids with changed metadata is a change the strip can see.
    model.setSets(setsOf({5}));
    QCOMPARE(spy.count(), 4);
    QVector<StickerSetInfo> renamed = setsOf({5});
    renamed[0].title = QStringLiteral("Renamed");
    model.setSets(renamed);
    QCOMPARE(spy.count(), 5);
    QCOMPARE(model.sets().first().title, QStringLiteral("Renamed"));
}

void StickerSetOrderModelTest::emitsChangedWhenASetLeavesTheCatalog()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2}));
    model.pin(2);

    QSignalSpy spy(&model, &StickerSetOrderModel::changed);

    // Dropping the pinned set is a real change even though the remaining
    // catalog order is the same prefix.
    model.setSets(setsOf({1}));
    QCOMPARE(spy.count(), 1);
    QVERIFY(model.pinned().isEmpty());

    model.setSets(setsOf({1}));
    QCOMPARE(spy.count(), 1);
}

void StickerSetOrderModelTest::emitsChangedForMove()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2, 3}));

    QSignalSpy spy(&model, &StickerSetOrderModel::changed);

    // Same position, unknown ids and bad indexes are silent no-ops.
    QVERIFY(!model.move(2, 1));
    QVERIFY(!model.move(9, 0));
    QVERIFY(!model.move(1, -1));
    QVERIFY(!model.move(1, 3));
    QCOMPARE(spy.count(), 0);

    QVERIFY(model.move(3, 0));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(idsOf(model.sets()), ids({3, 1, 2}));

    QVERIFY(model.move(3, 2));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(idsOf(model.sets()), ids({1, 2, 3}));
}

void StickerSetOrderModelTest::emitsChangedForPin()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2}));

    QSignalSpy spy(&model, &StickerSetOrderModel::changed);

    // An unknown id is not a pin.
    QVERIFY(!model.pin(9));
    QVERIFY(!model.pin(0));
    QCOMPARE(spy.count(), 0);

    QVERIFY(model.pin(2));
    QCOMPARE(spy.count(), 1);

    // The same set is already first, so nothing changes.
    QVERIFY(!model.pin(2));
    QCOMPARE(spy.count(), 1);

    QVERIFY(model.pin(1));
    QCOMPARE(spy.count(), 2);

    // Moving an older pin to the front is a visible reorder.
    QVERIFY(model.pin(2));
    QCOMPARE(spy.count(), 3);
    QCOMPARE(model.pinned(), ids({2, 1}));
}

void StickerSetOrderModelTest::emitsChangedOncePerClear()
{
    StickerSetOrderModel model;
    QSignalSpy spy(&model, &StickerSetOrderModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);

    model.setSets(setsOf({7}));
    model.pin(7);
    QCOMPARE(spy.count(), 2);

    model.clear();
    QCOMPARE(spy.count(), 3);

    model.clear();
    QCOMPARE(spy.count(), 3);
}

void StickerSetOrderModelTest::returnsIndependentCopies()
{
    StickerSetOrderModel model;
    model.setSets(setsOf({1, 2}));
    model.pin(2);

    QVector<StickerSetInfo> setSnapshot = model.sets();
    setSnapshot[0].title = QStringLiteral("Changed");
    setSnapshot.clear();

    QVector<qint64> pinSnapshot = model.pinned();
    pinSnapshot[0] = 99;
    pinSnapshot.append(3);

    QVector<StickerSetInfo> orderedSnapshot = model.ordered();
    orderedSnapshot.clear();

    QCOMPARE(model.count(), 2);
    QCOMPARE(model.sets().first().title, QStringLiteral("Set 1"));
    QCOMPARE(model.pinned(), ids({2}));
    QCOMPARE(idsOf(model.ordered()), ids({2, 1}));
}

QTEST_GUILESS_MAIN(StickerSetOrderModelTest)
#include "tst_sticker_set_order_model.moc"
