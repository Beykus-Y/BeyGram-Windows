#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace beygram::telegram {

/// Pure request builders for the Phase 2 conversation slice. They contain no
/// logging and no @extra; the transport adds correlation.
[[nodiscard]] QJsonObject sendTextMessageRequest(qint64 chatId, const QString& text);
[[nodiscard]] QJsonObject editTextMessageRequest(qint64 chatId, qint64 messageId,
                                                 const QString& text);
[[nodiscard]] QJsonObject getChatHistoryRequest(qint64 chatId, qint64 fromMessageId,
                                                int limit, bool onlyLocal = false);
[[nodiscard]] QJsonObject viewMessagesRequest(qint64 chatId, const QJsonArray& messageIds,
                                              bool forceRead = false);
[[nodiscard]] QJsonObject deleteMessagesRequest(qint64 chatId, const QJsonArray& messageIds,
                                                bool revoke = true);

} // namespace beygram::telegram
