#include "telegram/PrivacyRequests.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QtTest>

using namespace beygram::telegram;

namespace {

// Requests are compared after a JSON round trip so the assertions see exactly
// what would be handed to TDLib.
QJsonObject roundTrip(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

QString settingTypeOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("setting"))
        .toObject()
        .value(QStringLiteral("@type"))
        .toString();
}

// Returns the one rule carried by a setUserPrivacySettingRules request, or an
// empty object when the request does not carry exactly one rule.
QJsonObject singleRuleOf(const QJsonObject& request)
{
    const QJsonArray rules = request.value(QStringLiteral("rules"))
                                 .toObject()
                                 .value(QStringLiteral("rules"))
                                 .toArray();
    return rules.size() == 1 ? rules.at(0).toObject() : QJsonObject{};
}

QString ruleTypeOf(const QJsonObject& request)
{
    return singleRuleOf(request).value(QStringLiteral("@type")).toString();
}

} // namespace

class PrivacyRequestsTest final : public QObject {
    Q_OBJECT

private slots:
    void requestsPrivacySetting();
    void mapsSettingNames();
    void coversEveryPinnedSetting();
    void setsSingleRule();
    void mapsRuleChoices();
    void rejectsIdCarryingRules();
    void rejectsUnknownNames();
    void restrictsFindingByPhoneNumber();
    void limitsPremiumRuleToChatInvites();
    void clampsBlockedSendersPaging();
    void blockedSendersRequestShape();
    void blocksUserSender();
    void unblocksUserSenderWithNullBlockList();
    void requestsSurviveSerialization();
};

