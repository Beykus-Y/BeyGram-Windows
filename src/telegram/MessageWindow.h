#pragma once

#include <QJsonObject>
#include <QtGlobal>

namespace beygram::telegram {

/// The pair of TDLib requests that opens a message window around one message:
/// the surrounding history page plus the target message itself.
///
/// Both members are plain TDLib function objects without @extra; the transport
/// adds correlation. They contain no logging and no secrets.
///
/// Shape follows td_api.tl:
///   getChatHistory chat_id:int53 from_message_id:int53 offset:int32 limit:int32
///                   only_local:Bool = Messages;
///   getMessage chat_id:int53 message_id:int53 = Message;
struct MessageWindowRequest {
    QJsonObject history;
    QJsonObject message;
};

/// Builds the history page and the single-message request for messageId.
///
/// The history request starts at messageId with offset = -radius, which TDLib
/// reads as "the specified message and some newer ones", and asks for at most
/// `limit` entries with only_local = false so the server is consulted rather
/// than just the local cache.
///
/// TDLib requires -offset <= limit and rejects offsets below -99, so `limit` is
/// clamped to 1..100 first and `radius` is then clamped to 0..min(99, limit).
/// Identifiers are passed through verbatim: chatId may be negative (a channel
/// or a basic group), and a messageId of 0 means "from the last message".
[[nodiscard]] MessageWindowRequest windowAroundMessageRequest(qint64 chatId, qint64 messageId,
                                                             int radius = 15, int limit = 40);

/// Extracts the message id from a getMessage response: the "id" field, as a
/// number or a string. Returns 0 when the id is missing, when the response is
/// a TDLib error object, or for any other unexpected shape, so callers can
/// treat "no message" and "unparsable" alike.
[[nodiscard]] qint64 messageIdFromResponse(const QJsonObject& response);

} // namespace beygram::telegram
