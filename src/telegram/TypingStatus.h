#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Pure builders for TDLib sendChatAction. They contain no logging and no
/// secrets; the transport adds correlation.
///
/// Shape follows the TDLib schema used by this client:
///   sendChatAction chat_id:int53 topic_id:MessageTopic
///                   business_connection_id:string action:ChatAction = Ok;
///
/// The schema has no message_thread_id: threads are expressed through the
/// nullable topic_id, which stays null because this client has no topic
/// support. business_connection_id is empty, i.e. the action belongs to the
/// user's own account and not to a connected business account.
///
/// Uploading actions require a progress value; the builders report 0 because
/// the client announces the activity, not a percentage.

/// Builds a sendChatAction request with chatActionTyping.
[[nodiscard]] QJsonObject sendTypingRequest(qint64 chatId);

/// Builds a sendChatAction request with chatActionUploadingPhoto. The action
/// names are the ones this TDLib schema defines (chatActionUploadingPhoto, not
/// chatActionUploadPhoto).
[[nodiscard]] QJsonObject sendUploadPhotoRequest(qint64 chatId);

/// Builds a sendChatAction request with chatActionUploadingDocument.
[[nodiscard]] QJsonObject sendUploadDocumentRequest(qint64 chatId);

/// Maps a TDLib ChatAction @type to the line shown under the chat title.
/// chatActionTyping is "typing…", chatActionUploadingPhoto is
/// "sending a photo…" and chatActionUploadingDocument is "sending a file…".
/// chatActionCancel and any action this client does not display return an
/// empty string, so callers can clear the indicator.
[[nodiscard]] QString chatActionLabel(const QString& actionType);

} // namespace beygram::telegram
