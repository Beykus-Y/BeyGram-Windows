#include "GroupAdministration.h"

#include "TelegramTypes.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonValue>
#include <QStringList>

#include <algorithm>
#include <limits>
#include <optional>

namespace beygram::telegram {
namespace {

// banned_until_date is an int32; its maximum is the furthest ban date TDLib can
// represent, so a larger absolute time is clamped instead of overflowing.
constexpr qint64 kMaxInt32 = std::numeric_limits<int>::max();

QString trimmedText(const QJsonValue& value)
{
    return value.toString().trimmed();
}

/// A flag that is absent or not a JSON boolean is denied, so malformed data can
/// never grant a right.
bool boolField(const QJsonObject& object, const QString& key)
{
    const QJsonValue value = object.value(key);
    return value.isBool() && value.toBool();
}

QJsonObject userSender(qint64 userId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageSenderUser")},
        {QStringLiteral("user_id"), userId},
    };
}

/// Writes every member of chatAdministratorRights, because TDLib rejects a
/// partial object. The rights this struct does not model are always denied.
QJsonObject adminRightsObject(const AdminRights& rights)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("chatAdministratorRights")},
        {QStringLiteral("can_manage_chat"), false},
        {QStringLiteral("can_change_info"), rights.canChangeInfo},
        {QStringLiteral("can_post_messages"), false},
        {QStringLiteral("can_edit_messages"), false},
        {QStringLiteral("can_delete_messages"), rights.canDeleteMessages},
        {QStringLiteral("can_invite_users"), rights.canInviteUsers},
        {QStringLiteral("can_restrict_members"), rights.canBanUsers},
        {QStringLiteral("can_pin_messages"), rights.canPinMessages},
        {QStringLiteral("can_manage_topics"), rights.canManageTopics},
        {QStringLiteral("can_promote_members"), rights.canPromoteMembers},
        {QStringLiteral("can_manage_video_chats"), rights.canManageVideoChats},
        {QStringLiteral("can_post_stories"), false},
        {QStringLiteral("can_edit_stories"), false},
        {QStringLiteral("can_delete_stories"), false},
        {QStringLiteral("can_manage_direct_messages"), false},
        {QStringLiteral("can_manage_tags"), false},
        {QStringLiteral("is_anonymous"), rights.anonymous},
    };
}

/// The ChatMemberStatus object for a role this builder can set. An unknown role
/// returns an empty object, which the caller turns into an empty request.
QJsonObject statusObject(const QString& status)
{
    if (status == QLatin1String("administrator")) {
        return QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("chatMemberStatusAdministrator")},
            {QStringLiteral("can_be_edited"), false},
            {QStringLiteral("rights"), adminRightsObject(AdminRights{})},
        };
    }
    if (status == QLatin1String("member")) {
        return QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("chatMemberStatusMember")},
            {QStringLiteral("member_until_date"), 0},
        };
    }
    if (status == QLatin1String("banned")) {
        return QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("chatMemberStatusBanned")},
            {QStringLiteral("banned_until_date"), 0},
        };
    }
    if (status == QLatin1String("left")) {
        return QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("chatMemberStatusLeft")},
        };
    }
    return {};
}

QHash<qint64, QJsonObject> usersById(const QJsonObject& response)
{
    QHash<qint64, QJsonObject> users;

    const QJsonArray array = response.value(QStringLiteral("users")).toArray();
    for (const QJsonValue& value : array) {
        if (!value.isObject())
            continue;
        const QJsonObject user = value.toObject();
        const qint64 id = jsonInt64(user.value(QStringLiteral("id")));
        if (id > 0)
            users.insert(id, user);
    }
    return users;
}

/// TDLib reports active usernames as a list; older payloads carry the singular
/// active_username, and a flattened one a top-level username.
QString usernameFromUser(const QJsonObject& user)
{
    const QJsonObject usernames = user.value(QStringLiteral("usernames")).toObject();

    QString active = trimmedText(usernames.value(QStringLiteral("active_username")));
    if (active.isEmpty()) {
        const QJsonArray activeList =
            usernames.value(QStringLiteral("active_usernames")).toArray();
        for (const QJsonValue& value : activeList) {
            const QString candidate = value.toString().trimmed();
            if (!candidate.isEmpty()) {
                active = candidate;
                break;
            }
        }
    }
    if (active.isEmpty())
        active = trimmedText(user.value(QStringLiteral("username")));

    while (active.startsWith(QLatin1Char('@')))
        active.remove(0, 1);
    return active.trimmed();
}

