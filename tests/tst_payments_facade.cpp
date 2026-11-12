#include "ui/PaymentsFacade.h"

#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

using namespace beygram::ui;

namespace {

// A checkout with one invoice and a usable name and email, so each test only
// has to change the piece it is about.
void loadPayableInvoice(PaymentsFacade& facade)
{
    facade.setInvoice(QStringLiteral("42"), QStringLiteral("7"),
                      QStringLiteral("Coffee"), 123456, QStringLiteral("usd"));
    facade.setName(QStringLiteral("Bob"));
    facade.setEmail(QStringLiteral("bob@example.com"));
}

void loadMethods(PaymentsFacade& facade)
{
    facade.setMethods({QStringLiteral("card"), QStringLiteral("stars")},
                      {QStringLiteral("Card"), QStringLiteral("Telegram Stars")});
}

} // namespace

class PaymentsFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsInactive();
    void setInvoiceActivatesCheckout();
    void setInvoiceNormalizesTitleAndCurrency();
    void setInvoiceRejectsMalformedIds();
    void setInvoiceKeepsBuyerFields();
    void totalTextFormatsMinorUnits();
    void totalTextOmitsEmptyCurrency();
    void orderFieldsRoundTrip();
    void orderFieldsAreTrimmed();
    void canPayRequiresActiveInvoice();
    void canPayRequiresName();
    void canPayRequiresContact();
    void canPayAcceptsPhoneOnly();
    void canPayWithoutMethodList();
    void canPayRequiresSelectedMethodWhenOffered();
    void methodTitlesFollowGivenOrder();
    void methodTitlesFallBackToId();
    void setMethodsDropsEmptyIdsAndDuplicates();
    void setMethodsKeepsSelection();
    void setMethodsClearsVanishedSelection();
    void selectMethodMovesSelection();
    void selectMethodIgnoresOutOfRange();
    void selectMinusOneClearsSelection();
    void payEmitsPayload();
    void payWithEmptyMethodListEmitsEmptyMethodId();
    void payRefusedWhenInvalid();
    void payDoesNotResetCheckout();
    void resetClearsEverything();
    void resetEmitsChangedOnce();
    void resetOnEmptyFacadeIsSilent();
    void changedOnlyOnRealChanges();
};

void PaymentsFacadeTest::startsInactive()
{
    const PaymentsFacade facade;

    QVERIFY(!facade.active());
    QCOMPARE(facade.title(), QString());
    QCOMPARE(facade.totalText(), QStringLiteral("0.00"));
    QCOMPARE(facade.name(), QString());
    QCOMPARE(facade.email(), QString());
    QCOMPARE(facade.phone(), QString());
    QVERIFY(facade.methodTitles().isEmpty());
    QCOMPARE(facade.methodIndex(), -1);
    QVERIFY(!facade.canPay());
}

void PaymentsFacadeTest::setInvoiceActivatesCheckout()
{
    PaymentsFacade facade;

    facade.setInvoice(QStringLiteral("42"), QStringLiteral("7"),
                      QStringLiteral("Coffee"), 123456, QStringLiteral("USD"));

    QVERIFY(facade.active());
    QCOMPARE(facade.title(), QStringLiteral("Coffee"));
    QCOMPARE(facade.totalText(), QStringLiteral("1234.56 USD"));
}

void PaymentsFacadeTest::setInvoiceNormalizesTitleAndCurrency()
{
    PaymentsFacade facade;

    facade.setInvoice(QStringLiteral("42"), QStringLiteral("7"),
                      QStringLiteral("  Coffee  "), 100, QStringLiteral(" usd "));

    QCOMPARE(facade.title(), QStringLiteral("Coffee"));
    QCOMPARE(facade.totalText(), QStringLiteral("1.00 USD"));
}

void PaymentsFacadeTest::setInvoiceRejectsMalformedIds()
{
    PaymentsFacade facade;
    loadPayableInvoice(facade);

    facade.setInvoice(QStringLiteral("not-a-chat"), QStringLiteral("7"),
                      QStringLiteral("Coffee"), 100, QStringLiteral("USD"));

    QVERIFY(!facade.active());
    QCOMPARE(facade.title(), QString());
    QVERIFY(!facade.canPay());
}

