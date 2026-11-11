#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One entry of TDLib's folder list. The main list is represented by id 0 and
/// is always the first entry of ChatFoldersState::folders.
struct ChatFolderInfo {
    qint64 id = 0;                 // 0 is the main list
    QString title;
    QString icon;                  // TDLib custom emoji id as string, may be empty
    int position = 0;
    bool isMain = false;
    bool hasArchive = false;       // from updateChatFolders
    int unreadCount = 0;
};

struct ChatFoldersState {
    QVector<ChatFolderInfo> folders; // main list first
    qint64 archiveChatListId = 0;    // 0 if TDLib has no archive list
    bool hasArchive = false;
};

/// Parses updateChatFolders into a state with the main list first. Any other
/// update type yields an empty state.
[[nodiscard]] ChatFoldersState chatFoldersFromUpdate(const QJsonObject& update);

/// Builds getChatFolders for the main chat list.
[[nodiscard]] QJsonObject getChatFoldersRequest();

/// Builds getChatFolder for one folder; the main list is id 0.
[[nodiscard]] QJsonObject getChatFolderRequest(qint64 folderId);

} // namespace beygram::telegram
