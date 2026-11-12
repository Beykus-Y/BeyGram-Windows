#include "telegram/PaymentCheckoutModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

// A checkout with one invoice and a usable name and email, so each test only
// has to change the field it is about.
void loadUsableCheckout(PaymentCheckoutModel& model)
{
    model.setInvoice(42, 7, QStringLiteral("Coffee"), 123456, QStringLiteral("usd"));
    model.setName(QStringLiteral("Bob"));
    model.setEmail(QStringLiteral("bob@example.com"));
}

} // namespace

class PaymentCheckoutModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsInactive();
    void setInvoiceActivatesCheckout();
    void setInvoiceNormalizesTitleAndCurrency();
    void setInvoiceRejectsNonPositiveChatId();
    void setInvoiceRejectsNonPositiveMessageId();
    void setInvoiceRejectsNegativeAmount();
    void invalidInvoiceClearsWholeCheckout();
    void invalidInvoiceOnEmptyCheckoutIsSilent();
    void setInvoiceKeepsBuyerDetails();
    void setInvoiceIgnoresIdenticalInvoice();
    void setInvoiceRestartsAfterInvalidInvoice();
    void blankCurrencyIsDropped();
    void nameIsTrimmed();
    void nameOfOnlySpacesCountsAsEmpty();
    void emailAndPhoneAreTrimmed();
    void saveCredentialsRoundTrip();
    void saveCredentialsEmitsOnlyOnRealChange();
    void validRequiresActiveCheckout();
    void validRequiresName();
    void validAcceptsEmailWithoutPhone();
    void validAcceptsPhoneWithoutEmail();
    void validAcceptsPunctuatedPhone();
    void validRejectsMalformedEmail();
    void validRejectsShortPhone();
    void totalTextFormatsMinorUnits();
    void totalTextAppendsCurrency();
    void totalTextOmitsEmptyCurrency();
    void totalTextForLargeAmount();
    void resetClearsEverything();
    void resetOnEmptyCheckoutIsSilent();
    void changedOnlyOnRealChanges();
};

void PaymentCheckoutModelTest::startsInactive()
{
    const PaymentCheckoutModel model;

    QVERIFY(!model.active());
    QCOMPARE(model.chatId(), 0);
    QCOMPARE(model.messageId(), 0);
    QCOMPARE(model.title(), QString());
    QCOMPARE(model.totalAmount(), 0);
    QCOMPARE(model.currency(), QString());
    QCOMPARE(model.name(), QString());
    QCOMPARE(model.email(), QString());
    QCOMPARE(model.phone(), QString());
    QVERIFY(!model.saveCredentials());
    QVERIFY(!model.valid());
    QCOMPARE(model.totalText(), QStringLiteral("0.00"));
}

void PaymentCheckoutModelTest::setInvoiceActivatesCheckout()
{
    PaymentCheckoutModel model;

    model.setInvoice(42, 7, QStringLiteral("Coffee"), 123456, QStringLiteral("USD"));

    QVERIFY(model.active());
    QCOMPARE(model.chatId(), 42);
    QCOMPARE(model.messageId(), 7);
    QCOMPARE(model.title(), QStringLiteral("Coffee"));
    QCOMPARE(model.totalAmount(), 123456);
    QCOMPARE(model.currency(), QStringLiteral("USD"));
}

void PaymentCheckoutModelTest::setInvoiceNormalizesTitleAndCurrency()
{
    PaymentCheckoutModel model;

    model.setInvoice(1, 2, QStringLiteral("  Coffee beans  "), 100, QStringLiteral("  eur "));

    QCOMPARE(model.title(), QStringLiteral("Coffee beans"));
    QCOMPARE(model.currency(), QStringLiteral("EUR"));
}

void PaymentCheckoutModelTest::setInvoiceRejectsNonPositiveChatId()
{
    PaymentCheckoutModel model;

    model.setInvoice(0, 7, QStringLiteral("Coffee"), 100, QStringLiteral("USD"));
    QVERIFY(!model.active());
    QCOMPARE(model.chatId(), 0);
    QCOMPARE(model.messageId(), 0);
    QCOMPARE(model.title(), QString());

    model.setInvoice(-5, 7, QStringLiteral("Coffee"), 100, QStringLiteral("USD"));
    QVERIFY(!model.active());
    QCOMPARE(model.chatId(), 0);
}

