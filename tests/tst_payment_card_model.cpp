#include "telegram/PaymentCardModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

/// The four bullets maskedNumber() puts in front of the last four digits.
QString maskPrefix()
{
    return QString(4, QChar(0x2022));
}

/// The full mask for a known last four digits, e.g. "•••• 1234".
QString masked(const QString& lastFour)
{
    return maskPrefix() + QLatin1Char(' ') + lastFour;
}

// A card that satisfies every rule, so each test only has to break the field it
// is about.
void loadCompleteCard(PaymentCardModel& model)
{
    model.setNumber(QStringLiteral("4242424242424242"));
    model.setExpiry(QStringLiteral("12"), QStringLiteral("2030"));
    model.setSecurityCode(QStringLiteral("123"));
    model.setCardholderName(QStringLiteral("Jane Doe"));
}

} // namespace

class PaymentCardModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setNumberKeepsDigitsOnly();
    void setNumberCapsAtNineteenDigits();
    void setNumberEmitsOnlyOnRealChange();
    void maskedNumberIsEmptyBelowFourDigits();
    void maskedNumberShowsLastFourDigits();
    void maskedNumberTracksTheNumber();
    void setExpiryPadsSingleDigitMonth();
    void setExpiryKeepsTwoDigitMonth();
    void setExpiryRejectsMonthOutsideOneToTwelve();
    void setExpiryKeepsTwoDigitYear();
    void setExpiryKeepsFourDigitYear();
    void setExpiryRejectsOtherYearLengths();
    void setExpiryFieldsAreIndependent();
    void setExpiryEmitsOnlyOnRealChange();
    void setSecurityCodeKeepsDigitsOnly();
    void setSecurityCodeCapsAtFourDigits();
    void setSecurityCodeEmitsOnlyOnRealChange();
    void cardholderNameIsTrimmed();
    void cardholderNameOfOnlySpacesCountsAsEmpty();
    void cardholderNameEmitsOnlyOnRealChange();
    void brandFromPrefix_data();
    void brandFromPrefix();
    void brandFollowsTheNumber();
    void validAcceptsCompleteCard();
    void validRequiresThirteenToNineteenDigits();
    void validRequiresMonth();
    void validRequiresYear();
    void validRequiresThreeOrFourDigitSecurityCode();
    void validRequiresCardholderName();
    void validIsAFormatCheckNotABrandCheck();
    void resetClearsEverything();
    void resetOnEmptyModelIsSilent();
    void resetEmitsOnce();
    void changedTracksOnlyRealChanges();
};

void PaymentCardModelTest::startsEmpty()
{
    const PaymentCardModel model;

    QCOMPARE(model.number(), QString());
    QCOMPARE(model.maskedNumber(), QString());
    QCOMPARE(model.expiryMonth(), QString());
    QCOMPARE(model.expiryYear(), QString());
    QCOMPARE(model.securityCode(), QString());
    QCOMPARE(model.cardholderName(), QString());
    QCOMPARE(model.brand(), QStringLiteral("Unknown"));
    QVERIFY(!model.valid());
}

void PaymentCardModelTest::setNumberKeepsDigitsOnly()
{
    PaymentCardModel model;

    model.setNumber(QStringLiteral("4242 4242-4242 4242"));
    QCOMPARE(model.number(), QStringLiteral("4242424242424242"));

    // Letters and punctuation around the digits are dropped.
    model.setNumber(QStringLiteral("abc 4242/4242"));
    QCOMPARE(model.number(), QStringLiteral("42424242"));

    model.setNumber(QString());
    QCOMPARE(model.number(), QString());
}

void PaymentCardModelTest::setNumberCapsAtNineteenDigits()
{
    PaymentCardModel model;

    model.setNumber(QStringLiteral("12345678901234567890"));
    QCOMPARE(model.number(), QStringLiteral("1234567890123456789"));
    QCOMPARE(model.number().size(), 19);
}

