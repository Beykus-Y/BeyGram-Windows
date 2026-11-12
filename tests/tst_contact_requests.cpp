#include "telegram/ContactRequests.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

QByteArray wireBytes(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

QJsonObject contactOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("contact")).toObject();
}

QJsonArray contactsOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("contacts")).toArray();
}

/// Keeps the exact digits of every id, so a truncated int53 shows up as a
/// different string instead of a rounding difference.
QStringList idsOf(const QJsonObject& request)
{
    QStringList ids;
    for (const QJsonValue& value : request.value(QStringLiteral("user_ids")).toArray())
        ids.append(QString::number(value.toVariant().toLongLong()));
    return ids;
}

QString emojiName(int count)
{
    const QString emoji = QString::fromUtf8("\xF0\x9F\x98\x80");
    QString name;
    for (int i = 0; i < count; ++i)
        name.append(emoji);
    return name;
}

} // namespace

class ContactRequestsTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsGetContactsRequest();
    void getContactsRequestIgnoresThePagingWindow();
    void buildsAddContactRequestShape();
    void addContactNeverSharesOwnNumber();
    void trimsContactFields();
    void clampsNamesTo64CodePoints();
    void preservesLargeUserIds();
    void buildsRemoveContactsRequest();
    void dropsInvalidAndRepeatedRemoveIds();
    void buildsImportContactsRequest();
    void importContactsKeepsOneEntryPerInputInOrder();
    void importContactsDoesNotSplitNames();
    void importContactsHandlesEmptyInput();
    void writesNoCorrelationOrNoteFields();
};

void ContactRequestsTest::buildsGetContactsRequest()
{
    const QJsonObject request = serialized(getContactsRequest(0, 100));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getContacts"));
    QCOMPARE(request.keys().size(), qsizetype(1));
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void ContactRequestsTest::getContactsRequestIgnoresThePagingWindow()
{
    // This schema's getContacts has no offset/limit fields: TDLib returns the
    // whole contact list, so no window may leak into the payload.
    const QByteArray plain = wireBytes(getContactsRequest(0, 100));

    const QVector<QPair<int, int>> windows{
        {0, 0}, {-5, 0}, {7, 250}, {1000000, 1000000},
        {std::numeric_limits<int>::max(), std::numeric_limits<int>::max()},
        {std::numeric_limits<int>::min(), -1},
    };
    for (const QPair<int, int>& window : windows) {
        const QJsonObject request = serialized(getContactsRequest(window.first, window.second));
        QCOMPARE(wireBytes(request), plain);
        QVERIFY(!request.contains(QStringLiteral("offset")));
        QVERIFY(!request.contains(QStringLiteral("limit")));
    }
}

void ContactRequestsTest::buildsAddContactRequestShape()
{
    const QJsonObject request =
        serialized(addContactRequest(123456789, QStringLiteral("Ada"),
                                     QStringLiteral("Lovelace"),
                                     QStringLiteral("+44 7700 900123")));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("addContact"));
    QCOMPARE(numberAt(request, QStringLiteral("user_id")), qint64(123456789));
    QVERIFY(request.value(QStringLiteral("user_id")).isDouble());
    QCOMPARE(request.keys().size(), qsizetype(4));

    const QJsonObject contact = contactOf(request);
    QCOMPARE(contact.value(QStringLiteral("@type")).toString(),
             QStringLiteral("importedContact"));
    QCOMPARE(contact.value(QStringLiteral("phone_number")).toString(),
             QStringLiteral("+44 7700 900123"));
    QCOMPARE(contact.value(QStringLiteral("first_name")).toString(), QStringLiteral("Ada"));
    QCOMPARE(contact.value(QStringLiteral("last_name")).toString(),
             QStringLiteral("Lovelace"));
    QCOMPARE(contact.keys().size(), qsizetype(4));
}

void ContactRequestsTest::addContactNeverSharesOwnNumber()
{
    // Adding a contact must not hand out the current user's phone number; the
    // flag is always written, never omitted, so TDLib never falls back.
    const QJsonObject withNumber =
        serialized(addContactRequest(7, QStringLiteral("Ada"), QStringLiteral("Lovelace"),
                                     QStringLiteral("+15551234567")));
    QVERIFY(withNumber.contains(QStringLiteral("share_phone_number")));
    QVERIFY(withNumber.value(QStringLiteral("share_phone_number")).isBool());
    QVERIFY(!withNumber.value(QStringLiteral("share_phone_number")).toBool());

    // TDLib allows an empty number when the caller does not know it.
    const QJsonObject withoutNumber =
        serialized(addContactRequest(7, QStringLiteral("Ada"), QString(), QString()));
    QCOMPARE(contactOf(withoutNumber).value(QStringLiteral("phone_number")).toString(),
             QString());
    QVERIFY(!withoutNumber.value(QStringLiteral("share_phone_number")).toBool());
}