void PrivacyRequestsTest::requestsPrivacySetting()
{
    const QJsonObject request =
        getPrivacySettingRequest(QStringLiteral("userPrivacySettingShowStatus"));

    QCOMPARE(request.size(), 2);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getUserPrivacySettingRules"));

    const QJsonObject setting = request.value(QStringLiteral("setting")).toObject();
    QCOMPARE(setting.size(), 1);
    QCOMPARE(setting.value(QStringLiteral("@type")).toString(),
             QStringLiteral("userPrivacySettingShowStatus"));

    // The request carries no rules and no correlation field.
    QVERIFY(!request.contains(QStringLiteral("rules")));
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void PrivacyRequestsTest::mapsSettingNames()
{
    const QString canonical = QStringLiteral("userPrivacySettingShowProfilePhoto");

    // Loose spellings fold to the schema @type: case, separators and surrounding
    // whitespace do not matter.
    QCOMPARE(settingTypeOf(getPrivacySettingRequest(QStringLiteral("show_profile_photo"))),
             canonical);
    QCOMPARE(settingTypeOf(getPrivacySettingRequest(QStringLiteral("showProfilePhoto"))),
             canonical);
    QCOMPARE(settingTypeOf(getPrivacySettingRequest(QStringLiteral("  Show Profile Photo  "))),
             canonical);
    const QString spelledOut = QStringLiteral("user_privacy_setting_show_profile_photo");
    QCOMPARE(settingTypeOf(getPrivacySettingRequest(spelledOut)), canonical);

    const QString shouted = QStringLiteral("USERPRIVACYSETTINGALLOWCALLS");
    QCOMPARE(settingTypeOf(getPrivacySettingRequest(shouted)),
             QStringLiteral("userPrivacySettingAllowCalls"));
    QCOMPARE(settingTypeOf(getPrivacySettingRequest(QStringLiteral("allow_calls"))),
             QStringLiteral("userPrivacySettingAllowCalls"));

    // Unknown names and names that only look like a setting never reach TDLib.
    QVERIFY(getPrivacySettingRequest(QString()).isEmpty());
    QVERIFY(getPrivacySettingRequest(QStringLiteral("   ")).isEmpty());
    QVERIFY(getPrivacySettingRequest(QStringLiteral("status")).isEmpty());
    QVERIFY(getPrivacySettingRequest(QStringLiteral("show_profile")).isEmpty());
    QVERIFY(getPrivacySettingRequest(QStringLiteral("userPrivacySetting")).isEmpty());
    QVERIFY(getPrivacySettingRequest(QStringLiteral("userPrivacySettingNope")).isEmpty());
}

void PrivacyRequestsTest::coversEveryPinnedSetting()
{
    // The 14 UserPrivacySetting variants of the pinned 1.8.69 schema, in order.
    const QStringList expected{
        QStringLiteral("userPrivacySettingShowStatus"),
        QStringLiteral("userPrivacySettingShowProfilePhoto"),
        QStringLiteral("userPrivacySettingShowLinkInForwardedMessages"),
        QStringLiteral("userPrivacySettingShowPhoneNumber"),
        QStringLiteral("userPrivacySettingShowBio"),
        QStringLiteral("userPrivacySettingShowBirthdate"),
        QStringLiteral("userPrivacySettingShowProfileAudio"),
        QStringLiteral("userPrivacySettingAllowChatInvites"),
        QStringLiteral("userPrivacySettingAllowCalls"),
        QStringLiteral("userPrivacySettingAllowPeerToPeerCalls"),
        QStringLiteral("userPrivacySettingAllowFindingByPhoneNumber"),
        QStringLiteral("userPrivacySettingAllowPrivateVoiceAndVideoNoteMessages"),
        QStringLiteral("userPrivacySettingAutosaveGifts"),
        QStringLiteral("userPrivacySettingAllowUnpaidMessages"),
    };
    QCOMPARE(expected.size(), 14);

    for (const QString& type : expected) {
        const QJsonObject request = getPrivacySettingRequest(type);
        QVERIFY2(!request.isEmpty(), qPrintable(type));
        QCOMPARE(settingTypeOf(request), type);
        // Every setting must round-trip through the loose-name mapping too.
        const QString bare = type.mid(QStringLiteral("userPrivacySetting").size());
        QCOMPARE(settingTypeOf(getPrivacySettingRequest(bare)), type);
    }
}

void PrivacyRequestsTest::setsSingleRule()
{
    const QJsonObject request = setPrivacySettingRequest(
        QStringLiteral("show_status"), QStringLiteral("contacts"));

    QCOMPARE(request.size(), 3);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setUserPrivacySettingRules"));
    QCOMPARE(settingTypeOf(request), QStringLiteral("userPrivacySettingShowStatus"));
    QVERIFY(!request.contains(QStringLiteral("@extra")));

    // rules must be a complete userPrivacySettingRules object.
    const QJsonObject rules = request.value(QStringLiteral("rules")).toObject();
    QCOMPARE(rules.size(), 2);
    QCOMPARE(rules.value(QStringLiteral("@type")).toString(),
             QStringLiteral("userPrivacySettingRules"));
    const QJsonArray list = rules.value(QStringLiteral("rules")).toArray();
    QCOMPARE(list.size(), 1);

    // A scalar rule is the @type alone; it must not carry empty id lists.
    const QJsonObject rule = singleRuleOf(request);
    QCOMPARE(rule.size(), 1);
    QCOMPARE(rule.value(QStringLiteral("@type")).toString(),
             QStringLiteral("userPrivacySettingRuleAllowContacts"));
    QVERIFY(!rule.contains(QStringLiteral("user_ids")));
    QVERIFY(!rule.contains(QStringLiteral("chat_ids")));

    QCOMPARE(ruleTypeOf(setPrivacySettingRequest(QStringLiteral("show_bio"),
                                                 QStringLiteral("userPrivacySettingRuleAllowAll"))),
             QStringLiteral("userPrivacySettingRuleAllowAll"));
}

void PrivacyRequestsTest::mapsRuleChoices()
{
    const QString setting = QStringLiteral("show_phone_number");

    QCOMPARE(ruleTypeOf(setPrivacySettingRequest(setting, QStringLiteral("everyone"))),
             QStringLiteral("userPrivacySettingRuleAllowAll"));
    QCOMPARE(ruleTypeOf(setPrivacySettingRequest(setting, QStringLiteral("all"))),
             QStringLiteral("userPrivacySettingRuleAllowAll"));
    QCOMPARE(ruleTypeOf(setPrivacySettingRequest(setting, QStringLiteral("Anybody"))),
             QStringLiteral("userPrivacySettingRuleAllowAll"));
    QCOMPARE(ruleTypeOf(setPrivacySettingRequest(setting, QStringLiteral("contacts"))),
             QStringLiteral("userPrivacySettingRuleAllowContacts"));
    QCOMPARE(ruleTypeOf(setPrivacySettingRequest(setting, QStringLiteral("nobody"))),
             QStringLiteral("userPrivacySettingRuleRestrictAll"));

    // Loose and exact schema names of the remaining scalar rules.
    QCOMPARE(ruleTypeOf(setPrivacySettingRequest(setting, QStringLiteral("allow_bots"))),
             QStringLiteral("userPrivacySettingRuleAllowBots"));
    QCOMPARE(ruleTypeOf(setPrivacySettingRequest(
                 setting, QStringLiteral("userPrivacySettingRuleRestrictBots"))),
             QStringLiteral("userPrivacySettingRuleRestrictBots"));
    QCOMPARE(ruleTypeOf(setPrivacySettingRequest(setting, QStringLiteral("restrict_contacts"))),
             QStringLiteral("userPrivacySettingRuleRestrictContacts"));
    QCOMPARE(ruleTypeOf(setPrivacySettingRequest(
                 QStringLiteral("allow_chat_invites"),
                 QStringLiteral("userPrivacySettingRuleAllowPremiumUsers"))),
             QStringLiteral("userPrivacySettingRuleAllowPremiumUsers"));
}

void PrivacyRequestsTest::rejectsIdCarryingRules()
{
    const QString setting = QStringLiteral("show_status");

    // These variants need user_ids or chat_ids, which a single rule name cannot
    // express. Sending them with an empty list would silently mean "nobody".
    const QStringList rejected{
        QStringLiteral("userPrivacySettingRuleAllowUsers"),
        QStringLiteral("userPrivacySettingRuleRestrictUsers"),
        QStringLiteral("userPrivacySettingRuleAllowChatMembers"),
        QStringLiteral("userPrivacySettingRuleRestrictChatMembers"),
        QStringLiteral("allow_users"),
        QStringLiteral("restrict_chat_members"),
    };
    for (const QString& rule : rejected) {
        QVERIFY2(setPrivacySettingRequest(setting, rule).isEmpty(), qPrintable(rule));
    }
}

void PrivacyRequestsTest::rejectsUnknownNames()
{
    QVERIFY(setPrivacySettingRequest(QString(), QStringLiteral("contacts")).isEmpty());
    QVERIFY(setPrivacySettingRequest(QStringLiteral("nope"), QStringLiteral("contacts")).isEmpty());
    QVERIFY(setPrivacySettingRequest(QStringLiteral("show_status"), QString()).isEmpty());
    QVERIFY(setPrivacySettingRequest(QStringLiteral("show_status"), QStringLiteral("   ")).isEmpty());
    QVERIFY(setPrivacySettingRequest(QStringLiteral("show_status"),
                                     QStringLiteral("userPrivacySettingRuleAllowFriends"))
                .isEmpty());
}

void PrivacyRequestsTest::restrictsFindingByPhoneNumber()
{
    const QString setting = QStringLiteral("userPrivacySettingAllowFindingByPhoneNumber");

    // "Can be set only to Allow contacts or Allow all".
    QVERIFY(!setPrivacySettingRequest(setting, QStringLiteral("allow_all")).isEmpty());
    QVERIFY(!setPrivacySettingRequest(setting, QStringLiteral("contacts")).isEmpty());

    QVERIFY(setPrivacySettingRequest(setting, QStringLiteral("nobody")).isEmpty());
    QVERIFY(setPrivacySettingRequest(setting, QStringLiteral("restrict_all")).isEmpty());
    QVERIFY(setPrivacySettingRequest(setting, QStringLiteral("restrict_contacts")).isEmpty());
    QVERIFY(setPrivacySettingRequest(setting, QStringLiteral("allow_bots")).isEmpty());
    QVERIFY(setPrivacySettingRequest(setting, QStringLiteral("restrict_bots")).isEmpty());

    // The restriction is specific to that setting.
    QVERIFY(!setPrivacySettingRequest(QStringLiteral("show_phone_number"),
                                      QStringLiteral("nobody"))
                 .isEmpty());
}

void PrivacyRequestsTest::limitsPremiumRuleToChatInvites()
{
    // "currently, allowed only for userPrivacySettingAllowChatInvites".
    QVERIFY(!setPrivacySettingRequest(QStringLiteral("userPrivacySettingAllowChatInvites"),
                                      QStringLiteral("userPrivacySettingRuleAllowPremiumUsers"))
                 .isEmpty());

    QVERIFY(setPrivacySettingRequest(QStringLiteral("show_status"),
                                     QStringLiteral("userPrivacySettingRuleAllowPremiumUsers"))
                .isEmpty());
    QVERIFY(setPrivacySettingRequest(QStringLiteral("allow_calls"),
                                     QStringLiteral("allow_premium_users"))
                .isEmpty());
}

void PrivacyRequestsTest::clampsBlockedSendersPaging()
{
    QCOMPARE(getBlockedSendersRequest(-5, 20).value(QStringLiteral("offset")).toInt(), 0);
    QCOMPARE(getBlockedSendersRequest(0, 20).value(QStringLiteral("offset")).toInt(), 0);
    QCOMPARE(getBlockedSendersRequest(250, 20).value(QStringLiteral("offset")).toInt(), 250);

    // "limit: The maximum number of users and chats to return; up to 100".
    QCOMPARE(getBlockedSendersRequest(0, 0).value(QStringLiteral("limit")).toInt(), 1);
    QCOMPARE(getBlockedSendersRequest(0, -3).value(QStringLiteral("limit")).toInt(), 1);
    QCOMPARE(getBlockedSendersRequest(0, 1).value(QStringLiteral("limit")).toInt(), 1);
    QCOMPARE(getBlockedSendersRequest(0, 100).value(QStringLiteral("limit")).toInt(), 100);
    QCOMPARE(getBlockedSendersRequest(0, 101).value(QStringLiteral("limit")).toInt(), 100);
    QCOMPARE(getBlockedSendersRequest(0, 5000).value(QStringLiteral("limit")).toInt(), 100);
}

void PrivacyRequestsTest::blockedSendersRequestShape()
{
    const QJsonObject request = getBlockedSendersRequest(10, 50);

    QCOMPARE(request.size(), 4);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getBlockedMessageSenders"));
    QCOMPARE(request.value(QStringLiteral("offset")).toInt(), 10);
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 50);

    // The schema requires a concrete block list; this client uses the main one.
    const QJsonValue blockList = request.value(QStringLiteral("block_list"));
    QVERIFY(blockList.isObject());
    QCOMPARE(blockList.toObject().size(), 1);
    QCOMPARE(blockList.toObject().value(QStringLiteral("@type")).toString(),
             QStringLiteral("blockListMain"));
}