void PaymentCheckoutModelTest::setInvoiceRejectsNonPositiveMessageId()
{
    PaymentCheckoutModel model;

    model.setInvoice(42, 0, QStringLiteral("Coffee"), 100, QStringLiteral("USD"));
    QVERIFY(!model.active());
    QCOMPARE(model.chatId(), 0);
    QCOMPARE(model.messageId(), 0);

    model.setInvoice(42, -1, QStringLiteral("Coffee"), 100, QStringLiteral("USD"));
    QVERIFY(!model.active());
    QCOMPARE(model.messageId(), 0);
}

void PaymentCheckoutModelTest::setInvoiceRejectsNegativeAmount()
{
    PaymentCheckoutModel model;

    model.setInvoice(42, 7, QStringLiteral("Coffee"), -1, QStringLiteral("USD"));

    QVERIFY(!model.active());
    QCOMPARE(model.totalAmount(), 0);
    QCOMPARE(model.title(), QString());
}

void PaymentCheckoutModelTest::invalidInvoiceClearsWholeCheckout()
{
    PaymentCheckoutModel model;
    loadUsableCheckout(model);
    model.setPhone(QStringLiteral("+7 999 123-45-67"));
    model.setSaveCredentials(true);
    QSignalSpy spy(&model, &PaymentCheckoutModel::changed);

    model.setInvoice(0, 7, QStringLiteral("Coffee"), 123456, QStringLiteral("usd"));

    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.active());
    QCOMPARE(model.chatId(), 0);
    QCOMPARE(model.messageId(), 0);
    QCOMPARE(model.title(), QString());
    QCOMPARE(model.totalAmount(), 0);
    QCOMPARE(model.currency(), QString());
    QCOMPARE(model.name(), QString());
    QCOMPARE(model.email(), QString());
    QCOMPARE(model.phone(), QString());
    QVERIFY(!model.saveCredentials());
    QVERIFY(!model.valid());
}

void PaymentCheckoutModelTest::invalidInvoiceOnEmptyCheckoutIsSilent()
{
    PaymentCheckoutModel model;
    QSignalSpy spy(&model, &PaymentCheckoutModel::changed);

    model.setInvoice(0, 0, QString(), -1, QString());

    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.active());
}

void PaymentCheckoutModelTest::setInvoiceKeepsBuyerDetails()
{
    PaymentCheckoutModel model;
    loadUsableCheckout(model);
    model.setPhone(QStringLiteral("+7 999 123-45-67"));
    model.setSaveCredentials(true);

    model.setInvoice(43, 8, QStringLiteral("Tea"), 500, QStringLiteral("eur"));

    QCOMPARE(model.chatId(), 43);
    QCOMPARE(model.messageId(), 8);
    QCOMPARE(model.title(), QStringLiteral("Tea"));
    QCOMPARE(model.totalAmount(), 500);
    QCOMPARE(model.currency(), QStringLiteral("EUR"));
    QCOMPARE(model.name(), QStringLiteral("Bob"));
    QCOMPARE(model.email(), QStringLiteral("bob@example.com"));
    QCOMPARE(model.phone(), QStringLiteral("+7 999 123-45-67"));
    QVERIFY(model.saveCredentials());
}

void PaymentCheckoutModelTest::setInvoiceIgnoresIdenticalInvoice()
{
    PaymentCheckoutModel model;
    model.setInvoice(42, 7, QStringLiteral("Coffee"), 123456, QStringLiteral("USD"));
    QSignalSpy spy(&model, &PaymentCheckoutModel::changed);

    model.setInvoice(42, 7, QStringLiteral("  Coffee  "), 123456, QStringLiteral("usd"));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.title(), QStringLiteral("Coffee"));
    QCOMPARE(model.currency(), QStringLiteral("USD"));
}

void PaymentCheckoutModelTest::setInvoiceRestartsAfterInvalidInvoice()
{
    PaymentCheckoutModel model;
    loadUsableCheckout(model);
    model.setInvoice(0, 0, QString(), -1, QString());
    QVERIFY(!model.active());

    model.setInvoice(44, 9, QStringLiteral("Tea"), 250, QStringLiteral("gbp"));

    QVERIFY(model.active());
    QCOMPARE(model.chatId(), 44);
    QCOMPARE(model.messageId(), 9);
    QCOMPARE(model.currency(), QStringLiteral("GBP"));
    QVERIFY(!model.valid());
}

void PaymentCheckoutModelTest::blankCurrencyIsDropped()
{
    PaymentCheckoutModel model;

    model.setInvoice(42, 7, QStringLiteral("Coffee"), 123456, QStringLiteral("   "));

    QVERIFY(model.active());
    QCOMPARE(model.currency(), QString());
    QCOMPARE(model.totalText(), QStringLiteral("1234.56"));
}

