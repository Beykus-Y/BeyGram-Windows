#include "telegram/PrivacyRules.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject parseObject(const QByteArray& json)
{
    return QJsonDocument::fromJson(json).object();
}

QString allowAll()
{
    return QStringLiteral("userPrivacySettingRuleAllowAll");
}

QString allowContacts()
{
    return QStringLiteral("userPrivacySettingRuleAllowContacts");
}

QString restrictAll()
{
    return QStringLiteral("userPrivacySettingRuleRestrictAll");
}

QString restrictContacts()
{
    return QStringLiteral("userPrivacySettingRuleRestrictContacts");
}

QString allowPremium()
{
    return QStringLiteral("userPrivacySettingRuleAllowPremiumUsers");
}

} // namespace

class PrivacyRulesTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesBareRulesObject();
    void parsesWrappedRulesObject();
    void parsesRulesArrayWrapper();
    void readsSettingName();
    void parsesBaseRuleVariants();
    void parsesUserExceptionsInOrder();
    void parsesChatMemberExceptions();
    void keepsPremiumRule();
    void keepsFirstBaseRuleOnly();
    void ignoresUnknownRulesAndBadIds();
    void emptyOrMalformedResponseIsEmpty();
    void evaluatesBaseRules();
    void allowExceptionsOverrideBase();
    void restrictExceptionsOverrideBase();
    void allowWinsOverRestrict();
    void evaluatesChatMemberExceptions();
    void unverifiableBaseRulesDeny();
    void emptyBaseRuleDenies();
};

void PrivacyRulesTest::parsesBareRulesObject()
{
    const PrivacyRuleSet rules = privacyRulesFromResponse(parseObject(R"({
        "@type": "userPrivacySettingRules",
        "rules": [
            { "@type": "userPrivacySettingRuleAllowAll" }
        ]
    })"));

    QVERIFY(rules.setting.isEmpty());
    QCOMPARE(rules.baseRule, allowAll());
    QVERIFY(rules.allowUserIds.isEmpty());
    QVERIFY(rules.restrictUserIds.isEmpty());
    QVERIFY(rules.allowChatIds.isEmpty());
    QVERIFY(rules.restrictChatIds.isEmpty());
}

void PrivacyRulesTest::parsesWrappedRulesObject()
{
    // The shape of a cached answer that also remembers what was asked for.
    const PrivacyRuleSet rules = privacyRulesFromResponse(parseObject(R"({
        "@type": "getUserPrivacySettingRules",
        "setting": { "@type": "userPrivacySettingShowStatus" },
        "rules": {
            "@type": "userPrivacySettingRules",
            "rules": [
                { "@type": "userPrivacySettingRuleRestrictAll" }
            ]
        }
    })"));

    QCOMPARE(rules.setting, QStringLiteral("userPrivacySettingShowStatus"));
    QCOMPARE(rules.baseRule, restrictAll());
}

void PrivacyRulesTest::parsesRulesArrayWrapper()
{
    // A wrapper may hold the array directly under "rules".
    const PrivacyRuleSet rules = privacyRulesFromResponse(parseObject(R"({
        "rules": [
            { "@type": "userPrivacySettingRuleAllowContacts" }
        ]
    })"));

    QCOMPARE(rules.baseRule, allowContacts());
    QVERIFY(rules.setting.isEmpty());
}