void PaymentsFacadeTest::setInvoiceKeepsBuyerFields()
{
    PaymentsFacade facade;
    loadPayableInvoice(facade);

    facade.setInvoice(QStringLiteral("43"), QStringLiteral("8"),
                      QStringLiteral("Tea"), 500, QStringLiteral("USD"));

    QCOMPARE(facade.name(), QStringLiteral("Bob"));
    QCOMPARE(facade.email(), QStringLiteral("bob@example.com"));
}

void PaymentsFacadeTest::totalTextFormatsMinorUnits()
{
    PaymentsFacade facade;

    facade.setInvoice(QStringLiteral("42"), QStringLiteral("7"),
                      QStringLiteral("Coffee"), 5, QStringLiteral("EUR"));
    QCOMPARE(facade.totalText(), QStringLiteral("0.05 EUR"));

    facade.setInvoice(QStringLiteral("42"), QStringLiteral("7"),
                      QStringLiteral("Coffee"), 100000000, QStringLiteral("EUR"));
    QCOMPARE(facade.totalText(), QStringLiteral("1000000.00 EUR"));
}

void PaymentsFacadeTest::totalTextOmitsEmptyCurrency()
{
    PaymentsFacade facade;

    facade.setInvoice(QStringLiteral("42"), QStringLiteral("7"),
                      QStringLiteral("Coffee"), 250, QString());

    QCOMPARE(facade.totalText(), QStringLiteral("2.50"));
}

void PaymentsFacadeTest::orderFieldsRoundTrip()
{
    PaymentsFacade facade;

    facade.setName(QStringLiteral("Bob"));
    facade.setEmail(QStringLiteral("bob@example.com"));
    facade.setPhone(QStringLiteral("+1 555 0100"));

    QCOMPARE(facade.name(), QStringLiteral("Bob"));
    QCOMPARE(facade.email(), QStringLiteral("bob@example.com"));
    QCOMPARE(facade.phone(), QStringLiteral("+1 555 0100"));
}

void PaymentsFacadeTest::orderFieldsAreTrimmed()
{
    PaymentsFacade facade;

    facade.setName(QStringLiteral("  Bob  "));
    facade.setEmail(QStringLiteral("  bob@example.com  "));
    facade.setPhone(QStringLiteral("   "));

    QCOMPARE(facade.name(), QStringLiteral("Bob"));
    QCOMPARE(facade.email(), QStringLiteral("bob@example.com"));
    QCOMPARE(facade.phone(), QString());
}

void PaymentsFacadeTest::canPayRequiresActiveInvoice()
{
    PaymentsFacade facade;
    facade.setName(QStringLiteral("Bob"));
    facade.setEmail(QStringLiteral("bob@example.com"));

    QVERIFY(!facade.canPay());
}

void PaymentsFacadeTest::canPayRequiresName()
{
    PaymentsFacade facade;
    facade.setInvoice(QStringLiteral("42"), QStringLiteral("7"),
                      QStringLiteral("Coffee"), 100, QStringLiteral("USD"));
    facade.setEmail(QStringLiteral("bob@example.com"));

    QVERIFY(!facade.canPay());

    facade.setName(QStringLiteral("Bob"));
    QVERIFY(facade.canPay());
}

void PaymentsFacadeTest::canPayRequiresContact()
{
    PaymentsFacade facade;
    facade.setInvoice(QStringLiteral("42"), QStringLiteral("7"),
                      QStringLiteral("Coffee"), 100, QStringLiteral("USD"));
    facade.setName(QStringLiteral("Bob"));

    QVERIFY(!facade.canPay());

    facade.setEmail(QStringLiteral("bob@example.com"));
    QVERIFY(facade.canPay());
}

void PaymentsFacadeTest::canPayAcceptsPhoneOnly()
{
    PaymentsFacade facade;
    facade.setInvoice(QStringLiteral("42"), QStringLiteral("7"),
                      QStringLiteral("Coffee"), 100, QStringLiteral("USD"));
    facade.setName(QStringLiteral("Bob"));
    facade.setPhone(QStringLiteral("+1 (555) 010-0"));

    QVERIFY(facade.canPay());
}

