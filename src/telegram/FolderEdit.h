#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

// Request builders for chat folders. TDLib calls the server-side dialog filter
// a chat folder; the JSON payload is a chatFolder object. These helpers only
// shape that payload: no logging, no @extra (the transport adds the correlation
// id) and no network or session state.
//
// Shapes follow the pinned TDLib schema documented in docs/WINDOWS_TDLIB.md
// (1.8.69, commit 4c6377aa):
//
//   chatFolderIcon name:string = ChatFolderIcon;
//   chatFolderName text:formattedText animate_custom_emoji:Bool = ChatFolderName;
//   chatFolder name:chatFolderName icon:chatFolderIcon color_id:int32
//              is_shareable:Bool pinned_chat_ids:vector<int53>
//              included_chat_ids:vector<int53> excluded_chat_ids:vector<int53>
//              exclude_muted:Bool exclude_read:Bool exclude_archived:Bool
//              include_contacts:Bool include_non_contacts:Bool include_bots:Bool
//              include_groups:Bool include_channels:Bool = ChatFolder;
//   createChatFolder folder:chatFolder = ChatFolderInfo;
//   editChatFolder chat_folder_id:int32 folder:chatFolder = ChatFolderInfo;
//   deleteChatFolder chat_folder_id:int32 leave_chat_ids:vector<int53> = Ok;
//   getChatFolder chat_folder_id:int32 = ChatFolder;
//
// In this revision a folder name is a chatFolderName wrapping a formattedText,
// not the plain title:string used by older TDLib builds. icon may be null, in
// which case Telegram picks the default icon; color_id -1 disables the folder
// colour.

namespace beygram::telegram {

/// Builds createChatFolder with the minimal folder shape: one name and empty
/// filters. The title is folded to a single line, trimmed and cut to the
/// schema's 12-character limit; an empty title is passed through unchanged so
/// TDLib reports "Name must be non-empty" rather than a title being invented
/// here. Every chat id list is empty, every filter flag is false, icon is null
/// and color_id is -1 (no colour).
///
/// TDLib also requires a folder to contain at least one chat or one enabled
/// filter before it accepts it, so a title-only folder still needs membership
/// added by the caller before creation succeeds.
[[nodiscard]] QJsonObject createChatFolderRequest(const QString& title);

/// Builds editChatFolder for one custom folder. TDLib replaces the entire
/// folder with the supplied chatFolder, so this request carries the same
/// empty-filter folder as createChatFolderRequest with the new title. It is a
/// safe rename only for a folder that has no chats and no filters; callers that
/// need to preserve membership must merge the title into the getChatFolder
/// answer and send that object instead. folderId is passed through unchanged.
[[nodiscard]] QJsonObject editChatFolderRequest(qint64 folderId, const QString& title);

/// Builds deleteChatFolder for one custom folder. leave_chat_ids is always an
/// empty array: deleting a folder must not leave or delete its Telegram chats.
[[nodiscard]] QJsonObject deleteChatFolderRequest(qint64 folderId);

/// Builds getChatFolder for one custom folder. folderId must be a real folder
/// id from updateChatFolders; the main chat list is not a folder and id 0 is
/// rejected by TDLib.
[[nodiscard]] QJsonObject getChatFolderRequest(qint64 folderId);

} // namespace beygram::telegram
