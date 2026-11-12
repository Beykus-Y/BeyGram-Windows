#include "PrivacyRequests.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QStringList>

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int kMinBlockedSendersLimit = 1;
constexpr int kMaxBlockedSendersLimit = 100;
constexpr int kMinBlockedSendersOffset = 0;

// The schema's UserPrivacySetting variants, in schema order.
const QStringList& privacySettingTypes()
{
    static const QStringList types{
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
    return types;
}

// The UserPrivacySettingRule variants that consist of a @type alone. The four
// variants carrying user_ids or chat_ids cannot be named by a single rule.
const QStringList& scalarRuleTypes()
{
    static const QStringList types{
        QStringLiteral("userPrivacySettingRuleAllowAll"),
        QStringLiteral("userPrivacySettingRuleAllowContacts"),
        QStringLiteral("userPrivacySettingRuleAllowBots"),
        QStringLiteral("userPrivacySettingRuleAllowPremiumUsers"),
        QStringLiteral("userPrivacySettingRuleRestrictAll"),
        QStringLiteral("userPrivacySettingRuleRestrictContacts"),
        QStringLiteral("userPrivacySettingRuleRestrictBots"),
    };
    return types;
}

// Folds a name to its bare form: lower case and letters/digits only, without the
// given schema prefix. "userPrivacySettingRuleAllowContacts", "allow_contacts"
// and "Allow Contacts" all meet on "allowcontacts".
QString foldedName(const QString& name, const QString& prefix)
{
    QString folded;
    folded.reserve(name.size());
    for (const QChar character : name) {
        if (character.isLetterOrNumber())
            folded.append(character.toLower());
    }
    if (folded.startsWith(prefix))
        folded.remove(0, prefix.size());
    return folded;
}

QString canonicalSetting(const QString& setting)
{
    const QString prefix = QStringLiteral("userprivacysetting");
    const QString folded = foldedName(setting, prefix);
    if (folded.isEmpty())
        return {};

    for (const QString& type : privacySettingTypes()) {
        if (foldedName(type, prefix) == folded)
            return type;
    }
    return {};
}

QString canonicalRule(const QString& ruleType)
{
    const QString prefix = QStringLiteral("userprivacysettingrule");
    const QString folded = foldedName(ruleType, prefix);
    if (folded.isEmpty())
        return {};

    for (const QString& type : scalarRuleTypes()) {
        if (foldedName(type, prefix) == folded)
            return type;
    }

    // The three choices the privacy UI offers, plus the spellings of "everyone"
    // that a caller may use for the AllowAll rule.
    if (folded == QLatin1String("everyone") || folded == QLatin1String("everybody")
        || folded == QLatin1String("anyone") || folded == QLatin1String("anybody")
        || folded == QLatin1String("all")) {
        return QStringLiteral("userPrivacySettingRuleAllowAll");
    }
    if (folded == QLatin1String("contacts") || folded == QLatin1String("contact"))
        return QStringLiteral("userPrivacySettingRuleAllowContacts");
    if (folded == QLatin1String("nobody") || folded == QLatin1String("noone"))
        return QStringLiteral("userPrivacySettingRuleRestrictAll");

    // Unknown names and the id-carrying rules both end up here.
    return {};
}

bool ruleAllowedForSetting(const QString& settingType, const QString& ruleType)
{
    // "Can be set only to Allow contacts or Allow all".
    if (settingType == QLatin1String("userPrivacySettingAllowFindingByPhoneNumber")) {
        return ruleType == QLatin1String("userPrivacySettingRuleAllowAll")
            || ruleType == QLatin1String("userPrivacySettingRuleAllowContacts");
    }

    // "currently, allowed only for userPrivacySettingAllowChatInvites".
    if (ruleType == QLatin1String("userPrivacySettingRuleAllowPremiumUsers"))
        return settingType == QLatin1String("userPrivacySettingAllowChatInvites");

    return true;
}

QJsonObject typedObject(const QString& type)
{
    return QJsonObject{
        {QStringLiteral("@type"), type},
    };
}

QJsonObject blockListMain()
{
    return typedObject(QStringLiteral("blockListMain"));
}

} // namespace

QJsonObject getPrivacySettingRequest(const QString& setting)
{
    const QString settingType = canonicalSetting(setting);
    if (settingType.isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getUserPrivacySettingRules")},
        {QStringLiteral("setting"), typedObject(settingType)},
    };
}

QJsonObject setPrivacySettingRequest(const QString& setting, const QString& ruleType)
{
    const QString settingType = canonicalSetting(setting);
    if (settingType.isEmpty())
        return {};

    const QString type = canonicalRule(ruleType);
    if (type.isEmpty() || !ruleAllowedForSetting(settingType, type))
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setUserPrivacySettingRules")},
        {QStringLiteral("setting"), typedObject(settingType)},
        {QStringLiteral("rules"),
         QJsonObject{
             {QStringLiteral("@type"), QStringLiteral("userPrivacySettingRules")},
             {QStringLiteral("rules"), QJsonArray{typedObject(type)}},
         }},
    };
}

QJsonObject getBlockedSendersRequest(int offset, int limit)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getBlockedMessageSenders")},
        {QStringLiteral("block_list"), blockListMain()},
        {QStringLiteral("offset"), std::max(offset, kMinBlockedSendersOffset)},
        {QStringLiteral("limit"),
         std::clamp(limit, kMinBlockedSendersLimit, kMaxBlockedSendersLimit)},
    };
}

QJsonObject setBlockedSenderRequest(qint64 senderUserId, bool blocked)
{
    // BlockList has only the Main and Stories variants. The schema unblocks with
    // a null block_list, so there is no "none" object to build here.
    const QJsonValue blockList =
        blocked ? QJsonValue(blockListMain()) : QJsonValue(QJsonValue::Null);

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setMessageSenderBlockList")},
        {QStringLiteral("sender_id"),
         QJsonObject{
             {QStringLiteral("@type"), QStringLiteral("messageSenderUser")},
             {QStringLiteral("user_id"), senderUserId},
         }},
        {QStringLiteral("block_list"), blockList},
    };
}

} // namespace beygram::telegram
