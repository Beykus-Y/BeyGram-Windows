#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

// Request builders for editing the current user's own profile. They only shape
// TDLib's JSON payload: no logging, no @extra (the transport adds the
// correlation id) and no network or session state.
//
// Shapes follow the pinned TDLib schema documented in docs/WINDOWS_TDLIB.md
// (1.8.69):
//   setName first_name:string last_name:string = Ok;   // 1-64 / 0-64 chars
//   setBio bio:string = Ok;                            // no line feeds
//   setUsername username:string = Ok;                  // empty removes it
//   deleteProfilePhoto profile_photo_id:int64 = Ok;
//
// Uploading a new photo is a separate TDLib method, setProfilePhoto, which takes
// an inputChatPhoto plus an is_public flag; it is intentionally not modelled
// here because it needs a local file handle owned by the media layer.

namespace beygram::telegram {

/// Builds `setName` for the current user. Both parts are trimmed and an empty
/// last name is valid ("no last name"). TDLib requires 1-64 characters in the
/// first name, so an empty first name is passed through as-is and left for
/// TDLib to reject with FIRSTNAME_INVALID rather than silently invented here.
[[nodiscard]] QJsonObject setNameRequest(const QString& firstName, const QString& lastName);

/// Builds `setBio` with the bio as plain text. The value is never escaped,
/// stripped of markup or turned into a formattedText/entities object, so what
/// the user typed is what Telegram stores. Whitespace runs are folded to a
/// single space and the ends are trimmed because TDLib rejects bios that
/// contain line feeds; an empty result clears the bio.
[[nodiscard]] QJsonObject setBioRequest(const QString& bio);

/// Builds `setUsername` after normalisation: whitespace is removed, a leading
/// '@' is stripped and the result is not validated further (length and allowed
/// characters stay TDLib's job). An empty result clears the editable username,
/// which is exactly what TDLib expects for removal.
[[nodiscard]] QJsonObject setUsernameRequest(const QString& username);

/// Builds `deleteProfilePhoto` for one of the current user's profile photos.
/// The id is passed through unchanged; TDLib rejects ids it does not know.
[[nodiscard]] QJsonObject deleteProfilePhotoRequest(qint64 photoId);

} // namespace beygram::telegram