void PaymentCheckoutModelTest::nameIsTrimmed()
{
    PaymentCheckoutModel model;

    model.setName(QStringLiteral("  Bob  "));

    QCOMPARE(model.name(), QStringLiteral("Bob"));
}

void PaymentCheckoutModelTest::nameOfOnlySpacesCountsAsEmpty()
{
    PaymentCheckoutModel model;
    loadUsableCheckout(model);
    QVERIFY(model.valid());

    model.setName(QStringLiteral("   "));

    QCOMPARE(model.name(), QString());
    QVERIFY(!model.valid());
}

void PaymentCheckoutModelTest::emailAndPhoneAreTrimmed()
{
    PaymentCheckoutModel model;

    model.setEmail(QStringLiteral("  bob@example.com "));
    model.setPhone(QStringLiteral("  +7 999 123-45-67  "));

    QCOMPARE(model.email(), QStringLiteral("bob@example.com"));
    QCOMPARE(model.phone(), QStringLiteral("+7 999 123-45-67"));
}

void PaymentCheckoutModelTest::saveCredentialsRoundTrip()
{
    PaymentCheckoutModel model;

    QVERIFY(!model.saveCredentials());
    model.setSaveCredentials(true);
    QVERIFY(model.saveCredentials());
    model.setSaveCredentials(false);
    QVERIFY(!model.saveCredentials());
}

void PaymentCheckoutModelTest::saveCredentialsEmitsOnlyOnRealChange()
{
    PaymentCheckoutModel model;
    QSignalSpy spy(&model, &PaymentCheckoutModel::changed);

    model.setSaveCredentials(true);
    QCOMPARE(spy.count(), 1);

    model.setSaveCredentials(true);
    QCOMPARE(spy.count(), 1);

    model.setSaveCredentials(false);
    QCOMPARE(spy.count(), 2);

    model.setSaveCredentials(false);
    QCOMPARE(spy.count(), 2);
}

void PaymentCheckoutModelTest::validRequiresActiveCheckout()
{
    PaymentCheckoutModel model;
    model.setName(QStringLiteral("Bob"));
    model.setEmail(QStringLiteral("bob@example.com"));

    QVERIFY(!model.valid());

    model.setInvoice(42, 7, QStringLiteral("Coffee"), 123456, QStringLiteral("USD"));
    QVERIFY(model.valid());
}

void PaymentCheckoutModelTest::validRequiresName()
{
    PaymentCheckoutModel model;
    model.setInvoice(42, 7, QStringLiteral("Coffee"), 123456, QStringLiteral("USD"));
    model.setEmail(QStringLiteral("bob@example.com"));

    QVERIFY(!model.valid());

    model.setName(QStringLiteral("Bob"));
    QVERIFY(model.valid());
}

void PaymentCheckoutModelTest::validAcceptsEmailWithoutPhone()
{
    PaymentCheckoutModel model;
    loadUsableCheckout(model);

    QCOMPARE(model.phone(), QString());
    QVERIFY(model.valid());
}

void PaymentCheckoutModelTest::validAcceptsPhoneWithoutEmail()
{
    PaymentCheckoutModel model;
    model.setInvoice(42, 7, QStringLiteral("Coffee"), 123456, QStringLiteral("USD"));
    model.setName(QStringLiteral("Bob"));
    model.setPhone(QStringLiteral("12345"));

    QCOMPARE(model.email(), QString());
    QVERIFY(model.valid());
}

void PaymentCheckoutModelTest::validAcceptsPunctuatedPhone()
{
    PaymentCheckoutModel model;
    model.setInvoice(42, 7, QStringLiteral("Coffee"), 123456, QStringLiteral("USD"));
    model.setName(QStringLiteral("Bob"));
    model.setPhone(QStringLiteral("+7 (999) 12-34-5"));

    QVERIFY(model.valid());
}

void PaymentCheckoutModelTest::validRejectsMalformedEmail()
{
    PaymentCheckoutModel model;
    model.setInvoice(42, 7, QStringLiteral("Coffee"), 123456, QStringLiteral("USD"));
    model.setName(QStringLiteral("Bob"));

    model.setEmail(QStringLiteral("bob.example.com"));
    QVERIFY(!model.valid());

    model.setEmail(QStringLiteral("bob@example"));
    QVERIFY(!model.valid());

    model.setEmail(QStringLiteral("bob@example.com"));
    QVERIFY(model.valid());
}

void PaymentCheckoutModelTest::validRejectsShortPhone()
{
    PaymentCheckoutModel model;
    model.setInvoice(42, 7, QStringLiteral("Coffee"), 123456, QStringLiteral("USD"));
    model.setName(QStringLiteral("Bob"));

    model.setPhone(QStringLiteral("12-34"));
    QVERIFY(!model.valid());

    model.setPhone(QStringLiteral("12-34-5"));
    QVERIFY(model.valid());
}

