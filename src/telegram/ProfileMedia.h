#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

// Profile music and profile-photo helpers.
//
// The request builders only shape TDLib's JSON payload: no logging, no @extra
// (the transport adds the correlation id) and no network or session state.
// Shapes follow the pinned TDLib schema documented in docs/WINDOWS_TDLIB.md
// (1.8.69):
//   userFullInfo.first_profile_audio:audio
//   addProfileAudio audio:inputAudio = Ok;
//   removeProfileAudio file_id:int32 = Ok;
//   getUserProfilePhotos user_id:int53 offset:int32 limit:int32 = ChatPhotos;
//   setProfilePhoto photo:InputChatPhoto is_public:Bool = Ok;
//   deleteProfilePhoto profile_photo_id:int64 = Ok;
//
// TDLib has no `setProfileAudio` method; the app-level "set" operation is
// `addProfileAudio`, which puts the track at the front of the profile list.
// Builders that take an id return an empty object for a non-positive value, so
// callers can test the result instead of sending a broken payload.

namespace beygram::telegram {

/// The first track shown in a profile, as already parsed from TDLib's `audio`.
///
/// fileId is TDLib's file id from `audio.audio.id`; title and performer are the
/// tags exactly as TDLib reported them (either may be empty). A profile without
/// music, an audio without a usable file id, or a payload without an audio
/// object yields the default value with fileId == 0.
struct ProfileAudioInfo {
    qint64 fileId = 0;
    QString title;
    QString performer;
    int durationSeconds = 0;

    bool operator==(const ProfileAudioInfo&) const = default;
};

/// Reads `userFullInfo.first_profile_audio` from `user`.
///
/// The parameter is named after the profile object the caller has in hand: it
/// may be the full-info object itself, a user object with the full-info fields
/// merged in, or a bare `audio`. The alias `profile_audio` is accepted as well,
/// because some call sites cache the field under that name. This is profile
/// music, not the formatted `bio` text; a bio that happens to contain a link is
/// never interpreted as an audio object.
[[nodiscard]] ProfileAudioInfo profileAudioFromUser(const QJsonObject& user);

/// Builds `addProfileAudio` for a file already known to TDLib.
///
/// The requested interface carries only the file id, so the `inputAudio` gets
/// empty title and performer and a zero duration; TDLib derives nothing from
/// the id itself. A non-positive file id returns an empty object.
[[nodiscard]] QJsonObject setProfileAudioRequest(qint64 fileId);

/// Builds the `removeProfileAudio` envelope for the current user's profile.
///
/// The requested interface has no file-id parameter, but TDLib's method requires
/// `file_id:int32`; this builder therefore cannot produce a complete request on
/// its own. It is kept as the stable app-level entry point, and the caller that
/// owns the selected track must add its `file_id` before sending.
[[nodiscard]] QJsonObject removeProfileAudioRequest();

/// Builds `getUserProfilePhotos` for a user profile page.
///
/// A non-positive user id returns an empty object. A negative offset is clamped
/// to zero and the limit is clamped to TDLib's accepted 1..100 range, so the
/// builder never sends values the server would reject.
[[nodiscard]] QJsonObject getProfilePhotosRequest(qint64 userId, int offset, int limit);

/// Builds `setProfilePhoto` for a local image file.
///
/// An empty path (including whitespace only) returns an empty object; a real
/// path is passed through unchanged, because leading or trailing spaces can be
/// part of a file name. isPublic sets TDLib's `is_public`, i.e. the photo stays
/// visible even when the main photo is hidden by privacy settings.
[[nodiscard]] QJsonObject setProfilePhotoRequest(const QString& localPath, bool isPublic);

/// Builds `deleteProfilePhoto` for one of the current user's profile photos.
///
/// A non-positive photo id returns an empty object; TDLib photo ids are int64
/// and are passed through unchanged otherwise.
[[nodiscard]] QJsonObject deleteProfilePhotoRequest(qint64 photoId);

} // namespace beygram::telegram
