#include "telegram/StarPurchaseModel.h"

#include <QSignalSpy>
#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

StarPackage makePackage(qint64 stars, qint64 priceMinor,
                        const QString& currency = QStringLiteral("USD"))
{
    StarPackage package;
    package.stars = stars;
    package.priceMinor = priceMinor;
    package.currency = currency;
    return package;
}

// Three packages in ascending price order, so a test only has to change the row
// it is about.
QVector<StarPackage> samplePackages()
{
    return {makePackage(100, 199), makePackage(500, 999), makePackage(1000, 1999)};
}

} // namespace

class StarPurchaseModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setPackagesKeepsOrder();
    void setPackagesDropsZeroStars();
    void setPackagesDropsNegativeStars();
    void setPackagesKeepsFirstDuplicate();
    void setPackagesClampsNegativePrice();
    void setPackagesUppercasesCurrency();
    void setPackagesEmitsChangedOnce();
    void setPackagesEmptyOnEmptyModelIsSilent();
    void setPackagesSamePackagesIsSilent();
    void setPackagesPriceChangeIsAChange();
    void setPackagesCurrencyChangeIsAChange();
    void setPackagesReorderedIsAChange();
    void setPackagesReplacesPrevious();
    void setPackagesKeepsSelectionWhenInRange();
    void setPackagesDropsSelectionWhenOutOfRange();
    void packagesReturnsCopy();
    void countMatchesPackages();
    void selectStoresIndex();
    void selectEmitsChangedOnce();
    void selectNegativeOneClearsSelection();
    void selectNegativeOneWithoutSelectionIsSilent();
    void selectOutOfRangeIsIgnored();
    void selectOutOfRangeOnEmptyModelIsSilent();
    void selectSameIndexIsSilent();
    void selectAfterReload();
    void selectedPackageReturnsPackage();
    void selectedPackageDefaultWhenNoneSelected();
    void selectedPackageDefaultAfterReset();
    void priceTextEmptyWhenNoneSelected();
    void priceTextFormatsTwoDecimals();
    void priceTextPadsCents();
    void priceTextUsesStoredCurrency();
    void priceTextWithoutCurrency();
    void priceTextFollowsSelection();
    void canPurchaseFalseWhenEmpty();
    void canPurchaseTrueForValidIndex();
    void canPurchaseFalseAfterReset();
    void resetForgetsPackagesAndSelection();
    void resetOnEmptyModelIsSilent();
    void modelIsUsableAfterReset();
    void changedOnlyOnRealChanges();
};

void StarPurchaseModelTest::startsEmpty()
{
    const StarPurchaseModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.packages().isEmpty());
    QCOMPARE(model.selectedIndex(), -1);
    QVERIFY(model.selectedPackage().stars == 0);
    QCOMPARE(model.priceText(), QString());
    QVERIFY(!model.canPurchase());
}

void StarPurchaseModelTest::setPackagesKeepsOrder()
{
    StarPurchaseModel model;
    model.setPackages({makePackage(1000, 1999),
                       makePackage(100, 199),
                       makePackage(500, 999)});

    const QVector<StarPackage> packages = model.packages();
    QCOMPARE(int(packages.size()), 3);
    // The caller's order is the screen's order: nothing here sorts by stars or
    // by price.
    QCOMPARE(packages.at(0).stars, qint64(1000));
    QCOMPARE(packages.at(1).stars, qint64(100));
    QCOMPARE(packages.at(2).stars, qint64(500));
}

void StarPurchaseModelTest::setPackagesDropsZeroStars()
{
    StarPurchaseModel model;
    model.setPackages({makePackage(0, 99),
                       makePackage(100, 199),
                       makePackage(0, 299)});

    // A package granting nothing can be neither bought nor selected.
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.packages().at(0).stars, qint64(100));
}

void StarPurchaseModelTest::setPackagesDropsNegativeStars()
{
    StarPurchaseModel model;
    model.setPackages({makePackage(-50, 99), makePackage(500, 999)});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.packages().at(0).stars, qint64(500));
}

void StarPurchaseModelTest::setPackagesKeepsFirstDuplicate()
{
    StarPurchaseModel model;
    model.setPackages({makePackage(500, 999),
                       makePackage(500, 799),
                       makePackage(1000, 1999)});

    QCOMPARE(model.count(), 2);
    // The first package of an amount wins, including its price.
    QCOMPARE(model.packages().at(0).stars, qint64(500));
    QCOMPARE(model.packages().at(0).priceMinor, qint64(999));
    QCOMPARE(model.packages().at(1).stars, qint64(1000));
}