void PaymentCardModelTest::setNumberEmitsOnlyOnRealChange()
{
    PaymentCardModel model;
    QSignalSpy spy(&model, &PaymentCardModel::changed);

    model.setNumber(QStringLiteral("4242"));
    QCOMPARE(spy.count(), 1);

    // The same digits regrouped by the field is not an edit.
    model.setNumber(QStringLiteral("4242"));
    QCOMPARE(spy.count(), 1);
    model.setNumber(QStringLiteral(" 4242 "));
    QCOMPARE(spy.count(), 1);
    model.setNumber(QStringLiteral("42-42"));
    QCOMPARE(spy.count(), 1);

    model.setNumber(QStringLiteral("4243"));
    QCOMPARE(spy.count(), 2);

    model.setNumber(QString());
    QCOMPARE(spy.count(), 3);
}

void PaymentCardModelTest::maskedNumberIsEmptyBelowFourDigits()
{
    PaymentCardModel model;

    QVERIFY2(model.maskedNumber().isEmpty(), "an empty number has no mask");
    QVERIFY(model.maskedNumber().isEmpty());

    model.setNumber(QStringLiteral("4"));
    QVERIFY2(model.maskedNumber().isEmpty(), "one digit is not enough to mask");

    model.setNumber(QStringLiteral("424"));
    QVERIFY2(model.maskedNumber().isEmpty(), "three digits are not enough to mask");

    model.setNumber(QStringLiteral("4242"));
    QCOMPARE(model.maskedNumber(), masked(QStringLiteral("4242")));
}

void PaymentCardModelTest::maskedNumberShowsLastFourDigits()
{
    PaymentCardModel model;

    model.setNumber(QStringLiteral("4242424242424242"));
    QCOMPARE(model.maskedNumber(), masked(QStringLiteral("4242")));

    model.setNumber(QStringLiteral("5555555555554444"));
    QCOMPARE(model.maskedNumber(), masked(QStringLiteral("4444")));

    // Only the last four survive, whatever the length.
    model.setNumber(QStringLiteral("378282246310005"));
    QCOMPARE(model.maskedNumber(), masked(QStringLiteral("0005")));
}

void PaymentCardModelTest::maskedNumberTracksTheNumber()
{
    PaymentCardModel model;

    model.setNumber(QStringLiteral("4242424242424242"));
    QCOMPARE(model.maskedNumber(), masked(QStringLiteral("4242")));

    model.setNumber(QStringLiteral("4242424242424241"));
    QCOMPARE(model.maskedNumber(), masked(QStringLiteral("4241")));

    model.reset();
    QVERIFY(model.maskedNumber().isEmpty());
}

void PaymentCardModelTest::setExpiryPadsSingleDigitMonth()
{
    PaymentCardModel model;

    model.setExpiry(QStringLiteral("1"), QStringLiteral("26"));
    QCOMPARE(model.expiryMonth(), QStringLiteral("01"));
    QCOMPARE(model.expiryYear(), QStringLiteral("26"));

    model.setExpiry(QStringLiteral(" 9 "), QStringLiteral(" 26 "));
    QCOMPARE(model.expiryMonth(), QStringLiteral("09"));
    QCOMPARE(model.expiryYear(), QStringLiteral("26"));
}

void PaymentCardModelTest::setExpiryKeepsTwoDigitMonth()
{
    PaymentCardModel model;

    model.setExpiry(QStringLiteral("12"), QStringLiteral("2030"));
    QCOMPARE(model.expiryMonth(), QStringLiteral("12"));
    QCOMPARE(model.expiryYear(), QStringLiteral("2030"));

    model.setExpiry(QStringLiteral("07"), QStringLiteral("2030"));
    QCOMPARE(model.expiryMonth(), QStringLiteral("07"));
}

void PaymentCardModelTest::setExpiryRejectsMonthOutsideOneToTwelve()
{
    PaymentCardModel model;

    model.setExpiry(QStringLiteral("0"), QStringLiteral("2030"));
    QVERIFY2(model.expiryMonth().isEmpty(), "month 0 is not a month");
    QCOMPARE(model.expiryYear(), QStringLiteral("2030"));

    model.setExpiry(QStringLiteral("13"), QStringLiteral("2030"));
    QVERIFY2(model.expiryMonth().isEmpty(), "month 13 is not a month");
    QCOMPARE(model.expiryYear(), QStringLiteral("2030"));

    model.setExpiry(QString(), QStringLiteral("2030"));
    QVERIFY2(model.expiryMonth().isEmpty(), "no month is stored as no month");

    // A good month stored earlier is cleared by a later bad one.
    model.setExpiry(QStringLiteral("12"), QStringLiteral("2030"));
    QCOMPARE(model.expiryMonth(), QStringLiteral("12"));

    model.setExpiry(QStringLiteral("00"), QStringLiteral("2030"));
    QVERIFY(model.expiryMonth().isEmpty());
}

