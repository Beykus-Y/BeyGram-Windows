#include "telegram/RecentStickerModel.h"

#include <QSignalSpy>
#include <QVector>
#include <QtTest>

#include <initializer_list>

using namespace beygram::telegram;

namespace {

// A vector literal keeps the QCOMPARE expectations readable; sticker ids are
// qint64, so the initializer list needs the explicit element type.
QVector<qint64> ids(std::initializer_list<qint64> values)
{
    return QVector<qint64>(values);
}

QVector<qint64> countedIds(int count)
{
    QVector<qint64> values;
    values.reserve(count);
    for (int i = 1; i <= count; ++i)
        values.append(qint64(i));
    return values;
}

} // namespace

class RecentStickerModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void defaultsToThirtyEntries();

    void setStickersDropsNonPositiveIds();
    void setStickersDedupesKeepingFirst();
    void setStickersCapsAtMaxEntries();
    void setStickersReplacesPreviousList();
    void setStickersClampsMaxEntries();
    void setStickersWithEmptyListClears();

    void addPutsNewestFirst();
    void addMovesExistingStickerToFront();
    void addIgnoresNonPositiveIds();
    void addCapsAtMaxEntries();
    void addRespectsLoweredCap();
    void addOnEmptyListStartsList();

    void removeDropsSticker();
    void removeOfAbsentStickerIsNoOp();
    void removeKeepsOrderOfRemaining();

    void setMaxEntriesClampsToOne();
    void setMaxEntriesClampsToHundred();
    void setMaxEntriesTrimsOldest();
    void setMaxEntriesGrowsWithoutDropping();

    void clearDropsList();
    void clearOnEmptyIsNoOp();

    void emitsChangedOnlyOnRealSetStickersChanges();
    void emitsChangedForAdd();
    void emitsChangedForRemove();
    void emitsChangedForSetMaxEntries();
    void emitsChangedOncePerClear();

    void returnsIndependentCopy();
};

void RecentStickerModelTest::startsEmpty()
{
    RecentStickerModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.stickers().isEmpty());
    QCOMPARE(model.maxEntries(), 30);
}

void RecentStickerModelTest::defaultsToThirtyEntries()
{
    RecentStickerModel model;
    model.setStickers(countedIds(40));

    // The default cap keeps the thirty newest and drops the ten oldest.
    QCOMPARE(model.maxEntries(), 30);
    QCOMPARE(model.count(), 30);
    QCOMPARE(model.stickers(), countedIds(30));
}

void RecentStickerModelTest::setStickersDropsNonPositiveIds()
{
    RecentStickerModel model;
    model.setStickers(ids({0, -1, 5, -7, 6}));

    // Zero and negative ids are not stickers, so they never reach the list.
    QCOMPARE(model.stickers(), ids({5, 6}));
    QCOMPARE(model.count(), 2);
}

void RecentStickerModelTest::setStickersDedupesKeepingFirst()
{
    RecentStickerModel model;
    model.setStickers(ids({5, 3, 5, 4, 3}));

    // The first position of an id wins; later repeats are dropped.
    QCOMPARE(model.stickers(), ids({5, 3, 4}));
    QCOMPARE(model.count(), 3);
}

void RecentStickerModelTest::setStickersCapsAtMaxEntries()
{
    RecentStickerModel model;
    model.setStickers(ids({7, 8, 9, 10}), 3);

    // The list is newest first, so the cap drops from the tail: 10 is oldest.
    QCOMPARE(model.stickers(), ids({7, 8, 9}));
    QCOMPARE(model.maxEntries(), 3);
}

void RecentStickerModelTest::setStickersReplacesPreviousList()
{
    RecentStickerModel model;
    model.setStickers(ids({1, 2, 3}));
    model.setStickers(ids({4, 5}));

    QCOMPARE(model.stickers(), ids({4, 5}));
    QCOMPARE(model.count(), 2);
}

void RecentStickerModelTest::setStickersClampsMaxEntries()
{
    RecentStickerModel model;

    model.setStickers(ids({1, 2, 3}), 0);
    QCOMPARE(model.maxEntries(), 1);
    QCOMPARE(model.stickers(), ids({1}));

    model.setStickers(ids({1, 2, 3}), -4);
    QCOMPARE(model.maxEntries(), 1);
    QCOMPARE(model.stickers(), ids({1}));

    model.setStickers(ids({1}), 1000);
    QCOMPARE(model.maxEntries(), 100);
    QCOMPARE(model.stickers(), ids({1}));
}

