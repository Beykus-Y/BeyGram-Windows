#pragma once

#include "TelegramTypes.h"

#include <QString>
#include <QVector>

namespace beygram::telegram {

/// On-disk snapshot of the chat list.
///
/// The TDLib database is authoritative, but asking it for 900+ chats on every
/// launch costs a getChat round-trip per chat. The snapshot lets the window
/// paint the last known list immediately; TDLib then corrects it in place.
struct ChatCacheSnapshot {
    QVector<ChatData> chats;
    qint64 savedAt = 0;
};

/// Reads the snapshot. A missing, unreadable or older-version file yields an
/// empty snapshot; malformed rows are skipped individually.
[[nodiscard]] ChatCacheSnapshot loadChatCache(const QString& path);

/// Writes the snapshot atomically. Returns false when the file cannot be
/// written; the caller keeps running from TDLib data.
bool saveChatCache(const QString& path, const QVector<ChatData>& chats);

} // namespace beygram::telegram