void PaymentCardModelTest::setExpiryKeepsTwoDigitYear()
{
    PaymentCardModel model;

    model.setExpiry(QStringLiteral("12"), QStringLiteral("26"));
    QCOMPARE(model.expiryYear(), QStringLiteral("26"));
}

void PaymentCardModelTest::setExpiryKeepsFourDigitYear()
{
    PaymentCardModel model;

    model.setExpiry(QStringLiteral("12"), QStringLiteral("2026"));
    QCOMPARE(model.expiryYear(), QStringLiteral("2026"));

    // A five-digit year is capped to the four digits a year can have.
    model.setExpiry(QStringLiteral("12"), QStringLiteral("20266"));
    QCOMPARE(model.expiryYear(), QStringLiteral("2026"));
}

void PaymentCardModelTest::setExpiryRejectsOtherYearLengths()
{
    PaymentCardModel model;

    model.setExpiry(QStringLiteral("12"), QStringLiteral("2"));
    QVERIFY2(model.expiryYear().isEmpty(), "a single digit is not a year");
    QCOMPARE(model.expiryMonth(), QStringLiteral("12"));

    model.setExpiry(QStringLiteral("12"), QStringLiteral("202"));
    QVERIFY2(model.expiryYear().isEmpty(), "three digits are not a year");
    QCOMPARE(model.expiryMonth(), QStringLiteral("12"));

    model.setExpiry(QStringLiteral("12"), QString());
    QVERIFY(model.expiryYear().isEmpty());

    // A good year stored earlier is cleared by a later bad one.
    model.setExpiry(QStringLiteral("12"), QStringLiteral("2026"));
    QCOMPARE(model.expiryYear(), QStringLiteral("2026"));

    model.setExpiry(QStringLiteral("12"), QStringLiteral("2"));
    QVERIFY(model.expiryYear().isEmpty());
}

void PaymentCardModelTest::setExpiryFieldsAreIndependent()
{
    PaymentCardModel model;

    // A bad year leaves a good month alone.
    model.setExpiry(QStringLiteral("11"), QStringLiteral("3"));
    QCOMPARE(model.expiryMonth(), QStringLiteral("11"));
    QVERIFY(model.expiryYear().isEmpty());

    // And a bad month leaves a good year alone.
    model.setExpiry(QStringLiteral("14"), QStringLiteral("2031"));
    QVERIFY(model.expiryMonth().isEmpty());
    QCOMPARE(model.expiryYear(), QStringLiteral("2031"));
}

void PaymentCardModelTest::setExpiryEmitsOnlyOnRealChange()
{
    PaymentCardModel model;
    QSignalSpy spy(&model, &PaymentCardModel::changed);

    model.setExpiry(QStringLiteral("12"), QStringLiteral("2030"));
    QCOMPARE(spy.count(), 1);

    // "12" and " 12 " normalize to the same pair, so the second write is silent.
    model.setExpiry(QStringLiteral(" 12 "), QStringLiteral(" 2030 "));
    QCOMPARE(spy.count(), 1);

    model.setExpiry(QStringLiteral("1"), QStringLiteral("2030"));
    QCOMPARE(spy.count(), 2);

    // The month is already "01" at that point.
    model.setExpiry(QStringLiteral("01"), QStringLiteral("2030"));
    QCOMPARE(spy.count(), 2);

    // An out-of-range month clears the field, which is a real change.
    model.setExpiry(QStringLiteral("13"), QStringLiteral("2030"));
    QCOMPARE(spy.count(), 3);

    // Repeating the same rejection changes nothing.
    model.setExpiry(QStringLiteral("13"), QStringLiteral("2030"));
    QCOMPARE(spy.count(), 3);
}

void PaymentCardModelTest::setSecurityCodeKeepsDigitsOnly()
{
    PaymentCardModel model;

    model.setSecurityCode(QStringLiteral("12 3-4"));
    QCOMPARE(model.securityCode(), QStringLiteral("1234"));

    model.setSecurityCode(QStringLiteral("abc123"));
    QCOMPARE(model.securityCode(), QStringLiteral("123"));

    model.setSecurityCode(QString());
    QCOMPARE(model.securityCode(), QString());
}

