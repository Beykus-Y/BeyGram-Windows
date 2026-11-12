#include "telegram/GroupAdministration.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

// Supergroups and channels use negative chat ids in TDLib; a chat id is only
// invalid when it is 0.
constexpr qint64 kChatId = -1001234567890LL;
constexpr qint64 kUserId = 42;

QJsonObject parseObject(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

// Requests and responses are compared after a JSON round trip so the assertions
// see exactly what would cross the TDLib JSON interface.
QJsonObject roundTrip(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

QJsonObject statusOf(const QString& status)
{
    return setChatMemberStatusRequest(kChatId, kUserId, status)
        .value(QStringLiteral("status"))
        .toObject();
}

QStringList administratorRightFlags()
{
    return {
        QStringLiteral("can_manage_chat"),
        QStringLiteral("can_change_info"),
        QStringLiteral("can_post_messages"),
        QStringLiteral("can_edit_messages"),
        QStringLiteral("can_delete_messages"),
        QStringLiteral("can_invite_users"),
        QStringLiteral("can_restrict_members"),
        QStringLiteral("can_pin_messages"),
        QStringLiteral("can_manage_topics"),
        QStringLiteral("can_promote_members"),
        QStringLiteral("can_manage_video_chats"),
        QStringLiteral("can_post_stories"),
        QStringLiteral("can_edit_stories"),
        QStringLiteral("can_delete_stories"),
        QStringLiteral("can_manage_direct_messages"),
        QStringLiteral("can_manage_tags"),
        QStringLiteral("is_anonymous"),
    };
}

} // namespace

class GroupAdministrationTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsStatusRequest();
    void administratorStatusDeniesEveryRight();
    void rejectsUnknownStatus();
    void rejectsInvalidStatusIds();
    void buildsAdministratorsRequest();
    void buildsMemberRequest();
    void buildsTagRequest();
    void trimsAndClearsTag();
    void buildsBanRequest();
    void normalizesBanUntilDate();
    void rejectsInvalidBanIds();
    void parsesAllModelledRights();
    void deniesMissingAndMalformedRights();
    void fallsBackToCanBanUsers();
    void parsesMembersResponse();
    void parsesCreatorAnonymous();
    void skipsMalformedRows();
    void parsesSingleChatMember();
    void parsesChatAdministratorsAnswer();
    void resolvesNamesFromEmbeddedUserAndUsersArray();
    void resolvesNameFromUsername();
    void leavesNameEmptyWithoutUserData();
    void keepsOrderAndDuplicates();
    void emptyOrMalformedResponseYieldsNothing();
    void survivesSerialization();
};

