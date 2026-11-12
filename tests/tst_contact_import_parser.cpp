#include "telegram/ContactImportParser.h"

#include <QtTest>

using namespace beygram::telegram;

class ContactImportParserTest final : public QObject {
    Q_OBJECT

private slots:
    void normalizesPhoneNumbers();
    void parsesFormattedNameCard();
    void prefersFormattedNameOverStructuredName();
    void fallsBackToStructuredName();
    void decodesEscapedText();
    void takesFirstTelAndEmailValues();
    void unfoldsFoldedLines();
    void handlesUrisAndGroups();
    void skipsBlocksWithoutUsablePhone();
    void ignoresTextOutsideBlocks();
    void countsUnterminatedBlocks();
    void handlesEmptyInput();
};

void ContactImportParserTest::normalizesPhoneNumbers()
{
    QCOMPARE(normalizePhoneForImport(QStringLiteral("+1 (415) 555-0123")),
             QStringLiteral("+14155550123"));
    QCOMPARE(normalizePhoneForImport(QStringLiteral("  +7 999 123 45 67 ")),
             QStringLiteral("+79991234567"));
    QCOMPARE(normalizePhoneForImport(QStringLiteral("8 999 123-45-67")),
             QStringLiteral("+89991234567"));
    QCOMPARE(normalizePhoneForImport(QStringLiteral("0044 20 7946 0958")),
             QStringLiteral("+442079460958"));
    QCOMPARE(normalizePhoneForImport(QStringLiteral("tel:+7-999-123-45-67")),
             QStringLiteral("+79991234567"));

    // The bounds are inclusive: five and fifteen digits are both importable.
    QCOMPARE(normalizePhoneForImport(QStringLiteral("12345")), QStringLiteral("+12345"));
    QCOMPARE(normalizePhoneForImport(QStringLiteral("123456789012345")),
             QStringLiteral("+123456789012345"));

    QVERIFY(normalizePhoneForImport(QStringLiteral("1234")).isEmpty());
    QVERIFY(normalizePhoneForImport(QStringLiteral("+1234567890123456")).isEmpty());
    QVERIFY(normalizePhoneForImport(QStringLiteral("no digits here")).isEmpty());
    QVERIFY(normalizePhoneForImport(QString()).isEmpty());
}

void ContactImportParserTest::parsesFormattedNameCard()
{
    const ContactImportResult result = parseVCards(QStringLiteral(
        "BEGIN:VCARD\r\n"
        "VERSION:3.0\r\n"
        "FN:Ada Lovelace\r\n"
        "TEL;TYPE=CELL:+1 (415) 555-0123\r\n"
        "EMAIL;TYPE=INTERNET:ada@example.com\r\n"
        "END:VCARD\r\n"));

    QCOMPARE(result.skipped, 0);
    QCOMPARE(result.contacts.size(), qsizetype(1));

    const ImportedContact& contact = result.contacts.first();
    QCOMPARE(contact.firstName, QStringLiteral("Ada Lovelace"));
    QVERIFY(contact.lastName.isEmpty());
    QCOMPARE(contact.phone, QStringLiteral("+14155550123"));
    QCOMPARE(contact.email, QStringLiteral("ada@example.com"));
}

void ContactImportParserTest::prefersFormattedNameOverStructuredName()
{
    const ContactImportResult result = parseVCards(QStringLiteral(
        "BEGIN:VCARD\n"
        "N:Lovelace;Ada;;;\n"
        "FN:Ada Lovelace\n"
        "TEL:+14155550123\n"
        "END:VCARD\n"));

    QCOMPARE(result.contacts.size(), qsizetype(1));
    QCOMPARE(result.contacts.first().firstName, QStringLiteral("Ada Lovelace"));
    QVERIFY(result.contacts.first().lastName.isEmpty());
}

void ContactImportParserTest::fallsBackToStructuredName()
{
    const ContactImportResult result = parseVCards(QStringLiteral(
        "BEGIN:VCARD\n"
        "FN:   \n"
        "N:Lovelace;Ada;Byron;Dr.;Countess\n"
        "TEL:+14155550123\n"
        "END:VCARD\n"
        "BEGIN:VCARD\n"
        "TEL:+14155550124\n"
        "END:VCARD\n"));

    QCOMPARE(result.skipped, 0);
    QCOMPARE(result.contacts.size(), qsizetype(2));

    QCOMPARE(result.contacts.at(0).firstName, QStringLiteral("Ada"));
    QCOMPARE(result.contacts.at(0).lastName, QStringLiteral("Lovelace"));

    // A phone number alone is enough: the block is not rejected for its name.
    QVERIFY(result.contacts.at(1).firstName.isEmpty());
    QVERIFY(result.contacts.at(1).lastName.isEmpty());
    QCOMPARE(result.contacts.at(1).phone, QStringLiteral("+14155550124"));
}

void ContactImportParserTest::decodesEscapedText()
{
    const ContactImportResult structured = parseVCards(QStringLiteral(
        "BEGIN:VCARD\n"
        "N:Smith\\;Jones;Ada\n"
        "TEL:+14155550123\n"
        "END:VCARD\n"));

    QCOMPARE(structured.contacts.size(), qsizetype(1));
    QCOMPARE(structured.contacts.first().firstName, QStringLiteral("Ada"));
    QCOMPARE(structured.contacts.first().lastName, QStringLiteral("Smith;Jones"));

    const ContactImportResult formatted = parseVCards(QStringLiteral(
        "BEGIN:VCARD\n"
        "FN:Ada\\, Countess\\nLovelace\n"
        "TEL:+14155550123\n"
        "END:VCARD\n"));

    QCOMPARE(formatted.contacts.size(), qsizetype(1));
    QCOMPARE(formatted.contacts.first().firstName,
             QStringLiteral("Ada, Countess\nLovelace"));
}