/// Builds the display name from a user payload. Unlike the member-list parser
/// this leaves the name empty when there is nothing to show, so an administrator
/// row is never given an invented label; the caller can resolve the id itself.
QString displayNameFromUser(const QJsonObject& user)
{
    QStringList parts;
    const QString firstName = trimmedText(user.value(QStringLiteral("first_name")));
    const QString lastName = trimmedText(user.value(QStringLiteral("last_name")));
    if (!firstName.isEmpty())
        parts.append(firstName);
    if (!lastName.isEmpty())
        parts.append(lastName);

    const QString name = parts.join(QLatin1Char(' ')).trimmed();
    if (!name.isEmpty())
        return name;

    const QString displayName = trimmedText(user.value(QStringLiteral("display_name")));
    if (!displayName.isEmpty())
        return displayName;
    const QString plainName = trimmedText(user.value(QStringLiteral("name")));
    if (!plainName.isEmpty())
        return plainName;
    return usernameFromUser(user);
}

/// A chatMember has no name fields of its own; only a caller-merged payload or
/// a response-level users array supplies one.
bool hasNameFields(const QJsonObject& object)
{
    return object.contains(QStringLiteral("first_name"))
        || object.contains(QStringLiteral("last_name"))
        || object.contains(QStringLiteral("display_name"))
        || object.contains(QStringLiteral("name"))
        || object.contains(QStringLiteral("username"))
        || object.contains(QStringLiteral("usernames"));
}

QJsonObject userForMember(const QJsonObject& member, qint64 userId,
                          const QHash<qint64, QJsonObject>& users)
{
    const QJsonObject embedded = member.value(QStringLiteral("user")).toObject();
    if (hasNameFields(embedded))
        return embedded;

    const QJsonObject resolved = users.value(userId);
    if (!resolved.isEmpty())
        return resolved;
    return embedded;
}

std::optional<ChatMemberAdminRow> rowFromChatMember(const QJsonObject& member,
                                                    const QHash<qint64, QJsonObject>& users)
{
    // A chatMember identifies its member with member_id; a flattened or cached
    // payload may carry user_id or id directly. A messageSenderChat member is a
    // chat, not a user, so it resolves to no positive user id and is skipped.
    const QJsonObject sender = member.value(QStringLiteral("member_id")).toObject();
    qint64 userId = jsonInt64(sender.value(QStringLiteral("user_id")));
    if (userId <= 0)
        userId = jsonInt64(member.value(QStringLiteral("user_id")));
    if (userId <= 0)
        userId = jsonInt64(member.value(QStringLiteral("id")));
    if (userId <= 0)
        return std::nullopt;

    const QJsonObject status = member.value(QStringLiteral("status")).toObject();
    const QString type = status.value(QStringLiteral("@type")).toString();
    const bool isCreator = type == QLatin1String("chatMemberStatusCreator");
    const bool isAdministrator = type == QLatin1String("chatMemberStatusAdministrator");
    if (!isCreator && !isAdministrator)
        return std::nullopt;

    ChatMemberAdminRow row;
    row.userId = userId;
    row.isCreator = isCreator;
    row.status = isCreator ? QStringLiteral("creator") : QStringLiteral("administrator");
    if (isAdministrator) {
        row.rights = adminRightsFromJson(status.value(QStringLiteral("rights")).toObject());
    } else {
        // The creator status has no rights object, but it does report whether
        // the owner acts anonymously.
        row.rights.anonymous = boolField(status, QStringLiteral("is_anonymous"));
    }

    row.name = displayNameFromUser(userForMember(member, userId, users));
    return row;
}

std::optional<ChatMemberAdminRow> rowFromChatAdministrator(
    const QJsonObject& administrator, const QHash<qint64, QJsonObject>& users)
{
    const qint64 userId = jsonInt64(administrator.value(QStringLiteral("user_id")));
    if (userId <= 0)
        return std::nullopt;

    ChatMemberAdminRow row;
    row.userId = userId;
    row.isCreator = boolField(administrator, QStringLiteral("is_owner"));
    row.status = row.isCreator ? QStringLiteral("creator") : QStringLiteral("administrator");

    // The real chatAdministrator answer carries no rights; a payload merged with
    // a chatMember status may carry them, and then they are real data.
    const QJsonObject rights = administrator.value(QStringLiteral("rights")).toObject();
    if (!rights.isEmpty())
        row.rights = adminRightsFromJson(rights);

    row.name = displayNameFromUser(userForMember(administrator, userId, users));
    return row;
}

} // namespace

QJsonObject setChatMemberStatusRequest(qint64 chatId, qint64 userId, const QString& status)
{
    if (chatId == 0 || userId <= 0)
        return {};

    const QJsonObject memberStatus = statusObject(status);
    if (memberStatus.isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setChatMemberStatus")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("member_id"), userSender(userId)},
        {QStringLiteral("status"), memberStatus},
    };
}

