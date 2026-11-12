#include "telegram/StarBalanceModel.h"

#include <QSignalSpy>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

/// The star glyph as the model builds it: a code point, not a source literal, so
/// the test does not depend on the file's charset either.
QString star()
{
    return QString(QChar(0x2B50));
}

} // namespace

class StarBalanceModelTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultsToZero();
    void setBalanceStoresTheValue();
    void setBalanceClampsNegativeToZero();
    void setBalanceEmitsChangedOnlyForRealChange();
    void setBalanceFromPositiveToNegativeEmitsOnce();
    void canAffordRejectsNegativeAmounts();
    void canAffordAcceptsTheWholeBalance();
    void canAffordRejectsMoreThanTheBalance();
    void addCreditsTheBalance();
    void addDebitsTheBalance();
    void addClampsAtZero();
    void addZeroIsSilent();
    void addEmitsChangedOnlyForRealChange();
    void addSaturatesInsteadOfOverflowing();
    void displayTextFormatsZero();
    void displayTextGroupsThousandsWithSpaces();
    void displayTextKeepsShortNumbersUnGrouped();
    void clearResetsTheBalance();
    void clearIsSilentWhenAlreadyZero();
};

void StarBalanceModelTest::defaultsToZero()
{
    const StarBalanceModel model;

    QCOMPARE(model.balance(), qint64(0));
    QCOMPARE(model.displayText(), QStringLiteral("0 ") + star());
    QVERIFY2(model.canAfford(0), "an empty balance covers a free price");
    QVERIFY2(!model.canAfford(1), "an empty balance covers nothing else");
}

void StarBalanceModelTest::setBalanceStoresTheValue()
{
    StarBalanceModel model;

    model.setBalance(1234);

    QCOMPARE(model.balance(), qint64(1234));
    QCOMPARE(model.displayText(), QStringLiteral("1 234 ") + star());
}

void StarBalanceModelTest::setBalanceClampsNegativeToZero()
{
    StarBalanceModel model;
    model.setBalance(500);

    model.setBalance(-1);

    QCOMPARE(model.balance(), qint64(0));
    QVERIFY2(!model.canAfford(1), "a clamped balance cannot pay");
}

void StarBalanceModelTest::setBalanceEmitsChangedOnlyForRealChange()
{
    StarBalanceModel model;
    QSignalSpy spy(&model, &StarBalanceModel::changed);

    model.setBalance(0);
    QCOMPARE(spy.count(), 0);

    model.setBalance(250);
    QCOMPARE(spy.count(), 1);

    model.setBalance(250);
    QCOMPARE(spy.count(), 1);

    model.setBalance(251);
    QCOMPARE(spy.count(), 2);
}

void StarBalanceModelTest::setBalanceFromPositiveToNegativeEmitsOnce()
{
    StarBalanceModel model;
    model.setBalance(40);
    QSignalSpy spy(&model, &StarBalanceModel::changed);

    // -7 clamps to 0, which differs from 40: one change, not two.
    model.setBalance(-7);

    QCOMPARE(model.balance(), qint64(0));
    QCOMPARE(spy.count(), 1);

    // Already clamped at 0, so a second negative value is not a change.
    model.setBalance(-8);
    QCOMPARE(spy.count(), 1);
}

void StarBalanceModelTest::canAffordRejectsNegativeAmounts()
{
    StarBalanceModel model;
    model.setBalance(1000);

    QVERIFY2(!model.canAfford(-1), "a negative price is not a price");
    QVERIFY2(!model.canAfford(std::numeric_limits<qint64>::min()),
             "the most negative amount is rejected without overflowing");
}

void StarBalanceModelTest::canAffordAcceptsTheWholeBalance()
{
    StarBalanceModel model;
    model.setBalance(250);

    QVERIFY(model.canAfford(250));
    QVERIFY(model.canAfford(0));
}

void StarBalanceModelTest::canAffordRejectsMoreThanTheBalance()
{
    StarBalanceModel model;
    model.setBalance(250);

    QVERIFY(!model.canAfford(251));
    QVERIFY(!model.canAfford(std::numeric_limits<qint64>::max()));
}

void StarBalanceModelTest::addCreditsTheBalance()
{
    StarBalanceModel model;
    model.setBalance(100);
    QSignalSpy spy(&model, &StarBalanceModel::changed);

    model.add(25);

    QCOMPARE(model.balance(), qint64(125));
    QCOMPARE(spy.count(), 1);
}

