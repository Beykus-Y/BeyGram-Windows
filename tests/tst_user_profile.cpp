#include "telegram/UserProfile.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject jsonObject(const QByteArray& json)
{
    return QJsonDocument::fromJson(json).object();
}

QJsonObject fullUserFixture()
{
    return jsonObject(R"JSON({
        "@type": "user",
        "id": 1234567890123,
        "first_name": "Ada",
        "last_name": "Lovelace",
        "usernames": {
            "@type": "usernames",
            "active_usernames": ["ada"],
            "disabled_usernames": [],
            "editable_username": "ada",
            "collectible_usernames": [],
            "secondary_username": ""
        },
        "phone_number": "+15550100",
        "is_contact": true,
        "is_premium": false,
        "type": {"@type": "userTypeRegular"},
        "profile_photo": {
            "@type": "profilePhoto",
            "id": 900,
            "small": {
                "@type": "file",
                "id": 4242,
                "size": 1200,
                "local": {"@type": "localFile", "is_downloading_completed": true}
            },
            "big": {
                "@type": "file",
                "id": 4343,
                "size": 40000,
                "local": {"@type": "localFile", "is_downloading_completed": false}
            }
        }
    })JSON");
}

QJsonObject userFullInfoFixture()
{
    return jsonObject(R"JSON({
        "@type": "userFullInfo",
        "bio": {
            "@type": "formattedText",
            "text": "Mathematician",
            "entities": []
        }
    })JSON");
}

QJsonObject chatFixture(qint64 id, const char* title, const char* typeJson,
                        const char* photoJson = R"JSON(null)JSON")
{
    return jsonObject(QByteArrayLiteral("{\"@type\":\"chat\",\"id\":")
                          + QByteArray::number(id)
                          + QByteArrayLiteral(",\"title\":\"") + title
                          + QByteArrayLiteral("\",\"type\":") + typeJson
                          + QByteArrayLiteral(",\"photo\":") + photoJson + QByteArrayLiteral("}"));
}

} // namespace

class UserProfileTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesPrivateUserProfile();
    void prefersActiveUsername();
    void readsBotAndPremiumFlags();
    void fallsBackForEmptyUser();
    void parsesPrivateChatProfile();
    void parsesBasicGroupChat();
    void parsesSupergroupAndChannelChats();
    void buildsProfileRequests();
};

void UserProfileTest::parsesPrivateUserProfile()
{
    const UserProfile profile = profileFromUser(fullUserFixture(), userFullInfoFixture());

    QCOMPARE(profile.userId, qint64(1234567890123));
    QCOMPARE(profile.chatId, profile.userId);
    QCOMPARE(profile.displayName, QStringLiteral("Ada Lovelace"));
    QCOMPARE(profile.username, QStringLiteral("ada"));
    QCOMPARE(profile.bio, QStringLiteral("Mathematician"));
    QCOMPARE(profile.phoneNumber, QStringLiteral("+15550100"));
    QCOMPARE(profile.photoFileId, 4242);
    QVERIFY(profile.hasPhoto);
    QVERIFY(profile.isContact);
    QVERIFY(!profile.isPremium);
    QVERIFY(!profile.isBot);
    QVERIFY(!profile.isGroup);
    QVERIFY(!profile.isChannel);
    QCOMPARE(profile.memberCount, 0);

    // Without a small photo the big one is used.
    QJsonObject bigOnly = fullUserFixture();
    QJsonObject photo = bigOnly.value(QStringLiteral("profile_photo")).toObject();
    photo.remove(QStringLiteral("small"));
    bigOnly.insert(QStringLiteral("profile_photo"), photo);
    const UserProfile bigProfile = profileFromUser(bigOnly);
    QCOMPARE(bigProfile.photoFileId, 4343);
    QVERIFY(bigProfile.hasPhoto);
    // No full info: bio and phone stay empty rather than being synthesized.
    QVERIFY(bigProfile.bio.isEmpty());

    QJsonObject noPhone = fullUserFixture();
    noPhone.insert(QStringLiteral("phone_number"), QString());
    QVERIFY(profileFromUser(noPhone).phoneNumber.isEmpty());
}