void PrivacyRequestsTest::blocksUserSender()
{
    const QJsonObject request = setBlockedSenderRequest(42, true);

    QCOMPARE(request.size(), 3);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setMessageSenderBlockList"));

    // This schema has no toggleMessageSenderIsBlocked and no is_blocked flag.
    QVERIFY(!request.contains(QStringLiteral("is_blocked")));
    QVERIFY(!request.contains(QStringLiteral("user_id")));

    const QJsonObject sender = request.value(QStringLiteral("sender_id")).toObject();
    QCOMPARE(sender.size(), 2);
    QCOMPARE(sender.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSenderUser"));
    QCOMPARE(sender.value(QStringLiteral("user_id")).toVariant().toLongLong(), qint64(42));
    QVERIFY(!sender.contains(QStringLiteral("chat_id")));

    const QJsonValue blockList = request.value(QStringLiteral("block_list"));
    QVERIFY(blockList.isObject());
    QCOMPARE(blockList.toObject().size(), 1);
    QCOMPARE(blockList.toObject().value(QStringLiteral("@type")).toString(),
             QStringLiteral("blockListMain"));

    // int53 identifiers must not be truncated.
    QCOMPARE(setBlockedSenderRequest(9007199254740991LL, true)
                 .value(QStringLiteral("sender_id"))
                 .toObject()
                 .value(QStringLiteral("user_id"))
                 .toVariant()
                 .toLongLong(),
             qint64(9007199254740991LL));
}

void PrivacyRequestsTest::unblocksUserSenderWithNullBlockList()
{
    const QJsonObject request = setBlockedSenderRequest(42, false);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setMessageSenderBlockList"));
    // BlockList has no "none" variant, so unblocking is the null block_list.
    const QJsonValue blockList = request.value(QStringLiteral("block_list"));
    QVERIFY(blockList.isNull());
    QVERIFY(!request.contains(QStringLiteral("is_blocked")));
    QCOMPARE(request.value(QStringLiteral("sender_id"))
                 .toObject()
                 .value(QStringLiteral("@type"))
                 .toString(),
             QStringLiteral("messageSenderUser"));
    QCOMPARE(request.value(QStringLiteral("sender_id"))
                 .toObject()
                 .value(QStringLiteral("user_id"))
                 .toVariant()
                 .toLongLong(),
             qint64(42));
}

void PrivacyRequestsTest::requestsSurviveSerialization()
{
    const QJsonObject read = roundTrip(
        getPrivacySettingRequest(QStringLiteral("userPrivacySettingShowBirthdate")));
    QCOMPARE(read.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getUserPrivacySettingRules"));
    QCOMPARE(settingTypeOf(read), QStringLiteral("userPrivacySettingShowBirthdate"));

    const QJsonObject write = roundTrip(
        setPrivacySettingRequest(QStringLiteral("show_birthdate"), QStringLiteral("nobody")));
    QCOMPARE(write.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setUserPrivacySettingRules"));
    QCOMPARE(settingTypeOf(write), QStringLiteral("userPrivacySettingShowBirthdate"));
    QCOMPARE(write.value(QStringLiteral("rules"))
                 .toObject()
                 .value(QStringLiteral("@type"))
                 .toString(),
             QStringLiteral("userPrivacySettingRules"));
    QCOMPARE(ruleTypeOf(write), QStringLiteral("userPrivacySettingRuleRestrictAll"));
    QCOMPARE(singleRuleOf(write).size(), 1);

    const QJsonObject paged = roundTrip(getBlockedSendersRequest(20, 40));
    QCOMPARE(paged.value(QStringLiteral("offset")).toInt(), 20);
    QCOMPARE(paged.value(QStringLiteral("limit")).toInt(), 40);
    QCOMPARE(paged.value(QStringLiteral("block_list"))
                 .toObject()
                 .value(QStringLiteral("@type"))
                 .toString(),
             QStringLiteral("blockListMain"));

    const QJsonObject blocked = roundTrip(setBlockedSenderRequest(42, true));
    QCOMPARE(blocked.value(QStringLiteral("sender_id"))
                 .toObject()
                 .value(QStringLiteral("user_id"))
                 .toVariant()
                 .toLongLong(),
             qint64(42));
    QCOMPARE(blocked.value(QStringLiteral("block_list"))
                 .toObject()
                 .value(QStringLiteral("@type"))
                 .toString(),
             QStringLiteral("blockListMain"));

    // The null block list must stay null through the transport encoding.
    const QJsonObject unblocked = roundTrip(setBlockedSenderRequest(42, false));
    QVERIFY(unblocked.value(QStringLiteral("block_list")).isNull());
}

QTEST_GUILESS_MAIN(PrivacyRequestsTest)
#include "tst_privacy_requests.moc"
