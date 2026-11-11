#pragma once

#include <QJsonObject>
#include <QtGlobal>

namespace beygram::telegram {

/// Pure builders for the TDLib pinned-message methods. They contain no logging,
/// no secrets and no @extra; the transport adds correlation.
///
/// Shape follows td_api.tl:
///   getChatPinnedMessage chat_id:int53 = Message;
///   pinChatMessage chat_id:int53 message_id:int53 disable_notification:Bool
///                  only_for_self:Bool = Ok;
///   unpinChatMessage chat_id:int53 message_id:int53 = Ok;
///   unpinAllChatMessages chat_id:int53 = Ok;
///
/// getChatPinnedMessage returns the newest pinned message (a 404 error when the
/// chat has none), so there is no plural method to build.
[[nodiscard]] QJsonObject getChatPinnedMessageRequest(qint64 chatId);

/// Pins messageId. disableNotification is silently ignored by Telegram in
/// channels and private chats; onlyForSelf is accepted in private chats only.
[[nodiscard]] QJsonObject pinChatMessageRequest(qint64 chatId, qint64 messageId,
                                                bool disableNotification = false,
                                                bool onlyForSelf = false);

/// Unpins messageId, or every pinned message when messageId is 0 (the default).
///
/// TDLib has no "0 means all" shortcut inside unpinChatMessage: the id there must
/// name a real message. Unpinning everything is a separate method, so a
/// non-positive id is built as unpinAllChatMessages, which is what "unpin all"
/// means on the wire.
[[nodiscard]] QJsonObject unpinChatMessageRequest(qint64 chatId, qint64 messageId = 0);

/// Extracts the pinned message id carried by a TDLib update, or 0 when the update
/// says nothing is pinned or is shaped unexpectedly.
///
/// Reads updateChatPinnedMessage/pinned_message_id (number or string). Current
/// TDLib versions emit updateMessageIsPinned/message_id/is_pinned instead, so that
/// update is accepted too: the id is returned only while is_pinned is true.
[[nodiscard]] qint64 pinnedMessageIdFromUpdate(const QJsonObject& update);

} // namespace beygram::telegram
