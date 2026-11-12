#include "ChatMembers.h"

#include "TelegramTypes.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonValue>
#include <QStringList>

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int kMinMembersLimit = 1;
constexpr int kMaxMembersLimit = 200;

QString trimmedText(const QJsonValue& value)
{
    return value.toString().trimmed();
}

QString normalizeUsername(QString username)
{
    username = username.trimmed();
    while (username.startsWith(QLatin1Char('@')))
        username.remove(0, 1);
    return username.trimmed();
}

/// TDLib reports active usernames as a list (`usernames.active_usernames`);
/// older payloads carry the singular `usernames.active_username`, and even
/// older ones a top-level `username`.
QString usernameFromUser(const QJsonObject& user)
{
    const QJsonObject usernames = user.value(QStringLiteral("usernames")).toObject();

    QString active = trimmedText(usernames.value(QStringLiteral("active_username")));
    if (active.isEmpty()) {
        const QJsonArray activeList =
            usernames.value(QStringLiteral("active_usernames")).toArray();
        for (const QJsonValue& value : activeList) {
            const QString candidate = value.toString();
            if (!candidate.trimmed().isEmpty()) {
                active = candidate;
                break;
            }
        }
    }
    if (active.isEmpty())
        active = trimmedText(user.value(QStringLiteral("username")));
    return normalizeUsername(active);
}

/// Builds a display name from an object that carries user name fields. The
/// caller only passes an object that actually has a user payload, so the final
/// placeholder is never reached for an unrelated chatMember.
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

    // A merged payload may already carry a resolved name; the username is a
    // better fallback than a placeholder because it is still real data.
    const QString displayName = trimmedText(user.value(QStringLiteral("display_name")));
    if (!displayName.isEmpty())
        return displayName;
    const QString plainName = trimmedText(user.value(QStringLiteral("name")));
    if (!plainName.isEmpty())
        return plainName;
    const QString username = usernameFromUser(user);
    if (!username.isEmpty())
        return username;

    return QStringLiteral("Telegram user");
}

bool isBotUser(const QJsonObject& user)
{
    if (user.value(QStringLiteral("is_bot")).toBool(false))
        return true;
    return user.value(QStringLiteral("type"))
               .toObject()
               .value(QStringLiteral("@type"))
               .toString()
           == QLatin1String("userTypeBot");
}

/// Maps a ChatMemberStatus @type to the short role name this struct exposes.
/// Unknown variants keep their @type verbatim, so a newer TDLib does not turn
/// into "no status".
QString statusName(const QJsonValue& value)
{
    const QJsonObject status = value.toObject();
    if (status.isEmpty())
        return {};

    const QString type = status.value(QStringLiteral("@type")).toString();
    if (type == QLatin1String("chatMemberStatusCreator"))
        return QStringLiteral("creator");
    if (type == QLatin1String("chatMemberStatusAdministrator"))
        return QStringLiteral("administrator");
    if (type == QLatin1String("chatMemberStatusMember"))
        return QStringLiteral("member");
    if (type == QLatin1String("chatMemberStatusRestricted"))
        return QStringLiteral("restricted");
    if (type == QLatin1String("chatMemberStatusLeft"))
        return QStringLiteral("left");
    if (type == QLatin1String("chatMemberStatusBanned"))
        return QStringLiteral("banned");
    return type;
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

/// A chatMember has no name fields of its own; only a caller-merged payload or
/// a response-level users array supplies one. Direct name fields are accepted
/// as well, because some cached payloads flatten the user into the member.
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
    // A response-level users array can be richer than an id-only embedded user,
    // so a bare embedded object is only used when the array has nothing.
    if (hasNameFields(embedded) || isBotUser(embedded))
        return embedded;

    const QJsonObject resolved = users.value(userId);
    if (!resolved.isEmpty())
        return resolved;
    return embedded;
}

} // namespace

QJsonObject getChatMembersRequest(qint64 chatId, int offset, int limit,
                                  const QString& query)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getChatMembers")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("offset"), std::max(offset, 0)},
        {QStringLiteral("limit"), std::clamp(limit, kMinMembersLimit, kMaxMembersLimit)},
        {QStringLiteral("query"), query.trimmed()},
    };
}

QVector<ChatMember> chatMembersFromResponse(const QJsonObject& response)
{
    QVector<ChatMember> members;

    const QJsonValue membersValue = response.value(QStringLiteral("members"));
    if (!membersValue.isArray())
        return members;

    const QJsonArray array = membersValue.toArray();
    members.reserve(array.size());
    const QHash<qint64, QJsonObject> users = usersById(response);

    for (const QJsonValue& value : array) {
        if (!value.isObject())
            continue;
        const QJsonObject rawMember = value.toObject();

        // A chatMember identifies its member with member_id; a flattened or
        // cached payload may carry user_id or id directly.
        const QJsonObject sender = rawMember.value(QStringLiteral("member_id")).toObject();
        qint64 userId = jsonInt64(sender.value(QStringLiteral("user_id")));
        if (userId <= 0)
            userId = jsonInt64(rawMember.value(QStringLiteral("user_id")));
        if (userId <= 0)
            userId = jsonInt64(rawMember.value(QStringLiteral("id")));
        if (userId <= 0)
            continue;

        ChatMember member;
        member.userId = userId;

        const QJsonValue statusValue = rawMember.value(QStringLiteral("status"));
        const QJsonObject status = statusValue.toObject();
        const QString statusType = status.value(QStringLiteral("@type")).toString();
        member.status = statusName(statusValue);
        member.isOwner = statusType == QLatin1String("chatMemberStatusCreator");
        member.isAdmin = statusType == QLatin1String("chatMemberStatusAdministrator");

        const QJsonObject user = userForMember(rawMember, userId, users);
        if (!user.isEmpty()) {
            member.displayName = displayNameFromUser(user);
            member.isBot = isBotUser(user);
        } else if (hasNameFields(rawMember)) {
            member.displayName = displayNameFromUser(rawMember);
        }

        // The bot flag can also be flattened onto the member itself.
        if (!member.isBot) {
            member.isBot =
                rawMember.value(QStringLiteral("is_bot")).toBool(false)
                || rawMember.value(QStringLiteral("type"))
                           .toObject()
                           .value(QStringLiteral("@type"))
                           .toString()
                       == QLatin1String("userTypeBot");
        }

        members.append(member);
    }

    return members;
}

} // namespace beygram::telegram