void GroupAdministrationTest::buildsStatusRequest()
{
    const QJsonObject request =
        setChatMemberStatusRequest(kChatId, kUserId, QStringLiteral("member"));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setChatMemberStatus"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), kChatId);
    QCOMPARE(request.size(), qsizetype(4));

    const QJsonObject sender = request.value(QStringLiteral("member_id")).toObject();
    QCOMPARE(sender.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSenderUser"));
    QCOMPARE(sender.value(QStringLiteral("user_id")).toVariant().toLongLong(), kUserId);

    const QJsonObject status = request.value(QStringLiteral("status")).toObject();
    QCOMPARE(status.value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatMemberStatusMember"));
    QCOMPARE(status.value(QStringLiteral("member_until_date")).toInt(), 0);

    const QJsonObject banned = statusOf(QStringLiteral("banned"));
    QCOMPARE(banned.value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatMemberStatusBanned"));
    QCOMPARE(banned.value(QStringLiteral("banned_until_date")).toInt(), 0);

    const QJsonObject left = statusOf(QStringLiteral("left"));
    QCOMPARE(left.value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatMemberStatusLeft"));
    QCOMPARE(left.size(), qsizetype(1));

    const QJsonObject administrator = statusOf(QStringLiteral("administrator"));
    QCOMPARE(administrator.value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatMemberStatusAdministrator"));
    QCOMPARE(administrator.value(QStringLiteral("can_be_edited")).toBool(), false);
}

void GroupAdministrationTest::administratorStatusDeniesEveryRight()
{
    const QJsonObject rights =
        statusOf(QStringLiteral("administrator")).value(QStringLiteral("rights")).toObject();

    QCOMPARE(rights.value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatAdministratorRights"));
    // @type plus every chatAdministratorRights flag: TDLib rejects a partial object.
    QCOMPARE(rights.size(), qsizetype(18));

    const QStringList flags = administratorRightFlags();
    for (const QString& flag : flags) {
        QVERIFY2(rights.contains(flag), qPrintable(flag));
        // The default value makes a missing flag fail, not silently pass.
        QCOMPARE(rights.value(flag).toBool(true), false);
    }
}

void GroupAdministrationTest::rejectsUnknownStatus()
{
    const char* const statuses[] = {
        "",
        "creator",
        "restricted",
        "Administrator",
        "administrator ",
        "chatMemberStatusAdministrator",
        "member\n",
    };

    for (const char* status : statuses) {
        const QJsonObject request =
            setChatMemberStatusRequest(kChatId, kUserId, QString::fromLatin1(status));
        QVERIFY2(request.isEmpty(), status);
    }
}

void GroupAdministrationTest::rejectsInvalidStatusIds()
{
    QVERIFY(setChatMemberStatusRequest(0, kUserId, QStringLiteral("member")).isEmpty());
    QVERIFY(setChatMemberStatusRequest(kChatId, 0, QStringLiteral("member")).isEmpty());
    QVERIFY(setChatMemberStatusRequest(kChatId, -1, QStringLiteral("member")).isEmpty());
    QVERIFY(setChatMemberStatusRequest(0, 0, QStringLiteral("administrator")).isEmpty());

    // A negative chat id names a supergroup or channel and must survive.
    const QJsonObject request =
        setChatMemberStatusRequest(kChatId, kUserId, QStringLiteral("administrator"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), kChatId);
}

void GroupAdministrationTest::buildsAdministratorsRequest()
{
    const QJsonObject request = getChatAdministratorsRequest(kChatId);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getChatAdministrators"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), kChatId);
    QCOMPARE(request.size(), qsizetype(2));

    QVERIFY(getChatAdministratorsRequest(0).isEmpty());
}

void GroupAdministrationTest::buildsMemberRequest()
{
    const QJsonObject request = getChatMemberRequest(kChatId, kUserId);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getChatMember"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), kChatId);
    QCOMPARE(request.size(), qsizetype(3));

    const QJsonObject sender = request.value(QStringLiteral("member_id")).toObject();
    QCOMPARE(sender.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSenderUser"));
    QCOMPARE(sender.value(QStringLiteral("user_id")).toVariant().toLongLong(), kUserId);

    QVERIFY(getChatMemberRequest(0, kUserId).isEmpty());
    QVERIFY(getChatMemberRequest(kChatId, 0).isEmpty());
    QVERIFY(getChatMemberRequest(kChatId, -7).isEmpty());
}

void GroupAdministrationTest::buildsTagRequest()
{
    const QJsonObject request =
        setChatMemberTagRequest(kChatId, kUserId, QStringLiteral("  Moderator  "));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setChatMemberTag"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), kChatId);
    QCOMPARE(request.value(QStringLiteral("user_id")).toVariant().toLongLong(), kUserId);
    QCOMPARE(request.value(QStringLiteral("tag")).toString(), QStringLiteral("Moderator"));
    QCOMPARE(request.size(), qsizetype(4));
}

void GroupAdministrationTest::trimsAndClearsTag()
{
    const auto tagOf = [](const QString& tag) {
        return setChatMemberTagRequest(kChatId, kUserId, tag)
            .value(QStringLiteral("tag"))
            .toString();
    };

    QCOMPARE(tagOf(QStringLiteral("   ")), QString());
    QCOMPARE(tagOf(QString()), QString());
    QCOMPARE(tagOf(QStringLiteral("Head Moderator")), QStringLiteral("Head Moderator"));
    QCOMPARE(tagOf(QStringLiteral("\tModerator\n")), QStringLiteral("Moderator"));

    QVERIFY(setChatMemberTagRequest(0, kUserId, QStringLiteral("Moderator")).isEmpty());
    QVERIFY(setChatMemberTagRequest(kChatId, 0, QStringLiteral("Moderator")).isEmpty());
    QVERIFY(setChatMemberTagRequest(kChatId, -1, QStringLiteral("Moderator")).isEmpty());
}

void GroupAdministrationTest::buildsBanRequest()
{
    const QJsonObject request = banChatMemberRequest(kChatId, kUserId, 1700000000LL, true);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("banChatMember"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), kChatId);
    QCOMPARE(request.value(QStringLiteral("banned_until_date")).toInt(), 1700000000);
    QCOMPARE(request.value(QStringLiteral("revoke_messages")).toBool(), true);
    QCOMPARE(request.size(), qsizetype(5));

    const QJsonObject sender = request.value(QStringLiteral("member_id")).toObject();
    QCOMPARE(sender.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSenderUser"));
    QCOMPARE(sender.value(QStringLiteral("user_id")).toVariant().toLongLong(), kUserId);

    const QJsonObject keep = banChatMemberRequest(kChatId, kUserId, 0, false);
    QCOMPARE(keep.value(QStringLiteral("banned_until_date")).toInt(), 0);
    QCOMPARE(keep.value(QStringLiteral("revoke_messages")).toBool(), false);
}

void GroupAdministrationTest::normalizesBanUntilDate()
{
    const auto untilOf = [](qint64 untilDate) {
        return banChatMemberRequest(kChatId, kUserId, untilDate, false)
            .value(QStringLiteral("banned_until_date"))
            .toInt();
    };

    QCOMPARE(untilOf(0), 0);
    QCOMPARE(untilOf(-1), 0);
    QCOMPARE(untilOf(-9999999999LL), 0);
    QCOMPARE(untilOf(1), 1);
    QCOMPARE(untilOf(std::numeric_limits<int>::max()), std::numeric_limits<int>::max());
    QCOMPARE(untilOf(qint64(std::numeric_limits<int>::max()) + 1),
             std::numeric_limits<int>::max());
    QCOMPARE(untilOf(std::numeric_limits<qint64>::max()), std::numeric_limits<int>::max());
}

void GroupAdministrationTest::rejectsInvalidBanIds()
{
    QVERIFY(banChatMemberRequest(0, kUserId, 0, false).isEmpty());
    QVERIFY(banChatMemberRequest(kChatId, 0, 0, false).isEmpty());
    QVERIFY(banChatMemberRequest(kChatId, -1, 0, false).isEmpty());
}

void GroupAdministrationTest::parsesAllModelledRights()
{
    const QJsonObject rights = parseObject(R"JSON({
        "@type": "chatAdministratorRights",
        "can_manage_chat": true,
        "can_change_info": true,
        "can_post_messages": true,
        "can_edit_messages": true,
        "can_delete_messages": true,
        "can_invite_users": true,
        "can_restrict_members": true,
        "can_pin_messages": true,
        "can_manage_topics": true,
        "can_promote_members": true,
        "can_manage_video_chats": true,
        "can_post_stories": true,
        "can_edit_stories": true,
        "can_delete_stories": true,
        "can_manage_direct_messages": true,
        "can_manage_tags": true,
        "is_anonymous": true
    })JSON");

    const AdminRights parsed = adminRightsFromJson(rights);
    QVERIFY(parsed.canChangeInfo);
    QVERIFY(parsed.canDeleteMessages);
    QVERIFY(parsed.canBanUsers);
    QVERIFY(parsed.canInviteUsers);
    QVERIFY(parsed.canPinMessages);
    QVERIFY(parsed.canManageTopics);
    QVERIFY(parsed.canPromoteMembers);
    QVERIFY(parsed.canManageVideoChats);
    QVERIFY(parsed.anonymous);

    // Rights this struct does not model must not leak into a modelled one.
    const AdminRights publishingOnly = adminRightsFromJson(parseObject(R"JSON({
        "can_manage_chat": true,
        "can_post_messages": true,
        "can_edit_messages": true,
        "can_post_stories": true,
        "can_manage_tags": true
    })JSON"));
    QVERIFY(!publishingOnly.canChangeInfo);
    QVERIFY(!publishingOnly.canDeleteMessages);
    QVERIFY(!publishingOnly.canBanUsers);
    QVERIFY(!publishingOnly.canInviteUsers);
    QVERIFY(!publishingOnly.canPinMessages);
    QVERIFY(!publishingOnly.canManageTopics);
    QVERIFY(!publishingOnly.canPromoteMembers);
    QVERIFY(!publishingOnly.canManageVideoChats);
    QVERIFY(!publishingOnly.anonymous);
}

void GroupAdministrationTest::deniesMissingAndMalformedRights()
{
    const AdminRights missing = adminRightsFromJson(QJsonObject{});
    QVERIFY(!missing.canChangeInfo);
    QVERIFY(!missing.canDeleteMessages);
    QVERIFY(!missing.canBanUsers);
    QVERIFY(!missing.canInviteUsers);
    QVERIFY(!missing.canPinMessages);
    QVERIFY(!missing.canManageTopics);
    QVERIFY(!missing.canPromoteMembers);
    QVERIFY(!missing.canManageVideoChats);
    QVERIFY(!missing.anonymous);

    const AdminRights malformed = adminRightsFromJson(parseObject(R"JSON({
        "can_change_info": "true",
        "can_delete_messages": 1,
        "can_invite_users": null,
        "can_pin_messages": true,
        "can_manage_topics": 0,
        "can_promote_members": true,
        "can_manage_video_chats": "yes",
        "is_anonymous": 1
    })JSON"));
    QVERIFY(!malformed.canChangeInfo);
    QVERIFY(!malformed.canDeleteMessages);
    QVERIFY(!malformed.canInviteUsers);
    QVERIFY(malformed.canPinMessages);
    QVERIFY(!malformed.canManageTopics);
    QVERIFY(malformed.canPromoteMembers);
    QVERIFY(!malformed.canManageVideoChats);
    QVERIFY(!malformed.anonymous);
}

void GroupAdministrationTest::fallsBackToCanBanUsers()
{
    QVERIFY(adminRightsFromJson(parseObject(R"({"can_ban_users": true})")).canBanUsers);
    QVERIFY(!adminRightsFromJson(parseObject(R"({"can_ban_users": false})")).canBanUsers);

    // The schema name wins whenever it is a boolean, including when denied.
    QVERIFY(!adminRightsFromJson(parseObject(
                R"({"can_restrict_members": false, "can_ban_users": true})"))
                .canBanUsers);
    QVERIFY(adminRightsFromJson(parseObject(
                R"({"can_restrict_members": true, "can_ban_users": false})"))
                .canBanUsers);

    // A malformed schema field falls back to the alternate spelling.
    QVERIFY(adminRightsFromJson(parseObject(
                R"({"can_restrict_members": "true", "can_ban_users": true})"))
                .canBanUsers);
}

void GroupAdministrationTest::parsesMembersResponse()
{
    const QJsonObject response = parseObject(R"JSON({
        "@type": "chatMembers",
        "total_count": 4,
        "members": [
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 1},
                "status": {"@type": "chatMemberStatusCreator",
                           "is_anonymous": true, "is_member": true},
                "user": {"@type": "user", "id": 1,
                         "first_name": "Ada", "last_name": "Lovelace"}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 2},
                "status": {"@type": "chatMemberStatusAdministrator",
                           "can_be_edited": false,
                           "rights": {
                               "@type": "chatAdministratorRights",
                               "can_change_info": true,
                               "can_delete_messages": true,
                               "can_restrict_members": true,
                               "can_invite_users": true,
                               "can_pin_messages": true,
                               "can_manage_topics": true,
                               "can_promote_members": true,
                               "can_manage_video_chats": true,
                               "is_anonymous": true
                           }}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 3},
                "status": {"@type": "chatMemberStatusMember"}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 4},
                "status": {"@type": "chatMemberStatusBanned",
                           "banned_until_date": 1700000000}
            }
        ],
        "users": [
            {"@type": "user", "id": 2, "first_name": "Grace", "last_name": "Hopper"}
        ]
    })JSON");

    const QVector<ChatMemberAdminRow> rows = administratorsFromResponse(response);
    QCOMPARE(rows.size(), qsizetype(2));

    QCOMPARE(rows.at(0).userId, qint64(1));
    QCOMPARE(rows.at(0).name, QStringLiteral("Ada Lovelace"));
    QCOMPARE(rows.at(0).status, QStringLiteral("creator"));
    QVERIFY(rows.at(0).isCreator);
    QVERIFY(rows.at(0).rights.anonymous);
    QVERIFY(!rows.at(0).rights.canChangeInfo);
    QVERIFY(!rows.at(0).rights.canDeleteMessages);
    QVERIFY(!rows.at(0).rights.canBanUsers);

    QCOMPARE(rows.at(1).userId, qint64(2));
    QCOMPARE(rows.at(1).name, QStringLiteral("Grace Hopper"));
    QCOMPARE(rows.at(1).status, QStringLiteral("administrator"));
    QVERIFY(!rows.at(1).isCreator);
    QVERIFY(rows.at(1).rights.canChangeInfo);
    QVERIFY(rows.at(1).rights.canDeleteMessages);
    QVERIFY(rows.at(1).rights.canBanUsers);
    QVERIFY(rows.at(1).rights.canInviteUsers);
    QVERIFY(rows.at(1).rights.canPinMessages);
    QVERIFY(rows.at(1).rights.canManageTopics);
    QVERIFY(rows.at(1).rights.canPromoteMembers);
    QVERIFY(rows.at(1).rights.canManageVideoChats);
    QVERIFY(rows.at(1).rights.anonymous);
}

