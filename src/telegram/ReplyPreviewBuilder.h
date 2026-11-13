#pragma once

#include <QJsonObject>
#include <QString>

namespace beygram::telegram {

/// Builds the one-line strings a reply UI shows for a TDLib message.
///
/// The message passed in is the one being quoted, not the reply that carries
/// it: replyPreviewForMessage() reads message["content"] and
/// replySenderLabel() reads the sender fields the caller resolved onto the
/// message. ReplyInfo.h owns the target identity (which message is quoted);
/// this module only turns a target into display text.
///
/// replyPreviewForMessage() collapses whitespace, trims the result and caps it
/// at maxLength UTF-16 code units without splitting a surrogate pair. Text and
/// captions are used when present; content without text falls back to
/// mediaLabelForContent(). A message object without any content falls back to
/// the reply quote parsed by ReplyInfo.h, then to "Message". A non-positive
/// maxLength yields an empty string.
[[nodiscard]] QString replyPreviewForMessage(const QJsonObject& message, int maxLength = 120);

/// Sender shown for a quoted message: "You" when outgoing, otherwise
/// sender_name and then author_signature, collapsed to one line. Empty when
/// neither field carries a name, so a caller can omit the prefix.
[[nodiscard]] QString replySenderLabel(const QJsonObject& message, bool outgoing);

/// English label for a TDLib MessageContent object: "Photo", "Video", "GIF",
/// "Audio", "Voice message", "Document", "Sticker", "Location", "Contact",
/// "Poll", "Game" or "Dice". Text has no label of its own, so messageText,
/// unknown content and empty content all yield "Message".
[[nodiscard]] QString mediaLabelForContent(const QJsonObject& content);

} // namespace beygram::telegram