void RecentStickerModelTest::setStickersWithEmptyListClears()
{
    RecentStickerModel model;
    model.setStickers(ids({1, 2}));
    model.setStickers({});

    QCOMPARE(model.count(), 0);
    QVERIFY(model.stickers().isEmpty());
}

void RecentStickerModelTest::addPutsNewestFirst()
{
    RecentStickerModel model;
    model.add(1);
    model.add(2);
    model.add(3);

    QCOMPARE(model.stickers(), ids({3, 2, 1}));
}

void RecentStickerModelTest::addMovesExistingStickerToFront()
{
    RecentStickerModel model;
    model.setStickers(ids({3, 2, 1}));
    model.add(1);

    // Re-sending an older sticker reorders instead of duplicating it.
    QCOMPARE(model.stickers(), ids({1, 3, 2}));
    QCOMPARE(model.count(), 3);
}

void RecentStickerModelTest::addIgnoresNonPositiveIds()
{
    RecentStickerModel model;
    model.add(4);
    model.add(0);
    model.add(-9);

    QCOMPARE(model.stickers(), ids({4}));
    QCOMPARE(model.count(), 1);
}

void RecentStickerModelTest::addCapsAtMaxEntries()
{
    RecentStickerModel model;
    model.setStickers(ids({1, 2}), 2);

    model.add(3);
    QCOMPARE(model.stickers(), ids({3, 1}));

    model.add(4);
    QCOMPARE(model.stickers(), ids({4, 3}));
    QCOMPARE(model.count(), 2);
}

void RecentStickerModelTest::addRespectsLoweredCap()
{
    RecentStickerModel model;
    model.setStickers(ids({1, 2, 3}));
    model.setMaxEntries(2);
    QCOMPARE(model.stickers(), ids({1, 2}));

    // The cap set earlier still applies to later sends.
    model.add(9);
    QCOMPARE(model.stickers(), ids({9, 1}));
}

void RecentStickerModelTest::addOnEmptyListStartsList()
{
    RecentStickerModel model;
    model.add(42);

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.stickers(), ids({42}));
}

void RecentStickerModelTest::removeDropsSticker()
{
    RecentStickerModel model;
    model.setStickers(ids({1, 2}));

    QVERIFY(model.remove(2));
    QCOMPARE(model.stickers(), ids({1}));
    QCOMPARE(model.count(), 1);
}

void RecentStickerModelTest::removeOfAbsentStickerIsNoOp()
{
    RecentStickerModel model;
    model.setStickers(ids({1, 2}));

    QVERIFY(!model.remove(3));
    QVERIFY(!model.remove(0));
    QVERIFY(!model.remove(-1));

    QCOMPARE(model.stickers(), ids({1, 2}));
}

void RecentStickerModelTest::removeKeepsOrderOfRemaining()
{
    RecentStickerModel model;
    model.setStickers(ids({1, 2, 3, 4}));

    QVERIFY(model.remove(3));
    QCOMPARE(model.stickers(), ids({1, 2, 4}));
}

void RecentStickerModelTest::setMaxEntriesClampsToOne()
{
    RecentStickerModel model;
    model.setStickers(ids({1, 2, 3}));

    model.setMaxEntries(0);
    QCOMPARE(model.maxEntries(), 1);
    QCOMPARE(model.stickers(), ids({1}));

    // A negative cap is clamped to the same minimum.
    model.setMaxEntries(-5);
    QCOMPARE(model.maxEntries(), 1);
    QCOMPARE(model.stickers(), ids({1}));
}

void RecentStickerModelTest::setMaxEntriesClampsToHundred()
{
    RecentStickerModel model;
    model.setMaxEntries(1000);
    QCOMPARE(model.maxEntries(), 100);

    model.setStickers(countedIds(120), 100);
    QCOMPARE(model.count(), 100);
    QCOMPARE(model.stickers().last(), qint64(100));
}

void RecentStickerModelTest::setMaxEntriesTrimsOldest()
{
    RecentStickerModel model;
    model.setStickers(ids({1, 2, 3, 4}));

    model.setMaxEntries(2);
    QCOMPARE(model.maxEntries(), 2);
    // 3 and 4 were the oldest entries, so they are the ones dropped.
    QCOMPARE(model.stickers(), ids({1, 2}));
}

void RecentStickerModelTest::setMaxEntriesGrowsWithoutDropping()
{
    RecentStickerModel model;
    model.setStickers(ids({1, 2}), 2);

    model.setMaxEntries(10);
    QCOMPARE(model.maxEntries(), 10);
    QCOMPARE(model.stickers(), ids({1, 2}));

    model.add(3);
    QCOMPARE(model.stickers(), ids({3, 1, 2}));
}

