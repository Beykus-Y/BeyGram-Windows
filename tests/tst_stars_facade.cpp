#include "ui/StarsFacade.h"

#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

using namespace beygram::ui;

namespace {

QString starGlyph()
{
    return QString(QChar(0x2B50));
}

/// The label separator: space, middle dot, space.
QString dot()
{
    return QStringLiteral(" ") + QChar(0x00B7) + QStringLiteral(" ");
}

/// "100 ⭐", for rows whose amount needs no grouping.
QString stars(qint64 amount)
{
    return QString::number(amount) + QLatin1Char(' ') + starGlyph();
}

/// A three-package offer in ascending price order: 100 / 500 / 1000 stars.
void loadSamplePackages(StarsFacade& facade)
{
    facade.setPackages({QStringLiteral("100"), QStringLiteral("500"), QStringLiteral("1000")},
                       {QStringLiteral("199"), QStringLiteral("999"), QStringLiteral("1999")},
                       {QStringLiteral("usd"), QStringLiteral("USD"), QStringLiteral("eur")});
}

} // namespace

class StarsFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void balanceTextGroupsThousands();
    void balanceTextClampsNegative();
    void balanceChangeEmitsChangedOnce();
    void balanceSameValueIsSilent();
    void packagesBuildLabels();
    void packagesSkipMalformedStars();
    void packagesKeepFirstDuplicate();
    void packagesShortListsReadAsZero();
    void emptyPackagesClearSelection();
    void selectionUpdatesPriceAndCanBuy();
    void selectOutOfRangeKeepsSelection();
    void selectNegativeOneClearsSelection();
    void selectionSurvivesSameSizeReload();
    void selectionDroppedOnShorterReload();
    void buyWithoutSelectionEmitsNothing();
    void buyEmitsSelectedPayload();
    void buyEmitsSecondPackagePayload();
    void transactionsFormatNewestFirst();
    void transactionsUseFlagForSign();
    void transactionsOmitMissingTitleAndDate();
    void transactionsDropInvalidIds();
    void transactionsReplaceAndClear();
    void changedCoversEveryModelUpdate();
};

void StarsFacadeTest::startsEmpty()
{
    StarsFacade facade;

    QCOMPARE(facade.balanceText(), QStringLiteral("0 ") + starGlyph());
    QVERIFY(facade.packageLabels().isEmpty());
    QCOMPARE(facade.selectedIndex(), -1);
    QVERIFY(facade.priceText().isEmpty());
    QVERIFY(!facade.canBuy());
    QVERIFY(facade.transactions().isEmpty());
}

void StarsFacadeTest::balanceTextGroupsThousands()
{
    StarsFacade facade;

    facade.setBalance(999);
    QCOMPARE(facade.balanceText(), QStringLiteral("999 ") + starGlyph());

    facade.setBalance(1234);
    QCOMPARE(facade.balanceText(), QStringLiteral("1 234 ") + starGlyph());

    facade.setBalance(1234567);
    QCOMPARE(facade.balanceText(), QStringLiteral("1 234 567 ") + starGlyph());
}

void StarsFacadeTest::balanceTextClampsNegative()
{
    StarsFacade facade;

    facade.setBalance(-42);
    QCOMPARE(facade.balanceText(), QStringLiteral("0 ") + starGlyph());
}

void StarsFacadeTest::balanceChangeEmitsChangedOnce()
{
    StarsFacade facade;
    QSignalSpy spy(&facade, &StarsFacade::changed);

    facade.setBalance(10);
    QCOMPARE(spy.count(), 1);

    facade.setBalance(-5);
    QCOMPARE(spy.count(), 2);
}

void StarsFacadeTest::balanceSameValueIsSilent()
{
    StarsFacade facade;
    facade.setBalance(10);

    QSignalSpy spy(&facade, &StarsFacade::changed);
    facade.setBalance(10);
    QCOMPARE(spy.count(), 0);

    // Zeroing the balance is a real change; a negative value then clamps to
    // the already stored 0 and stays silent.
    facade.setBalance(0);
    QCOMPARE(spy.count(), 1);
    facade.setBalance(-1);
    QCOMPARE(spy.count(), 1);
}

