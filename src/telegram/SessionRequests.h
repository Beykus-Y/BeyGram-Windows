#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One entry of TDLib's active session list. It deliberately carries only what
/// the device-management UI shows; password and confirmation state stay in
/// TDLib, and no secret ever enters this struct.
struct SessionInfo {
    qint64 id = 0;
    QString device;
    QString location;
    QString ip;
    qint64 lastActive = 0;
    bool current = false;
};

/// Pure builders and a parser for device-session management. They contain no
/// logging; the transport adds the correlation id.
///
/// Shapes follow the TDLib schema pinned by this client (1.8.69, commit
/// 4c6377aa, see docs/WINDOWS_TDLIB.md):
///
///   session id:int64 is_current:Bool is_password_pending:Bool
///           is_unconfirmed:Bool can_accept_secret_chats:Bool
///           can_accept_calls:Bool device_type:SessionDeviceType api_id:int32
///           application_name:string application_version:string
///           is_official_application:Bool device_model:string platform:string
///           system_version:string log_in_date:int32 last_active_date:int32
///           ip_address:string location:string = Session;
///   sessions sessions:vector<session> inactive_session_ttl_days:int32 = Sessions;
///   getActiveSessions = Sessions;
///   terminateSession session_id:int64 = Ok;
///   terminateAllOtherSessions = Ok;
///
/// The current session is part of the list and is marked with is_current; it is
/// never terminated by the other-device actions, so callers must exclude it.

/// Builds getActiveSessions, the first request of the device-session screen.
[[nodiscard]] QJsonObject getActiveSessionsRequest();

/// Builds terminateSession for one session id taken from the list returned by
/// getActiveSessions. The builder does not re-check the id against that list;
/// the caller must refuse the current or an unknown session before sending.
[[nodiscard]] QJsonObject terminateSessionRequest(qint64 sessionId);

/// Builds terminateAllOtherSessions, which signs out every session except the
/// current one. It is destructive and belongs behind a confirmation.
[[nodiscard]] QJsonObject terminateAllOtherSessionsRequest();

/// Reads a Sessions answer into the UI model, preserving Telegram's order.
/// Returns an empty vector when the response is not a sessions object. Entries
/// without a usable positive id are skipped. device_model falls back to
/// platform, matching the session list shown by the official clients.
[[nodiscard]] QVector<SessionInfo> sessionsFromResponse(const QJsonObject& response);

} // namespace beygram::telegram
