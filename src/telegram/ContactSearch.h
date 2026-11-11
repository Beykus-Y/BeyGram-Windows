#pragma once

#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QVector>
#include <QtGlobal>

// Contact search built on TDLib `searchContacts`.
//
// Both functions are pure: no session state, no I/O and no logging. The parser
// only copies fields TDLib actually returned; it never derives a username,
// phone number or photo from other fields.

namespace beygram::telegram {

struct ContactInfo {
    qint64 userId = 0;
    QString displayName;
    QString username;   // without the leading '@'
    int photoFileId = 0; // profile photo `small` file id
    bool isBot = false;
    bool isPremium = false;
};

/// Builds a TDLib `searchContacts` request. `limit` is clamped to 1..100 and
/// the query is trimmed of surrounding whitespace.
[[nodiscard]] QJsonObject searchContactsRequest(const QString& query, int limit);

/// Parses a response whose `users` field is an array of TDLib `user` objects.
///
/// TDLib's own `users` result only carries `user_ids`, so the caller has to
/// resolve those ids first; a raw `users` object parses to an empty list rather
/// than inventing names. Entries that are not objects or that carry no positive
/// user id are skipped, so a partially malformed payload still yields the valid
/// contacts in order.
[[nodiscard]] QVector<ContactInfo> contactsFromResponse(const QJsonObject& response);

} // namespace beygram::telegram

Q_DECLARE_METATYPE(beygram::telegram::ContactInfo)
