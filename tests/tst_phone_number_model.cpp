#include "telegram/PhoneNumberModel.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtTest>

using beygram::telegram::PhoneNumberModel;

namespace {

/// A national number of exactly the requested length, all nines.
QString filled(int length)
{
    return QString(length, QLatin1Char('9'));
}

} // namespace

class PhoneNumberModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setCountryCodeStoresParts();
    void countryIsoIsStoredVerbatim();
    void dialCodeStripsDecoration();
    void dialCodeCappedAtFourDigits();
    void emptyDialCodeClearsOnlyTheDialPart();
    void nationalNumberStripsNonDigits();
    void nationalNumberCappedAtFifteen();
    void fullNumberNeedsBothParts();
    void fullNumberCombinesParts();
    void validAcceptsBounds();
    void validRejectsOutOfBounds();
    void displayTextGroupsFullNumber();
    void displayTextShowsOnlyExistingBlocks();
    void displayTextWithoutNationalNumber();
    void displayTextWithoutDialCode();
    void displayTextKeepsDigitsPastTheFourthBlock();
    void clearResetsEverything();
    void clearOnEmptyIsSilent();
    void changedOnCountryCodeChange();
    void changedOnNationalNumberChange();
    void changedOnlyOnRealChanges();
    void repeatedNationalNumberIsSilent();
    void acceptsNewNumberAfterClear();
};

void PhoneNumberModelTest::startsEmpty()
{
    PhoneNumberModel model;

    QVERIFY(model.countryIso().isEmpty());
    QVERIFY(model.dialCode().isEmpty());
    QVERIFY(model.nationalNumber().isEmpty());
    QVERIFY(model.fullNumber().isEmpty());
    QVERIFY(model.displayText().isEmpty());
    QVERIFY(!model.valid());
}

void PhoneNumberModelTest::setCountryCodeStoresParts()
{
    PhoneNumberModel model;

    model.setCountryCode(QStringLiteral("RU"), QStringLiteral("+7"));

    QCOMPARE(model.countryIso(), QStringLiteral("RU"));
    QCOMPARE(model.dialCode(), QStringLiteral("7"));
}

void PhoneNumberModelTest::countryIsoIsStoredVerbatim()
{
    PhoneNumberModel model;

    // The ISO code is whatever the picker handed over; only the dial code is
    // reduced to digits, so a lower case code stays lower case.
    model.setCountryCode(QStringLiteral("ru"), QStringLiteral("7"));
    QCOMPARE(model.countryIso(), QStringLiteral("ru"));

    model.setCountryCode(QStringLiteral("DE"), QStringLiteral("49"));
    QCOMPARE(model.countryIso(), QStringLiteral("DE"));
    QCOMPARE(model.dialCode(), QStringLiteral("49"));
}

void PhoneNumberModelTest::dialCodeStripsDecoration()
{
    const QStringList decorated{
        QStringLiteral("+7"),
        QStringLiteral("7"),
        QStringLiteral(" 7 "),
        QStringLiteral("+ 7"),
        QStringLiteral("(+7)"),
        QStringLiteral("+7-"),
    };

    for (const QString& text : decorated) {
        PhoneNumberModel model;
        model.setCountryCode(QStringLiteral("RU"), text);
        QCOMPARE(model.dialCode(), QStringLiteral("7"));
    }

    PhoneNumberModel model;
    model.setCountryCode(QStringLiteral("US"), QStringLiteral("+1 (0)"));
    QCOMPARE(model.dialCode(), QStringLiteral("10"));
}

void PhoneNumberModelTest::dialCodeCappedAtFourDigits()
{
    PhoneNumberModel model;

    model.setCountryCode(QStringLiteral("XX"), QStringLiteral("12345"));
    QCOMPARE(model.dialCode(), QStringLiteral("1234"));

    model.setCountryCode(QStringLiteral("XX"), QStringLiteral("+99999999"));
    QCOMPARE(model.dialCode(), QStringLiteral("9999"));
}