void StarsFacadeTest::packagesBuildLabels()
{
    StarsFacade facade;
    loadSamplePackages(facade);

    const QStringList expected{
        stars(100) + dot() + QStringLiteral("1.99 USD"),
        stars(500) + dot() + QStringLiteral("9.99 USD"),
        QStringLiteral("1 000 ") + starGlyph() + dot() + QStringLiteral("19.99 EUR"),
    };

    QCOMPARE(facade.packageLabels(), expected);
}

void StarsFacadeTest::packagesSkipMalformedStars()
{
    StarsFacade facade;

    // "abc" is not a number, 0 and -5 are not amounts a package can grant.
    facade.setPackages({QStringLiteral("abc"), QStringLiteral("0"), QStringLiteral("-5"),
                        QStringLiteral("250")},
                       {QStringLiteral("100"), QStringLiteral("100"), QStringLiteral("100"),
                        QStringLiteral("5")},
                       {QStringLiteral("USD"), QStringLiteral("USD"), QStringLiteral("USD"),
                        QStringLiteral("usd")});

    QCOMPARE(facade.packageLabels().size(), 1);
    QCOMPARE(facade.packageLabels().at(0),
             stars(250) + dot() + QStringLiteral("0.05 USD"));
}

void StarsFacadeTest::packagesKeepFirstDuplicate()
{
    StarsFacade facade;

    facade.setPackages({QStringLiteral("100"), QStringLiteral("100")},
                       {QStringLiteral("199"), QStringLiteral("149")},
                       {QStringLiteral("USD"), QStringLiteral("USD")});

    QCOMPARE(facade.packageLabels().size(), 1);
    QCOMPARE(facade.packageLabels().at(0),
             stars(100) + dot() + QStringLiteral("1.99 USD"));
}

void StarsFacadeTest::packagesShortListsReadAsZero()
{
    StarsFacade facade;

    // No price and no currency at all: the label still reads, without a
    // dangling space or an empty currency code.
    facade.setPackages({QStringLiteral("300")}, {}, {});
    QCOMPARE(facade.packageLabels().size(), 1);
    QCOMPARE(facade.packageLabels().at(0), stars(300) + dot() + QStringLiteral("0.00"));

    // A malformed price reads as 0 too.
    facade.setPackages({QStringLiteral("300")}, {QStringLiteral("free")},
                       {QStringLiteral("USD")});
    QCOMPARE(facade.packageLabels().at(0),
             stars(300) + dot() + QStringLiteral("0.00 USD"));
}

void StarsFacadeTest::emptyPackagesClearSelection()
{
    StarsFacade facade;
    loadSamplePackages(facade);
    facade.select(2);

    facade.setPackages({}, {}, {});

    QVERIFY(facade.packageLabels().isEmpty());
    QCOMPARE(facade.selectedIndex(), -1);
    QVERIFY(!facade.canBuy());
    QVERIFY(facade.priceText().isEmpty());
}

void StarsFacadeTest::selectionUpdatesPriceAndCanBuy()
{
    StarsFacade facade;
    loadSamplePackages(facade);

    QVERIFY(!facade.canBuy());
    QVERIFY(facade.priceText().isEmpty());

    facade.select(1);
    QCOMPARE(facade.selectedIndex(), 1);
    QVERIFY(facade.canBuy());
    QCOMPARE(facade.priceText(), QStringLiteral("9.99 USD"));

    facade.select(2);
    QCOMPARE(facade.selectedIndex(), 2);
    QCOMPARE(facade.priceText(), QStringLiteral("19.99 EUR"));
}

void StarsFacadeTest::selectOutOfRangeKeepsSelection()
{
    StarsFacade facade;
    loadSamplePackages(facade);
    facade.select(1);

    facade.select(3);
    facade.select(99);
    QCOMPARE(facade.selectedIndex(), 1);
    QCOMPARE(facade.priceText(), QStringLiteral("9.99 USD"));
}