void GroupAdministrationTest::parsesCreatorAnonymous()
{
    const QJsonObject response = parseObject(R"JSON({
        "@type": "chatMember",
        "member_id": {"@type": "messageSenderUser", "user_id": 5},
        "status": {"@type": "chatMemberStatusCreator", "is_anonymous": false,
                   "is_member": true}
    })JSON");

    const QVector<ChatMemberAdminRow> rows = administratorsFromResponse(response);
    QCOMPARE(rows.size(), qsizetype(1));
    QCOMPARE(rows.at(0).status, QStringLiteral("creator"));
    QVERIFY(rows.at(0).isCreator);
    QVERIFY(!rows.at(0).rights.anonymous);
}

void GroupAdministrationTest::skipsMalformedRows()
{
    const QJsonObject response = parseObject(R"JSON({
        "@type": "chatMembers",
        "members": [
            "not an object",
            null,
            {},
            {"@type": "chatMember", "status": {"@type": "chatMemberStatusAdministrator"}},
            {"@type": "chatMember",
             "member_id": {"@type": "messageSenderUser", "user_id": 0},
             "status": {"@type": "chatMemberStatusAdministrator"}},
            {"@type": "chatMember",
             "member_id": {"@type": "messageSenderUser", "user_id": -5},
             "status": {"@type": "chatMemberStatusAdministrator"}},
            {"@type": "chatMember",
             "member_id": {"@type": "messageSenderChat", "chat_id": -1001234567890},
             "status": {"@type": "chatMemberStatusAdministrator"}},
            {"@type": "chatMember",
             "member_id": {"@type": "messageSenderUser", "user_id": 10}},
            {"@type": "chatMember",
             "member_id": {"@type": "messageSenderUser", "user_id": 11},
             "status": "chatMemberStatusAdministrator"},
            {"@type": "chatMember",
             "member_id": {"@type": "messageSenderUser", "user_id": 12},
             "status": {"@type": "chatMemberStatusMember"}},
            {"@type": "chatMember",
             "member_id": {"@type": "messageSenderUser", "user_id": 13},
             "status": {"@type": "chatMemberStatusRestricted", "is_member": true}},
            {"@type": "chatMember",
             "member_id": {"@type": "messageSenderUser", "user_id": 14},
             "status": {"@type": "chatMemberStatusBanned"}},
            {"@type": "chatMember",
             "member_id": {"@type": "messageSenderUser", "user_id": 15},
             "status": {"@type": "chatMemberStatusLeft"}},
            {"@type": "chatMember",
             "member_id": {"@type": "messageSenderUser", "user_id": 16},
             "status": {"@type": "chatMemberStatusFutureRole"}},
            {"@type": "chatMember",
             "member_id": {"@type": "messageSenderUser", "user_id": 17},
             "status": {"@type": 7}},
            {"@type": "chatAdministrator", "user_id": 18, "custom_title": "Mod",
             "is_owner": false},
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 40},
                "status": {"@type": "chatMemberStatusAdministrator",
                           "rights": {"can_pin_messages": true}}
            }
        ]
    })JSON");

    const QVector<ChatMemberAdminRow> rows = administratorsFromResponse(response);
    QCOMPARE(rows.size(), qsizetype(1));
    QCOMPARE(rows.at(0).userId, qint64(40));
    QCOMPARE(rows.at(0).status, QStringLiteral("administrator"));
    QVERIFY(rows.at(0).rights.canPinMessages);
}