void PaymentsFacadeTest::canPayWithoutMethodList()
{
    PaymentsFacade facade;
    loadPayableInvoice(facade);

    QVERIFY(facade.methodTitles().isEmpty());
    QVERIFY(facade.canPay());
}

void PaymentsFacadeTest::canPayRequiresSelectedMethodWhenOffered()
{
    PaymentsFacade facade;
    loadPayableInvoice(facade);
    loadMethods(facade);

    QVERIFY(!facade.canPay());

    facade.selectMethod(1);
    QVERIFY(facade.canPay());
}

void PaymentsFacadeTest::methodTitlesFollowGivenOrder()
{
    PaymentsFacade facade;

    loadMethods(facade);

    QCOMPARE(facade.methodTitles(),
             QStringList({QStringLiteral("Card"), QStringLiteral("Telegram Stars")}));
}

void PaymentsFacadeTest::methodTitlesFallBackToId()
{
    PaymentsFacade facade;

    facade.setMethods({QStringLiteral("card"), QStringLiteral("stars")},
                      {QString(), QStringLiteral("Telegram Stars")});

    QCOMPARE(facade.methodTitles(),
             QStringList({QStringLiteral("card"), QStringLiteral("Telegram Stars")}));
}

void PaymentsFacadeTest::setMethodsDropsEmptyIdsAndDuplicates()
{
    PaymentsFacade facade;

    facade.setMethods({QStringLiteral("card"), QString(), QStringLiteral("card"),
                       QStringLiteral("stars")},
                      {QStringLiteral("Card"), QStringLiteral("Ghost"),
                       QStringLiteral("Second card"), QStringLiteral("Stars")});

    QCOMPARE(facade.methodTitles(),
             QStringList({QStringLiteral("Card"), QStringLiteral("Stars")}));
}

void PaymentsFacadeTest::setMethodsKeepsSelection()
{
    PaymentsFacade facade;
    loadPayableInvoice(facade);
    loadMethods(facade);
    facade.selectMethod(1);
    QCOMPARE(facade.methodIndex(), 1);

    facade.setMethods({QStringLiteral("stars"), QStringLiteral("card")},
                      {QStringLiteral("Stars"), QStringLiteral("Card")});

    QCOMPARE(facade.methodIndex(), 0);
    QVERIFY(facade.canPay());
}

void PaymentsFacadeTest::setMethodsClearsVanishedSelection()
{
    PaymentsFacade facade;
    loadPayableInvoice(facade);
    loadMethods(facade);
    facade.selectMethod(1);

    facade.setMethods({QStringLiteral("card")}, {QStringLiteral("Card")});

    QCOMPARE(facade.methodIndex(), -1);
    QVERIFY(!facade.canPay());
}

void PaymentsFacadeTest::selectMethodMovesSelection()
{
    PaymentsFacade facade;
    loadMethods(facade);

    QCOMPARE(facade.methodIndex(), -1);

    facade.selectMethod(0);
    QCOMPARE(facade.methodIndex(), 0);

    facade.selectMethod(1);
    QCOMPARE(facade.methodIndex(), 1);
}

void PaymentsFacadeTest::selectMethodIgnoresOutOfRange()
{
    PaymentsFacade facade;
    loadMethods(facade);
    facade.selectMethod(0);

    facade.selectMethod(2);
    facade.selectMethod(99);
    facade.selectMethod(-2);
    QCOMPARE(facade.methodIndex(), 0);
}

void PaymentsFacadeTest::selectMinusOneClearsSelection()
{
    PaymentsFacade facade;
    loadPayableInvoice(facade);
    loadMethods(facade);
    facade.selectMethod(0);
    QVERIFY(facade.canPay());

    facade.selectMethod(-1);

    QCOMPARE(facade.methodIndex(), -1);
    QVERIFY(!facade.canPay());
}

