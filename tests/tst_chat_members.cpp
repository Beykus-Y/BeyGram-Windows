#include "telegram/ChatMembers.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject parseObject(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

// Requests are compared after a JSON round trip so the assertions see exactly
// what would be handed to TDLib.
QJsonObject roundTrip(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

} // namespace

class ChatMembersTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsPageRequest();
    void trimsQueryAndKeepsDefaultEmpty();
    void clampsPagingToSchemaRange();
    void preservesInt53ChatId();
    void parsesEveryStatus();
    void ownerAndAdminAreDistinct();
    void resolvesEmbeddedUserNameAndBot();
    void resolvesNamesFromUsersArray();
    void fallsBackToUsernameThenPlaceholder();
    void leavesNameEmptyWithoutUserData();
    void skipsMalformedMembers();
    void emptyOrMalformedResponseYieldsNothing();
    void preservesUnknownStatus();
    void ignoresNonObjectStatus();
    void keepsOrderAndDuplicates();
    void survivesSerialization();
};

void ChatMembersTest::buildsPageRequest()
{
    const QJsonObject request =
        getChatMembersRequest(-1001234567890LL, 40, 50, QStringLiteral("  ada  "));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getChatMembers"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(-1001234567890LL));
    QCOMPARE(request.value(QStringLiteral("offset")).toInt(), 40);
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 50);
    QCOMPARE(request.value(QStringLiteral("query")).toString(), QStringLiteral("ada"));
    QCOMPARE(request.size(), qsizetype(5));
}

void ChatMembersTest::trimsQueryAndKeepsDefaultEmpty()
{
    const QJsonObject request = getChatMembersRequest(42, 0, 10);
    QCOMPARE(request.value(QStringLiteral("query")).toString(), QString());
    QVERIFY(request.contains(QStringLiteral("query")));

    const QJsonObject padded =
        getChatMembersRequest(42, 0, 10, QStringLiteral("  Grace Hopper  "));
    QCOMPARE(padded.value(QStringLiteral("query")).toString(),
             QStringLiteral("Grace Hopper"));
}

void ChatMembersTest::clampsPagingToSchemaRange()
{
    const auto offsetOf = [](int offset) {
        return getChatMembersRequest(42, offset, 10).value(QStringLiteral("offset")).toInt();
    };
    const auto limitOf = [](int limit) {
        return getChatMembersRequest(42, 0, limit).value(QStringLiteral("limit")).toInt();
    };

    QCOMPARE(offsetOf(-50), 0);
    QCOMPARE(offsetOf(0), 0);
    QCOMPARE(offsetOf(25), 25);

    QCOMPARE(limitOf(-10), 1);
    QCOMPARE(limitOf(0), 1);
    QCOMPARE(limitOf(1), 1);
    QCOMPARE(limitOf(100), 100);
    QCOMPARE(limitOf(200), 200);
    QCOMPARE(limitOf(201), 200);
    QCOMPARE(limitOf(100000), 200);
}

void ChatMembersTest::preservesInt53ChatId()
{
    const QJsonObject request = getChatMembersRequest(9007199254740991LL, 0, 10);
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(9007199254740991LL));

    const QJsonObject negative =
        getChatMembersRequest(-1001234567890LL, 0, 10);
    QCOMPARE(negative.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(-1001234567890LL));
}

