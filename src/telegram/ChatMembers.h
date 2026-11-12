#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One member of a group or channel, parsed from TDLib's chatMember object.
///
/// The struct holds no pointers, no session state and no secrets, so it can be
/// cached per account and copied across threads.
///
/// Shape follows the TDLib schema pinned by this client (see
/// docs/WINDOWS_TDLIB.md):
///
///   chatMembers total_count:int32 members:vector<chatMember> = ChatMembers;
///   chatMember member_id:MessageSender tag:string inviter_user_id:int53
///              joined_chat_date:int32 status:ChatMemberStatus = ChatMember;
///   chatMemberStatusCreator is_anonymous:Bool is_member:Bool
///       = ChatMemberStatus;
///   chatMemberStatusAdministrator can_be_edited:Bool
///       rights:chatAdministratorRights = ChatMemberStatus;
///   chatMemberStatusMember member_until_date:int32 = ChatMemberStatus;
///   chatMemberStatusRestricted is_member:Bool restricted_until_date:int32
///       permissions:chatPermissions = ChatMemberStatus;
///   chatMemberStatusLeft = ChatMemberStatus;
///   chatMemberStatusBanned banned_until_date:int32 = ChatMemberStatus;
///
/// A chatMember carries no user name or bot flag of its own: those live on the
/// user object TDLib returns separately. The parser reads them when the caller
/// has merged the matching `user` object into the member, or when the response
/// carries a top-level `users` array, and otherwise leaves displayName empty
/// instead of inventing a name.
struct ChatMember {
    /// Positive user identifier. Members whose member_id is a chat (a banned or
    /// left supergroup/channel) carry no user id and are skipped by the parser.
    qint64 userId = 0;

    /// "First Last", then the user's display/name field, then the first active
    /// username, then "Telegram user". Empty when no user data was available,
    /// so the caller can resolve the id itself.
    QString displayName;

    /// Role name taken from the ChatMemberStatus @type: "creator",
    /// "administrator", "member", "restricted", "left" or "banned". Empty when
    /// the member has no usable status object. A status variant this build does
    /// not know is preserved verbatim rather than discarded.
    QString status;

    /// True only for chatMemberStatusAdministrator. The owner is reported by
    /// isOwner instead, so the two roles stay distinguishable; a caller that
    /// only needs "may administer" checks isAdmin || isOwner.
    bool isAdmin = false;

    /// True only for chatMemberStatusCreator.
    bool isOwner = false;

    /// True for a user whose type is userTypeBot, or whose merged user object
    /// carries is_bot. A chat member that is not a user is never a bot.
    bool isBot = false;
};

/// Builds a page request for a chat's member list:
///
///   getChatMembers chat_id:int53 offset:int32 limit:int32 query:string
///       = ChatMembers;
///
/// `offset` is clamped to at least 0 and `limit` to 1..200, the range TDLib
/// member lists accept. The query is trimmed of surrounding whitespace and sent
/// even when empty, which asks for every member rather than a name match.
[[nodiscard]] QJsonObject getChatMembersRequest(qint64 chatId, int offset, int limit,
                                                const QString& query = {});

/// Parses a chatMembers answer in wire order.
///
/// `members` entries that are not objects, that carry no positive user id, or
/// whose member_id is a chat are skipped; a partially malformed payload still
/// yields the valid members in their original order. A missing or malformed
/// `members` array yields an empty vector. Duplicates are preserved so paging
/// code can de-duplicate across pages itself.
[[nodiscard]] QVector<ChatMember> chatMembersFromResponse(const QJsonObject& response);

} // namespace beygram::telegram
