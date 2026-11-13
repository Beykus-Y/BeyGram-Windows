#include "OwnProfile.h"

#include "TelegramTypes.h"
#include "TextFormatting.h"

#include <QJsonArray>
#include <QJsonValue>

#include <limits>

namespace beygram::telegram {
namespace {

QString trimmedText(const QJsonValue& value)
{
    return value.toString().trimmed();
}

/// Own usernames are shown without the '@' sigil.
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

    // editable_username is the name the owner can change right now;
    // active_usernames is the list TDLib reports alongside it, with the first
    // entry as the primary one.
    QString username =
        normalizeUsername(trimmedText(usernames.value(QStringLiteral("editable_username"))));
    if (username.isEmpty()) {
        const QJsonArray active = usernames.value(QStringLiteral("active_usernames")).toArray();
        for (const QJsonValue& value : active) {
            username = normalizeUsername(value.toString());
            if (!username.isEmpty())
                break;
        }
    }
    // Older payloads and fixtures may only carry the singular top-level field.
    if (username.isEmpty())
        username = normalizeUsername(trimmedText(user.value(QStringLiteral("username"))));
    return username;
}

QString bioFromFullInfo(const QJsonObject& fullInfo)
{
    const QJsonValue bio = fullInfo.value(QStringLiteral("bio"));
    if (bio.isString())
        return bio.toString().trimmed();
    if (!bio.isObject())
        return {};
    // TDLib sends a formattedText; formatTdlibText reads its `text` member and
    // leaves the entities to the HTML renderer.
    return formatTdlibText(bio.toObject()).plainText.trimmed();
}

int fileIdFromJson(const QJsonValue& value)
{
    const qint64 id = jsonInt64(value);
    if (id <= 0 || id > std::numeric_limits<int>::max())
        return 0;
    return static_cast<int>(id);
}

int smallPhotoFileId(const QJsonObject& holder)
{
    const QJsonObject photo = holder.value(QStringLiteral("profile_photo")).toObject();
    if (photo.isEmpty())
        return 0;
    const QJsonObject small = photo.value(QStringLiteral("small")).toObject();
    return fileIdFromJson(small.value(QStringLiteral("id")));
}

int photoFileId(const QJsonObject& user, const QJsonObject& fullInfo)
{
    // getMe carries the authoritative profile_photo; the full-info payload is
    // only a fallback for callers that attach the photo there.
    const int fromUser = smallPhotoFileId(user);
    if (fromUser > 0)
        return fromUser;
    return smallPhotoFileId(fullInfo);
}

} // namespace

QJsonObject getOwnUserRequest()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getMe")},
    };
}

QJsonObject getOwnFullInfoRequest()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getUserFullInfo")},
        {QStringLiteral("user_id"), qint64(0)},
    };
}

OwnProfileState ownProfileFromUser(const QJsonObject& user, const QJsonObject& fullInfo)
{
    OwnProfileState state;
    state.userId = jsonInt64(user.value(QStringLiteral("id")));
    state.firstName = trimmedText(user.value(QStringLiteral("first_name")));
    state.lastName = trimmedText(user.value(QStringLiteral("last_name")));
    state.username = usernameFromUser(user);
    state.phoneNumber = trimmedText(user.value(QStringLiteral("phone_number")));
    state.bio = bioFromFullInfo(fullInfo);
    state.photoFileId = photoFileId(user, fullInfo);
    state.isPremium = user.value(QStringLiteral("is_premium")).toBool();
    state.isVerified = user.value(QStringLiteral("is_verified")).toBool();
    return state;
}

QString ownProfileDisplayName(const OwnProfileState& state)
{
    const QString firstName = state.firstName.trimmed();
    const QString lastName = state.lastName.trimmed();

    QString name;
    if (firstName.isEmpty()) {
        name = lastName;
    } else if (lastName.isEmpty()) {
        name = firstName;
    } else {
        name = firstName + QLatin1Char(' ') + lastName;
    }
    if (!name.isEmpty())
        return name;

    const QString username = state.username.trimmed();
    if (!username.isEmpty())
        return username;

    const QString phoneNumber = state.phoneNumber.trimmed();
    if (!phoneNumber.isEmpty())
        return phoneNumber;

    return QStringLiteral("You");
}

} // namespace beygram::telegram
