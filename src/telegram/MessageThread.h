#pragma once

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// The discussion thread of one channel post, as returned by getMessageThread.
///
/// A channel keeps its comments in a linked supergroup, so chatId is the
/// discussion chat and messageId is the thread id inside it; neither is the
/// channel post that opened the thread. The parser leaves both at 0 when the
/// response does not carry them.
struct MessageThreadInfo {
    qint64 chatId = 0;
    qint64 messageId = 0;
    int replyCount = 0;
    QVector<qint64> recentMessageIds;
};

/// Builds getMessageThread:
///
///   getMessageThread chat_id:int53 message_id:int53 = MessageThreadInfo;
///
/// Both ids must be positive; otherwise {} is returned because TDLib has no
/// chat or message 0.
[[nodiscard]] QJsonObject getMessageThreadRequest(qint64 chatId, qint64 messageId);

/// Builds getMessageThreadHistory:
///
///   getMessageThreadHistory chat_id:int53 message_id:int53
///                           from_message_id:int53 offset:int32 limit:int32
///                           = Messages;
///
/// chatId and messageId must be positive. fromMessageId is the paging cursor:
/// 0 asks for the newest page, and a negative cursor is stored as 0. offset is
/// clamped to 0 and limit to 1..100, the page size TDLib accepts.
[[nodiscard]] QJsonObject getMessageThreadHistoryRequest(qint64 chatId, qint64 messageId,
                                                          qint64 fromMessageId, int offset,
                                                          int limit);

/// Parses a messageThreadInfo response.
///
/// replyCount comes from reply_info.reply_count and is clamped to
/// 0..INT_MAX. recent_message_ids is read in server order; non-positive and
/// duplicate ids are skipped. A missing reply_info or recent_message_ids
/// yields 0 and an empty list. messageId prefers TDLib's message_thread_id and
/// accepts message_id as a fallback for flattened payloads.
[[nodiscard]] MessageThreadInfo messageThreadFromResponse(const QJsonObject& response);

} // namespace beygram::telegram
