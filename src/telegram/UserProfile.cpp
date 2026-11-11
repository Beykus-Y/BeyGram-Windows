#include "UserProfile.h"

#include "TelegramTypes.h"
#include "TextFormatting.h"

#include <QJsonArray>
#include <QStringList>

#include <limits>

namespace beygram::telegram {
namespace {

QString trimmedText(const QJsonValue& value)
{
    return value.toString().trimmed();
}

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
    return QStringLiteral("Telegram user");
}

QString normalizeUsername(QString username)
{
    username = username.trimmed();
    while (username.startsWith(QLatin1Char('@')))
        username.remove(0, 1);
    return username.trimmed();
}

QString usernameFromUser(const QJsonObject& user)
{
    const QJsonObject usernames = user.value(QStringLiteral("usernames")).toObject();

    // TDLib reports the active username as a list (`active_usernames`); older
    // payloads and fixtures may carry the singular `active_username`.
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

int fileIdFromJson(const QJsonValue& value)
{
    const QJsonObject file = value.toObject();
    if (file.isEmpty())
        return 0;
    const qint64 id = jsonInt64(file.value(QStringLiteral("id")));
    if (id <= 0 || id > std::numeric_limits<int>::max())
        return 0;
    return static_cast<int>(id);
}

/// TDLib file ids are int32 and are assigned before a download starts, so the
/// id alone tells us a photo exists. `local.is_downloading_completed` is the
/// MediaFileStore's concern; it is deliberately not cached here.
int profilePhotoFileId(const QJsonObject& photo)
{
    if (photo.isEmpty())
        return 0;
    const int small = fileIdFromJson(photo.value(QStringLiteral("small")));
    if (small > 0)
        return small;
    return fileIdFromJson(photo.value(QStringLiteral("big")));
}

QString bioFromUserFullInfo(const QJsonObject& fullInfo)
{
    const QJsonObject bio = fullInfo.value(QStringLiteral("bio")).toObject();
    if (bio.isEmpty())
        return {};
    return formatTdlibText(bio).plainText.trimmed();
}

QString descriptionFromChatFullInfo(const QJsonObject& fullInfo)
{
    return trimmedText(fullInfo.value(QStringLiteral("description")));
}

int memberCountFromFullInfo(const QJsonObject& fullInfo)
{
    const int count = fullInfo.value(QStringLiteral("member_count")).toInt();
    if (count > 0)
        return count;
    // `basicGroupFullInfo` has no member_count; its member list is the only
    // source available for basic groups.
    const QJsonArray members = fullInfo.value(QStringLiteral("members")).toArray();
    return static_cast<int>(members.size());
}

} // namespace

QJsonObject getUserRequest(qint64 userId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getUser")},
        {QStringLiteral("user_id"), userId},
    };
}

QJsonObject getUserFullInfoRequest(qint64 userId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getUserFullInfo")},
        {QStringLiteral("user_id"), userId},
    };
}

QJsonObject getChatRequest(qint64 chatId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getChat")},
        {QStringLiteral("chat_id"), chatId},
    };
}

QJsonObject getSupergroupFullInfoRequest(qint64 supergroupId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getSupergroupFullInfo")},
        {QStringLiteral("supergroup_id"), supergroupId},
    };
}

UserProfile profileFromUser(const QJsonObject& user, const QJsonObject& fullInfo)
{
    UserProfile profile;
    profile.userId = jsonInt64(user.value(QStringLiteral("id")));
    // A private chat with a user shares the user's id, so a profile card can
    // open the conversation without an extra lookup.
    profile.chatId = profile.userId;
    profile.displayName = displayNameFromUser(user);
    profile.username = usernameFromUser(user);
    profile.bio = bioFromUserFullInfo(fullInfo);
    profile.phoneNumber = trimmedText(user.value(QStringLiteral("phone_number")));
    profile.photoFileId =
        profilePhotoFileId(user.value(QStringLiteral("profile_photo")).toObject());
    profile.hasPhoto = profile.photoFileId > 0;
    profile.isPremium = user.value(QStringLiteral("is_premium")).toBool();
    profile.isBot =
        user.value(QStringLiteral("type")).toObject().value(QStringLiteral("@type")).toString()
            == QLatin1String("userTypeBot")
        || user.value(QStringLiteral("is_bot")).toBool();
    profile.isContact = user.value(QStringLiteral("is_contact")).toBool();
    return profile;
}

UserProfile profileFromChat(const QJsonObject& chat, const QJsonObject& fullInfo)
{
    UserProfile profile;
    profile.chatId = jsonInt64(chat.value(QStringLiteral("id")));
    profile.displayName = trimmedText(chat.value(QStringLiteral("title")));
    profile.photoFileId =
        profilePhotoFileId(chat.value(QStringLiteral("photo")).toObject());
    profile.hasPhoto = profile.photoFileId > 0;

    const QJsonObject type = chat.value(QStringLiteral("type")).toObject();
    const QString typeName = type.value(QStringLiteral("@type")).toString();
    if (typeName == QLatin1String("chatTypePrivate")
        || typeName == QLatin1String("chatTypeSecret")) {
        profile.userId = jsonInt64(type.value(QStringLiteral("user_id")));
    } else if (typeName == QLatin1String("chatTypeBasicGroup")) {
        profile.isGroup = true;
    } else if (typeName == QLatin1String("chatTypeSupergroup")) {
        const bool channel = type.value(QStringLiteral("is_channel")).toBool();
        profile.isChannel = channel;
        profile.isGroup = !channel;
    }

    if (profile.isGroup || profile.isChannel) {
        profile.bio = descriptionFromChatFullInfo(fullInfo);
        profile.memberCount = memberCountFromFullInfo(fullInfo);
    }
    return profile;
}

} // namespace beygram::telegram
