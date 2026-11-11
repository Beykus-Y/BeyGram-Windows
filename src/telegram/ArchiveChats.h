#pragma once

#include <QJsonObject>

namespace beygram::telegram {

/// Builds getChats for the Archive chat list. TDLib encodes the archive as a
/// chatListArchive object without an identifier. The limit is clamped to 1..100.
[[nodiscard]] QJsonObject getArchivedChatsRequest(int limit);

/// Builds loadChats for the Archive chat list with the same chat_list encoding
/// as getArchivedChatsRequest. The limit is clamped to 1..100.
[[nodiscard]] QJsonObject loadArchivedChatsRequest(int limit);

/// Builds the request that archives or unarchives one chat. This TDLib schema
/// has no toggleChatIsArchived: the archive is expressed through addChatToList,
/// which moves the chat to chatListArchive or back to chatListMain and removes
/// it from the other list automatically.
[[nodiscard]] QJsonObject setChatArchivedRequest(qint64 chatId, bool archived);

/// True when the response is a TDLib chats object. The archive uses the same
/// Chats response type as every other chat list.
[[nodiscard]] bool responseIsArchivedChats(const QJsonObject& response);

} // namespace beygram::telegram
