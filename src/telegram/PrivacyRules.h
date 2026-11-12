#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One account's privacy policy for one setting, flattened out of TDLib's
/// userPrivacySettingRules. The struct holds no pointers, no session state and
/// no secrets, so it can be cached per account and copied across threads.
///
/// Shape follows the TDLib schema pinned by this client (1.8.69, commit
/// 4c6377aa, see docs/WINDOWS_TDLIB.md):
///
///   getUserPrivacySettingRules setting:UserPrivacySetting
///       = UserPrivacySettingRules;
///   userPrivacySettingRules rules:vector<UserPrivacySettingRule>
///       = UserPrivacySettingRules;
///
///   userPrivacySettingRuleAllowAll = UserPrivacySettingRule;
///   userPrivacySettingRuleAllowContacts = UserPrivacySettingRule;
///   userPrivacySettingRuleAllowUsers user_ids:vector<int53>
///       = UserPrivacySettingRule;
///   userPrivacySettingRuleAllowChatMembers chat_ids:vector<int53>
///       = UserPrivacySettingRule;
///   userPrivacySettingRuleAllowPremiumUsers = UserPrivacySettingRule;
///   userPrivacySettingRuleRestrictAll = UserPrivacySettingRule;
///   userPrivacySettingRuleRestrictContacts = UserPrivacySettingRule;
///   userPrivacySettingRuleRestrictUsers user_ids:vector<int53>
///       = UserPrivacySettingRule;
///   userPrivacySettingRuleRestrictChatMembers chat_ids:vector<int53>
///       = UserPrivacySettingRule;
///
/// TDLib matches the list in order and the first matching rule decides, so the
/// exceptions ("always allow" and "never allow" users and chats) are listed
/// before the base audience and override it. The struct keeps the exception
/// lists in server order and the first base rule (allow/restrict all or
/// contacts, or the premium variant) as the base audience; further base rules
/// are ignored because they could only add reach the base cannot express.
///
/// The premium variant is preserved in baseRule but never grants access: the
/// sender's Premium status is not part of isAllowedByRules, and a rule this
/// code cannot verify must not be read as a permission. Bot rules are not
/// represented either; an unknown rule type is ignored rather than granted.
struct PrivacyRuleSet {
    /// @type of the setting the rules belong to, e.g.
    /// "userPrivacySettingShowStatus", or empty when the payload does not name
    /// one (a bare userPrivacySettingRules object carries no setting).
    QString setting;

    /// @type of the base audience rule, e.g.
    /// "userPrivacySettingRuleAllowContacts", or empty when the response
    /// carried no base rule. Only the first base rule is kept.
    QString baseRule;

    /// userPrivacySettingRuleAllowUsers user_ids, in server order.
    QVector<qint64> allowUserIds;

    /// userPrivacySettingRuleRestrictUsers user_ids, in server order.
    QVector<qint64> restrictUserIds;

    /// userPrivacySettingRuleAllowChatMembers chat_ids, in server order.
    QVector<qint64> allowChatIds;

    /// userPrivacySettingRuleRestrictChatMembers chat_ids, in server order.
    QVector<qint64> restrictChatIds;
};

/// Parses a getUserPrivacySettingRules answer into a PrivacyRuleSet.
///
/// `response` may be the userPrivacySettingRules object itself
/// ({"@type":"userPrivacySettingRules","rules":[...]}) or a wrapper that also
/// carries the setting it was requested for:
///
///   {"setting":{"@type":"userPrivacySettingShowStatus"},
///    "rules":{"@type":"userPrivacySettingRules","rules":[...]}}
///
/// A wrapper whose "rules" is the array directly, and a setting given as a
/// plain string, are accepted as well. Rule names are matched after folding
/// case, separators and the "userPrivacySettingRule" prefix, so
/// "userPrivacySettingRuleAllowUsers" and "allow_users" are the same rule.
///
/// Ids are taken only from JSON numbers that are whole, in range and non-zero:
/// user ids must be positive, chat ids are normally negative for groups and
/// channels but only zero is rejected. Duplicates keep their first position,
/// so the lists stay in the order TDLib returned. A missing, null or malformed
/// rules list yields an empty set, which isAllowedByRules reads as "nobody".
[[nodiscard]] PrivacyRuleSet privacyRulesFromResponse(const QJsonObject& response);

/// Applies a parsed rule set to one sender, following TDLib's first-match
/// order: allow exceptions, then restrict exceptions, then the base audience.
///
/// `userId` is the sender's user id and `chatId` is the chat a chat-member
/// exception names (the peer chat itself, or a chat the sender belongs to,
/// depending on the caller). `isContact` is the account's own knowledge of
/// whether the sender is a contact; TDLib does not resolve that for the caller.
///
/// An id in both exception lists is allowed, because the allow list is checked
/// first, matching the order this client writes its own rules in. Without a
/// base rule, and for a premium or unknown base rule, no rule matches and the
/// sender is denied: TDLib's documented default for an unmatched rule is "not
/// allowed", so a malformed or unsupported policy never grants access.
[[nodiscard]] bool isAllowedByRules(const PrivacyRuleSet& rules, qint64 userId,
                                    qint64 chatId = 0, bool isContact = false);

} // namespace beygram::telegram