void ChatMembersTest::parsesEveryStatus()
{
    const QJsonObject response = parseObject(R"JSON({
        "@type": "chatMembers",
        "total_count": 6,
        "members": [
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 1},
                "status": {"@type": "chatMemberStatusCreator", "is_anonymous": false,
                           "is_member": true}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 2},
                "status": {"@type": "chatMemberStatusAdministrator",
                           "can_be_edited": false, "rights": {}}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 3},
                "status": {"@type": "chatMemberStatusMember", "member_until_date": 0}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 4},
                "status": {"@type": "chatMemberStatusRestricted", "is_member": true,
                           "restricted_until_date": 0, "permissions": {}}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 5},
                "status": {"@type": "chatMemberStatusLeft"}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 6},
                "status": {"@type": "chatMemberStatusBanned", "banned_until_date": 0}
            }
        ]
    })JSON");

    const QVector<ChatMember> members = chatMembersFromResponse(response);
    QCOMPARE(members.size(), qsizetype(6));

    QCOMPARE(members.at(0).status, QStringLiteral("creator"));
    QVERIFY(members.at(0).isOwner);
    QVERIFY(!members.at(0).isAdmin);

    QCOMPARE(members.at(1).status, QStringLiteral("administrator"));
    QVERIFY(members.at(1).isAdmin);
    QVERIFY(!members.at(1).isOwner);

    QCOMPARE(members.at(2).status, QStringLiteral("member"));
    QCOMPARE(members.at(3).status, QStringLiteral("restricted"));
    QCOMPARE(members.at(4).status, QStringLiteral("left"));
    QCOMPARE(members.at(5).status, QStringLiteral("banned"));

    for (qsizetype index = 2; index < members.size(); ++index) {
        QVERIFY(!members.at(index).isAdmin);
        QVERIFY(!members.at(index).isOwner);
    }
}

void ChatMembersTest::ownerAndAdminAreDistinct()
{
    const QJsonObject response = parseObject(R"JSON({
        "@type": "chatMembers",
        "members": [
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 10},
                "status": {"@type": "chatMemberStatusCreator", "is_member": true}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 11},
                "status": {"@type": "chatMemberStatusAdministrator",
                           "can_be_edited": true, "rights": {}}
            }
        ]
    })JSON");

    const QVector<ChatMember> members = chatMembersFromResponse(response);
    QCOMPARE(members.size(), qsizetype(2));
    QVERIFY(members.at(0).isOwner);
    QVERIFY(!members.at(0).isAdmin);
    QVERIFY(members.at(1).isAdmin);
    QVERIFY(!members.at(1).isOwner);
}

void ChatMembersTest::resolvesEmbeddedUserNameAndBot()
{
    const QJsonObject response = parseObject(R"JSON({
        "@type": "chatMembers",
        "members": [
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 7},
                "status": {"@type": "chatMemberStatusAdministrator",
                           "can_be_edited": false, "rights": {}},
                "user": {
                    "@type": "user",
                    "id": 7,
                    "first_name": "Ada",
                    "last_name": "Lovelace",
                    "type": {"@type": "userTypeRegular"}
                }
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 8},
                "status": {"@type": "chatMemberStatusMember"},
                "user": {
                    "@type": "user",
                    "id": 8,
                    "first_name": "Helper",
                    "type": {"@type": "userTypeBot"}
                }
            }
        ]
    })JSON");

    const QVector<ChatMember> members = chatMembersFromResponse(response);
    QCOMPARE(members.size(), qsizetype(2));
    QCOMPARE(members.at(0).displayName, QStringLiteral("Ada Lovelace"));
    QVERIFY(!members.at(0).isBot);
    QCOMPARE(members.at(1).displayName, QStringLiteral("Helper"));
    QVERIFY(members.at(1).isBot);
}

void ChatMembersTest::resolvesNamesFromUsersArray()
{
    const QJsonObject response = parseObject(R"JSON({
        "@type": "chatMembers",
        "total_count": 3,
        "members": [
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 9},
                "status": {"@type": "chatMemberStatusMember"}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 10},
                "status": {"@type": "chatMemberStatusMember"}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 11},
                "status": {"@type": "chatMemberStatusMember"},
                "user": {"@type": "user", "id": 11}
            }
        ],
        "users": [
            {
                "@type": "user",
                "id": 9,
                "first_name": "Grace",
                "last_name": "Hopper",
                "type": {"@type": "userTypeRegular"}
            },
            {
                "@type": "user",
                "id": 10,
                "first_name": "Bot",
                "type": {"@type": "userTypeBot"}
            },
            {
                "@type": "user",
                "id": 11,
                "first_name": "Katherine",
                "last_name": "Johnson",
                "type": {"@type": "userTypeRegular"}
            }
        ]
    })JSON");

    const QVector<ChatMember> members = chatMembersFromResponse(response);
    QCOMPARE(members.size(), qsizetype(3));
    QCOMPARE(members.at(0).displayName, QStringLiteral("Grace Hopper"));
    QVERIFY(!members.at(0).isBot);
    QCOMPARE(members.at(1).displayName, QStringLiteral("Bot"));
    QVERIFY(members.at(1).isBot);
    // An id-only embedded user is completed from the response's users array.
    QCOMPARE(members.at(2).displayName, QStringLiteral("Katherine Johnson"));
}

