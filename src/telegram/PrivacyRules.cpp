#include "PrivacyRules.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QString>

#include <cmath>

namespace beygram::telegram {
namespace {

const QString& rulePrefix()
{
    static const QString prefix = QStringLiteral("userprivacysettingrule");
    return prefix;
}

// Folds a rule name to its bare form: lower case and letters/digits only,
// without the "userPrivacySettingRule" prefix. "userPrivacySettingRuleAllowUsers",
// "allow_users" and "Allow Users" all meet on "allowusers".
QString foldedRuleName(const QString& type)
{
    QString folded;
    folded.reserve(type.size());
    for (const QChar character : type) {
        if (character.isLetterOrNumber())
            folded.append(character.toLower());
    }
    if (folded.startsWith(rulePrefix()))
        folded.remove(0, rulePrefix().size());
    return folded;
}

// The base audience named by a folded rule name, or an empty string when the
// rule is an exception or a variant this struct cannot evaluate.
QString baseRuleType(const QString& foldedName)
{
    if (foldedName == QLatin1String("allowall"))
        return QStringLiteral("userPrivacySettingRuleAllowAll");
    if (foldedName == QLatin1String("allowcontacts"))
        return QStringLiteral("userPrivacySettingRuleAllowContacts");
    if (foldedName == QLatin1String("restrictall"))
        return QStringLiteral("userPrivacySettingRuleRestrictAll");
    if (foldedName == QLatin1String("restrictcontacts"))
        return QStringLiteral("userPrivacySettingRuleRestrictContacts");
    // The premium rule is kept for display, but isAllowedByRules cannot verify
    // the sender's Premium status and therefore never treats it as a grant.
    if (foldedName == QLatin1String("allowpremiumusers"))
        return QStringLiteral("userPrivacySettingRuleAllowPremiumUsers");
    return {};
}

// Appends the ids of one exception rule. Only whole JSON numbers survive:
// user ids must be positive, chat ids must be non-zero. Duplicates keep their
// first position so the list stays in the server's order.
void appendIds(QVector<qint64>& target, const QJsonValue& value, bool userIds)
{
    if (!value.isArray())
        return;

    const QJsonArray ids = value.toArray();
    for (const QJsonValue& entry : ids) {
        if (!entry.isDouble())
            continue;

        const double raw = entry.toDouble();
        if (!std::isfinite(raw))
            continue;
        // int53 is the widest id TDLib sends, and every int53 is exact in a
        // double. Rejecting anything larger also keeps the cast defined.
        constexpr double kMaxInt53 = 9007199254740992.0;
        if (raw < -kMaxInt53 || raw > kMaxInt53)
            continue;

        const qint64 id = static_cast<qint64>(raw);
        if (static_cast<double>(id) != raw)
            continue;
        if (userIds ? id <= 0 : id == 0)
            continue;
        if (!target.contains(id))
            target.append(id);
    }
}

// The rules array of a response. A bare userPrivacySettingRules object carries
// it directly; a wrapper may nest the whole rules object under "rules".
QJsonArray rulesArrayOf(const QJsonObject& response)
{
    const QJsonValue rules = response.value(QStringLiteral("rules"));
    if (rules.isArray())
        return rules.toArray();
    if (rules.isObject()) {
        const QJsonValue nested = rules.toObject().value(QStringLiteral("rules"));
        if (nested.isArray())
            return nested.toArray();
    }
    return {};
}

QString settingOf(const QJsonObject& response)
{
    const QJsonValue setting = response.value(QStringLiteral("setting"));
    if (setting.isObject())
        return setting.toObject().value(QStringLiteral("@type")).toString();
    if (setting.isString())
        return setting.toString();
    return {};
}

} // namespace

PrivacyRuleSet privacyRulesFromResponse(const QJsonObject& response)
{
    PrivacyRuleSet rules;
    rules.setting = settingOf(response);

    const QJsonArray entries = rulesArrayOf(response);
    for (const QJsonValue& value : entries) {
        if (!value.isObject())
            continue;

        const QJsonObject rule = value.toObject();
        const QString name =
            foldedRuleName(rule.value(QStringLiteral("@type")).toString());
        if (name.isEmpty())
            continue;

        if (name == QLatin1String("allowusers")) {
            appendIds(rules.allowUserIds, rule.value(QStringLiteral("user_ids")), true);
        } else if (name == QLatin1String("restrictusers")) {
            appendIds(rules.restrictUserIds, rule.value(QStringLiteral("user_ids")), true);
        } else if (name == QLatin1String("allowchatmembers")) {
            appendIds(rules.allowChatIds, rule.value(QStringLiteral("chat_ids")), false);
        } else if (name == QLatin1String("restrictchatmembers")) {
            appendIds(rules.restrictChatIds, rule.value(QStringLiteral("chat_ids")), false);
        } else if (rules.baseRule.isEmpty()) {
            // TDLib matches in order, so the first base rule is the fallback;
            // later base rules can only widen or narrow it in ways the struct
            // does not represent and are ignored.
            rules.baseRule = baseRuleType(name);
        }
    }

    return rules;
}

bool isAllowedByRules(const PrivacyRuleSet& rules, qint64 userId, qint64 chatId,
                      bool isContact)
{
    // Exceptions precede the base audience in a TDLib rule list. Allow comes
    // before restrict, which is the order this client writes its own rules in,
    // so a sender that both lists name is allowed rather than denied.
    if (userId > 0 && rules.allowUserIds.contains(userId))
        return true;
    if (chatId != 0 && rules.allowChatIds.contains(chatId))
        return true;
    if (userId > 0 && rules.restrictUserIds.contains(userId))
        return false;
    if (chatId != 0 && rules.restrictChatIds.contains(chatId))
        return false;

    const QString base = foldedRuleName(rules.baseRule);
    if (base == QLatin1String("allowall"))
        return true;
    if (base == QLatin1String("allowcontacts"))
        return isContact;
    if (base == QLatin1String("restrictcontacts"))
        return !isContact;

    // RestrictAll, an empty base rule, the premium rule and anything unknown
    // all fall through to TDLib's "no rule matched, not allowed" default.
    return false;
}

} // namespace beygram::telegram
