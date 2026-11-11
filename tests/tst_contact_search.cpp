#include "telegram/ContactSearch.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject jsonObject(const QByteArray& json)
{
    return QJsonDocument::fromJson(json).object();
}

QJsonObject contactsResponse(const QByteArray& usersJson)
{
    return jsonObject(QByteArrayLiteral("{\"users\":") + usersJson + QByteArrayLiteral("}"));
}

} // namespace

class ContactSearchTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsSearchContactsRequest();
    void clampsLimit();
    void parsesContacts();
    void readsBotAndPremiumFlags();
    void prefersActiveUsername();
    void fallsBackForNamelessUsers();
    void handlesEmptyAndMalformedResponses();
};

void ContactSearchTest::buildsSearchContactsRequest()
{
    const QJsonObject request = searchContactsRequest(QStringLiteral("ada"), 20);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("searchContacts"));
    QCOMPARE(request.value(QStringLiteral("query")).toString(), QStringLiteral("ada"));
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 20);
    QCOMPARE(request.keys().size(), qsizetype(3));

    // Surrounding whitespace is not part of the query TDLib should match.
    QCOMPARE(searchContactsRequest(QStringLiteral("  ada lovelace  "), 5)
                 .value(QStringLiteral("query"))
                 .toString(),
             QStringLiteral("ada lovelace"));

    // An empty query still produces a well-formed request.
    const QJsonObject empty = searchContactsRequest(QString(), 10);
    QCOMPARE(empty.value(QStringLiteral("@type")).toString(),
             QStringLiteral("searchContacts"));
    QVERIFY(empty.value(QStringLiteral("query")).toString().isEmpty());
    QCOMPARE(empty.value(QStringLiteral("limit")).toInt(), 10);
}

void ContactSearchTest::clampsLimit()
{
    const auto limitOf = [](int limit) {
        return searchContactsRequest(QStringLiteral("q"), limit)
            .value(QStringLiteral("limit"))
            .toInt();
    };

    QCOMPARE(limitOf(0), 1);
    QCOMPARE(limitOf(-25), 1);
    QCOMPARE(limitOf(1), 1);
    QCOMPARE(limitOf(50), 50);
    QCOMPARE(limitOf(100), 100);
    QCOMPARE(limitOf(101), 100);
    QCOMPARE(limitOf(100000), 100);
}

void ContactSearchTest::parsesContacts()
{
    const QVector<ContactInfo> contacts = contactsFromResponse(contactsResponse(R"JSON([
        {
            "@type": "user",
            "id": 1234567890123,
            "first_name": "Ada",
            "last_name": "Lovelace",
            "usernames": {
                "@type": "usernames",
                "active_usernames": ["ada"],
                "disabled_usernames": [],
                "editable_username": "ada"
            },
            "is_premium": false,
            "type": {"@type": "userTypeRegular"},
            "profile_photo": {
                "@type": "profilePhoto",
                "id": 900,
                "small": {"@type": "file", "id": 4242},
                "big": {"@type": "file", "id": 4343}
            }
        },
        {
            "@type": "user",
            "id": 777,
            "first_name": "Grace",
            "last_name": "Hopper",
            "type": {"@type": "userTypeRegular"},
            "profile_photo": {
                "@type": "profilePhoto",
                "id": 901,
                "big": {"@type": "file", "id": 5252}
            }
        }
    ])JSON"));

    QCOMPARE(contacts.size(), qsizetype(2));

    QCOMPARE(contacts.at(0).userId, qint64(1234567890123));
    QCOMPARE(contacts.at(0).displayName, QStringLiteral("Ada Lovelace"));
    QCOMPARE(contacts.at(0).username, QStringLiteral("ada"));
    QCOMPARE(contacts.at(0).photoFileId, 4242);
    QVERIFY(!contacts.at(0).isBot);
    QVERIFY(!contacts.at(0).isPremium);

    // No `small` file: the big one is the only usable thumbnail.
    QCOMPARE(contacts.at(1).userId, qint64(777));
    QCOMPARE(contacts.at(1).displayName, QStringLiteral("Grace Hopper"));
    QVERIFY(contacts.at(1).username.isEmpty());
    QCOMPARE(contacts.at(1).photoFileId, 5252);

    // A user without a profile photo reports no file id.
    const QVector<ContactInfo> bare = contactsFromResponse(contactsResponse(R"JSON([
        {"@type": "user", "id": 8, "first_name": "Alan"}
    ])JSON"));
    QCOMPARE(bare.size(), qsizetype(1));
    QCOMPARE(bare.at(0).photoFileId, 0);
}