void PaymentCheckoutModelTest::totalTextFormatsMinorUnits()
{
    PaymentCheckoutModel model;

    QCOMPARE(model.totalText(), QStringLiteral("0.00"));

    model.setInvoice(42, 7, QString(), 5, QString());
    QCOMPARE(model.totalText(), QStringLiteral("0.05"));

    model.setInvoice(42, 7, QString(), 70, QString());
    QCOMPARE(model.totalText(), QStringLiteral("0.70"));

    model.setInvoice(42, 7, QString(), 199, QString());
    QCOMPARE(model.totalText(), QStringLiteral("1.99"));

    model.setInvoice(42, 7, QString(), 123456, QString());
    QCOMPARE(model.totalText(), QStringLiteral("1234.56"));
}

void PaymentCheckoutModelTest::totalTextAppendsCurrency()
{
    PaymentCheckoutModel model;

    model.setInvoice(42, 7, QStringLiteral("Coffee"), 123456, QStringLiteral(" usd "));

    QCOMPARE(model.totalText(), QStringLiteral("1234.56 USD"));
}

void PaymentCheckoutModelTest::totalTextOmitsEmptyCurrency()
{
    PaymentCheckoutModel model;

    model.setInvoice(42, 7, QStringLiteral("Coffee"), 123456, QString());

    QCOMPARE(model.totalText(), QStringLiteral("1234.56"));
}

void PaymentCheckoutModelTest::totalTextForLargeAmount()
{
    PaymentCheckoutModel model;

    model.setInvoice(42, 7, QStringLiteral("Coffee"), 1234567890, QStringLiteral("USD"));

    QCOMPARE(model.totalText(), QStringLiteral("12345678.90 USD"));
}

void PaymentCheckoutModelTest::resetClearsEverything()
{
    PaymentCheckoutModel model;
    loadUsableCheckout(model);
    model.setPhone(QStringLiteral("+7 999 123-45-67"));
    model.setSaveCredentials(true);
    QSignalSpy spy(&model, &PaymentCheckoutModel::changed);

    model.reset();

    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.active());
    QCOMPARE(model.chatId(), 0);
    QCOMPARE(model.messageId(), 0);
    QCOMPARE(model.title(), QString());
    QCOMPARE(model.totalAmount(), 0);
    QCOMPARE(model.currency(), QString());
    QCOMPARE(model.name(), QString());
    QCOMPARE(model.email(), QString());
    QCOMPARE(model.phone(), QString());
    QVERIFY(!model.saveCredentials());
    QCOMPARE(model.totalText(), QStringLiteral("0.00"));
}

void PaymentCheckoutModelTest::resetOnEmptyCheckoutIsSilent()
{
    PaymentCheckoutModel model;
    QSignalSpy spy(&model, &PaymentCheckoutModel::changed);

    model.reset();
    QCOMPARE(spy.count(), 0);

    model.setInvoice(0, 0, QString(), -1, QString());
    QCOMPARE(spy.count(), 0);

    model.reset();
    QCOMPARE(spy.count(), 0);
}

void PaymentCheckoutModelTest::changedOnlyOnRealChanges()
{
    PaymentCheckoutModel model;
    QSignalSpy spy(&model, &PaymentCheckoutModel::changed);

    model.setName(QStringLiteral("Bob"));
    QCOMPARE(spy.count(), 1);

    model.setName(QStringLiteral(" Bob "));
    QCOMPARE(spy.count(), 1);

    model.setEmail(QStringLiteral("bob@example.com"));
    QCOMPARE(spy.count(), 2);

    model.setEmail(QStringLiteral("bob@example.com"));
    QCOMPARE(spy.count(), 2);

    model.setPhone(QStringLiteral("12345"));
    QCOMPARE(spy.count(), 3);

    model.setInvoice(42, 7, QStringLiteral("Coffee"), 123456, QStringLiteral("USD"));
    QCOMPARE(spy.count(), 4);

    model.setInvoice(42, 7, QStringLiteral("Coffee"), 123456, QStringLiteral("usd"));
    QCOMPARE(spy.count(), 4);

    model.setInvoice(43, 7, QStringLiteral("Coffee"), 123456, QStringLiteral("USD"));
    QCOMPARE(spy.count(), 5);

    model.reset();
    QCOMPARE(spy.count(), 6);

    model.reset();
    QCOMPARE(spy.count(), 6);
}

QTEST_GUILESS_MAIN(PaymentCheckoutModelTest)
#include "tst_payment_checkout_model.moc"