void GroupAdministrationTest::parsesSingleChatMember()
{
    const QJsonObject response = parseObject(R"JSON({
        "@type": "chatMember",
        "member_id": {"@type": "messageSenderUser", "user_id": 9},
        "status": {"@type": "chatMemberStatusAdministrator",
                   "can_be_edited": true,
                   "rights": {"can_pin_messages": true}}
    })JSON");

    const QVector<ChatMemberAdminRow> rows = administratorsFromResponse(response);
    QCOMPARE(rows.size(), qsizetype(1));
    QCOMPARE(rows.at(0).userId, qint64(9));
    QCOMPARE(rows.at(0).status, QStringLiteral("administrator"));
    QVERIFY(!rows.at(0).isCreator);
    QVERIFY(rows.at(0).rights.canPinMessages);
    QVERIFY(!rows.at(0).rights.canDeleteMessages);
    QVERIFY(rows.at(0).name.isEmpty());
}

void GroupAdministrationTest::parsesChatAdministratorsAnswer()
{
    const QJsonObject response = parseObject(R"JSON({
        "@type": "chatAdministrators",
        "administrators": [
            {"@type": "chatAdministrator", "user_id": 1, "custom_title": "Owner",
             "is_owner": true, "can_be_edited": false},
            {"@type": "chatAdministrator", "user_id": 2, "custom_title": "Mod",
             "is_owner": false, "can_be_edited": true},
            {"@type": "chatAdministrator", "user_id": 3, "custom_title": "Merged",
             "is_owner": false, "rights": {"can_delete_messages": true}},
            {"@type": "chatAdministrator", "user_id": 0, "custom_title": "Nobody"},
            "not an object"
        ]
    })JSON");

    const QVector<ChatMemberAdminRow> rows = administratorsFromResponse(response);
    QCOMPARE(rows.size(), qsizetype(3));

    QCOMPARE(rows.at(0).userId, qint64(1));
    QCOMPARE(rows.at(0).status, QStringLiteral("creator"));
    QVERIFY(rows.at(0).isCreator);
    QVERIFY(rows.at(0).name.isEmpty());
    QVERIFY(!rows.at(0).rights.canDeleteMessages);

    QCOMPARE(rows.at(1).userId, qint64(2));
    QCOMPARE(rows.at(1).status, QStringLiteral("administrator"));
    QVERIFY(!rows.at(1).isCreator);
    QVERIFY(!rows.at(1).rights.canChangeInfo);

    QCOMPARE(rows.at(2).userId, qint64(3));
    QVERIFY(rows.at(2).rights.canDeleteMessages);
}

