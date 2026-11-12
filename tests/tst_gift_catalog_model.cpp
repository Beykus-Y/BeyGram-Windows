#include "telegram/GiftCatalogModel.h"

#include <QSignalSpy>
#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

GiftRow row(qint64 giftId, qint64 starCount, const QString& title = {},
            int stickerFileId = 0, bool limited = false)
{
    return GiftRow{giftId, title, starCount, stickerFileId, limited};
}

QVector<qint64> idsOf(const QVector<GiftRow>& rows)
{
    QVector<qint64> ids;
    ids.reserve(rows.size());
    for (const GiftRow& item : rows)
        ids.append(item.giftId);
    return ids;
}

} // namespace

class GiftCatalogModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();

    void setGiftsDropsNonPositiveIds();
    void setGiftsDropsNegativePrice();
    void setGiftsKeepsFreeGift();
    void setGiftsDedupesByIdKeepingFirst();
    void setGiftsKeepsIncomingOrder();
    void setGiftsReplacesPreviousCatalog();

    void sortedByPriceAscending();
    void sortedByPriceDescending();
    void sortedByPriceIsStableForTies();
    void sortedByPriceLeavesStoredOrderAlone();
    void sortedByPriceOfEmptyCatalogIsEmpty();

    void limitedFiltersRows();
    void limitedKeepsStoredOrder();
    void limitedOfCatalogWithoutLimitedGiftsIsEmpty();

    void giftReturnsStoredRow();
    void giftOfMissingIdReturnsDefaultRow();
    void giftOfNonPositiveIdReturnsDefaultRow();
    void giftFindsRowAfterFiltering();

    void clearDropsCatalog();
    void clearOnEmptyIsNoOp();

    void emitsChangedOnRealSetGifts();
    void emitsChangedOnlyOncePerClear();
    void returnsIndependentCopies();
};

void GiftCatalogModelTest::startsEmpty()
{
    GiftCatalogModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.gifts().isEmpty());
    QVERIFY(model.sortedByPrice().isEmpty());
    QVERIFY(model.limited().isEmpty());
    QCOMPARE(model.gift(1).giftId, 0);
}

void GiftCatalogModelTest::setGiftsDropsNonPositiveIds()
{
    GiftCatalogModel model;
    model.setGifts({
        row(0, 100, QStringLiteral("Zero")),
        row(-3, 100, QStringLiteral("Negative")),
        row(7, 100, QStringLiteral("Kept")),
    });

    QCOMPARE(model.count(), 1);
    QCOMPARE(idsOf(model.gifts()), QVector<qint64>({7}));
}

void GiftCatalogModelTest::setGiftsDropsNegativePrice()
{
    GiftCatalogModel model;
    model.setGifts({
        row(1, -1, QStringLiteral("Negative price")),
        row(2, 0, QStringLiteral("Free")),
        row(3, 500, QStringLiteral("Paid")),
    });

    QCOMPARE(model.count(), 2);
    QCOMPARE(idsOf(model.gifts()), QVector<qint64>({2, 3}));
}

void GiftCatalogModelTest::setGiftsKeepsFreeGift()
{
    GiftCatalogModel model;
    model.setGifts({row(4, 0, QStringLiteral("Free"))});

    // Zero stars is a valid price, not a missing one.
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.gift(4).starCount, 0);
}

void GiftCatalogModelTest::setGiftsDedupesByIdKeepingFirst()
{
    GiftCatalogModel model;
    model.setGifts({
        row(1, 100, QStringLiteral("First"), 11),
        row(2, 200, QStringLiteral("Second"), 22),
        row(1, 900, QStringLiteral("Duplicate"), 99, true),
        row(2, 300, QStringLiteral("Third"), 33),
    });

    QCOMPARE(model.count(), 2);
    QCOMPARE(idsOf(model.gifts()), QVector<qint64>({1, 2}));
    QCOMPARE(model.gift(1).title, QStringLiteral("First"));
    QCOMPARE(model.gift(1).starCount, 100);
    QCOMPARE(model.gift(1).stickerFileId, 11);
    QCOMPARE(model.gift(1).limited, false);
    QCOMPARE(model.gift(2).title, QStringLiteral("Second"));
}

void GiftCatalogModelTest::setGiftsKeepsIncomingOrder()
{
    GiftCatalogModel model;
    model.setGifts({
        row(30, 500),
        row(10, 100),
        row(20, 300),
    });

    QCOMPARE(idsOf(model.gifts()), QVector<qint64>({30, 10, 20}));
}

