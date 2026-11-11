#include "ProfileEdit.h"

namespace beygram::telegram {
namespace {

/// Collapses every whitespace run (including the line feeds TDLib refuses in a
/// bio) into one space and drops leading/trailing whitespace.
QString foldToSingleLine(const QString& text)
{
    QString folded;
    folded.reserve(text.size());
    bool pendingSpace = false;
    for (const QChar character : text) {
        if (character.isSpace()) {
            pendingSpace = true;
            continue;
        }
        if (pendingSpace && !folded.isEmpty())
            folded.append(QLatin1Char(' '));
        pendingSpace = false;
        folded.append(character);
    }
    return folded;
}

/// Turns "@My Name" into "myname"-shaped input: no whitespace, no leading '@'.
/// Case and the remaining characters are left untouched for TDLib to judge.
QString normalizeUsername(const QString& username)
{
    QString value;
    value.reserve(username.size());
    for (const QChar character : username) {
        if (!character.isSpace())
            value.append(character);
    }
    while (value.startsWith(QLatin1Char('@')))
        value.remove(0, 1);
    return value;
}

} // namespace

QJsonObject setNameRequest(const QString& firstName, const QString& lastName)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setName")},
        {QStringLiteral("first_name"), firstName.trimmed()},
        {QStringLiteral("last_name"), lastName.trimmed()},
    };
}

QJsonObject setBioRequest(const QString& bio)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setBio")},
        {QStringLiteral("bio"), foldToSingleLine(bio)},
    };
}

QJsonObject setUsernameRequest(const QString& username)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setUsername")},
        {QStringLiteral("username"), normalizeUsername(username)},
    };
}

QJsonObject deleteProfilePhotoRequest(qint64 photoId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("deleteProfilePhoto")},
        {QStringLiteral("profile_photo_id"), photoId},
    };
}

} // namespace beygram::telegram