void ContactRequestsTest::trimsContactFields()
{
    const QJsonObject contact =
        contactOf(serialized(addContactRequest(7, QStringLiteral("  Ada  "),
                                               QStringLiteral("  Lovelace "),
                                               QStringLiteral("  +15551234567  "))));

    QCOMPARE(contact.value(QStringLiteral("first_name")).toString(), QStringLiteral("Ada"));
    QCOMPARE(contact.value(QStringLiteral("last_name")).toString(),
             QStringLiteral("Lovelace"));
    QCOMPARE(contact.value(QStringLiteral("phone_number")).toString(),
             QStringLiteral("+15551234567"));

    // An empty last name is valid and stays an empty string, not a missing key.
    const QJsonObject single =
        contactOf(serialized(addContactRequest(7, QStringLiteral("Ada"), QString(), QString())));
    QVERIFY(single.contains(QStringLiteral("last_name")));
    QCOMPARE(single.value(QStringLiteral("last_name")).toString(), QString());
}

void ContactRequestsTest::clampsNamesTo64CodePoints()
{
    const QString longFirst(70, QLatin1Char('a'));
    const QString longLast(80, QLatin1Char('b'));

    const QJsonObject contact =
        contactOf(serialized(addContactRequest(7, longFirst, longLast, QString())));
    QCOMPARE(contact.value(QStringLiteral("first_name")).toString(), QString(64, QLatin1Char('a')));
    QCOMPARE(contact.value(QStringLiteral("last_name")).toString(), QString(64, QLatin1Char('b')));

    // A name that already fits is passed through untouched.
    const QString exact(64, QLatin1Char('c'));
    QCOMPARE(contactOf(serialized(addContactRequest(7, exact, QString(), QString())))
                 .value(QStringLiteral("first_name"))
                 .toString(),
             exact);

    // 70 emoji are 140 UTF-16 units but only 70 code points: the cut happens on
    // a code point boundary and never leaves a lone surrogate behind.
    const QString emoji = emojiName(70);
    const QString clamped = contactOf(serialized(addContactRequest(7, emoji, QString(), QString())))
                                .value(QStringLiteral("first_name"))
                                .toString();
    QCOMPARE(clamped.size(), qsizetype(128));
    QCOMPARE(clamped, emojiName(64));
    QVERIFY(!clamped.at(clamped.size() - 1).isHighSurrogate());

    // The import path is clamped the same way, so one long device contact
    // cannot make TDLib reject the whole batch.
    const QJsonArray imported =
        contactsOf(serialized(importContactsRequest({{QStringLiteral("+15551234567"), emoji}})));
    QCOMPARE(imported.size(), qsizetype(1));
    QCOMPARE(imported.at(0).toObject().value(QStringLiteral("first_name")).toString(),
             emojiName(64));
}

void ContactRequestsTest::preservesLargeUserIds()
{
    // Telegram user ids fit in int53; they must not be rounded by the JSON
    // number or truncated to 32 bits.
    const qint64 userId = 9007199254740991LL; // 2^53 - 1

    const QJsonObject add = serialized(addContactRequest(userId, QStringLiteral("A"),
                                                         QStringLiteral("B"), QString()));
    QCOMPARE(numberAt(add, QStringLiteral("user_id")), userId);

    const QStringList ids = idsOf(serialized(removeContactsRequest({userId, 1})));
    QCOMPARE(ids, QStringList({QString::number(userId), QStringLiteral("1")}));
}

void ContactRequestsTest::buildsRemoveContactsRequest()
{
    const QJsonObject request = serialized(removeContactsRequest({4, 2, 9}));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("removeContacts"));
    QCOMPARE(request.keys().size(), qsizetype(2));
    QVERIFY(request.value(QStringLiteral("user_ids")).isArray());
    QCOMPARE(idsOf(request), QStringList({QStringLiteral("4"), QStringLiteral("2"),
                                          QStringLiteral("9")}));
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void ContactRequestsTest::dropsInvalidAndRepeatedRemoveIds()
{
    // 0 and negative values are not user ids; repeats would only make TDLib
    // look the same user up twice.
    const QStringList ids =
        idsOf(serialized(removeContactsRequest({0, -5, 7, 7, 3, -100, 3})));
    QCOMPARE(ids, QStringList({QStringLiteral("7"), QStringLiteral("3")}));

    // Nothing left to delete is still a well-formed request; TDLib answers it
    // with Ok without touching the server.
    const QJsonObject empty = serialized(removeContactsRequest({}));
    QCOMPARE(empty.value(QStringLiteral("@type")).toString(),
             QStringLiteral("removeContacts"));
    QVERIFY(empty.value(QStringLiteral("user_ids")).isArray());
    QVERIFY(empty.value(QStringLiteral("user_ids")).toArray().isEmpty());
}

