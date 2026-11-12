#pragma once

#include "UserProfile.h"

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Presence state of a user and the account's own online flag.
///
/// Shapes follow the TDLib schema pinned by this client (1.8.69, commit
/// 4c6377aa, see docs/WINDOWS_TDLIB.md):
///
///   setOption name:string value:OptionValue = Ok;
///   optionValueBoolean value:Bool = OptionValue;
///   user id:int53 ... status:UserStatus ... = User;
///   userStatusEmpty = UserStatus;
///   userStatusOnline expires:int32 = UserStatus;
///   userStatusOffline was_online:int32 = UserStatus;
///   userStatusRecently by_my_privacy_settings:Bool = UserStatus;
///   userStatusLastWeek by_my_privacy_settings:Bool = UserStatus;
///   userStatusLastMonth by_my_privacy_settings:Bool = UserStatus;
///
/// These builders and the parser are pure: no logging, no secrets and no
/// session state. The transport adds correlation.

/// Builds setOption for the "online" option, which is how the client declares
/// itself online or offline to Telegram. true writes optionValueBoolean true;
/// false is what honours a hidden-presence ("hide online") setting.
[[nodiscard]] QJsonObject setOnlineRequest(bool online);

// The two profile lookups below are declared in UserProfile.h and defined once
// in UserProfile.cpp. They are part of the presence API because refreshing a
// user's status starts with getUser/getUserFullInfo, but defining a second copy
// here would break the one-definition rule. They are redeclared so callers can
// include this header alone.
[[nodiscard]] QJsonObject getUserRequest(qint64 userId);
[[nodiscard]] QJsonObject getUserFullInfoRequest(qint64 userId);

struct PresenceInfo {
    /// The UserStatus @type exactly as TDLib reported it, for example
    /// "userStatusOnline", "userStatusOffline", "userStatusRecently",
    /// "userStatusLastWeek", "userStatusLastMonth" or "userStatusEmpty".
    /// Empty when the user carries no status object (or a malformed one), so
    /// a missing payload stays distinguishable from userStatusEmpty. Unknown
    /// future variants are preserved verbatim instead of being discarded.
    QString status;

    /// Unix seconds taken from userStatusOffline.was_online. It is 0 for every
    /// other variant: userStatusOnline.expires is when online state lapses,
    /// not when the user was last seen. Negative values are clamped to 0.
    qint64 lastSeen = 0;

    /// True only for userStatusOnline. expires is not compared against a clock
    /// here: TDLib reports Online only while the server considers the user
    /// online, and this parser has no clock of its own.
    bool online = false;
};

/// Parses a TDLib user object. Only the "status" member is read; every other
/// field, including expires and by_my_privacy_settings, is ignored.
[[nodiscard]] PresenceInfo presenceFromUser(const QJsonObject& user);

} // namespace beygram::telegram