void UserProfileTest::prefersActiveUsername()
{
    const QJsonObject user = jsonObject(R"JSON({
        "@type": "user",
        "id": 5,
        "first_name": "Neo",
        "username": "old_handle",
        "usernames": {
            "@type": "usernames",
            "active_username": "@neo",
            "active_usernames": ["@trinity"]
        }
    })JSON");
    QCOMPARE(profileFromUser(user).username, QStringLiteral("neo"));

    const QJsonObject listOnly = jsonObject(R"JSON({
        "@type": "user",
        "id": 6,
        "usernames": {"@type": "usernames", "active_usernames": ["@trinity"]},
        "username": "old_handle"
    })JSON");
    QCOMPARE(profileFromUser(listOnly).username, QStringLiteral("trinity"));

    const QJsonObject legacy = jsonObject(R"JSON({
        "@type": "user",
        "id": 7,
        "username": "@legacy"
    })JSON");
    QCOMPARE(profileFromUser(legacy).username, QStringLiteral("legacy"));

    const QJsonObject none = jsonObject(R"JSON({"@type": "user", "id": 8})JSON");
    QVERIFY(profileFromUser(none).username.isEmpty());
}

void UserProfileTest::readsBotAndPremiumFlags()
{
    const QJsonObject bot = jsonObject(R"JSON({
        "@type": "user",
        "id": 9,
        "first_name": "Helper",
        "is_bot": true,
        "is_premium": true,
        "type": {"@type": "userTypeBot", "can_join_groups": true}
    })JSON");
    const UserProfile botProfile = profileFromUser(bot);
    QVERIFY(botProfile.isBot);
    QVERIFY(botProfile.isPremium);
    QVERIFY(!botProfile.isContact);
    QCOMPARE(botProfile.displayName, QStringLiteral("Helper"));

    const QJsonObject regular = jsonObject(R"JSON({
        "@type": "user",
        "id": 10,
        "first_name": "Grace",
        "is_premium": true,
        "type": {"@type": "userTypeRegular"}
    })JSON");
    const UserProfile regularProfile = profileFromUser(regular);
    QVERIFY(!regularProfile.isBot);
    QVERIFY(regularProfile.isPremium);
}

void UserProfileTest::fallsBackForEmptyUser()
{
    const UserProfile profile = profileFromUser(jsonObject(R"JSON({})JSON"));

    QCOMPARE(profile.userId, qint64(0));
    QCOMPARE(profile.chatId, qint64(0));
    QCOMPARE(profile.displayName, QStringLiteral("Telegram user"));
    QVERIFY(profile.username.isEmpty());
    QVERIFY(profile.bio.isEmpty());
    QVERIFY(profile.phoneNumber.isEmpty());
    QCOMPARE(profile.photoFileId, 0);
    QVERIFY(!profile.hasPhoto);
    QVERIFY(!profile.isPremium);
    QVERIFY(!profile.isBot);
    QVERIFY(!profile.isContact);
    QVERIFY(!profile.isGroup);
    QVERIFY(!profile.isChannel);
    QCOMPARE(profile.memberCount, 0);

    // Whitespace-only names also fall back.
    const QJsonObject blankUser = jsonObject(R"JSON({
        "@type": "user",
        "id": 3,
        "first_name": "  ",
        "last_name": " "
    })JSON");
    QCOMPARE(profileFromUser(blankUser).displayName, QStringLiteral("Telegram user"));
}

void UserProfileTest::parsesPrivateChatProfile()
{
    const QJsonObject chat = chatFixture(
        777, "Ada Lovelace",
        R"JSON({"@type": "chatTypePrivate", "user_id": 1234567890123})JSON",
        R"JSON({
            "@type": "chatPhotoInfo",
            "small": {
                "@type": "file",
                "id": 5151,
                "local": {"@type": "localFile", "is_downloading_completed": true}
            },
            "big": {"@type": "file", "id": 5252}
        })JSON");

    const UserProfile profile = profileFromChat(chat);
    QCOMPARE(profile.chatId, qint64(777));
    QCOMPARE(profile.userId, qint64(1234567890123));
    QCOMPARE(profile.displayName, QStringLiteral("Ada Lovelace"));
    QCOMPARE(profile.photoFileId, 5151);
    QVERIFY(profile.hasPhoto);
    QVERIFY(!profile.isGroup);
    QVERIFY(!profile.isChannel);
    QCOMPARE(profile.memberCount, 0);
    QVERIFY(profile.bio.isEmpty());
}