void StarPurchaseModelTest::setPackagesClampsNegativePrice()
{
    StarPurchaseModel model;
    model.setPackages({makePackage(100, -199)});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.packages().at(0).priceMinor, qint64(0));
}

void StarPurchaseModelTest::setPackagesUppercasesCurrency()
{
    StarPurchaseModel model;
    model.setPackages({makePackage(100, 199, QStringLiteral("usd")),
                       makePackage(500, 999, QStringLiteral("eur"))});

    QCOMPARE(model.packages().at(0).currency, QStringLiteral("USD"));
    QCOMPARE(model.packages().at(1).currency, QStringLiteral("EUR"));
}

void StarPurchaseModelTest::setPackagesEmitsChangedOnce()
{
    StarPurchaseModel model;
    QSignalSpy spy(&model, &StarPurchaseModel::changed);

    model.setPackages(samplePackages());

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.count(), 3);
}

void StarPurchaseModelTest::setPackagesEmptyOnEmptyModelIsSilent()
{
    StarPurchaseModel model;
    QSignalSpy spy(&model, &StarPurchaseModel::changed);

    // Every package is unusable, so the normalized list is the one already
    // stored.
    model.setPackages({makePackage(0, 199)});

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 0);
}

void StarPurchaseModelTest::setPackagesSamePackagesIsSilent()
{
    StarPurchaseModel model;
    const QVector<StarPackage> packages = samplePackages();
    model.setPackages(packages);

    QSignalSpy spy(&model, &StarPurchaseModel::changed);
    model.setPackages(packages);

    // A refresh that brings the same packages must not repaint the list.
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 3);
}

void StarPurchaseModelTest::setPackagesPriceChangeIsAChange()
{
    StarPurchaseModel model;
    model.setPackages({makePackage(100, 199)});

    // The store re-prices the package, so the rendered amount moves.
    QSignalSpy spy(&model, &StarPurchaseModel::changed);
    model.setPackages({makePackage(100, 249)});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.packages().at(0).priceMinor, qint64(249));
}

void StarPurchaseModelTest::setPackagesCurrencyChangeIsAChange()
{
    StarPurchaseModel model;
    model.setPackages({makePackage(100, 199, QStringLiteral("USD"))});

    QSignalSpy spy(&model, &StarPurchaseModel::changed);
    model.setPackages({makePackage(100, 199, QStringLiteral("CAD"))});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.packages().at(0).currency, QStringLiteral("CAD"));
}

void StarPurchaseModelTest::setPackagesReorderedIsAChange()
{
    StarPurchaseModel model;
    model.setPackages({makePackage(100, 199), makePackage(500, 999)});

    QSignalSpy spy(&model, &StarPurchaseModel::changed);
    model.setPackages({makePackage(500, 999), makePackage(100, 199)});

    // Same packages, different display order: the screen would look different.
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.packages().at(0).stars, qint64(500));
}

void StarPurchaseModelTest::setPackagesReplacesPrevious()
{
    StarPurchaseModel model;
    model.setPackages(samplePackages());
    model.setPackages({makePackage(500, 999)});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.packages().at(0).stars, qint64(500));
}

void StarPurchaseModelTest::setPackagesKeepsSelectionWhenInRange()
{
    StarPurchaseModel model;
    model.setPackages(samplePackages());
    model.select(1);

    // A reload may re-price the packages; the buyer's choice must not jump back
    // to nothing for it.
    QSignalSpy spy(&model, &StarPurchaseModel::changed);
    model.setPackages({makePackage(100, 179), makePackage(500, 899),
                       makePackage(1000, 1799)});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.selectedIndex(), 1);
    QCOMPARE(model.selectedPackage().priceMinor, qint64(899));
}

void StarPurchaseModelTest::setPackagesDropsSelectionWhenOutOfRange()
{
    StarPurchaseModel model;
    model.setPackages(samplePackages());
    model.select(2);

    QSignalSpy spy(&model, &StarPurchaseModel::changed);
    model.setPackages({makePackage(100, 199), makePackage(500, 999)});

    // The index must never name a row the list no longer offers.
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.selectedIndex(), -1);
    QVERIFY(!model.canPurchase());
    QCOMPARE(model.priceText(), QString());
}

void StarPurchaseModelTest::packagesReturnsCopy()
{
    StarPurchaseModel model;
    model.setPackages({makePackage(100, 199)});

    QVector<StarPackage> copy = model.packages();
    copy[0].priceMinor = 1;

    // Handing out a copy keeps the model's own state private.
    QCOMPARE(model.packages().at(0).priceMinor, qint64(199));
}