void PaymentCardModelTest::setSecurityCodeCapsAtFourDigits()
{
    PaymentCardModel model;

    model.setSecurityCode(QStringLiteral("123456"));
    QCOMPARE(model.securityCode(), QStringLiteral("1234"));

    // Two digits are stored while the user is still typing; valid() is the gate.
    model.setSecurityCode(QStringLiteral("12"));
    QCOMPARE(model.securityCode(), QStringLiteral("12"));
}

void PaymentCardModelTest::setSecurityCodeEmitsOnlyOnRealChange()
{
    PaymentCardModel model;
    QSignalSpy spy(&model, &PaymentCardModel::changed);

    model.setSecurityCode(QStringLiteral("123"));
    QCOMPARE(spy.count(), 1);

    model.setSecurityCode(QStringLiteral(" 123 "));
    QCOMPARE(spy.count(), 1);

    model.setSecurityCode(QStringLiteral("1234"));
    QCOMPARE(spy.count(), 2);

    model.setSecurityCode(QStringLiteral("12345"));
    QCOMPARE(spy.count(), 2);
}

void PaymentCardModelTest::cardholderNameIsTrimmed()
{
    PaymentCardModel model;

    model.setCardholderName(QStringLiteral("  Jane Doe  "));
    QCOMPARE(model.cardholderName(), QStringLiteral("Jane Doe"));

    model.setCardholderName(QStringLiteral("JANE DOE"));
    QCOMPARE(model.cardholderName(), QStringLiteral("JANE DOE"));
}

void PaymentCardModelTest::cardholderNameOfOnlySpacesCountsAsEmpty()
{
    PaymentCardModel model;

    model.setCardholderName(QStringLiteral("Jane Doe"));
    QCOMPARE(model.cardholderName(), QStringLiteral("Jane Doe"));

    model.setCardholderName(QStringLiteral("   "));
    QVERIFY(model.cardholderName().isEmpty());
}

void PaymentCardModelTest::cardholderNameEmitsOnlyOnRealChange()
{
    PaymentCardModel model;
    QSignalSpy spy(&model, &PaymentCardModel::changed);

    model.setCardholderName(QStringLiteral("Jane Doe"));
    QCOMPARE(spy.count(), 1);

    // The same name with padding is the same stored name.
    model.setCardholderName(QStringLiteral("  Jane Doe "));
    QCOMPARE(spy.count(), 1);

    model.setCardholderName(QStringLiteral("Jane"));
    QCOMPARE(spy.count(), 2);

    model.setCardholderName(QString());
    QCOMPARE(spy.count(), 3);
}

void PaymentCardModelTest::brandFromPrefix_data()
{
    QTest::addColumn<QString>("number");
    QTest::addColumn<QString>("brand");

    QTest::newRow("empty") << QString() << QStringLiteral("Unknown");
    QTest::newRow("nine") << QStringLiteral("9") << QStringLiteral("Unknown");

    QTest::newRow("visa one digit") << QStringLiteral("4") << QStringLiteral("Visa");
    QTest::newRow("visa thirteen") << QStringLiteral("4000000000000") << QStringLiteral("Visa");
    QTest::newRow("visa sixteen") << QStringLiteral("4242424242424242") << QStringLiteral("Visa");

    QTest::newRow("mastercard 51") << QStringLiteral("5105105105105100")
                                   << QStringLiteral("Mastercard");
    QTest::newRow("mastercard 55") << QStringLiteral("5555555555554444")
                                   << QStringLiteral("Mastercard");
    QTest::newRow("mastercard 2221") << QStringLiteral("2221000000000009")
                                     << QStringLiteral("Mastercard");
    QTest::newRow("mastercard 2720") << QStringLiteral("2720990000000000")
                                     << QStringLiteral("Mastercard");
    QTest::newRow("mastercard 50") << QStringLiteral("5000000000000000")
                                   << QStringLiteral("Unknown");
    QTest::newRow("mastercard 56") << QStringLiteral("5600000000000000")
                                   << QStringLiteral("Unknown");
    QTest::newRow("mastercard 2220") << QStringLiteral("2220000000000000")
                                     << QStringLiteral("Unknown");
    QTest::newRow("mastercard 2721") << QStringLiteral("2721000000000000")
                                     << QStringLiteral("Unknown");
    QTest::newRow("two series too short") << QStringLiteral("2") << QStringLiteral("Unknown");
    QTest::newRow("two series three digits") << QStringLiteral("222") << QStringLiteral("Unknown");

    QTest::newRow("amex 34") << QStringLiteral("340000000000009")
                             << QStringLiteral("Amex");
    QTest::newRow("amex 37") << QStringLiteral("378282246310005")
                             << QStringLiteral("Amex");
    QTest::newRow("amex 35") << QStringLiteral("3500000000000000")
                             << QStringLiteral("Unknown");
    QTest::newRow("amex one digit") << QStringLiteral("3") << QStringLiteral("Unknown");
}

