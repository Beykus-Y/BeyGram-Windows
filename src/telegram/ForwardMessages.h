#pragma once

#include <QJsonObject>
#include <QVector>

namespace beygram::telegram {

/// Pure builders for TDLib forwardMessages. They contain no logging, no secrets
/// and no @extra; the transport adds correlation.
///
/// Shape follows td_api.tl:
///   forwardMessages chat_id:int53 topic_id:MessageTopic from_chat_id:int53
///                   message_ids:vector<int53> options:messageSendOptions
///                   send_copy:Bool remove_caption:Bool = Messages;
/// topic_id is null (no topic support in this API) and only_preview is carried
/// inside messageSendOptions, which is where the schema defines it.
[[nodiscard]] QJsonObject forwardMessagesRequest(qint64 toChatId, qint64 fromChatId,
                                                const QVector<qint64>& messageIds,
                                                bool silently = false);

/// True when at least one id is usable for forwarding, i.e. survives the
/// positive-and-unique filter applied by forwardMessagesRequest.
[[nodiscard]] bool canForward(const QVector<qint64>& messageIds);

} // namespace beygram::telegram
