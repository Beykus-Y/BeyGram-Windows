#pragma once

#include <QJsonObject>
#include <QtGlobal>

namespace beygram::telegram {

/// Pure builders for the per-message and per-chat actions of the message
/// context menu: pinning, clearing or deleting history, the manual unread
/// marker and the message TTL. They contain no logging, no secrets and no
/// @extra; the transport adds correlation.
///
/// Shapes follow the TDLib JSON interface. For the pinned schema (1.8.69,
/// commit 4c6377aa, see docs/WINDOWS_TDLIB.md):
///
///   pinChatMessage chat_id:int53 message_id:int53 disable_notification:Bool
///                  only_for_self:Bool = Ok;
///   unpinChatMessage chat_id:int53 message_id:int53 = Ok;
///   unpinAllChatMessages chat_id:int53 = Ok;
///   deleteChatHistory chat_id:int53 remove_from_chat_list:Bool revoke:Bool = Ok;
///   toggleChatIsMarkedAsUnread chat_id:int53 is_marked_as_unread:Bool = Ok;
///   getChatPinnedMessage chat_id:int53 = Message;
///
/// TDLib has no clearChatHistory function. Clearing the messages while keeping
/// the chat is deleteChatHistory with remove_from_chat_list = false, so
/// clearChatHistoryRequest builds exactly that request instead of inventing a
/// method name the pinned server would reject. deleteChatHistoryRequest builds
/// the same method with remove_from_chat_list = true, which also drops the chat
/// from the lists.
///
/// setChatMessageTtl is the newer spelling of the chat's message TTL. The
/// pinned 1.8.69 schema calls the same setting setChatMessageAutoDeleteTime
/// with message_auto_delete_time and a 365-day cap; the transport must
/// translate this request when it talks to a pinned server.
///
/// Every builder rejects chat_id 0 (negative ids are valid for groups and
/// channels) and returns an empty object, which the caller must not send.

/// Pins messageId when pin is true and unpins it when false.
///
/// Both directions name one message, so messageId must be positive;
/// unpinAllChatMessagesRequest is the way to drop every pin. The pin request
/// writes disable_notification and only_for_self as false because TDLib
/// requires both fields and this signature carries no flags: the action is
/// notified normally and is pinned for everyone, not only for the current user.
[[nodiscard]] QJsonObject messageActionsPinRequest(qint64 chatId, qint64 messageId, bool pin);

/// Removes every pinned message in the chat.
[[nodiscard]] QJsonObject unpinAllChatMessagesRequest(qint64 chatId);

/// Clears the chat's messages but keeps the chat in the lists.
///
/// removeFromServer is the schema's revoke flag: true deletes the messages for
/// all participants where Telegram allows it, false only for the current user.
/// Builds deleteChatHistory with remove_from_chat_list = false.
[[nodiscard]] QJsonObject clearChatHistoryRequest(qint64 chatId, bool removeFromServer);

/// Deletes the chat together with its history and removes it from the lists.
///
/// revoke=true deletes the history for all participants where Telegram allows
/// it; false deletes it only for the current user. Builds deleteChatHistory
/// with remove_from_chat_list = true.
[[nodiscard]] QJsonObject deleteChatHistoryRequest(qint64 chatId, bool revoke);

/// Sets or clears the chat's manual unread marker.
[[nodiscard]] QJsonObject messageActionsMarkUnreadRequest(qint64 chatId, bool unread);

/// Sets the chat's message TTL.
///
/// ttlSeconds is clamped to 0..604800: 0 disables automatic deletion, negative
/// values become 0 and values above one week become 604800. The clamp keeps the
/// value inside the range Telegram accepts and inside the schema's int32.
[[nodiscard]] QJsonObject setChatMessageTtlRequest(qint64 chatId, int ttlSeconds);

/// Requests the newest pinned message. A chat with no pin answers with an
/// error, not with an empty message.
[[nodiscard]] QJsonObject messageActionsPinnedMessageRequest(qint64 chatId);

} // namespace beygram::telegram