void PrivacyRulesTest::readsSettingName()
{
    const PrivacyRuleSet fromString = privacyRulesFromResponse(parseObject(R"({
        "setting": "userPrivacySettingAllowCalls",
        "rules": []
    })"));
    QCOMPARE(fromString.setting, QStringLiteral("userPrivacySettingAllowCalls"));

    // A bare rules object carries no setting.
    QVERIFY(privacyRulesFromResponse(parseObject(R"({
        "@type": "userPrivacySettingRules",
        "rules": []
    })")).setting.isEmpty());
}

void PrivacyRulesTest::parsesBaseRuleVariants()
{
    const auto baseOf = [](const QString& ruleType) {
        const QString json =
            QStringLiteral(R"({ "rules": [ { "@type": "%1" } ] })").arg(ruleType);
        return privacyRulesFromResponse(parseObject(json.toUtf8())).baseRule;
    };

    QCOMPARE(baseOf("userPrivacySettingRuleAllowAll"), allowAll());
    QCOMPARE(baseOf("userPrivacySettingRuleAllowContacts"), allowContacts());
    QCOMPARE(baseOf("userPrivacySettingRuleRestrictAll"), restrictAll());
    QCOMPARE(baseOf("userPrivacySettingRuleRestrictContacts"), restrictContacts());

    // Folded spellings are accepted too.
    QCOMPARE(baseOf("allow_contacts"), allowContacts());
    QCOMPARE(baseOf("AllowAll"), allowAll());
}

void PrivacyRulesTest::parsesUserExceptionsInOrder()
{
    const PrivacyRuleSet rules = privacyRulesFromResponse(parseObject(R"({
        "@type": "userPrivacySettingRules",
        "rules": [
            {
                "@type": "userPrivacySettingRuleAllowUsers",
                "user_ids": [ 42, 7, 42 ]
            },
            {
                "@type": "userPrivacySettingRuleRestrictUsers",
                "user_ids": [ 99 ]
            },
            { "@type": "userPrivacySettingRuleAllowContacts" }
        ]
    })"));

    QCOMPARE(rules.allowUserIds, (QVector<qint64>{42, 7}));
    QCOMPARE(rules.restrictUserIds, (QVector<qint64>{99}));
    QCOMPARE(rules.baseRule, allowContacts());
}

void PrivacyRulesTest::parsesChatMemberExceptions()
{
    const PrivacyRuleSet rules = privacyRulesFromResponse(parseObject(R"({
        "@type": "userPrivacySettingRules",
        "rules": [
            {
                "@type": "userPrivacySettingRuleAllowChatMembers",
                "chat_ids": [ -1001234567890, -1001 ]
            },
            {
                "@type": "userPrivacySettingRuleRestrictChatMembers",
                "chat_ids": [ -1009 ]
            },
            { "@type": "userPrivacySettingRuleRestrictAll" }
        ]
    })"));

    QCOMPARE(rules.allowChatIds, (QVector<qint64>{-1001234567890LL, -1001LL}));
    QCOMPARE(rules.restrictChatIds, (QVector<qint64>{-1009LL}));
    QCOMPARE(rules.baseRule, restrictAll());
}

void PrivacyRulesTest::keepsPremiumRule()
{
    // The rule is preserved for display, but see unverifiableBaseRulesDeny.
    const PrivacyRuleSet rules = privacyRulesFromResponse(parseObject(R"({
        "@type": "userPrivacySettingRules",
        "rules": [
            { "@type": "userPrivacySettingRuleAllowPremiumUsers" }
        ]
    })"));

    QCOMPARE(rules.baseRule, allowPremium());
}

void PrivacyRulesTest::keepsFirstBaseRuleOnly()
{
    // TDLib matches in order, so the first base rule is the fallback. A second
    // base rule cannot be represented and is not merged into the first.
    const PrivacyRuleSet rules = privacyRulesFromResponse(parseObject(R"({
        "@type": "userPrivacySettingRules",
        "rules": [
            { "@type": "userPrivacySettingRuleAllowPremiumUsers" },
            { "@type": "userPrivacySettingRuleAllowContacts" },
            { "@type": "userPrivacySettingRuleRestrictAll" }
        ]
    })"));

    QCOMPARE(rules.baseRule, allowPremium());
}

void PrivacyRulesTest::ignoresUnknownRulesAndBadIds()
{
    const PrivacyRuleSet rules = privacyRulesFromResponse(parseObject(R"({
        "@type": "userPrivacySettingRules",
        "rules": [
            "userPrivacySettingRuleAllowAll",
            null,
            { "@type": "userPrivacySettingRuleAllowBots" },
            { "@type": "userPrivacySettingRuleRestrictBots" },
            { "@type": "userPrivacySettingRuleFutureVariant" },
            {
                "@type": "userPrivacySettingRuleAllowUsers",
                "user_ids": [ 0, -5, 12.5, "77", 1e300, 77 ]
            },
            {
                "@type": "userPrivacySettingRuleAllowChatMembers",
                "chat_ids": [ 0, 5.5, -77 ]
            },
            { "@type": "userPrivacySettingRuleAllowAll" }
        ]
    })"));

    // Unknown and bot rules never become a base audience.
    QCOMPARE(rules.baseRule, allowAll());
    QCOMPARE(rules.allowUserIds, (QVector<qint64>{77}));
    QCOMPARE(rules.allowChatIds, (QVector<qint64>{-77}));
    QVERIFY(rules.restrictUserIds.isEmpty());
    QVERIFY(rules.restrictChatIds.isEmpty());
}

void PrivacyRulesTest::emptyOrMalformedResponseIsEmpty()
{
    const auto verifyEmpty = [](const QJsonObject& response) {
        const PrivacyRuleSet rules = privacyRulesFromResponse(response);
        QVERIFY(rules.setting.isEmpty());
        QVERIFY(rules.baseRule.isEmpty());
        QVERIFY(rules.allowUserIds.isEmpty());
        QVERIFY(rules.restrictUserIds.isEmpty());
        QVERIFY(rules.allowChatIds.isEmpty());
        QVERIFY(rules.restrictChatIds.isEmpty());
    };

    verifyEmpty(QJsonObject{});
    verifyEmpty(parseObject(R"({ "@type": "userPrivacySettingRules" })"));
    verifyEmpty(parseObject(R"({ "@type": "userPrivacySettingRules", "rules": null })"));
    verifyEmpty(parseObject(R"({ "rules": "allow all" })"));
    verifyEmpty(parseObject(R"({ "rules": {} })"));
    verifyEmpty(parseObject(R"({ "rules": [ 1, 2, 3 ] })"));
}

void PrivacyRulesTest::evaluatesBaseRules()
{
    PrivacyRuleSet rules;
    rules.baseRule = allowAll();
    QVERIFY(isAllowedByRules(rules, 42));
    QVERIFY(isAllowedByRules(rules, 42, 0, false));

    rules.baseRule = restrictAll();
    QVERIFY(!isAllowedByRules(rules, 42));
    QVERIFY(!isAllowedByRules(rules, 42, 0, true));

    rules.baseRule = allowContacts();
    QVERIFY(isAllowedByRules(rules, 42, 0, true));
    QVERIFY(!isAllowedByRules(rules, 42, 0, false));

    rules.baseRule = restrictContacts();
    QVERIFY(!isAllowedByRules(rules, 42, 0, true));
    QVERIFY(isAllowedByRules(rules, 42, 0, false));
}

void PrivacyRulesTest::allowExceptionsOverrideBase()
{
    PrivacyRuleSet rules;
    rules.baseRule = restrictAll();
    rules.allowUserIds = {7};
    QVERIFY(isAllowedByRules(rules, 7));
    QVERIFY(!isAllowedByRules(rules, 8));

    // A chat-member exception also overrides a restrictive base.
    rules.allowChatIds = {-1001};
    QVERIFY(isAllowedByRules(rules, 8, -1001));
    QVERIFY(!isAllowedByRules(rules, 8, -1002));

    // An unrelated chat id does not unlock a user.
    QVERIFY(!isAllowedByRules(rules, 9, 0));
}

void PrivacyRulesTest::restrictExceptionsOverrideBase()
{
    PrivacyRuleSet rules;
    rules.baseRule = allowAll();
    rules.restrictUserIds = {7};
    QVERIFY(!isAllowedByRules(rules, 7));
    QVERIFY(isAllowedByRules(rules, 8));

    rules.restrictChatIds = {-1001};
    QVERIFY(!isAllowedByRules(rules, 8, -1001));
    QVERIFY(isAllowedByRules(rules, 8, -1002));

    // Contacts are still denied by an explicit exception.
    rules.baseRule = allowContacts();
    QVERIFY(!isAllowedByRules(rules, 7, 0, true));
    QVERIFY(isAllowedByRules(rules, 8, 0, true));
}

void PrivacyRulesTest::allowWinsOverRestrict()
{
    // Allow exceptions are evaluated first, matching the order the rule builder
    // writes them in, so a contradictory deny does not flip the decision.
    PrivacyRuleSet rules;
    rules.baseRule = restrictAll();
    rules.allowUserIds = {7};
    rules.restrictUserIds = {7};
    QVERIFY(isAllowedByRules(rules, 7));

    rules.allowChatIds = {-1001};
    rules.restrictChatIds = {-1001};
    QVERIFY(isAllowedByRules(rules, 8, -1001));
}

void PrivacyRulesTest::evaluatesChatMemberExceptions()
{
    PrivacyRuleSet rules;
    rules.baseRule = allowContacts();
    rules.allowChatIds = {-1001};

    QVERIFY(isAllowedByRules(rules, 7, -1001));
    QVERIFY(!isAllowedByRules(rules, 7, -1002));

    rules.restrictChatIds = {-1002};
    QVERIFY(!isAllowedByRules(rules, 7, -1002));

    // chatId 0 never matches a chat exception; an invalid user id never
    // matches a user exception either.
    rules.restrictChatIds = {0};
    QVERIFY(isAllowedByRules(rules, 7, 0, true));

    PrivacyRuleSet invalidUser;
    invalidUser.baseRule = restrictAll();
    invalidUser.allowUserIds = {7};
    QVERIFY(isAllowedByRules(invalidUser, 7));
    QVERIFY(!isAllowedByRules(invalidUser, 0));
    QVERIFY(!isAllowedByRules(invalidUser, -7));
}

void PrivacyRulesTest::unverifiableBaseRulesDeny()
{
    // Premium status is not an argument, so the premium rule cannot grant.
    PrivacyRuleSet rules;
    rules.baseRule = allowPremium();
    QVERIFY(!isAllowedByRules(rules, 42));
    QVERIFY(!isAllowedByRules(rules, 42, 0, true));

    // Unknown names fail closed as well.
    rules.baseRule = QStringLiteral("userPrivacySettingRuleAllowBots");
    QVERIFY(!isAllowedByRules(rules, 42));
}

void PrivacyRulesTest::emptyBaseRuleDenies()
{
    // TDLib's documented default when no rule matches is "not allowed".
    QVERIFY(!isAllowedByRules(PrivacyRuleSet{}, 42));
    QVERIFY(!isAllowedByRules(PrivacyRuleSet{}, 42, -1001, true));

    // Exceptions alone still work without a base rule.
    PrivacyRuleSet rules;
    rules.allowUserIds = {42};
    QVERIFY(isAllowedByRules(rules, 42));
    QVERIFY(!isAllowedByRules(rules, 43));
}

QTEST_GUILESS_MAIN(PrivacyRulesTest)
#include "tst_privacy_rules.moc"