QJsonObject getChatAdministratorsRequest(qint64 chatId)
{
    if (chatId == 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getChatAdministrators")},
        {QStringLiteral("chat_id"), chatId},
    };
}

QJsonObject getChatMemberRequest(qint64 chatId, qint64 userId)
{
    if (chatId == 0 || userId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getChatMember")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("member_id"), userSender(userId)},
    };
}

QJsonObject setChatMemberTagRequest(qint64 chatId, qint64 userId, const QString& tag)
{
    if (chatId == 0 || userId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setChatMemberTag")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("user_id"), userId},
        {QStringLiteral("tag"), tag.trimmed()},
    };
}

QJsonObject banChatMemberRequest(qint64 chatId, qint64 userId, qint64 untilDate,
                                 bool revokeMessages)
{
    if (chatId == 0 || userId <= 0)
        return {};

    // 0 and every negative value mean "forever"; the schema's int32 cannot
    // carry a larger absolute time than its maximum.
    const qint64 bannedUntilDate =
        untilDate > 0 ? std::min(untilDate, kMaxInt32) : qint64(0);

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("banChatMember")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("member_id"), userSender(userId)},
        {QStringLiteral("banned_until_date"), static_cast<int>(bannedUntilDate)},
        {QStringLiteral("revoke_messages"), revokeMessages},
    };
}

QVector<ChatMemberAdminRow> administratorsFromResponse(const QJsonObject& response)
{
    QVector<ChatMemberAdminRow> rows;
    if (response.isEmpty())
        return rows;

    const QHash<qint64, QJsonObject> users = usersById(response);

    const QJsonValue membersValue = response.value(QStringLiteral("members"));
    if (membersValue.isArray()) {
        const QJsonArray members = membersValue.toArray();
        rows.reserve(members.size());
        for (const QJsonValue& value : members) {
            if (!value.isObject())
                continue;
            if (const std::optional<ChatMemberAdminRow> row =
                    rowFromChatMember(value.toObject(), users)) {
                rows.append(*row);
            }
        }
        return rows;
    }

    const QJsonValue administratorsValue = response.value(QStringLiteral("administrators"));
    if (administratorsValue.isArray()) {
        const QJsonArray administrators = administratorsValue.toArray();
        rows.reserve(administrators.size());
        for (const QJsonValue& value : administrators) {
            if (!value.isObject())
                continue;
            if (const std::optional<ChatMemberAdminRow> row =
                    rowFromChatAdministrator(value.toObject(), users)) {
                rows.append(*row);
            }
        }
        return rows;
    }

    // A getChatMember answer is one chatMember object rather than an array.
    const bool looksLikeMember =
        response.value(QStringLiteral("status")).isObject()
        || response.value(QStringLiteral("@type")).toString() == QLatin1String("chatMember");
    if (looksLikeMember) {
        if (const std::optional<ChatMemberAdminRow> row = rowFromChatMember(response, users))
            rows.append(*row);
        return rows;
    }

    // A bare chatAdministrator object is accepted too, but only when it really
    // looks like one: a status-less chatMember must stay skipped as malformed.
    const bool looksLikeAdministrator =
        response.value(QStringLiteral("@type")).toString() == QLatin1String("chatAdministrator")
        || response.contains(QStringLiteral("is_owner"))
        || response.contains(QStringLiteral("custom_title"));
    if (looksLikeAdministrator) {
        if (const std::optional<ChatMemberAdminRow> row =
                rowFromChatAdministrator(response, users)) {
            rows.append(*row);
        }
    }
    return rows;
}

AdminRights adminRightsFromJson(const QJsonObject& rights)
{
    AdminRights parsed;
    parsed.canChangeInfo = boolField(rights, QStringLiteral("can_change_info"));
    parsed.canDeleteMessages = boolField(rights, QStringLiteral("can_delete_messages"));
    parsed.canInviteUsers = boolField(rights, QStringLiteral("can_invite_users"));
    parsed.canPinMessages = boolField(rights, QStringLiteral("can_pin_messages"));
    parsed.canManageTopics = boolField(rights, QStringLiteral("can_manage_topics"));
    parsed.canPromoteMembers = boolField(rights, QStringLiteral("can_promote_members"));
    parsed.canManageVideoChats = boolField(rights, QStringLiteral("can_manage_video_chats"));
    parsed.anonymous = boolField(rights, QStringLiteral("is_anonymous"));

    // Banning is a restriction in TDLib. can_restrict_members wins when it is
    // present, even when denied; can_ban_users is only a fallback spelling.
    const QJsonValue restrict = rights.value(QStringLiteral("can_restrict_members"));
    parsed.canBanUsers = restrict.isBool() ? restrict.toBool()
                                           : boolField(rights, QStringLiteral("can_ban_users"));
    return parsed;
}

} // namespace beygram::telegram