void GroupAdministrationTest::resolvesNamesFromEmbeddedUserAndUsersArray()
{
    const QJsonObject response = parseObject(R"JSON({
        "@type": "chatMembers",
        "members": [
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 20},
                "status": {"@type": "chatMemberStatusAdministrator", "rights": {}},
                "user": {"@type": "user", "id": 20, "first_name": "Ada"}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 21},
                "status": {"@type": "chatMemberStatusAdministrator", "rights": {}},
                "user": {"@type": "user", "id": 21}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 22},
                "status": {"@type": "chatMemberStatusAdministrator", "rights": {}},
                "user": {"@type": "user", "id": 22,
                         "display_name": "The Machine"}
            }
        ],
        "users": [
            {"@type": "user", "id": 21, "first_name": "Grace", "last_name": "Hopper"}
        ]
    })JSON");

    const QVector<ChatMemberAdminRow> rows = administratorsFromResponse(response);
    QCOMPARE(rows.size(), qsizetype(3));
    QCOMPARE(rows.at(0).name, QStringLiteral("Ada"));
    QCOMPARE(rows.at(1).name, QStringLiteral("Grace Hopper"));
    QCOMPARE(rows.at(2).name, QStringLiteral("The Machine"));
}

void GroupAdministrationTest::resolvesNameFromUsername()
{
    const QJsonObject response = parseObject(R"JSON({
        "@type": "chatMembers",
        "members": [
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 30},
                "status": {"@type": "chatMemberStatusAdministrator", "rights": {}},
                "user": {"@type": "user", "id": 30,
                         "usernames": {"active_usernames": ["ghost"],
                                       "active_username": ""}}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 31},
                "status": {"@type": "chatMemberStatusAdministrator", "rights": {}},
                "user": {"@type": "user", "id": 31, "username": "@neo"}
            }
        ]
    })JSON");

    const QVector<ChatMemberAdminRow> rows = administratorsFromResponse(response);
    QCOMPARE(rows.size(), qsizetype(2));
    QCOMPARE(rows.at(0).name, QStringLiteral("ghost"));
    QCOMPARE(rows.at(1).name, QStringLiteral("neo"));
}