void StarBalanceModelTest::addDebitsTheBalance()
{
    StarBalanceModel model;
    model.setBalance(100);
    QSignalSpy spy(&model, &StarBalanceModel::changed);

    model.add(-40);

    QCOMPARE(model.balance(), qint64(60));
    QCOMPARE(spy.count(), 1);
}

void StarBalanceModelTest::addClampsAtZero()
{
    StarBalanceModel model;
    model.setBalance(30);
    QSignalSpy spy(&model, &StarBalanceModel::changed);

    model.add(-30);
    QCOMPARE(model.balance(), qint64(0));
    QCOMPARE(spy.count(), 1);

    // Spending more than is there stops at 0 instead of going negative.
    model.setBalance(5);
    model.add(-1000);
    QCOMPARE(model.balance(), qint64(0));

    model.add(std::numeric_limits<qint64>::min());
    QCOMPARE(model.balance(), qint64(0));
}

void StarBalanceModelTest::addZeroIsSilent()
{
    StarBalanceModel model;
    model.setBalance(10);
    QSignalSpy spy(&model, &StarBalanceModel::changed);

    model.add(0);

    QCOMPARE(model.balance(), qint64(10));
    QCOMPARE(spy.count(), 0);
}

void StarBalanceModelTest::addEmitsChangedOnlyForRealChange()
{
    StarBalanceModel model;
    model.setBalance(10);
    QSignalSpy spy(&model, &StarBalanceModel::changed);

    // A debit larger than the balance lands on 0, which is a change from 10.
    model.add(-50);
    QCOMPARE(spy.count(), 1);

    // Still 0: further debits change nothing and stay silent.
    model.add(-1);
    QCOMPARE(model.balance(), qint64(0));
    QCOMPARE(spy.count(), 1);

    // A credit of 0 is not a change either.
    model.add(0);
    QCOMPARE(spy.count(), 1);
}

void StarBalanceModelTest::addSaturatesInsteadOfOverflowing()
{
    const qint64 max = std::numeric_limits<qint64>::max();

    StarBalanceModel model;
    model.setBalance(max - 5);

    model.add(10);

    QCOMPARE(model.balance(), max);
    QVERIFY2(model.balance() > 0, "credit overflow must not wrap negative");

    // At the ceiling, another credit is not a change.
    QSignalSpy spy(&model, &StarBalanceModel::changed);
    model.add(1);
    QCOMPARE(model.balance(), max);
    QCOMPARE(spy.count(), 0);
}

void StarBalanceModelTest::displayTextFormatsZero()
{
    const StarBalanceModel model;

    QCOMPARE(model.displayText(), QStringLiteral("0 ") + star());
}

void StarBalanceModelTest::displayTextGroupsThousandsWithSpaces()
{
    StarBalanceModel model;

    model.setBalance(1000);
    QCOMPARE(model.displayText(), QStringLiteral("1 000 ") + star());

    model.setBalance(1234);
    QCOMPARE(model.displayText(), QStringLiteral("1 234 ") + star());

    model.setBalance(12345);
    QCOMPARE(model.displayText(), QStringLiteral("12 345 ") + star());

    model.setBalance(123456);
    QCOMPARE(model.displayText(), QStringLiteral("123 456 ") + star());

    model.setBalance(1234567);
    QCOMPARE(model.displayText(), QStringLiteral("1 234 567 ") + star());

    model.setBalance(std::numeric_limits<qint64>::max());
    QCOMPARE(model.displayText(),
             QStringLiteral("9 223 372 036 854 775 807 ") + star());
}

void StarBalanceModelTest::displayTextKeepsShortNumbersUnGrouped()
{
    StarBalanceModel model;

    model.setBalance(1);
    QCOMPARE(model.displayText(), QStringLiteral("1 ") + star());

    model.setBalance(999);
    QCOMPARE(model.displayText(), QStringLiteral("999 ") + star());
}

void StarBalanceModelTest::clearResetsTheBalance()
{
    StarBalanceModel model;
    model.setBalance(4200);
    QSignalSpy spy(&model, &StarBalanceModel::changed);

    model.clear();

    QCOMPARE(model.balance(), qint64(0));
    QCOMPARE(model.displayText(), QStringLiteral("0 ") + star());
    QCOMPARE(spy.count(), 1);
}

void StarBalanceModelTest::clearIsSilentWhenAlreadyZero()
{
    StarBalanceModel model;
    QSignalSpy spy(&model, &StarBalanceModel::changed);

    model.clear();

    QCOMPARE(model.balance(), qint64(0));
    QCOMPARE(spy.count(), 0);
}

QTEST_GUILESS_MAIN(StarBalanceModelTest)
#include "tst_star_balance_model.moc"