void GiftCatalogModelTest::setGiftsReplacesPreviousCatalog()
{
    GiftCatalogModel model;
    model.setGifts({row(1, 100), row(2, 200)});
    model.setGifts({row(3, 300)});

    QCOMPARE(model.count(), 1);
    QCOMPARE(idsOf(model.gifts()), QVector<qint64>({3}));
}

void GiftCatalogModelTest::sortedByPriceAscending()
{
    GiftCatalogModel model;
    model.setGifts({row(1, 500), row(2, 100), row(3, 300)});

    QCOMPARE(idsOf(model.sortedByPrice()), QVector<qint64>({2, 3, 1}));
    QCOMPARE(idsOf(model.sortedByPrice(true)), QVector<qint64>({2, 3, 1}));
}

void GiftCatalogModelTest::sortedByPriceDescending()
{
    GiftCatalogModel model;
    model.setGifts({row(1, 500), row(2, 100), row(3, 300)});

    QCOMPARE(idsOf(model.sortedByPrice(false)), QVector<qint64>({1, 3, 2}));
}

void GiftCatalogModelTest::sortedByPriceIsStableForTies()
{
    GiftCatalogModel model;
    model.setGifts({
        row(1, 200),
        row(2, 100),
        row(3, 200),
        row(4, 100),
        row(5, 200),
    });

    // Equal prices keep their stored order, both ascending and descending.
    QCOMPARE(idsOf(model.sortedByPrice()), QVector<qint64>({2, 4, 1, 3, 5}));
    QCOMPARE(idsOf(model.sortedByPrice(false)),
             QVector<qint64>({1, 3, 5, 2, 4}));
}

void GiftCatalogModelTest::sortedByPriceLeavesStoredOrderAlone()
{
    GiftCatalogModel model;
    model.setGifts({row(1, 500), row(2, 100), row(3, 300)});

    QVERIFY(!model.sortedByPrice().isEmpty());
    QCOMPARE(idsOf(model.gifts()), QVector<qint64>({1, 2, 3}));
}

void GiftCatalogModelTest::sortedByPriceOfEmptyCatalogIsEmpty()
{
    GiftCatalogModel model;

    QVERIFY(model.sortedByPrice().isEmpty());
    QVERIFY(model.sortedByPrice(false).isEmpty());
}

void GiftCatalogModelTest::limitedFiltersRows()
{
    GiftCatalogModel model;
    model.setGifts({
        row(1, 100, QStringLiteral("Regular")),
        row(2, 200, QStringLiteral("Limited"), 0, true),
        row(3, 300, QStringLiteral("Also limited"), 0, true),
    });

    QCOMPARE(idsOf(model.limited()), QVector<qint64>({2, 3}));
    QCOMPARE(model.count(), 3);
}

void GiftCatalogModelTest::limitedKeepsStoredOrder()
{
    GiftCatalogModel model;
    model.setGifts({
        row(1, 900, QStringLiteral("Expensive"), 0, true),
        row(2, 100, QStringLiteral("Regular")),
        row(3, 50, QStringLiteral("Cheap"), 0, true),
    });

    // No price sorting: the catalog order is kept.
    QCOMPARE(idsOf(model.limited()), QVector<qint64>({1, 3}));
}

void GiftCatalogModelTest::limitedOfCatalogWithoutLimitedGiftsIsEmpty()
{
    GiftCatalogModel model;
    model.setGifts({row(1, 100), row(2, 200)});

    QVERIFY(model.limited().isEmpty());
}

void GiftCatalogModelTest::giftReturnsStoredRow()
{
    GiftCatalogModel model;
    model.setGifts({row(1, 100, QStringLiteral("Bear"), 42, true)});

    const GiftRow found = model.gift(1);
    QCOMPARE(found.giftId, 1);
    QCOMPARE(found.title, QStringLiteral("Bear"));
    QCOMPARE(found.starCount, 100);
    QCOMPARE(found.stickerFileId, 42);
    QCOMPARE(found.limited, true);
}

void GiftCatalogModelTest::giftOfMissingIdReturnsDefaultRow()
{
    GiftCatalogModel model;
    model.setGifts({row(1, 100, QStringLiteral("Bear"))});

    const GiftRow missing = model.gift(99);
    QCOMPARE(missing.giftId, 0);
    QVERIFY(missing.title.isEmpty());
    QCOMPARE(missing.starCount, 0);
    QCOMPARE(missing.stickerFileId, 0);
    QCOMPARE(missing.limited, false);
}

