#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// User-selected options for one chat-history export.
///
/// Dates are Unix seconds. Zero means "no bound"; negative values are
/// normalized to zero and a reversed range is swapped so from_date <= to_date.
/// The per-content switches are independent: includeMedia describes the
/// export as a whole, while the remaining switches choose which attachments
/// the server should list.
struct ChatExportOptions {
    bool includeMedia = false;
    bool includePhotos = true;
    bool includeVideos = true;
    bool includeFiles = true;
    bool includeVoice = true;
    bool includeLinks = true;
    bool onlyMyMessages = false;
    qint64 fromDate = 0;
    qint64 toDate = 0;
};

/// Progress reported by an export update. Counters are never negative and
/// done is set when the update reports completion or a completed file
/// transfer. error is trimmed and otherwise left as the update supplied it.
struct ChatExportProgress {
    int messagesExported = 0;
    int messagesTotal = 0;
    int filesExported = 0;
    bool done = false;
    QString error;
};

/// Builds an exportChatHistory request. A non-positive chat id yields an empty
/// object, because there is no chat to export. The limit is clamped to 1..1000
/// and a negative fromMessageId becomes 0 (the newest message).
[[nodiscard]] QJsonObject exportChatHistoryRequest(qint64 chatId,
                                                   const ChatExportOptions& options,
                                                   qint64 fromMessageId, int limit);

/// Parses an export progress update. Both TDLib's snake_case members and the
/// camelCase spellings used by the app bridge are accepted. Counters are
/// clamped to non-negative values and error is trimmed. A completed file
/// transfer (updateFile with local.is_downloading_completed) also marks the
/// progress as done. An update without recognized fields yields default
/// progress.
[[nodiscard]] ChatExportProgress exportProgressFromUpdate(const QJsonObject& update);

} // namespace beygram::telegram