void GroupAdministrationTest::leavesNameEmptyWithoutUserData()
{
    const QJsonObject response = parseObject(R"JSON({
        "@type": "chatMembers",
        "members": [
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 50},
                "status": {"@type": "chatMemberStatusAdministrator", "rights": {}}
            }
        ]
    })JSON");

    const QVector<ChatMemberAdminRow> rows = administratorsFromResponse(response);
    QCOMPARE(rows.size(), qsizetype(1));
    QVERIFY(rows.at(0).name.isEmpty());
}

void GroupAdministrationTest::keepsOrderAndDuplicates()
{
    const QJsonObject response = parseObject(R"JSON({
        "@type": "chatMembers",
        "members": [
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 70},
                "status": {"@type": "chatMemberStatusAdministrator", "rights": {}}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 71},
                "status": {"@type": "chatMemberStatusCreator", "is_member": true}
            },
            {
                "@type": "chatMember",
                "member_id": {"@type": "messageSenderUser", "user_id": 70},
                "status": {"@type": "chatMemberStatusAdministrator", "rights": {}}
            }
        ]
    })JSON");

    const QVector<ChatMemberAdminRow> rows = administratorsFromResponse(response);
    QCOMPARE(rows.size(), qsizetype(3));
    QCOMPARE(rows.at(0).userId, qint64(70));
    QCOMPARE(rows.at(1).userId, qint64(71));
    QCOMPARE(rows.at(1).status, QStringLiteral("creator"));
    QCOMPARE(rows.at(2).userId, qint64(70));
}