void ContactRequestsTest::buildsImportContactsRequest()
{
    const QJsonObject request = serialized(importContactsRequest({
        {QStringLiteral("  +15551234567  "), QStringLiteral("  Ada Lovelace  ")},
        {QStringLiteral("+15557654321"), QStringLiteral("Grace Hopper")},
    }));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("importContacts"));
    QCOMPARE(request.keys().size(), qsizetype(2));

    const QJsonArray contacts = contactsOf(request);
    QCOMPARE(contacts.size(), qsizetype(2));

    const QJsonObject first = contacts.at(0).toObject();
    QCOMPARE(first.value(QStringLiteral("@type")).toString(),
             QStringLiteral("importedContact"));
    QCOMPARE(first.value(QStringLiteral("phone_number")).toString(),
             QStringLiteral("+15551234567"));
    QCOMPARE(first.value(QStringLiteral("first_name")).toString(),
             QStringLiteral("Ada Lovelace"));
    QCOMPARE(first.value(QStringLiteral("last_name")).toString(), QString());
    QCOMPARE(first.keys().size(), qsizetype(4));

    const QJsonObject second = contacts.at(1).toObject();
    QCOMPARE(second.value(QStringLiteral("phone_number")).toString(),
             QStringLiteral("+15557654321"));
    QCOMPARE(second.value(QStringLiteral("first_name")).toString(),
             QStringLiteral("Grace Hopper"));
}

void ContactRequestsTest::importContactsKeepsOneEntryPerInputInOrder()
{
    // ImportedContacts.user_ids is positional, so every pair keeps its slot
    // even when its number is unknown and it can never be matched.
    const QJsonArray contacts = contactsOf(serialized(importContactsRequest({
        {QString(), QStringLiteral("No Number")},
        {QStringLiteral("+15550000001"), QStringLiteral("Ada")},
        {QStringLiteral("+15550000002"), QString()},
    })));

    QCOMPARE(contacts.size(), qsizetype(3));
    QCOMPARE(contacts.at(0).toObject().value(QStringLiteral("phone_number")).toString(),
             QString());
    QCOMPARE(contacts.at(1).toObject().value(QStringLiteral("phone_number")).toString(),
             QStringLiteral("+15550000001"));
    QCOMPARE(contacts.at(2).toObject().value(QStringLiteral("phone_number")).toString(),
             QStringLiteral("+15550000002"));
    QCOMPARE(contacts.at(2).toObject().value(QStringLiteral("first_name")).toString(),
             QString());
}

void ContactRequestsTest::importContactsDoesNotSplitNames()
{
    // A display name says nothing about where the family name starts, so the
    // whole name stays in first_name instead of being guessed apart.
    const QJsonArray contacts = contactsOf(serialized(importContactsRequest({
        {QStringLiteral("+15550000001"), QStringLiteral("van der Berg")},
        {QStringLiteral("+15550000002"), QStringLiteral("Анна-Мария Ивановна")},
    })));

    QCOMPARE(contacts.at(0).toObject().value(QStringLiteral("first_name")).toString(),
             QStringLiteral("van der Berg"));
    QVERIFY(contacts.at(0).toObject().value(QStringLiteral("last_name")).toString().isEmpty());
    QCOMPARE(contacts.at(1).toObject().value(QStringLiteral("first_name")).toString(),
             QStringLiteral("Анна-Мария Ивановна"));
    QVERIFY(contacts.at(1).toObject().value(QStringLiteral("last_name")).toString().isEmpty());
}

void ContactRequestsTest::importContactsHandlesEmptyInput()
{
    const QJsonObject request = serialized(importContactsRequest({}));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("importContacts"));
    QVERIFY(request.value(QStringLiteral("contacts")).isArray());
    QVERIFY(contactsOf(request).isEmpty());
}

void ContactRequestsTest::writesNoCorrelationOrNoteFields()
{
    const QJsonObject add = serialized(addContactRequest(7, QStringLiteral("Ada"),
                                                         QStringLiteral("Lovelace"),
                                                         QStringLiteral("+15551234567")));
    const QJsonObject remove = serialized(removeContactsRequest({7}));
    const QJsonObject importRequest = serialized(importContactsRequest(
        {{QStringLiteral("+15551234567"), QStringLiteral("Ada Lovelace")}}));
    const QJsonObject getRequest = serialized(getContactsRequest(0, 50));

    // The transport owns @extra; the builders never guess a correlation id.
    for (const QJsonObject& request : {add, remove, importRequest, getRequest})
        QVERIFY(!request.contains(QStringLiteral("@extra")));

    // Omitting `note` is what keeps an existing contact note intact; an empty
    // formattedText would clear it.
    QVERIFY(!contactOf(add).contains(QStringLiteral("note")));
    QVERIFY(!contactsOf(importRequest).at(0).toObject().contains(QStringLiteral("note")));
}

QTEST_GUILESS_MAIN(ContactRequestsTest)
#include "tst_contact_requests.moc"