void StarPurchaseModelTest::countMatchesPackages()
{
    StarPurchaseModel model;
    QCOMPARE(model.count(), 0);

    model.setPackages(samplePackages());
    QCOMPARE(model.count(), int(model.packages().size()));
    QCOMPARE(model.count(), 3);
}

void StarPurchaseModelTest::selectStoresIndex()
{
    StarPurchaseModel model;
    model.setPackages(samplePackages());
    QCOMPARE(model.selectedIndex(), -1);

    model.select(2);

    QCOMPARE(model.selectedIndex(), 2);
    QCOMPARE(model.selectedPackage().stars, qint64(1000));
}

void StarPurchaseModelTest::selectEmitsChangedOnce()
{
    StarPurchaseModel model;
    model.setPackages(samplePackages());
    QSignalSpy spy(&model, &StarPurchaseModel::changed);

    model.select(1);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.selectedIndex(), 1);
}

void StarPurchaseModelTest::selectNegativeOneClearsSelection()
{
    StarPurchaseModel model;
    model.setPackages(samplePackages());
    model.select(0);

    QSignalSpy spy(&model, &StarPurchaseModel::changed);
    model.select(-1);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.selectedIndex(), -1);
    QVERIFY(!model.canPurchase());
}

void StarPurchaseModelTest::selectNegativeOneWithoutSelectionIsSilent()
{
    StarPurchaseModel model;
    model.setPackages(samplePackages());
    QSignalSpy spy(&model, &StarPurchaseModel::changed);

    model.select(-1);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.selectedIndex(), -1);
}

void StarPurchaseModelTest::selectOutOfRangeIsIgnored()
{
    StarPurchaseModel model;
    model.setPackages(samplePackages());
    model.select(0);

    // A tap on a row the last reload removed must not select an invisible
    // package.
    QSignalSpy spy(&model, &StarPurchaseModel::changed);
    model.select(3);
    model.select(99);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.selectedIndex(), 0);
}

void StarPurchaseModelTest::selectOutOfRangeOnEmptyModelIsSilent()
{
    StarPurchaseModel model;
    QSignalSpy spy(&model, &StarPurchaseModel::changed);

    model.select(0);
    model.select(7);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.selectedIndex(), -1);
}

void StarPurchaseModelTest::selectSameIndexIsSilent()
{
    StarPurchaseModel model;
    model.setPackages(samplePackages());
    model.select(1);

    QSignalSpy spy(&model, &StarPurchaseModel::changed);
    model.select(1);

    // Re-selecting the current row must not restart the screen's animation.
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.selectedIndex(), 1);
}

void StarPurchaseModelTest::selectAfterReload()
{
    StarPurchaseModel model;
    model.setPackages(samplePackages());
    model.select(0);
    model.setPackages(samplePackages());

    // The reload kept the selection; picking another row still works.
    model.select(2);

    QCOMPARE(model.selectedIndex(), 2);
    QCOMPARE(model.selectedPackage().stars, qint64(1000));
}

void StarPurchaseModelTest::selectedPackageReturnsPackage()
{
    StarPurchaseModel model;
    model.setPackages(samplePackages());
    model.select(1);

    const StarPackage package = model.selectedPackage();
    QCOMPARE(package.stars, qint64(500));
    QCOMPARE(package.priceMinor, qint64(999));
    QCOMPARE(package.currency, QStringLiteral("USD"));
}

void StarPurchaseModelTest::selectedPackageDefaultWhenNoneSelected()
{
    StarPurchaseModel model;
    model.setPackages(samplePackages());

    // A caller may read the fields without a has-selection branch.
    const StarPackage package = model.selectedPackage();
    QCOMPARE(package.stars, qint64(0));
    QCOMPARE(package.priceMinor, qint64(0));
    QVERIFY(package.currency.isEmpty());
}

void StarPurchaseModelTest::selectedPackageDefaultAfterReset()
{
    StarPurchaseModel model;
    model.setPackages(samplePackages());
    model.select(0);
    model.reset();

    QCOMPARE(model.selectedPackage().stars, qint64(0));
    QCOMPARE(model.selectedIndex(), -1);
}

void StarPurchaseModelTest::priceTextEmptyWhenNoneSelected()
{
    StarPurchaseModel model;
    QCOMPARE(model.priceText(), QString());

    // Packages without a selection still have no price to show.
    model.setPackages(samplePackages());
    QCOMPARE(model.priceText(), QString());
}

void StarPurchaseModelTest::priceTextFormatsTwoDecimals()
{
    StarPurchaseModel model;
    model.setPackages({makePackage(100, 199), makePackage(500, 200)});

    model.select(0);
    QCOMPARE(model.priceText(), QStringLiteral("1.99 USD"));

    // A whole price keeps its two decimals instead of reading "2 USD".
    model.select(1);
    QCOMPARE(model.priceText(), QStringLiteral("2.00 USD"));
}

