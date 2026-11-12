#include "SessionRequests.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>

namespace beygram::telegram {
namespace {

/// device_model is what the application reported and is normally the most
/// specific label; platform ("Windows", "Android", ...) is the schema's
/// fallback when the client did not report a model.
QString deviceLabel(const QJsonObject& session)
{
    const QString model = session.value(QStringLiteral("device_model")).toString();
    if (!model.isEmpty())
        return model;
    return session.value(QStringLiteral("platform")).toString();
}

SessionInfo sessionFromObject(const QJsonObject& session)
{
    SessionInfo info;
    info.id = jsonInt64(session.value(QStringLiteral("id")));
    info.device = deviceLabel(session);
    info.location = session.value(QStringLiteral("location")).toString();
    info.ip = session.value(QStringLiteral("ip_address")).toString();
    info.lastActive = jsonInt64(session.value(QStringLiteral("last_active_date")));
    info.current = session.value(QStringLiteral("is_current")).toBool(false);
    return info;
}

} // namespace

QJsonObject getActiveSessionsRequest()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getActiveSessions")},
    };
}

QJsonObject terminateSessionRequest(qint64 sessionId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("terminateSession")},
        {QStringLiteral("session_id"), sessionId},
    };
}

QJsonObject terminateAllOtherSessionsRequest()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("terminateAllOtherSessions")},
    };
}

QVector<SessionInfo> sessionsFromResponse(const QJsonObject& response)
{
    QVector<SessionInfo> sessions;
    if (response.value(QStringLiteral("@type")).toString() != QLatin1String("sessions"))
        return sessions;

    const QJsonArray items = response.value(QStringLiteral("sessions")).toArray();
    sessions.reserve(items.size());
    for (const QJsonValue& value : items) {
        if (!value.isObject())
            continue;
        SessionInfo session = sessionFromObject(value.toObject());
        // A session id is a positive int64; an entry without one cannot be
        // shown or terminated, so a malformed object is skipped rather than
        // surfacing an unusable row.
        if (session.id <= 0)
            continue;
        sessions.append(session);
    }
    return sessions;
}

} // namespace beygram::telegram
