#pragma once

#include <QJsonObject>
#include <QVector>

namespace beygram::telegram {

/// Builds getChats for one chat list. folderId 0 is the main chat list and is
/// sent as a null chat_list; a positive id is sent as chatListFolder.
/// The limit is clamped to 1..100.
[[nodiscard]] QJsonObject getChatsForFolderRequest(qint64 folderId, int limit);

/// Builds loadChats for one chat list with the same chat_list encoding as
/// getChatsForFolderRequest. The limit is clamped to 1..100.
[[nodiscard]] QJsonObject loadChatsForFolderRequest(qint64 folderId, int limit);

/// Reads chat_ids from a chats object in list order. Numeric strings are
/// accepted, invalid entries and zero ids are skipped and duplicates are
/// dropped while the first occurrence keeps its position.
[[nodiscard]] QVector<qint64> chatIdsFromResponse(const QJsonObject& response);

/// True when the response is a TDLib chats object.
[[nodiscard]] bool responseIsChats(const QJsonObject& response);

} // namespace beygram::telegram
