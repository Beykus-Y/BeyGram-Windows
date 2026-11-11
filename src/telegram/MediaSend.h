#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Media kinds that can be sent as a single message through sendMessage.
enum class OutgoingMediaKind { Photo, Video, Document };

/// Builds a TDLib sendMessage request whose input_message_content is an
/// inputMessagePhoto, inputMessageVideo or inputMessageDocument pointing at a
/// local file. The caption is plain formattedText without entities; an empty
/// caption stays an empty string. A reply target is added only for a positive
/// message id. The builder performs no file access, no logging and adds no
/// @extra; the transport adds correlation.
[[nodiscard]] QJsonObject sendMediaMessageRequest(qint64 chatId, OutgoingMediaKind kind,
                                                  const QString& localPath,
                                                  const QString& caption,
                                                  qint64 replyToMessageId = 0);

/// Checks a user-selected local path without reading the file: the path must be
/// non-empty, local (no URL scheme), point at an existing regular file, and use
/// an extension allowed for the kind. Photo and video are case-insensitive
/// allow-lists; a document accepts any non-empty extension.
[[nodiscard]] bool isSupportedMediaPath(const QString& localPath,
                                        OutgoingMediaKind kind);

} // namespace beygram::telegram
