#pragma once

#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

// Snapshot of the signed-in user's own Telegram profile, assembled from the
// `user` returned by getMe plus the optional `userFullInfo` returned by
// getUserFullInfo.
//
// The struct is plain data: no pointers, no session state, and the phone number
// is only what TDLib returns for the account owner. It is safe to copy across
// threads and to cache per account.

namespace beygram::telegram {

struct OwnProfileState {
    qint64 userId = 0;
    QString firstName;
    QString lastName;
    QString username;
    QString phoneNumber;
    QString bio;
    int photoFileId = 0;
    bool isPremium = false;
    bool isVerified = false;
};

/// Builds `getMe`, which returns the signed-in user.
[[nodiscard]] QJsonObject getOwnUserRequest();

/// Builds `getUserFullInfo` for the signed-in user.
///
/// TDLib has no self-scoped full-info method and requires `user_id`, so this
/// parameter-less builder emits the placeholder 0. A caller must copy the id
/// from the `getMe` answer into `user_id` before sending; TDLib rejects the
/// placeholder unchanged.
[[nodiscard]] QJsonObject getOwnFullInfoRequest();

/// Parses a TDLib `user` plus optional `userFullInfo`.
///
/// bio comes from `fullInfo.bio`, which TDLib sends as a formattedText object;
/// a plain string is accepted as well. photoFileId comes from the user's
/// `profile_photo.small.id`, falling back to the full-info copy when a caller
/// attaches one. An empty fullInfo leaves bio empty, and photoFileId stays 0
/// when the user has no photo either.
[[nodiscard]] OwnProfileState ownProfileFromUser(const QJsonObject& user,
                                                 const QJsonObject& fullInfo = {});

/// Joins the trimmed first and last name, then falls back to the username, the
/// phone number and finally "You", so the account row always has a label.
[[nodiscard]] QString ownProfileDisplayName(const OwnProfileState& state);

} // namespace beygram::telegram

Q_DECLARE_METATYPE(beygram::telegram::OwnProfileState)