void ChatMembersTest::fallsBackToUsernameThenPlaceholder()
{
    const QJsonObject response = parseObject(R"JSON({
        "@type": "chatMembers",
        "members": [
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 20},
                "status": {"@type": "chatMemberStatusMember"},
                "user": {
                    "@type": "user",
                    "id": 20,
                    "usernames": {
                        "@type": "usernames",
                        "active_usernames": ["ghost"],
                        "active_username": "",
                        "disabled_usernames": []
                    }
                }
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 21},
                "status": {"@type": "chatMemberStatusMember"},
                "user": {"@type": "user", "id": 21}
            }
        ]
    })JSON");

    const QVector<ChatMember> members = chatMembersFromResponse(response);
    QCOMPARE(members.size(), qsizetype(2));
    QCOMPARE(members.at(0).displayName, QStringLiteral("ghost"));
    QCOMPARE(members.at(1).displayName, QStringLiteral("Telegram user"));
}

void ChatMembersTest::leavesNameEmptyWithoutUserData()
{
    // A bare chatMember carries no name; the parser must not invent one.
    const QJsonObject response = parseObject(R"JSON({
        "@type": "chatMembers",
        "members": [
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 30},
                "tag": "Moderator",
                "status": {"@type": "chatMemberStatusMember"}
            }
        ]
    })JSON");

    const QVector<ChatMember> members = chatMembersFromResponse(response);
    QCOMPARE(members.size(), qsizetype(1));
    QVERIFY(members.at(0).displayName.isEmpty());
    QCOMPARE(members.at(0).status, QStringLiteral("member"));
}

void ChatMembersTest::skipsMalformedMembers()
{
    const QJsonObject response = parseObject(R"JSON({
        "@type": "chatMembers",
        "members": [
            "not an object",
            null,
            {},
            {"@type": "chatMember", "status": {"@type": "chatMemberStatusMember"}},
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 0},
                "status": {"@type": "chatMemberStatusMember"}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": -5},
                "status": {"@type": "chatMemberStatusMember"}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderChat", "chat_id": -1001234567890},
                "status": {"@type": "chatMemberStatusBanned"}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 40},
                "status": {"@type": "chatMemberStatusMember"}
            },
            {
                "@type": "chatMember",
                "user_id": 41,
                "status": {"@type": "chatMemberStatusAdministrator",
                           "can_be_edited": false, "rights": {}}
            },
            {
                "@type": "chatMember",
                "id": 42,
                "status": {"@type": "chatMemberStatusCreator", "is_member": true}
            }
        ]
    })JSON");

    const QVector<ChatMember> members = chatMembersFromResponse(response);
    QCOMPARE(members.size(), qsizetype(3));
    QCOMPARE(members.at(0).userId, qint64(40));
    QCOMPARE(members.at(1).userId, qint64(41));
    QVERIFY(members.at(1).isAdmin);
    QCOMPARE(members.at(2).userId, qint64(42));
    QVERIFY(members.at(2).isOwner);
}

