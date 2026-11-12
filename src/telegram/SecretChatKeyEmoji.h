#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One emoji of a secret-chat key comparison, with its readable name.
///
/// emoji is the character(s) a view draws and label is the English name of that
/// emoji, meant for accessibility text and tests. Both are already-parsed
/// values, so nothing here touches TDLib or the UI.
struct KeyEmoji {
    QString emoji;
    QString label;
};

/// Returns the four emoji of a secret chat's key hash.
///
/// Reads TDLib's secretChat.key_hash (a base64 string) and passes it to
/// keyEmojiFromHash(). A missing, non-string or empty key_hash yields an empty
/// vector, exactly like an invalid one.
[[nodiscard]] QVector<KeyEmoji> keyEmojiFromState(const QJsonObject& secretChat);

/// Maps a base64 key hash to exactly four emoji.
///
/// The base64 text itself is hashed with SHA-256 and the first four digest
/// bytes pick entries of a fixed 64-emoji table: digest byte N selects entry
/// byte(N) % 64. The result therefore depends only on the string and never on
/// the current locale, time or environment, and the same hash always produces
/// the same four emoji in the same order. The table order is part of the
/// comparison, so callers must not sort or deduplicate the result.
///
/// Empty, whitespace-only and malformed base64 input yields an empty vector.
/// Surrounding whitespace is ignored. The decoded bytes are only validated,
/// never used, because the mapping is defined over the base64 text.
[[nodiscard]] QVector<KeyEmoji> keyEmojiFromHash(QString keyHashBase64);

/// Returns an 8-character lowercase hex fingerprint of a base64 key hash.
///
/// The fingerprint is the first four bytes of the SHA-256 of the same
/// normalized base64 text keyEmojiFromHash() hashes, printed in lowercase hex.
/// It is stable for one hash and independent of the emoji table. Empty,
/// whitespace-only and malformed input yields an empty string.
[[nodiscard]] QString keyHashFingerprint(QString keyHashBase64);

/// Whether two key emoji sequences describe the same key.
///
/// Both sides must be non-empty and the same length, and every emoji must be
/// identical at the same position; order matters. An empty sequence means "no
/// key to compare" and never matches, not even another empty one. Labels are
/// presentation text derived from the emoji, so they are not compared.
[[nodiscard]] bool keyEmojiMatches(const QVector<KeyEmoji>& left,
                                   const QVector<KeyEmoji>& right);

} // namespace beygram::telegram
