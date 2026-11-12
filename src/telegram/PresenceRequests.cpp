#include "PresenceRequests.h"

#include "TelegramTypes.h"

#include <algorithm>

namespace beygram::telegram {

QJsonObject setOnlineRequest(bool online)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setOption")},
        {QStringLiteral("name"), QStringLiteral("online")},
        {QStringLiteral("value"),
         QJsonObject{
             {QStringLiteral("@type"), QStringLiteral("optionValueBoolean")},
             {QStringLiteral("value"), online},
         }},
    };
}

PresenceInfo presenceFromUser(const QJsonObject& user)
{
    PresenceInfo info;

    // A missing, null or wrongly typed status is an empty object, which leaves
    // info.status empty and keeps the defaults. Unknown @type values are kept.
    const QJsonObject status = user.value(QStringLiteral("status")).toObject();
    info.status = status.value(QStringLiteral("@type")).toString();

    if (info.status == QLatin1String("userStatusOnline")) {
        // expires only bounds how long the online flag stays valid; it is not
        // a last-seen timestamp, so lastSeen intentionally stays 0.
        info.online = true;
    } else if (info.status == QLatin1String("userStatusOffline")) {
        info.lastSeen = std::max<qint64>(
            jsonInt64(status.value(QStringLiteral("was_online"))), 0);
    }

    return info;
}

} // namespace beygram::telegram