void RecentStickerModelTest::clearDropsList()
{
    RecentStickerModel model;
    model.setStickers(ids({1, 2, 3}));

    model.clear();

    QCOMPARE(model.count(), 0);
    QVERIFY(model.stickers().isEmpty());
    // The cap survives a clear; only the entries are forgotten.
    QCOMPARE(model.maxEntries(), 30);
}

void RecentStickerModelTest::clearOnEmptyIsNoOp()
{
    RecentStickerModel model;
    QSignalSpy spy(&model, &RecentStickerModel::changed);

    model.clear();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 0);
}

void RecentStickerModelTest::emitsChangedOnlyOnRealSetStickersChanges()
{
    RecentStickerModel model;
    QSignalSpy spy(&model, &RecentStickerModel::changed);

    model.setStickers(ids({1, 2}));
    QCOMPARE(spy.count(), 1);

    // The same list again is not a change.
    model.setStickers(ids({1, 2}));
    QCOMPARE(spy.count(), 1);

    // Ids that normalize away leave the stored list untouched.
    model.setStickers(ids({1, 2, 2, 0, -3}));
    QCOMPARE(spy.count(), 1);

    model.setStickers(ids({2, 1}));
    QCOMPARE(spy.count(), 2);

    // The same ids under a different cap is a change of the model's state.
    model.setStickers(ids({2, 1}), 5);
    QCOMPARE(spy.count(), 3);
    QCOMPARE(model.maxEntries(), 5);

    model.setStickers({});
    QCOMPARE(spy.count(), 4);

    // Already empty at the same cap stays silent.
    model.setStickers({});
    QCOMPARE(spy.count(), 4);
}

void RecentStickerModelTest::emitsChangedForAdd()
{
    RecentStickerModel model;
    QSignalSpy spy(&model, &RecentStickerModel::changed);

    model.add(5);
    QCOMPARE(spy.count(), 1);

    // The sticker is already newest, so re-sending it changes nothing.
    model.add(5);
    QCOMPARE(spy.count(), 1);

    model.add(6);
    QCOMPARE(spy.count(), 2);

    // Moving an older sticker to the front is a visible reorder.
    model.add(5);
    QCOMPARE(spy.count(), 3);
    QCOMPARE(model.stickers(), ids({5, 6}));

    // Ignored ids do not notify.
    model.add(0);
    model.add(-3);
    QCOMPARE(spy.count(), 3);
}

void RecentStickerModelTest::emitsChangedForRemove()
{
    RecentStickerModel model;
    model.setStickers(ids({5, 6}));

    QSignalSpy spy(&model, &RecentStickerModel::changed);

    QVERIFY(model.remove(6));
    QCOMPARE(spy.count(), 1);

    // Removing a sticker that is gone is a silent no-op.
    QVERIFY(!model.remove(6));
    QVERIFY(!model.remove(0));
    QCOMPARE(spy.count(), 1);

    QVERIFY(model.remove(5));
    QCOMPARE(spy.count(), 2);
    QVERIFY(model.stickers().isEmpty());
}

void RecentStickerModelTest::emitsChangedForSetMaxEntries()
{
    RecentStickerModel model;
    model.setStickers(ids({1, 2, 3, 4}));

    QSignalSpy spy(&model, &RecentStickerModel::changed);

    // The cap is already 30, so nothing changes.
    model.setMaxEntries(30);
    QCOMPARE(spy.count(), 0);

    model.setMaxEntries(2);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.stickers(), ids({1, 2}));

    model.setMaxEntries(2);
    QCOMPARE(spy.count(), 1);

    // A cap change alone still counts as a change of the model.
    model.setMaxEntries(5);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.stickers(), ids({1, 2}));
}

void RecentStickerModelTest::emitsChangedOncePerClear()
{
    RecentStickerModel model;
    QSignalSpy spy(&model, &RecentStickerModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);

    model.add(7);
    QCOMPARE(spy.count(), 1);

    model.clear();
    QCOMPARE(spy.count(), 2);

    model.clear();
    QCOMPARE(spy.count(), 2);
}

void RecentStickerModelTest::returnsIndependentCopy()
{
    RecentStickerModel model;
    model.setStickers(ids({1, 2}));

    QVector<qint64> snapshot = model.stickers();
    snapshot[0] = 99;
    snapshot.append(3);

    QCOMPARE(model.count(), 2);
    QCOMPARE(model.stickers(), ids({1, 2}));
}

QTEST_GUILESS_MAIN(RecentStickerModelTest)
#include "tst_recent_sticker_model.moc"