void ContactSearchTest::readsBotAndPremiumFlags()
{
    const QVector<ContactInfo> contacts = contactsFromResponse(contactsResponse(R"JSON([
        {
            "@type": "user",
            "id": 11,
            "first_name": "Helper",
            "is_bot": true,
            "is_premium": true,
            "type": {"@type": "userTypeBot", "can_join_groups": true}
        },
        {
            "@type": "user",
            "id": 12,
            "first_name": "Grace",
            "is_premium": true,
            "type": {"@type": "userTypeRegular"}
        },
        {
            "@type": "user",
            "id": 13,
            "first_name": "Plain",
            "type": {"@type": "userTypeRegular"}
        }
    ])JSON"));

    QCOMPARE(contacts.size(), qsizetype(3));
    QVERIFY(contacts.at(0).isBot);
    QVERIFY(contacts.at(0).isPremium);
    QVERIFY(!contacts.at(1).isBot);
    QVERIFY(contacts.at(1).isPremium);
    QVERIFY(!contacts.at(2).isBot);
    QVERIFY(!contacts.at(2).isPremium);
}

void ContactSearchTest::prefersActiveUsername()
{
    const QVector<ContactInfo> contacts = contactsFromResponse(contactsResponse(R"JSON([
        {
            "@type": "user",
            "id": 21,
            "first_name": "Neo",
            "username": "old_handle",
            "usernames": {
                "@type": "usernames",
                "active_username": "@neo",
                "active_usernames": ["@trinity"]
            }
        },
        {
            "@type": "user",
            "id": 22,
            "first_name": "Morpheus",
            "username": "old_handle",
            "usernames": {"@type": "usernames", "active_usernames": ["@morpheus"]}
        },
        {
            "@type": "user",
            "id": 23,
            "first_name": "Legacy",
            "username": "@legacy_handle"
        },
        {
            "@type": "user",
            "id": 24,
            "first_name": "NoHandle"
        }
    ])JSON"));

    QCOMPARE(contacts.size(), qsizetype(4));
    QCOMPARE(contacts.at(0).username, QStringLiteral("neo"));
    QCOMPARE(contacts.at(1).username, QStringLiteral("morpheus"));
    QCOMPARE(contacts.at(2).username, QStringLiteral("legacy_handle"));
    QVERIFY(contacts.at(3).username.isEmpty());
}

void ContactSearchTest::fallsBackForNamelessUsers()
{
    const QVector<ContactInfo> contacts = contactsFromResponse(contactsResponse(R"JSON([
        {
            "@type": "user",
            "id": 31,
            "first_name": "   ",
            "last_name": " ",
            "usernames": {"@type": "usernames", "active_usernames": ["@ghost"]}
        },
        {"@type": "user", "id": 32},
        {"@type": "user", "id": 33, "first_name": "Solo"}
    ])JSON"));

    QCOMPARE(contacts.size(), qsizetype(3));
    QCOMPARE(contacts.at(0).displayName, QStringLiteral("ghost"));
    QCOMPARE(contacts.at(1).displayName, QStringLiteral("Telegram user"));
    QCOMPARE(contacts.at(2).displayName, QStringLiteral("Solo"));
}

void ContactSearchTest::handlesEmptyAndMalformedResponses()
{
    QVERIFY(contactsFromResponse(QJsonObject{}).isEmpty());
    QVERIFY(contactsFromResponse(jsonObject(R"JSON({"users": []})JSON")).isEmpty());

    // `users` is not an array: nothing can be parsed.
    QVERIFY(contactsFromResponse(jsonObject(R"JSON({"users": "nope"})JSON")).isEmpty());
    QVERIFY(contactsFromResponse(jsonObject(R"JSON({"users": {"total_count": 0}})JSON"))
                .isEmpty());
    // A bare TDLib `users` object only carries ids, so it yields no contacts.
    QVERIFY(contactsFromResponse(jsonObject(R"JSON({
        "@type": "users",
        "total_count": 2,
        "user_ids": [1, 2]
    })JSON")).isEmpty());

    // Malformed entries are skipped while the valid ones keep their order.
    const QVector<ContactInfo> mixed = contactsFromResponse(contactsResponse(R"JSON([
        null,
        5,
        "user",
        {"@type": "user"},
        {"@type": "user", "id": 0, "first_name": "Zero"},
        {"@type": "user", "id": -4, "first_name": "Negative"},
        {"@type": "user", "id": 3, "first_name": "Three"},
        {"@type": "user", "id": "1", "first_name": "One"},
        {"@type": "user", "id": 2, "first_name": "Two"}
    ])JSON"));

    QCOMPARE(mixed.size(), qsizetype(3));
    QCOMPARE(mixed.at(0).userId, qint64(3));
    QCOMPARE(mixed.at(0).displayName, QStringLiteral("Three"));
    QCOMPARE(mixed.at(1).userId, qint64(1));
    QCOMPARE(mixed.at(1).displayName, QStringLiteral("One"));
    QCOMPARE(mixed.at(2).userId, qint64(2));
    QCOMPARE(mixed.at(2).displayName, QStringLiteral("Two"));
}

QTEST_GUILESS_MAIN(ContactSearchTest)
#include "tst_contact_search.moc"