void PaymentCardModelTest::brandFromPrefix()
{
    QFETCH(QString, number);
    QFETCH(QString, brand);

    PaymentCardModel model;
    model.setNumber(number);

    QCOMPARE(model.brand(), brand);
}

void PaymentCardModelTest::brandFollowsTheNumber()
{
    PaymentCardModel model;

    model.setNumber(QStringLiteral("4242424242424242"));
    QCOMPARE(model.brand(), QStringLiteral("Visa"));

    model.setNumber(QStringLiteral("5555555555554444"));
    QCOMPARE(model.brand(), QStringLiteral("Mastercard"));

    model.setNumber(QStringLiteral("378282246310005"));
    QCOMPARE(model.brand(), QStringLiteral("Amex"));

    model.setNumber(QStringLiteral("9999999999999999"));
    QCOMPARE(model.brand(), QStringLiteral("Unknown"));

    model.reset();
    QCOMPARE(model.brand(), QStringLiteral("Unknown"));
}

void PaymentCardModelTest::validAcceptsCompleteCard()
{
    PaymentCardModel model;
    QVERIFY2(!model.valid(), "an empty card is not valid");

    loadCompleteCard(model);
    QVERIFY(model.valid());
}

void PaymentCardModelTest::validRequiresThirteenToNineteenDigits()
{
    PaymentCardModel model;
    loadCompleteCard(model);

    // Twelve digits is below the shortest card number.
    model.setNumber(QStringLiteral("424242424242"));
    QVERIFY2(!model.valid(), "12 digits are too short");

    model.setNumber(QStringLiteral("4242424242424"));
    QVERIFY2(model.valid(), "13 digits are the shortest accepted number");

    model.setNumber(QStringLiteral("4242424242424242424"));
    QVERIFY2(model.valid(), "19 digits are the longest accepted number");

    // A twentieth digit is dropped by the setter, so the stored number is still
    // nineteen digits and still valid.
    model.setNumber(QStringLiteral("42424242424242424242"));
    QCOMPARE(model.number().size(), 19);
    QVERIFY(model.valid());

    model.setNumber(QString());
    QVERIFY(!model.valid());
}

void PaymentCardModelTest::validRequiresMonth()
{
    PaymentCardModel model;
    loadCompleteCard(model);

    model.setExpiry(QString(), QStringLiteral("2030"));
    QVERIFY2(!model.valid(), "a card without a month is not valid");

    model.setExpiry(QStringLiteral("0"), QStringLiteral("2030"));
    QVERIFY2(!model.valid(), "month 0 is not a month");

    model.setExpiry(QStringLiteral("13"), QStringLiteral("2030"));
    QVERIFY2(!model.valid(), "month 13 is not a month");

    model.setExpiry(QStringLiteral("1"), QStringLiteral("2030"));
    QVERIFY(model.valid());
}

void PaymentCardModelTest::validRequiresYear()
{
    PaymentCardModel model;
    loadCompleteCard(model);

    model.setExpiry(QStringLiteral("12"), QString());
    QVERIFY2(!model.valid(), "a card without a year is not valid");

    model.setExpiry(QStringLiteral("12"), QStringLiteral("2"));
    QVERIFY2(!model.valid(), "a one-digit year is not a year");

    model.setExpiry(QStringLiteral("12"), QStringLiteral("202"));
    QVERIFY2(!model.valid(), "a three-digit year is not a year");

    model.setExpiry(QStringLiteral("12"), QStringLiteral("26"));
    QVERIFY2(model.valid(), "a two-digit year is enough");

    model.setExpiry(QStringLiteral("12"), QStringLiteral("2026"));
    QVERIFY(model.valid());
}