void StarsFacadeTest::selectNegativeOneClearsSelection()
{
    StarsFacade facade;
    loadSamplePackages(facade);
    facade.select(0);
    QVERIFY(facade.canBuy());

    facade.select(-1);
    QCOMPARE(facade.selectedIndex(), -1);
    QVERIFY(!facade.canBuy());
    QVERIFY(facade.priceText().isEmpty());
}

void StarsFacadeTest::selectionSurvivesSameSizeReload()
{
    StarsFacade facade;
    loadSamplePackages(facade);
    facade.select(1);

    // A price refresh of the same offer must not lose the buyer's choice.
    facade.setPackages({QStringLiteral("100"), QStringLiteral("500"), QStringLiteral("1000")},
                       {QStringLiteral("189"), QStringLiteral("949"), QStringLiteral("1899")},
                       {QStringLiteral("USD"), QStringLiteral("USD"), QStringLiteral("USD")});

    QCOMPARE(facade.selectedIndex(), 1);
    QCOMPARE(facade.priceText(), QStringLiteral("9.49 USD"));
}

void StarsFacadeTest::selectionDroppedOnShorterReload()
{
    StarsFacade facade;
    loadSamplePackages(facade);
    facade.select(2);

    facade.setPackages({QStringLiteral("100")}, {QStringLiteral("199")},
                       {QStringLiteral("USD")});

    QCOMPARE(facade.selectedIndex(), -1);
    QVERIFY(!facade.canBuy());
}

void StarsFacadeTest::buyWithoutSelectionEmitsNothing()
{
    StarsFacade facade;
    loadSamplePackages(facade);
    QSignalSpy spy(&facade, &StarsFacade::purchaseRequested);

    facade.buy();
    QCOMPARE(spy.count(), 0);
}

void StarsFacadeTest::buyEmitsSelectedPayload()
{
    StarsFacade facade;
    loadSamplePackages(facade);
    facade.select(0);

    QSignalSpy spy(&facade, &StarsFacade::purchaseRequested);
    facade.buy();

    QCOMPARE(spy.count(), 1);
    const QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toLongLong(), qint64(100));
    QCOMPARE(arguments.at(1).toString(), QStringLiteral("USD"));
    QCOMPARE(arguments.at(2).toLongLong(), qint64(199));

    // Buying reports the choice; it does not clear it.
    QCOMPARE(facade.selectedIndex(), 0);
}

void StarsFacadeTest::buyEmitsSecondPackagePayload()
{
    StarsFacade facade;
    loadSamplePackages(facade);
    facade.select(2);

    QSignalSpy spy(&facade, &StarsFacade::purchaseRequested);
    facade.buy();

    QCOMPARE(spy.count(), 1);
    const QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toLongLong(), qint64(1000));
    QCOMPARE(arguments.at(1).toString(), QStringLiteral("EUR"));
    QCOMPARE(arguments.at(2).toLongLong(), qint64(1999));
}

void StarsFacadeTest::transactionsFormatNewestFirst()
{
    StarsFacade facade;

    // 2024-05-01 and 2024-05-02, UTC. The older row is listed first on the
    // wire; the model sorts the newer one to the top.
    facade.setTransactions({QStringLiteral("1"), QStringLiteral("2")},
                           {QStringLiteral("1714521600"), QStringLiteral("1714608000")},
                           {QStringLiteral("100"), QStringLiteral("50")},
                           {QStringLiteral("Stars purchase"), QStringLiteral("Gift")},
                           {QStringLiteral("true"), QStringLiteral("false")});

    const QStringList expected{
        QStringLiteral("-50 ") + starGlyph() + dot() + QStringLiteral("Gift")
            + dot() + QStringLiteral("2024-05-02"),
        QStringLiteral("+100 ") + starGlyph() + dot() + QStringLiteral("Stars purchase")
            + dot() + QStringLiteral("2024-05-01"),
    };

    QCOMPARE(facade.transactions(), expected);
}