void UserProfileTest::parsesBasicGroupChat()
{
    const QJsonObject chat = chatFixture(
        -1001, "Study Group",
        R"JSON({"@type": "chatTypeBasicGroup", "basic_group_id": 42})JSON");
    const QJsonObject fullInfo = jsonObject(R"JSON({
        "@type": "basicGroupFullInfo",
        "description": "Notes and coffee",
        "members": [
            {"@type": "chatMember", "member_id": {"@type": "messageSenderUser", "user_id": 1}},
            {"@type": "chatMember", "member_id": {"@type": "messageSenderUser", "user_id": 2}},
            {"@type": "chatMember", "member_id": {"@type": "messageSenderUser", "user_id": 3}}
        ]
    })JSON");

    const UserProfile profile = profileFromChat(chat, fullInfo);
    QCOMPARE(profile.chatId, qint64(-1001));
    QCOMPARE(profile.userId, qint64(0));
    QCOMPARE(profile.displayName, QStringLiteral("Study Group"));
    QVERIFY(profile.isGroup);
    QVERIFY(!profile.isChannel);
    QCOMPARE(profile.memberCount, 3);
    QCOMPARE(profile.bio, QStringLiteral("Notes and coffee"));
    QVERIFY(!profile.hasPhoto);
    QCOMPARE(profile.photoFileId, 0);
}

void UserProfileTest::parsesSupergroupAndChannelChats()
{
    const QJsonObject supergroup = chatFixture(
        -1002, "Qt Devs",
        R"JSON({"@type": "chatTypeSupergroup", "supergroup_id": 99, "is_channel": false})JSON");
    const UserProfile groupProfile = profileFromChat(
        supergroup, jsonObject(R"JSON({
            "@type": "supergroupFullInfo",
            "description": "All about Qt",
            "member_count": 5123
        })JSON"));
    QCOMPARE(groupProfile.chatId, qint64(-1002));
    QVERIFY(groupProfile.isGroup);
    QVERIFY(!groupProfile.isChannel);
    QCOMPARE(groupProfile.memberCount, 5123);
    QCOMPARE(groupProfile.bio, QStringLiteral("All about Qt"));

    const QJsonObject channel = chatFixture(
        -1003, "BeyGram News",
        R"JSON({"@type": "chatTypeSupergroup", "supergroup_id": 100, "is_channel": true})JSON");
    const UserProfile channelProfile = profileFromChat(
        channel, jsonObject(R"JSON({
            "@type": "supergroupFullInfo",
            "member_count": 42000
        })JSON"));
    QCOMPARE(channelProfile.chatId, qint64(-1003));
    QVERIFY(channelProfile.isChannel);
    QVERIFY(!channelProfile.isGroup);
    QCOMPARE(channelProfile.memberCount, 42000);
    QVERIFY(channelProfile.bio.isEmpty());

    // A supergroup without full info still reports its flags.
    const UserProfile bare = profileFromChat(channel);
    QVERIFY(bare.isChannel);
    QCOMPARE(bare.memberCount, 0);
}

void UserProfileTest::buildsProfileRequests()
{
    const QJsonObject user = getUserRequest(1234567890123);
    QCOMPARE(user.value(QStringLiteral("@type")).toString(), QStringLiteral("getUser"));
    QCOMPARE(user.value(QStringLiteral("user_id")).toVariant().toLongLong(),
             qint64(1234567890123));
    QCOMPARE(user.keys().size(), qsizetype(2));

    const QJsonObject userFull = getUserFullInfoRequest(1234567890123);
    QCOMPARE(userFull.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getUserFullInfo"));
    QCOMPARE(userFull.value(QStringLiteral("user_id")).toVariant().toLongLong(),
             qint64(1234567890123));
    QCOMPARE(userFull.keys().size(), qsizetype(2));

    const QJsonObject chat = getChatRequest(-1001);
    QCOMPARE(chat.value(QStringLiteral("@type")).toString(), QStringLiteral("getChat"));
    QCOMPARE(chat.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(-1001));
    QCOMPARE(chat.keys().size(), qsizetype(2));

    const QJsonObject supergroup = getSupergroupFullInfoRequest(99);
    QCOMPARE(supergroup.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getSupergroupFullInfo"));
    QCOMPARE(supergroup.value(QStringLiteral("supergroup_id")).toVariant().toLongLong(),
             qint64(99));
    QCOMPARE(supergroup.keys().size(), qsizetype(2));
}

QTEST_GUILESS_MAIN(UserProfileTest)
#include "tst_user_profile.moc"