void PhoneNumberModelTest::emptyDialCodeClearsOnlyTheDialPart()
{
    PhoneNumberModel model;
    model.setCountryCode(QStringLiteral("RU"), QStringLiteral("+7"));
    model.setNationalNumber(QStringLiteral("9991234567"));

    model.setCountryCode(QStringLiteral("RU"), QString());

    QCOMPARE(model.countryIso(), QStringLiteral("RU"));
    QVERIFY(model.dialCode().isEmpty());
    QCOMPARE(model.nationalNumber(), QStringLiteral("9991234567"));
    QVERIFY(model.fullNumber().isEmpty());
}

void PhoneNumberModelTest::nationalNumberStripsNonDigits()
{
    const QStringList typed{
        QStringLiteral("999 123-45-67"),
        QStringLiteral("(999) 1234567"),
        QStringLiteral("999.123.45.67"),
        QStringLiteral("9991234567"),
    };

    for (const QString& text : typed) {
        PhoneNumberModel model;
        model.setNationalNumber(text);
        QCOMPARE(model.nationalNumber(), QStringLiteral("9991234567"));
    }
}

void PhoneNumberModelTest::nationalNumberCappedAtFifteen()
{
    PhoneNumberModel model;

    model.setNationalNumber(filled(16));
    QCOMPARE(model.nationalNumber(), filled(15));
    QCOMPARE(model.nationalNumber().size(), 15);

    // Decoration does not count towards the cap, only the digits do.
    model.setNationalNumber(QStringLiteral("999 999 999 999 999 999"));
    QCOMPARE(model.nationalNumber(), filled(15));
}

void PhoneNumberModelTest::fullNumberNeedsBothParts()
{
    PhoneNumberModel model;

    model.setCountryCode(QStringLiteral("RU"), QStringLiteral("+7"));
    QVERIFY(model.fullNumber().isEmpty());

    model.setNationalNumber(QStringLiteral("9991234567"));
    QCOMPARE(model.fullNumber(), QStringLiteral("+79991234567"));

    // A national number alone cannot be dialled either.
    PhoneNumberModel nationalOnly;
    nationalOnly.setNationalNumber(QStringLiteral("9991234567"));
    QVERIFY(nationalOnly.fullNumber().isEmpty());
}

void PhoneNumberModelTest::fullNumberCombinesParts()
{
    PhoneNumberModel model;

    model.setCountryCode(QStringLiteral("DE"), QStringLiteral("+49"));
    model.setNationalNumber(QStringLiteral("30 1234 5678"));

    QCOMPARE(model.fullNumber(), QStringLiteral("+493012345678"));
    // The formatted text keeps the same digits, only grouped.
    QCOMPARE(model.displayText(), QStringLiteral("+49 301 234-56-78"));
}

void PhoneNumberModelTest::validAcceptsBounds()
{
    PhoneNumberModel model;
    model.setCountryCode(QStringLiteral("RU"), QStringLiteral("7"));

    model.setNationalNumber(filled(4));
    QVERIFY(model.valid());

    model.setNationalNumber(filled(15));
    QVERIFY(model.valid());

    // One-digit and four-digit dial codes are both real.
    model.setCountryCode(QStringLiteral("US"), QStringLiteral("1"));
    model.setNationalNumber(filled(10));
    QVERIFY(model.valid());

    model.setCountryCode(QStringLiteral("XX"), QStringLiteral("1234"));
    QVERIFY(model.valid());
}

void PhoneNumberModelTest::validRejectsOutOfBounds()
{
    PhoneNumberModel model;
    model.setCountryCode(QStringLiteral("RU"), QStringLiteral("7"));

    model.setNationalNumber(filled(3));
    QVERIFY(!model.valid());

    model.setNationalNumber(filled(16));
    QCOMPARE(model.nationalNumber(), filled(15));
    QVERIFY(model.valid());

    // No dial code means no country, so the number cannot be valid.
    model.setCountryCode(QString(), QString());
    QVERIFY(!model.valid());

    // A national number on its own is not valid either.
    PhoneNumberModel nationalOnly;
    nationalOnly.setNationalNumber(filled(10));
    QVERIFY(!nationalOnly.valid());
}