void ChatMembersTest::emptyOrMalformedResponseYieldsNothing()
{
    QVERIFY(chatMembersFromResponse(QJsonObject{}).isEmpty());
    QVERIFY(chatMembersFromResponse(
                parseObject(R"({"@type": "chatMembers", "total_count": 5})"))
                .isEmpty());
    QVERIFY(chatMembersFromResponse(
                parseObject(R"({"@type": "chatMembers", "members": null})"))
                .isEmpty());
    QVERIFY(chatMembersFromResponse(
                parseObject(R"({"@type": "chatMembers", "members": {}})"))
                .isEmpty());
    QVERIFY(chatMembersFromResponse(
                parseObject(R"({"@type": "chatMembers", "members": "none"})"))
                .isEmpty());
    QVERIFY(chatMembersFromResponse(
                parseObject(R"({"@type": "chatMembers", "members": []})"))
                .isEmpty());
}

void ChatMembersTest::preservesUnknownStatus()
{
    const QJsonObject response = parseObject(R"JSON({
        "@type": "chatMembers",
        "members": [
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 50},
                "status": {"@type": "chatMemberStatusFutureRole"}
            }
        ]
    })JSON");

    const QVector<ChatMember> members = chatMembersFromResponse(response);
    QCOMPARE(members.size(), qsizetype(1));
    QCOMPARE(members.at(0).status, QStringLiteral("chatMemberStatusFutureRole"));
    QVERIFY(!members.at(0).isAdmin);
    QVERIFY(!members.at(0).isOwner);
}

void ChatMembersTest::ignoresNonObjectStatus()
{
    const QJsonObject response = parseObject(R"JSON({
        "@type": "chatMembers",
        "members": [
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 60},
                "status": "chatMemberStatusCreator"
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 61},
                "status": null
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 62},
                "status": {}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 63},
                "status": {"@type": 7}
            }
        ]
    })JSON");

    const QVector<ChatMember> members = chatMembersFromResponse(response);
    QCOMPARE(members.size(), qsizetype(4));
    for (const ChatMember& member : members) {
        QVERIFY(member.status.isEmpty());
        QVERIFY(!member.isAdmin);
        QVERIFY(!member.isOwner);
    }
}

void ChatMembersTest::keepsOrderAndDuplicates()
{
    const QJsonObject response = parseObject(R"JSON({
        "@type": "chatMembers",
        "members": [
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 70},
                "status": {"@type": "chatMemberStatusMember"}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 71},
                "status": {"@type": "chatMemberStatusCreator", "is_member": true}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 70},
                "status": {"@type": "chatMemberStatusAdministrator",
                           "can_be_edited": false, "rights": {}}
            }
        ]
    })JSON");

    const QVector<ChatMember> members = chatMembersFromResponse(response);
    QCOMPARE(members.size(), qsizetype(3));
    QCOMPARE(members.at(0).userId, qint64(70));
    QCOMPARE(members.at(1).userId, qint64(71));
    QCOMPARE(members.at(2).userId, qint64(70));
}

void ChatMembersTest::survivesSerialization()
{
    const QJsonObject request = roundTrip(
        getChatMembersRequest(-1001234567890LL, 20, 100, QStringLiteral("  ada  ")));
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getChatMembers"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(-1001234567890LL));
    QCOMPARE(request.value(QStringLiteral("offset")).toInt(), 20);
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 100);
    QCOMPARE(request.value(QStringLiteral("query")).toString(), QStringLiteral("ada"));

    const QJsonObject response = roundTrip(parseObject(R"JSON({
        "@type": "chatMembers",
        "members": [
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 80},
                "status": {"@type": "chatMemberStatusCreator", "is_member": true},
                "user": {"@type": "user", "id": 80, "first_name": "Ada"}
            }
        ]
    })JSON"));

    const QVector<ChatMember> members = chatMembersFromResponse(response);
    QCOMPARE(members.size(), qsizetype(1));
    QCOMPARE(members.at(0).userId, qint64(80));
    QCOMPARE(members.at(0).displayName, QStringLiteral("Ada"));
    QCOMPARE(members.at(0).status, QStringLiteral("creator"));
    QVERIFY(members.at(0).isOwner);
}

QTEST_GUILESS_MAIN(ChatMembersTest)
#include "tst_chat_members.moc"
