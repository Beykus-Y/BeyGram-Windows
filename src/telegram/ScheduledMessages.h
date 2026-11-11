#pragma once

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Pure builders for TDLib scheduled-message requests and scheduling states.
/// They contain no logging, no secrets and no @extra; the transport adds
/// correlation.
///
/// Shape follows td_api.tl (TDLib 1.8.69):
///   messageSchedulingStateSendAtDate send_date:int32 repeat_period:int32
///       = MessageSchedulingState;
///   messageSchedulingStateSendWhenOnline = MessageSchedulingState;
///   getChatScheduledMessages chat_id:int53 = Messages;
///   deleteMessages chat_id:int53 message_ids:vector<int53> revoke:Bool = Ok;
///
/// The scheduling-state objects are not requests of their own: they are
/// embedded in sendMessage.options.scheduling_state (and the equivalent
/// options of forwardMessages), where the schema types the field as
/// MessageSchedulingState.
///
/// TDLib 1.8.69 has no deleteScheduledMessages method (it was replaced by
/// deleteMessages with revoke = false), so deleteScheduledMessagesRequest
/// builds that schema-accurate request for the same operation.

/// Delivery at sendAt, in Unix seconds. repeat_period is 0, i.e. the message is
/// sent once; TDLib accepts dates at most 367 days in the future.
[[nodiscard]] QJsonObject scheduledSendState(qint64 sendAt);

/// "Send when online" state, valid only in private chats whose peer's online
/// status is known. It carries no fields beyond @type.
[[nodiscard]] QJsonObject onlineSendState();

/// Every scheduled message of chatId, newest first (decreasing message id).
[[nodiscard]] QJsonObject getScheduledMessagesRequest(qint64 chatId);

/// Removes the listed scheduled messages through deleteMessages with
/// revoke = false, which is how TDLib 1.8.69 cancels them. Non-positive and
/// duplicate ids are dropped before the request is built; an empty result is
/// still a well-formed request with an empty message_ids array, which the
/// caller should skip.
[[nodiscard]] QJsonObject deleteScheduledMessagesRequest(qint64 chatId,
                                                         const QVector<qint64>& ids);

} // namespace beygram::telegram