void StarPurchaseModelTest::priceTextPadsCents()
{
    StarPurchaseModel model;
    model.setPackages({makePackage(100, 5), makePackage(200, 50), makePackage(300, 0)});

    model.select(0);
    QCOMPARE(model.priceText(), QStringLiteral("0.05 USD"));

    model.select(1);
    QCOMPARE(model.priceText(), QStringLiteral("0.50 USD"));

    model.select(2);
    QCOMPARE(model.priceText(), QStringLiteral("0.00 USD"));
}

void StarPurchaseModelTest::priceTextUsesStoredCurrency()
{
    StarPurchaseModel model;
    // The model upper-cases the code, so the text is "1.99 EUR", not "1.99 eur".
    model.setPackages({makePackage(100, 199, QStringLiteral("eur"))});
    model.select(0);

    QCOMPARE(model.priceText(), QStringLiteral("1.99 EUR"));
}

void StarPurchaseModelTest::priceTextWithoutCurrency()
{
    StarPurchaseModel model;
    model.setPackages({makePackage(100, 199, QString())});
    model.select(0);

    // No code means no trailing separator: the amount stands alone.
    QCOMPARE(model.priceText(), QStringLiteral("1.99"));
}

void StarPurchaseModelTest::priceTextFollowsSelection()
{
    StarPurchaseModel model;
    model.setPackages(samplePackages());

    model.select(0);
    QCOMPARE(model.priceText(), QStringLiteral("1.99 USD"));

    model.select(2);
    QCOMPARE(model.priceText(), QStringLiteral("19.99 USD"));

    model.select(-1);
    QCOMPARE(model.priceText(), QString());
}

void StarPurchaseModelTest::canPurchaseFalseWhenEmpty()
{
    StarPurchaseModel model;

    QVERIFY(!model.canPurchase());

    model.setPackages(samplePackages());
    QVERIFY(!model.canPurchase());
}

void StarPurchaseModelTest::canPurchaseTrueForValidIndex()
{
    StarPurchaseModel model;
    model.setPackages(samplePackages());

    model.select(0);
    QVERIFY(model.canPurchase());

    model.select(2);
    QVERIFY(model.canPurchase());
}

void StarPurchaseModelTest::canPurchaseFalseAfterReset()
{
    StarPurchaseModel model;
    model.setPackages(samplePackages());
    model.select(1);
    QVERIFY(model.canPurchase());

    model.reset();

    QVERIFY(!model.canPurchase());
}

void StarPurchaseModelTest::resetForgetsPackagesAndSelection()
{
    StarPurchaseModel model;
    model.setPackages(samplePackages());
    model.select(0);

    QSignalSpy spy(&model, &StarPurchaseModel::changed);
    model.reset();

    QCOMPARE(spy.count(), 1);
    QVERIFY(model.packages().isEmpty());
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.selectedIndex(), -1);
    QCOMPARE(model.priceText(), QString());
}

void StarPurchaseModelTest::resetOnEmptyModelIsSilent()
{
    StarPurchaseModel model;
    QSignalSpy spy(&model, &StarPurchaseModel::changed);

    model.reset();
    model.reset();

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.packages().isEmpty());
}

void StarPurchaseModelTest::modelIsUsableAfterReset()
{
    StarPurchaseModel model;
    model.setPackages(samplePackages());
    model.select(2);
    model.reset();

    // A fresh answer after the screen reopened behaves like a first load.
    model.setPackages({makePackage(500, 999)});
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.selectedIndex(), -1);
    QVERIFY(!model.canPurchase());

    model.select(0);
    QCOMPARE(model.selectedPackage().stars, qint64(500));
    QVERIFY(model.canPurchase());
}

void StarPurchaseModelTest::changedOnlyOnRealChanges()
{
    StarPurchaseModel model;
    const QVector<StarPackage> packages = samplePackages();
    QSignalSpy spy(&model, &StarPurchaseModel::changed);

    model.setPackages(packages);        // 1: first load
    model.setPackages(packages);        // silent: same packages
    model.select(1);                    // 2: new selection
    model.select(1);                    // silent: same selection
    model.select(9);                    // silent: out of range
    model.select(-1);                   // 3: selection cleared
    model.select(-1);                   // silent: already cleared
    model.reset();                      // 4: packages dropped
    model.reset();                      // silent: already empty

    QCOMPARE(spy.count(), 4);
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.selectedIndex(), -1);
}

QTEST_GUILESS_MAIN(StarPurchaseModelTest)
#include "tst_star_purchase_model.moc"