void GiftCatalogModelTest::giftOfNonPositiveIdReturnsDefaultRow()
{
    GiftCatalogModel model;
    model.setGifts({row(1, 100)});

    // Filtered ids are not stored, so they read back as the default row.
    QCOMPARE(model.gift(0).giftId, 0);
    QCOMPARE(model.gift(-1).giftId, 0);
}

void GiftCatalogModelTest::giftFindsRowAfterFiltering()
{
    GiftCatalogModel model;
    model.setGifts({
        row(0, 100, QStringLiteral("Dropped id")),
        row(5, -2, QStringLiteral("Dropped price")),
        row(6, 250, QStringLiteral("Stored")),
    });

    QCOMPARE(model.gift(6).title, QStringLiteral("Stored"));
    QCOMPARE(model.gift(5).giftId, 0);
}

void GiftCatalogModelTest::clearDropsCatalog()
{
    GiftCatalogModel model;
    model.setGifts({row(1, 100), row(2, 200, QString(), 0, true)});

    model.clear();

    QCOMPARE(model.count(), 0);
    QVERIFY(model.gifts().isEmpty());
    QVERIFY(model.sortedByPrice().isEmpty());
    QVERIFY(model.limited().isEmpty());
}

void GiftCatalogModelTest::clearOnEmptyIsNoOp()
{
    GiftCatalogModel model;
    QSignalSpy spy(&model, &GiftCatalogModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 0);
}

void GiftCatalogModelTest::emitsChangedOnRealSetGifts()
{
    GiftCatalogModel model;
    QSignalSpy spy(&model, &GiftCatalogModel::changed);

    model.setGifts({row(1, 100, QStringLiteral("Bear"))});
    QCOMPARE(spy.count(), 1);

    // The same catalog again is not a change.
    model.setGifts({row(1, 100, QStringLiteral("Bear"))});
    QCOMPARE(spy.count(), 1);

    // A different price for the same gift is a change.
    model.setGifts({row(1, 150, QStringLiteral("Bear"))});
    QCOMPARE(spy.count(), 2);

    // A different title is a change.
    model.setGifts({row(1, 150, QStringLiteral("Panda"))});
    QCOMPARE(spy.count(), 3);

    // A different sticker is a change.
    model.setGifts({row(1, 150, QStringLiteral("Panda"), 9)});
    QCOMPARE(spy.count(), 4);

    // Becoming limited is a change.
    model.setGifts({row(1, 150, QStringLiteral("Panda"), 9, true)});
    QCOMPARE(spy.count(), 5);

    // Adding a gift is a change.
    model.setGifts({row(1, 150, QStringLiteral("Panda"), 9, true),
                    row(2, 300)});
    QCOMPARE(spy.count(), 6);

    // Dropping every gift is a change.
    model.setGifts({});
    QCOMPARE(spy.count(), 7);

    // Already empty stays silent.
    model.setGifts({});
    QCOMPARE(spy.count(), 7);
}

void GiftCatalogModelTest::emitsChangedOnlyOncePerClear()
{
    GiftCatalogModel model;
    QSignalSpy spy(&model, &GiftCatalogModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);

    model.setGifts({row(1, 100)});
    QCOMPARE(spy.count(), 1);

    model.clear();
    QCOMPARE(spy.count(), 2);

    model.clear();
    QCOMPARE(spy.count(), 2);
}

void GiftCatalogModelTest::returnsIndependentCopies()
{
    GiftCatalogModel model;
    model.setGifts({row(1, 100, QStringLiteral("Bear")),
                    row(2, 200, QStringLiteral("Panda"), 0, true)});

    QVector<GiftRow> snapshot = model.gifts();
    snapshot[0].title = QStringLiteral("mutated");
    snapshot.append(row(3, 300));
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.gifts().at(0).title, QStringLiteral("Bear"));

    QVector<GiftRow> sorted = model.sortedByPrice();
    sorted[0].giftId = 99;
    QCOMPARE(model.sortedByPrice().at(0).giftId, 1);

    QVector<GiftRow> limited = model.limited();
    limited[0].giftId = 98;
    QCOMPARE(model.limited().at(0).giftId, 2);
}

QTEST_GUILESS_MAIN(GiftCatalogModelTest)
#include "tst_gift_catalog_model.moc"
