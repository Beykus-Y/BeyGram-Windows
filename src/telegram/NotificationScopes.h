#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One notification scope: the account-wide default or a named custom scope
/// that chats can be assigned to.
///
/// Scope id 0 names the main scope, which always exists. Positive ids name
/// custom scopes. Negative ids name no scope at all: the request builders
/// refuse them and the parser skips them.
///
/// The struct holds no pointers and no session state, so it can be copied
/// across threads and cached per account.
struct NotificationScope {
    /// 0 for the main scope, positive for a custom scope.
    qint64 scopeId = 0;

    /// Display name. Stored without surrounding whitespace.
    QString title;

    /// True while notifications from chats in this scope are muted.
    bool muted = false;

    /// True when message content may be shown in a notification.
    bool showPreview = true;

    /// True when a notification plays a sound.
    bool sound = true;
};

/// Builds the request that reads one scope:
///
///   getNotificationScope scope_id:int64 = NotificationScope;
///
/// Scope id 0 (the main scope) and positive custom ids are accepted. A
/// negative id names no scope, so the builder returns an empty object and the
/// caller never sends a request for it.
[[nodiscard]] QJsonObject getNotificationScopeRequest(qint64 scopeId);

/// Builds the request that replaces one scope:
///
///   setNotificationScope scope_id:int64 title:string muted:Bool
///       show_preview:Bool sound:Bool = Ok;
///
/// Every field is written, including the defaults, so a save never leaves a
/// field of the stored scope unchanged by accident. The title is trimmed of
/// surrounding whitespace before it is sent. A negative scope id returns an
/// empty object; id 0 (the main scope) is valid.
[[nodiscard]] QJsonObject setNotificationScopeRequest(const NotificationScope& scope);

/// Builds the request that adds a chat to a scope or removes it from one:
///
///   setScopeChat scope_id:int64 chat_id:int53 enabled:Bool = Ok;
///
/// `enabled` true assigns the chat to the scope, false takes it out. Scope id 0
/// is the main scope and is accepted. A negative scope id names no scope, and
/// chat id 0 names no chat, so both return an empty object. Negative chat ids
/// are valid supergroup and channel ids and are not rejected.
[[nodiscard]] QJsonObject setScopeChatRequest(qint64 scopeId, qint64 chatId, bool enabled);

/// Parses a notificationScopes answer:
///
///   notificationScopes scopes:vector<notificationScope> = NotificationScopes;
///   notificationScope scope_id:int64 title:string muted:Bool
///       show_preview:Bool sound:Bool = NotificationScope;
///
/// Entries are returned in wire order. An entry is skipped as malformed when it
/// is not an object, when its scope_id is missing, is not an integer (numbers
/// and numeric strings are accepted) or is negative, or when a field it does
/// carry has the wrong JSON type. Missing fields keep the struct defaults, so a
/// scope that carries only its id is still usable. A title is trimmed; interior
/// whitespace is preserved. Duplicate scope ids keep the first valid entry.
///
/// A missing or wrongly typed `scopes` member yields an empty vector rather
/// than an error. The parser performs no I/O and does not log.
[[nodiscard]] QVector<NotificationScope> notificationScopesFromResponse(const QJsonObject& response);

} // namespace beygram::telegram
