#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// One composer draft as Telegram stores it in the cloud. `unsynced` is a local
/// flag: it marks text that only exists on this machine and still has to be
/// written back to TDLib. Updates parsed from TDLib are always synced.
struct ChatDraft {
    qint64 chatId = 0;
    qint64 replyToMessageId = 0;
    QString text;
    bool unsynced = false;
};

/// Pure builders and parsers for the TDLib cloud-draft methods. They contain no
/// logging, no secrets and no @extra; the transport adds correlation.
///
/// Shape follows td_api.tl for the TDLib revision shipped with the Android app:
///   setChatDraftMessage chat_id:int53 topic_id:MessageTopic
///                      draft_message:draftMessage = Ok;
///   updateChatDraftMessage chat_id:int53 draft_message:draftMessage
///                          positions:vector<chatPosition> = Update;
///   draftMessage reply_to:InputMessageReplyTo date:int32
///                 content:DraftMessageContent effect_id:int64
///                 suggested_post_info:inputSuggestedPostInfo = DraftMessage;
///   draftMessageContentText text:formattedText
///                           link_preview_options:linkPreviewOptions
///                           = DraftMessageContent;
///
/// topic_id stays null because this client has no forum-topic support yet.
[[nodiscard]] QJsonObject setChatDraftMessageRequest(qint64 chatId, const QString& text,
                                                     qint64 replyToMessageId = 0);

/// Parses updateChatDraftMessage into a ChatDraft. A null draft_message is a
/// valid cloud clear and yields an empty draft for the chat. Any other update
/// shape yields a default ChatDraft.
[[nodiscard]] ChatDraft draftFromUpdate(const QJsonObject& update);

/// Builds setChatDraftMessage with draft_message = null, which removes the
/// cloud draft instead of storing an empty one.
[[nodiscard]] QJsonObject clearChatDraftRequest(qint64 chatId);

} // namespace beygram::telegram
