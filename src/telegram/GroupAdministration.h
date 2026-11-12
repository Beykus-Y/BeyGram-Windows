#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Administrative rights of one member of a group or channel, parsed from
/// TDLib's chatAdministratorRights.
///
/// The struct holds no pointers, no session state and no secrets, so a row can
/// be cached per account and copied across threads.
///
/// Shape follows the TDLib schema pinned by this client (1.8.69, commit
/// 4c6377aa, see docs/WINDOWS_TDLIB.md):
///
///   chatAdministratorRights can_manage_chat:Bool can_change_info:Bool
///       can_post_messages:Bool can_edit_messages:Bool can_delete_messages:Bool
///       can_invite_users:Bool can_restrict_members:Bool can_pin_messages:Bool
///       can_manage_topics:Bool can_promote_members:Bool
///       can_manage_video_chats:Bool can_post_stories:Bool can_edit_stories:Bool
///       can_delete_stories:Bool can_manage_direct_messages:Bool
///       can_manage_tags:Bool is_anonymous:Bool = ChatAdministratorRights;
///
/// Only the rights this client's member editor owns are modelled. Channel
/// publishing rights (can_post_messages, can_edit_messages), story rights,
/// can_manage_chat, can_manage_direct_messages and can_manage_tags are not
/// represented: the parser ignores them and a request writes them as denied.
///
/// canBanUsers is TDLib's can_restrict_members: banning and restricting are one
/// right in this schema, and there is no can_ban_users field. A payload that
/// carries can_ban_users instead (a cached or hand-written object) is accepted
/// as a fallback when can_restrict_members is absent or not a boolean.
struct AdminRights {
    bool canChangeInfo = false;
    bool canDeleteMessages = false;
    bool canBanUsers = false;
    bool canInviteUsers = false;
    bool canPinMessages = false;
    bool canManageTopics = false;
    bool canPromoteMembers = false;
    bool canManageVideoChats = false;
    bool anonymous = false;
};

/// One administrator or the owner of a group or channel, as the member screen
/// shows them.
///
/// The struct is a plain value type: no pointers, no session state and no
/// secrets, so a row can be cached per account and copied across threads.
///
/// status is the short role name used elsewhere in this client: "creator" for
/// the owner and "administrator" for everyone else the parser keeps. rights is
/// read from status.rights of an administrator; the creator status carries no
/// rights object, so the owner keeps the default rights and is identified by
/// isCreator instead.
struct ChatMemberAdminRow {
    qint64 userId = 0;
    QString name;
    QString status;
    AdminRights rights;
    bool isCreator = false;
};

/// Builds setChatMemberStatus for one member:
///
///   setChatMemberStatus chat_id:int53 member_id:MessageSender
///       status:ChatMemberStatus = Ok;
///
/// status must be exactly one of "administrator", "member", "banned" or
/// "left"; any other value (including the TDLib @type names and a differently
/// cased spelling) yields an empty object, so a typo never demotes or bans
/// somebody. "restricted" is deliberately not accepted: a restriction carries
/// per-chat permissions and has its own editor.
///
/// The administrator variant carries can_be_edited false and a fully denied
/// chatAdministratorRights object. This builder changes the role only; a caller
/// that grants rights builds the status with those rights itself.
///
/// chatId 0 and a non-positive userId yield an empty object. A chat id may be
/// negative (supergroups and channels), so only 0 is rejected.
[[nodiscard]] QJsonObject setChatMemberStatusRequest(qint64 chatId, qint64 userId,
                                                     const QString& status);

/// Builds getChatAdministrators:
///
///   getChatAdministrators chat_id:int53 = ChatAdministrators;
///
/// The answer is chatAdministrator rows (user id, custom title, owner flag), not
/// chatMember rows, so it carries no granular rights.
/// administratorsFromResponse accepts that answer as well, with default rights;
/// getChatMember is what reports the rights of one administrator.
///
/// chatId 0 yields an empty object; a negative chat id is valid.
[[nodiscard]] QJsonObject getChatAdministratorsRequest(qint64 chatId);

/// Builds getChatMember:
///
///   getChatMember chat_id:int53 member_id:MessageSender = ChatMember;
///
/// The answer is one chatMember object, which administratorsFromResponse
/// accepts directly. chatId 0 and a non-positive userId yield an empty object.
[[nodiscard]] QJsonObject getChatMemberRequest(qint64 chatId, qint64 userId);

/// Builds setChatMemberTag:
///
///   setChatMemberTag chat_id:int53 user_id:int53 tag:string = Ok;
///
/// The tag is trimmed of surrounding whitespace; an empty result clears the
/// member's tag, which is what TDLib expects for "no tag". The length limit is
/// enforced by the server, not by this builder, so a long tag is sent as is.
///
/// chatId 0 and a non-positive userId yield an empty object.
[[nodiscard]] QJsonObject setChatMemberTagRequest(qint64 chatId, qint64 userId,
                                                  const QString& tag);

/// Builds banChatMember:
///
///   banChatMember chat_id:int53 member_id:MessageSender
///       banned_until_date:int32 revoke_messages:Bool = Ok;
///
/// untilDate is an absolute Unix time in seconds. 0 and any negative value mean
/// "forever" and are sent as 0; a positive value above INT_MAX is clamped to
/// it, the largest date an int32 field can carry. revokeMessages is sent as is,
/// so the caller decides whether the banned member's messages also disappear.
///
/// chatId 0 and a non-positive userId yield an empty object.
[[nodiscard]] QJsonObject banChatMemberRequest(qint64 chatId, qint64 userId,
                                               qint64 untilDate, bool revokeMessages);

/// Parses the administrators out of a TDLib answer.
///
/// Accepted shapes:
///  - chatMembers: a "members" array of chatMember objects;
///  - chatAdministrators: an "administrators" array of chatAdministrator
///    objects, i.e. the getChatAdministrators answer;
///  - a bare chatMember object (the getChatMember answer) or a bare
///    chatAdministrator object.
///
/// A chatMember row is kept only when its member_id (or a flattened user_id/id)
/// resolves to a positive user id and its status is
/// chatMemberStatusAdministrator or chatMemberStatusCreator. The status becomes
/// "administrator" or "creator".
/// Rights are read from status.rights for an administrator; the creator status
/// has no rights object, so the owner keeps the default rights and isCreator
/// true, while its is_anonymous flag is still copied. A chatAdministrator row
/// carries no rights of its own and keeps the defaults; its is_owner becomes
/// isCreator, and a merged rights object is read when one is present.
///
/// Rows that are not objects, that carry no positive user id (including a
/// messageSenderChat member, which is a chat and not a user), or whose status
/// is missing, malformed or a role this parser does not keep are skipped, so a
/// partially malformed answer still yields its valid rows in wire order.
/// Duplicates are preserved so paging code can de-duplicate across pages.
///
/// Names are resolved from the member's embedded user, then from a top-level
/// "users" array. When neither carries name fields the name is left empty for
/// the caller to resolve rather than invented.
///
/// A missing or malformed array contributes no rows.
[[nodiscard]] QVector<ChatMemberAdminRow> administratorsFromResponse(const QJsonObject& response);

/// Parses a chatAdministratorRights object.
///
/// Every modelled flag that is absent or not a JSON boolean is treated as
/// denied, so malformed data can never grant a right. canBanUsers reads
/// can_restrict_members, falling back to can_ban_users only when the schema
/// name is absent or not a boolean; anonymous reads is_anonymous. Fields this
/// struct does not model are ignored.
[[nodiscard]] AdminRights adminRightsFromJson(const QJsonObject& rights);

} // namespace beygram::telegram