void StarsFacadeTest::transactionsUseFlagForSign()
{
    StarsFacade facade;

    // The amount is a magnitude: the same stored 75 reads as a credit or a
    // debit depending only on the incoming flag, and a negative wire amount
    // does not flip it.
    facade.setTransactions({QStringLiteral("1"), QStringLiteral("2")},
                           {QStringLiteral("1714521600"), QStringLiteral("1714521600")},
                           {QStringLiteral("75"), QStringLiteral("-75")},
                           {QStringLiteral("Spend"), QStringLiteral("Refund")},
                           {QStringLiteral("false"), QStringLiteral("true")});

    QCOMPARE(facade.transactions().size(), 2);
    QCOMPARE(facade.transactions().at(0),
             QStringLiteral("+75 ") + starGlyph() + dot() + QStringLiteral("Refund")
                 + dot() + QStringLiteral("2024-05-01"));
    QCOMPARE(facade.transactions().at(1),
             QStringLiteral("-75 ") + starGlyph() + dot() + QStringLiteral("Spend")
                 + dot() + QStringLiteral("2024-05-01"));
}

void StarsFacadeTest::transactionsOmitMissingTitleAndDate()
{
    StarsFacade facade;

    facade.setTransactions({QStringLiteral("5")}, {QStringLiteral("0")},
                           {QStringLiteral("10")}, {QString()}, {QStringLiteral("1")});

    QCOMPARE(facade.transactions().size(), 1);
    QCOMPARE(facade.transactions().at(0), QStringLiteral("+10 ") + starGlyph());
}

void StarsFacadeTest::transactionsDropInvalidIds()
{
    StarsFacade facade;

    facade.setTransactions({QStringLiteral("0"), QStringLiteral("abc"), QStringLiteral("7")},
                           {QStringLiteral("1714521600"), QStringLiteral("1714521600"),
                            QStringLiteral("1714521600")},
                           {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3")},
                           {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")},
                           {QStringLiteral("true"), QStringLiteral("true"),
                            QStringLiteral("true")});

    QCOMPARE(facade.transactions().size(), 1);
    QCOMPARE(facade.transactions().at(0),
             QStringLiteral("+3 ") + starGlyph() + dot() + QStringLiteral("c")
                 + dot() + QStringLiteral("2024-05-01"));
}

void StarsFacadeTest::transactionsReplaceAndClear()
{
    StarsFacade facade;

    facade.setTransactions({QStringLiteral("1")}, {QStringLiteral("1714521600")},
                           {QStringLiteral("10")}, {QStringLiteral("First")},
                           {QStringLiteral("true")});
    QCOMPARE(facade.transactions().size(), 1);

    facade.setTransactions({QStringLiteral("2"), QStringLiteral("3")},
                           {QStringLiteral("1714521600"), QStringLiteral("1714608000")},
                           {QStringLiteral("20"), QStringLiteral("30")},
                           {QStringLiteral("Second"), QStringLiteral("Third")},
                           {QStringLiteral("false"), QStringLiteral("false")});
    QCOMPARE(facade.transactions().size(), 2);
    QVERIFY(facade.transactions().at(0).startsWith(QStringLiteral("-30 ")));

    facade.setTransactions({}, {}, {}, {}, {});
    QVERIFY(facade.transactions().isEmpty());
}

void StarsFacadeTest::changedCoversEveryModelUpdate()
{
    StarsFacade facade;
    QSignalSpy spy(&facade, &StarsFacade::changed);

    facade.setBalance(100);
    QCOMPARE(spy.count(), 1);

    loadSamplePackages(facade);
    QCOMPARE(spy.count(), 2);

    facade.select(0);
    QCOMPARE(spy.count(), 3);

    facade.setTransactions({QStringLiteral("1")}, {QStringLiteral("1714521600")},
                           {QStringLiteral("10")}, {QStringLiteral("Stars")},
                           {QStringLiteral("true")});
    QCOMPARE(spy.count(), 4);

    // A redundant reload emits nothing, so a binding does not repaint on it.
    loadSamplePackages(facade);
    facade.select(0);
    QCOMPARE(spy.count(), 4);
}

QTEST_GUILESS_MAIN(StarsFacadeTest)

#include "tst_stars_facade.moc"