void PaymentsFacadeTest::payEmitsPayload()
{
    PaymentsFacade facade;
    loadPayableInvoice(facade);
    loadMethods(facade);
    facade.selectMethod(1);
    facade.setPhone(QStringLiteral("+1 555 0100"));

    QSignalSpy spy(&facade, &PaymentsFacade::payRequested);
    QVERIFY(facade.pay());

    QCOMPARE(spy.count(), 1);
    const QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QStringLiteral("42"));
    QCOMPARE(arguments.at(1).toString(), QStringLiteral("7"));
    QCOMPARE(arguments.at(2).toString(), QStringLiteral("Bob"));
    QCOMPARE(arguments.at(3).toString(), QStringLiteral("bob@example.com"));
    QCOMPARE(arguments.at(4).toString(), QStringLiteral("+1 555 0100"));
    QCOMPARE(arguments.at(5).toString(), QStringLiteral("stars"));
}

void PaymentsFacadeTest::payWithEmptyMethodListEmitsEmptyMethodId()
{
    PaymentsFacade facade;
    loadPayableInvoice(facade);

    QSignalSpy spy(&facade, &PaymentsFacade::payRequested);
    QVERIFY(facade.pay());

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(5).toString(), QString());
}

void PaymentsFacadeTest::payRefusedWhenInvalid()
{
    PaymentsFacade facade;
    facade.setInvoice(QStringLiteral("42"), QStringLiteral("7"),
                      QStringLiteral("Coffee"), 100, QStringLiteral("USD"));

    QSignalSpy spy(&facade, &PaymentsFacade::payRequested);
    QVERIFY(!facade.pay());
    QCOMPARE(spy.count(), 0);
}

void PaymentsFacadeTest::payDoesNotResetCheckout()
{
    PaymentsFacade facade;
    loadPayableInvoice(facade);

    QVERIFY(facade.pay());

    QVERIFY(facade.active());
    QCOMPARE(facade.name(), QStringLiteral("Bob"));
    QVERIFY(facade.canPay());
}

void PaymentsFacadeTest::resetClearsEverything()
{
    PaymentsFacade facade;
    loadPayableInvoice(facade);
    loadMethods(facade);
    facade.selectMethod(0);

    facade.reset();

    QVERIFY(!facade.active());
    QCOMPARE(facade.title(), QString());
    QCOMPARE(facade.totalText(), QStringLiteral("0.00"));
    QCOMPARE(facade.name(), QString());
    QCOMPARE(facade.email(), QString());
    QCOMPARE(facade.phone(), QString());
    QVERIFY(facade.methodTitles().isEmpty());
    QCOMPARE(facade.methodIndex(), -1);
    QVERIFY(!facade.canPay());
}

void PaymentsFacadeTest::resetEmitsChangedOnce()
{
    PaymentsFacade facade;
    loadPayableInvoice(facade);
    loadMethods(facade);
    facade.selectMethod(0);

    QSignalSpy spy(&facade, &PaymentsFacade::changed);
    facade.reset();

    QCOMPARE(spy.count(), 1);
}

void PaymentsFacadeTest::resetOnEmptyFacadeIsSilent()
{
    PaymentsFacade facade;

    QSignalSpy spy(&facade, &PaymentsFacade::changed);
    facade.reset();

    QCOMPARE(spy.count(), 0);
}

void PaymentsFacadeTest::changedOnlyOnRealChanges()
{
    PaymentsFacade facade;

    QSignalSpy spy(&facade, &PaymentsFacade::changed);

    facade.setName(QStringLiteral("Bob"));
    QCOMPARE(spy.count(), 1);

    facade.setName(QStringLiteral("  Bob  "));
    QCOMPARE(spy.count(), 1);

    facade.setInvoice(QStringLiteral("42"), QStringLiteral("7"),
                      QStringLiteral("Coffee"), 100, QStringLiteral("USD"));
    QCOMPARE(spy.count(), 2);

    facade.setInvoice(QStringLiteral("42"), QStringLiteral("7"),
                      QStringLiteral("Coffee"), 100, QStringLiteral("USD"));
    QCOMPARE(spy.count(), 2);

    loadMethods(facade);
    QCOMPARE(spy.count(), 3);

    loadMethods(facade);
    QCOMPARE(spy.count(), 3);

    facade.selectMethod(0);
    QCOMPARE(spy.count(), 4);

    facade.selectMethod(0);
    QCOMPARE(spy.count(), 4);
}

QTEST_GUILESS_MAIN(PaymentsFacadeTest)

#include "tst_payments_facade.moc"