void PhoneNumberModelTest::displayTextGroupsFullNumber()
{
    PhoneNumberModel model;
    model.setCountryCode(QStringLiteral("RU"), QStringLiteral("+7"));
    model.setNationalNumber(QStringLiteral("9991234567"));

    QCOMPARE(model.displayText(), QStringLiteral("+7 999 123-45-67"));
}

void PhoneNumberModelTest::displayTextShowsOnlyExistingBlocks()
{
    PhoneNumberModel model;
    model.setCountryCode(QStringLiteral("RU"), QStringLiteral("7"));

    model.setNationalNumber(QStringLiteral("999"));
    QCOMPARE(model.displayText(), QStringLiteral("+7 999"));

    model.setNationalNumber(QStringLiteral("99912"));
    QCOMPARE(model.displayText(), QStringLiteral("+7 999 12"));

    model.setNationalNumber(QStringLiteral("9991234"));
    QCOMPARE(model.displayText(), QStringLiteral("+7 999 123-4"));

    model.setNationalNumber(QStringLiteral("99912345"));
    QCOMPARE(model.displayText(), QStringLiteral("+7 999 123-45"));
}

void PhoneNumberModelTest::displayTextWithoutNationalNumber()
{
    PhoneNumberModel model;

    model.setCountryCode(QStringLiteral("RU"), QStringLiteral("+7"));
    QCOMPARE(model.displayText(), QStringLiteral("+7"));

    // Clearing the number leaves the picked country on screen.
    model.setNationalNumber(QStringLiteral("9991234567"));
    model.setNationalNumber(QString());
    QCOMPARE(model.displayText(), QStringLiteral("+7"));
}

void PhoneNumberModelTest::displayTextWithoutDialCode()
{
    PhoneNumberModel model;
    model.setNationalNumber(QStringLiteral("9991234567"));

    // Without a country there is nothing to put a '+' in front of.
    QCOMPARE(model.displayText(), QStringLiteral("999 123-45-67"));
}

void PhoneNumberModelTest::displayTextKeepsDigitsPastTheFourthBlock()
{
    PhoneNumberModel model;
    model.setCountryCode(QStringLiteral("XX"), QStringLiteral("99"));

    // Ten digits fill the four blocks; the remaining five stay together so no
    // digit is ever hidden from the user.
    model.setNationalNumber(QStringLiteral("123456789012345"));
    QCOMPARE(model.displayText(), QStringLiteral("+99 123 456-78-90-12345"));
}

void PhoneNumberModelTest::clearResetsEverything()
{
    PhoneNumberModel model;
    model.setCountryCode(QStringLiteral("RU"), QStringLiteral("+7"));
    model.setNationalNumber(QStringLiteral("9991234567"));
    QSignalSpy spy(&model, &PhoneNumberModel::changed);

    model.clear();

    QVERIFY(model.countryIso().isEmpty());
    QVERIFY(model.dialCode().isEmpty());
    QVERIFY(model.nationalNumber().isEmpty());
    QVERIFY(model.fullNumber().isEmpty());
    QVERIFY(model.displayText().isEmpty());
    QVERIFY(!model.valid());
    QCOMPARE(spy.count(), 1);
}

void PhoneNumberModelTest::clearOnEmptyIsSilent()
{
    PhoneNumberModel model;
    QSignalSpy spy(&model, &PhoneNumberModel::changed);

    model.clear();
    model.clear();

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.displayText().isEmpty());
}