void GroupAdministrationTest::emptyOrMalformedResponseYieldsNothing()
{
    QVERIFY(administratorsFromResponse(QJsonObject{}).isEmpty());
    QVERIFY(administratorsFromResponse(
                parseObject(R"({"@type": "chatMembers", "total_count": 5})"))
                .isEmpty());
    QVERIFY(administratorsFromResponse(
                parseObject(R"({"@type": "chatMembers", "members": null})"))
                .isEmpty());
    QVERIFY(administratorsFromResponse(
                parseObject(R"({"@type": "chatMembers", "members": {}})"))
                .isEmpty());
    QVERIFY(administratorsFromResponse(
                parseObject(R"({"@type": "chatMembers", "members": "none"})"))
                .isEmpty());
    QVERIFY(administratorsFromResponse(
                parseObject(R"({"@type": "chatMembers", "members": []})"))
                .isEmpty());
    QVERIFY(administratorsFromResponse(
                parseObject(R"({"@type": "chatAdministrators", "administrators": null})"))
                .isEmpty());
    QVERIFY(administratorsFromResponse(
                parseObject(R"({"@type": "chatAdministrators", "administrators": {}})"))
                .isEmpty());
    QVERIFY(administratorsFromResponse(
                parseObject(R"({"@type": "chatAdministrators", "administrators": []})"))
                .isEmpty());
}

void GroupAdministrationTest::survivesSerialization()
{
    const QJsonObject statusRequest = roundTrip(
        setChatMemberStatusRequest(kChatId, kUserId, QStringLiteral("administrator")));
    QCOMPARE(statusRequest.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setChatMemberStatus"));
    QCOMPARE(statusRequest.value(QStringLiteral("chat_id")).toVariant().toLongLong(), kChatId);
    const QJsonObject status = statusRequest.value(QStringLiteral("status")).toObject();
    QCOMPARE(status.value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatMemberStatusAdministrator"));
    QCOMPARE(status.value(QStringLiteral("rights")).toObject().size(), qsizetype(18));

    const QJsonObject banRequest =
        roundTrip(banChatMemberRequest(kChatId, kUserId, 1700000000LL, true));
    QCOMPARE(banRequest.value(QStringLiteral("banned_until_date")).toInt(), 1700000000);
    QCOMPARE(banRequest.value(QStringLiteral("revoke_messages")).toBool(), true);

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

    const QVector<ChatMemberAdminRow> rows = administratorsFromResponse(response);
    QCOMPARE(rows.size(), qsizetype(1));
    QCOMPARE(rows.at(0).userId, qint64(80));
    QCOMPARE(rows.at(0).name, QStringLiteral("Ada"));
    QCOMPARE(rows.at(0).status, QStringLiteral("creator"));
    QVERIFY(rows.at(0).isCreator);
}

QTEST_GUILESS_MAIN(GroupAdministrationTest)
#include "tst_group_administration.moc"
