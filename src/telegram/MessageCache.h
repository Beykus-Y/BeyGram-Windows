#pragma once

#include <QJsonArray>
#include <QString>

namespace beygram::telegram {

/// Per-chat cache of the newest history page, stored as the raw TDLib JSON.
///
/// Keeping the raw payload means the cache needs no schema of its own: the
/// same messageFromJson() used for live history parses it back, so every
/// display field survives. The cache is a paint-ahead buffer only; TDLib
/// history always replaces it.
struct CachedMessages {
    QJsonArray messages;
    qint64 savedAt = 0;
};

[[nodiscard]] QString messageCacheFilePath(const QString& directory, qint64 chatId);

[[nodiscard]] CachedMessages loadCachedMessages(const QString& directory, qint64 chatId);

/// Stores at most maxMessages of the newest messages (the array is expected in
/// TDLib order, newest first). Returns false when the file cannot be written.
bool saveCachedMessages(const QString& directory, qint64 chatId,
                        const QJsonArray& messages, int maxMessages = 60);

void removeCachedMessages(const QString& directory, qint64 chatId);

} // namespace beygram::telegram
