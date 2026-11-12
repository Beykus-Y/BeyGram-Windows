#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Pure builders for reading and writing privacy rules and the blocked-sender
/// list. They contain no logging, no secrets and no @extra; the transport adds
/// correlation.
///
/// Shapes follow the TDLib schema pinned by this client (1.8.69, commit
/// 4c6377aa, see docs/WINDOWS_TDLIB.md):
///
///   getUserPrivacySettingRules setting:UserPrivacySetting
///       = UserPrivacySettingRules;
///   setUserPrivacySettingRules setting:UserPrivacySetting
///       rules:userPrivacySettingRules = Ok;
///   userPrivacySettingRules rules:vector<UserPrivacySettingRule>
///       = UserPrivacySettingRules;
///   getBlockedMessageSenders block_list:BlockList offset:int32 limit:int32
///       = MessageSenders;
///   setMessageSenderBlockList sender_id:MessageSender block_list:BlockList = Ok;
///
/// This revision has 14 UserPrivacySetting variants and 11 UserPrivacySettingRule
/// variants. There is no "editable" flag on the rules object and no
/// getBlockedMessageSenders overload that omits block_list.

/// Builds getUserPrivacySettingRules for one setting.
///
/// `setting` accepts the schema @type ("userPrivacySettingShowStatus") as well as
/// a loose spelling of it ("show_status", "showStatus", "Show Status",
/// "user_privacy_setting_show_status"). Case, separators and surrounding
/// whitespace are ignored. An unknown or empty name returns an empty object, so a
/// typo is never sent to TDLib.
[[nodiscard]] QJsonObject getPrivacySettingRequest(const QString& setting);

/// Builds setUserPrivacySettingRules with exactly one rule for one setting.
///
/// `ruleType` accepts a scalar UserPrivacySettingRule @type
/// ("userPrivacySettingRuleAllowContacts") or its loose name ("allow_contacts",
/// "AllowContacts"). The three UI choices are accepted as well: "everyone" (and
/// "all", "anybody") writes AllowAll, "contacts" writes AllowContacts, and
/// "nobody" (and "noone") writes RestrictAll.
///
/// A single rule is the whole policy, because TDLib matches rules in order and
/// denies a user that matches none: AllowContacts alone means "contacts only",
/// RestrictAll alone means "nobody", and AllowAll alone means "everybody".
///
/// The id-carrying rules (userPrivacySettingRuleAllowUsers,
/// ...RestrictUsers, ...AllowChatMembers, ...RestrictChatMembers) cannot be
/// expressed by one name and return an empty object; they need a full rules list.
///
/// Two schema restrictions are enforced and also return an empty object:
///   - userPrivacySettingAllowFindingByPhoneNumber accepts only AllowAll or
///     AllowContacts ("Can be set only to Allow contacts or Allow all"), so
///     nobody/restrict and bot rules are rejected there.
///   - userPrivacySettingRuleAllowPremiumUsers is valid only for
///     userPrivacySettingAllowChatInvites ("currently, allowed only for ...").
///
/// An unknown setting or rule name returns an empty object.
[[nodiscard]] QJsonObject setPrivacySettingRequest(const QString& setting,
                                                   const QString& ruleType);

/// Builds getBlockedMessageSenders for the main block list. The schema requires a
/// concrete block_list, and this client has no stories block list yet, so
/// blockListMain is always sent. offset is clamped to a non-negative value and
/// limit to 1..100, the range TDLib accepts.
[[nodiscard]] QJsonObject getBlockedSendersRequest(int offset, int limit);

/// Builds setMessageSenderBlockList for a user sender. senderUserId is an int53
/// user identifier and must be positive; the builder does not guess the
/// messageSenderUser variant from the sign, so a chat id can never leak into
/// user_id. blocked=true sends blockListMain; blocked=false sends the null
/// block_list, which is how this schema unblocks a sender (BlockList has only the
/// Main and Stories variants, so there is no "none" object to send).
[[nodiscard]] QJsonObject setBlockedSenderRequest(qint64 senderUserId, bool blocked);

} // namespace beygram::telegram
