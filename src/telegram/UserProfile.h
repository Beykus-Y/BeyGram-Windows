#pragma once

#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

// Read-only view of a Telegram user or chat, built from TDLib responses.
//
// The struct holds no pointers, no session state and no secrets: it is safe to
// copy across threads and to cache per account. Phone numbers are only ever
// copied from what TDLib returns; they are never derived from other fields.

namespace beygram::telegram {

struct UserProfile {
    qint64 userId = 0;
    qint64 chatId = 0;
    QString displayName;
    QString username;
    QString bio;
    QString phoneNumber;      // only when TDLib actually returns it
    int photoFileId = 0;
    bool isPremium = false;
    bool isBot = false;
    bool isContact = false;
    bool isChannel = false;
    bool isGroup = false;
    int memberCount = 0;
    bool hasPhoto = false;
};

/// Builds a TDLib `getUser` request for a user id.
[[nodiscard]] QJsonObject getUserRequest(qint64 userId);
/// Builds a TDLib `getUserFullInfo` request for a user id.
[[nodiscard]] QJsonObject getUserFullInfoRequest(qint64 userId);
/// Builds a TDLib `getChat` request for a chat id.
[[nodiscard]] QJsonObject getChatRequest(qint64 chatId);
/// Builds a TDLib `getSupergroupFullInfo` request for a supergroup id.
[[nodiscard]] QJsonObject getSupergroupFullInfoRequest(qint64 supergroupId);

/// Parses a TDLib `user` plus optional `userFullInfo`.
[[nodiscard]] UserProfile profileFromUser(const QJsonObject& user,
                                          const QJsonObject& fullInfo = {});
/// Parses a TDLib `chat` plus optional `chatFullInfo` for groups/channels.
[[nodiscard]] UserProfile profileFromChat(const QJsonObject& chat,
                                          const QJsonObject& fullInfo = {});

} // namespace beygram::telegram

Q_DECLARE_METATYPE(beygram::telegram::UserProfile)