void PhoneNumberModelTest::changedOnCountryCodeChange()
{
    PhoneNumberModel model;
    QSignalSpy spy(&model, &PhoneNumberModel::changed);

    model.setCountryCode(QStringLiteral("RU"), QStringLiteral("+7"));
    QCOMPARE(spy.count(), 1);

    // Same country, different dial code is a real change.
    model.setCountryCode(QStringLiteral("RU"), QStringLiteral("8"));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.dialCode(), QStringLiteral("8"));

    // Same dial code, different country is a real change too.
    model.setCountryCode(QStringLiteral("KZ"), QStringLiteral("8"));
    QCOMPARE(spy.count(), 3);
    QCOMPARE(model.countryIso(), QStringLiteral("KZ"));

    // Exactly the same pair again is not.
    model.setCountryCode(QStringLiteral("KZ"), QStringLiteral("+8"));
    QCOMPARE(spy.count(), 3);
}

void PhoneNumberModelTest::changedOnNationalNumberChange()
{
    PhoneNumberModel model;
    QSignalSpy spy(&model, &PhoneNumberModel::changed);

    model.setNationalNumber(QStringLiteral("999"));
    QCOMPARE(spy.count(), 1);

    model.setNationalNumber(QStringLiteral("9991"));
    QCOMPARE(spy.count(), 2);

    // Deleting a digit is a change as well.
    model.setNationalNumber(QStringLiteral("999"));
    QCOMPARE(spy.count(), 3);
}

void PhoneNumberModelTest::changedOnlyOnRealChanges()
{
    PhoneNumberModel model;
    QSignalSpy spy(&model, &PhoneNumberModel::changed);

    // Nothing stored yet, so these are all silent.
    model.setCountryCode(QString(), QString());
    model.setNationalNumber(QString());
    model.setNationalNumber(QStringLiteral(" - () "));
    QCOMPARE(spy.count(), 0);

    model.setCountryCode(QStringLiteral("RU"), QStringLiteral("+7"));
    QCOMPARE(spy.count(), 1);

    model.setCountryCode(QStringLiteral("RU"), QStringLiteral("7"));
    QCOMPARE(spy.count(), 1);

    model.setNationalNumber(QStringLiteral("9991234567"));
    QCOMPARE(spy.count(), 2);

    // The same digits with decoration are the same number.
    model.setNationalNumber(QStringLiteral("999 123-45-67"));
    QCOMPARE(spy.count(), 2);

    // Digits past the cap change nothing once the cap is reached.
    model.setNationalNumber(filled(15));
    QCOMPARE(spy.count(), 3);
    model.setNationalNumber(filled(20));
    QCOMPARE(spy.count(), 3);
}

void PhoneNumberModelTest::repeatedNationalNumberIsSilent()
{
    PhoneNumberModel model;
    model.setCountryCode(QStringLiteral("RU"), QStringLiteral("7"));
    model.setNationalNumber(QStringLiteral("9991234567"));
    QSignalSpy spy(&model, &PhoneNumberModel::changed);

    // A field that reformats its own text reports the same digits back.
    model.setNationalNumber(QStringLiteral("999 123-45-67"));
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.fullNumber(), QStringLiteral("+79991234567"));
}

void PhoneNumberModelTest::acceptsNewNumberAfterClear()
{
    PhoneNumberModel model;
    model.setCountryCode(QStringLiteral("RU"), QStringLiteral("7"));
    model.setNationalNumber(QStringLiteral("9991234567"));
    model.clear();

    QSignalSpy spy(&model, &PhoneNumberModel::changed);

    model.setCountryCode(QStringLiteral("KZ"), QStringLiteral("+7"));
    model.setNationalNumber(QStringLiteral("7012345678"));

    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.countryIso(), QStringLiteral("KZ"));
    QCOMPARE(model.fullNumber(), QStringLiteral("+77012345678"));
    QCOMPARE(model.displayText(), QStringLiteral("+7 701 234-56-78"));
    QVERIFY(model.valid());
}

QTEST_GUILESS_MAIN(PhoneNumberModelTest)
#include "tst_phone_number_model.moc"