void ContactImportParserTest::takesFirstTelAndEmailValues()
{
    const ContactImportResult result = parseVCards(QStringLiteral(
        "BEGIN:VCARD\n"
        "FN:First Values\n"
        "TEL;TYPE=HOME:\n"
        "TEL;TYPE=CELL:+1 555 000 1111\n"
        "TEL;TYPE=WORK:+1 555 000 2222\n"
        "EMAIL:\n"
        "EMAIL:first@example.com\n"
        "EMAIL:second@example.com\n"
        "END:VCARD\n"));

    QCOMPARE(result.contacts.size(), qsizetype(1));
    QCOMPARE(result.contacts.first().phone, QStringLiteral("+15550001111"));
    QCOMPARE(result.contacts.first().email, QStringLiteral("first@example.com"));
}

void ContactImportParserTest::unfoldsFoldedLines()
{
    const ContactImportResult result = parseVCards(QStringLiteral(
        "BEGIN:VCARD\r\n"
        "FN:Ada\r\n"
        "  Lovelace\r\n"
        "TEL:+1 415 555\r\n"
        "\t-0123\r\n"
        "EMAIL:ada@\r\n"
        " example.com\r\n"
        "END:VCARD\r\n"));

    QCOMPARE(result.contacts.size(), qsizetype(1));
    QCOMPARE(result.contacts.first().firstName, QStringLiteral("Ada Lovelace"));
    QCOMPARE(result.contacts.first().phone, QStringLiteral("+14155550123"));
    QCOMPARE(result.contacts.first().email, QStringLiteral("ada@example.com"));
}

void ContactImportParserTest::handlesUrisAndGroups()
{
    const ContactImportResult result = parseVCards(QStringLiteral(
        "begin:vcard\n"
        "item1.TEL;TYPE=\"voice:home\":tel:+1-555-000-1111\n"
        "item2.EMAIL;TYPE=INTERNET:mailto:user@example.com\n"
        "END:VCARD\n"));

    QCOMPARE(result.skipped, 0);
    QCOMPARE(result.contacts.size(), qsizetype(1));
    QCOMPARE(result.contacts.first().phone, QStringLiteral("+15550001111"));
    QCOMPARE(result.contacts.first().email, QStringLiteral("user@example.com"));
}

void ContactImportParserTest::skipsBlocksWithoutUsablePhone()
{
    const ContactImportResult result = parseVCards(QStringLiteral(
        "BEGIN:VCARD\n"
        "FN:No Phone\n"
        "END:VCARD\n"
        "BEGIN:VCARD\n"
        "FN:Too Short\n"
        "TEL:1234\n"
        "END:VCARD\n"
        "BEGIN:VCARD\n"
        "FN:Too Long\n"
        "TEL:+1234567890123456\n"
        "END:VCARD\n"
        "BEGIN:VCARD\n"
        "FN:Valid\n"
        "TEL:+1 555 000 9999\n"
        "END:VCARD\n"));

    QCOMPARE(result.skipped, 3);
    QCOMPARE(result.contacts.size(), qsizetype(1));
    QCOMPARE(result.contacts.first().firstName, QStringLiteral("Valid"));
    QCOMPARE(result.contacts.first().phone, QStringLiteral("+15550009999"));
}

void ContactImportParserTest::ignoresTextOutsideBlocks()
{
    const ContactImportResult result = parseVCards(QStringLiteral(
        "junk before the cards\n"
        "END:VCARD\n"
        "BEGIN:VCARD\n"
        "FN:First\n"
        "TEL:+15550000001\n"
        "END:VCARD\n"
        "junk between the cards\n"
        "BEGIN:VCARD\n"
        "FN:Second\n"
        "TEL:+15550000002\n"
        "END:VCARD\n"
        "junk after the cards\n"));

    QCOMPARE(result.skipped, 0);
    QCOMPARE(result.contacts.size(), qsizetype(2));
    QCOMPARE(result.contacts.at(0).firstName, QStringLiteral("First"));
    QCOMPARE(result.contacts.at(1).firstName, QStringLiteral("Second"));
}

void ContactImportParserTest::countsUnterminatedBlocks()
{
    const ContactImportResult result = parseVCards(QStringLiteral(
        "BEGIN:VCARD\n"
        "FN:Complete\n"
        "TEL:+15550000001\n"
        "END:VCARD\n"
        "BEGIN:VCARD\n"
        "FN:Unterminated\n"
        "TEL:+15550000002\n"));

    QCOMPARE(result.contacts.size(), qsizetype(1));
    QCOMPARE(result.contacts.first().firstName, QStringLiteral("Complete"));
    QCOMPARE(result.skipped, 1);
}

void ContactImportParserTest::handlesEmptyInput()
{
    for (const QString& text : {QString(), QStringLiteral("   \r\n\t\r\n"),
                                QStringLiteral("not a vcard")}) {
        const ContactImportResult result = parseVCards(text);
        QVERIFY(result.contacts.isEmpty());
        QCOMPARE(result.skipped, 0);
    }
}

QTEST_GUILESS_MAIN(ContactImportParserTest)
#include "tst_contact_import_parser.moc"
