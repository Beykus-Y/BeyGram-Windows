#include "ContactSearch.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QStringList>

#include <algorithm>
#include <limits>

namespace beygram::telegram {
namespace {

constexpr int kMinLimit = 1;
constexpr int kMaxLimit = 100;

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

QString displayNameFromUser(const QJsonObject& user, const QString& username)
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
    // Telegram requires a first name, but fixtures, deleted accounts and bots
    // can still arrive nameless; the username reads better than a placeholder.
    if (!username.isEmpty())
        return username;
    return QStringLiteral("Telegram user");
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

/// `profilePhoto` carries a `small` and a `big` file; the small one is what a
/// contact row shows.
int profilePhotoFileId(const QJsonObject& user)
{
    const QJsonObject photo = user.value(QStringLiteral("profile_photo")).toObject();
    if (photo.isEmpty())
        return 0;
    const int small = fileIdFromJson(photo.value(QStringLiteral("small")));
    if (small > 0)
        return small;
    return fileIdFromJson(photo.value(QStringLiteral("big")));
}

bool isBotUser(const QJsonObject& user)
{
    const QJsonObject type = user.value(QStringLiteral("type")).toObject();
    if (type.value(QStringLiteral("@type")).toString() == QLatin1String("userTypeBot"))
        return true;
    return user.value(QStringLiteral("is_bot")).toBool();
}

} // namespace

QJsonObject searchContactsRequest(const QString& query, int limit)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("searchContacts")},
        {QStringLiteral("query"), query.trimmed()},
        {QStringLiteral("limit"), std::clamp(limit, kMinLimit, kMaxLimit)},
    };
}

QVector<ContactInfo> contactsFromResponse(const QJsonObject& response)
{
    QVector<ContactInfo> contacts;

    const QJsonValue usersValue = response.value(QStringLiteral("users"));
    if (!usersValue.isArray())
        return contacts;

    const QJsonArray users = usersValue.toArray();
    contacts.reserve(users.size());

    for (const QJsonValue& value : users) {
        if (!value.isObject())
            continue;
        const QJsonObject user = value.toObject();

        ContactInfo contact;
        contact.userId = jsonInt64(user.value(QStringLiteral("id")));
        if (contact.userId <= 0)
            continue;

        contact.username = usernameFromUser(user);
        contact.displayName = displayNameFromUser(user, contact.username);
        contact.photoFileId = profilePhotoFileId(user);
        contact.isBot = isBotUser(user);
        contact.isPremium = user.value(QStringLiteral("is_premium")).toBool();

        contacts.append(contact);
    }

    return contacts;
}

} // namespace beygram::telegram