void PaymentCardModelTest::validRequiresThreeOrFourDigitSecurityCode()
{
    PaymentCardModel model;
    loadCompleteCard(model);

    model.setSecurityCode(QString());
    QVERIFY2(!model.valid(), "a card without a security code is not valid");

    model.setSecurityCode(QStringLiteral("1"));
    QVERIFY2(!model.valid(), "one digit is too short");

    model.setSecurityCode(QStringLiteral("12"));
    QVERIFY2(!model.valid(), "two digits are too short");

    model.setSecurityCode(QStringLiteral("123"));
    QVERIFY2(model.valid(), "three digits are accepted");

    model.setSecurityCode(QStringLiteral("1234"));
    QVERIFY2(model.valid(), "four digits are accepted");

    // A fifth digit is dropped by the setter, so the code stays four digits.
    model.setSecurityCode(QStringLiteral("12345"));
    QCOMPARE(model.securityCode(), QStringLiteral("1234"));
    QVERIFY(model.valid());
}

void PaymentCardModelTest::validRequiresCardholderName()
{
    PaymentCardModel model;
    loadCompleteCard(model);

    model.setCardholderName(QString());
    QVERIFY2(!model.valid(), "a card without a name is not valid");

    model.setCardholderName(QStringLiteral("   "));
    QVERIFY2(!model.valid(), "a name of only spaces is not a name");

    model.setCardholderName(QStringLiteral("J"));
    QVERIFY(model.valid());
}

void PaymentCardModelTest::validIsAFormatCheckNotABrandCheck()
{
    PaymentCardModel model;
    loadCompleteCard(model);

    // A number with an unknown prefix is still a well-formed number: the brand
    // is display information, not a validity rule.
    model.setNumber(QStringLiteral("9999999999999999"));
    QCOMPARE(model.brand(), QStringLiteral("Unknown"));
    QVERIFY(model.valid());
}

void PaymentCardModelTest::resetClearsEverything()
{
    PaymentCardModel model;
    loadCompleteCard(model);
    QVERIFY(model.valid());

    model.reset();

    QCOMPARE(model.number(), QString());
    QCOMPARE(model.maskedNumber(), QString());
    QCOMPARE(model.expiryMonth(), QString());
    QCOMPARE(model.expiryYear(), QString());
    QCOMPARE(model.securityCode(), QString());
    QCOMPARE(model.cardholderName(), QString());
    QCOMPARE(model.brand(), QStringLiteral("Unknown"));
    QVERIFY(!model.valid());
}

void PaymentCardModelTest::resetOnEmptyModelIsSilent()
{
    PaymentCardModel model;
    QSignalSpy spy(&model, &PaymentCardModel::changed);

    model.reset();
    QCOMPARE(spy.count(), 0);

    // A card that came back to empty is empty again, so reset() stays silent.
    loadCompleteCard(model);
    model.reset();
    spy.clear();

    model.reset();
    QCOMPARE(spy.count(), 0);
}

void PaymentCardModelTest::resetEmitsOnce()
{
    PaymentCardModel model;
    loadCompleteCard(model);
    QSignalSpy spy(&model, &PaymentCardModel::changed);

    // One signal for the whole reset, not one per field.
    model.reset();
    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.valid());
}

void PaymentCardModelTest::changedTracksOnlyRealChanges()
{
    PaymentCardModel model;
    QSignalSpy spy(&model, &PaymentCardModel::changed);

    // Four fields, four real changes.
    loadCompleteCard(model);
    QCOMPARE(spy.count(), 4);

    // Writing the same values again adds nothing.
    loadCompleteCard(model);
    QCOMPARE(spy.count(), 4);

    // And a reset is the fifth.
    model.reset();
    QCOMPARE(spy.count(), 5);

    // Resetting an already empty card stays silent.
    model.reset();
    QCOMPARE(spy.count(), 5);
}

QTEST_GUILESS_MAIN(PaymentCardModelTest)
#include "tst_payment_card_model.moc"
