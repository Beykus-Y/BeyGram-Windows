#pragma once

#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

// Attachment-type, sender and date filters for TDLib `searchChatMessages` and
// `searchMessages`.
//
// Both builders are pure: no session state, no I/O and no logging. The query is
// passed through verbatim and is never logged; only the request shape is built.

namespace beygram::telegram {

/// Filter shared by in-chat and global message search.
///
/// `kind` is one of "any" (the default), "photo", "video", "document", "audio",
/// "voice", "url" or "mention", matched case-insensitively after trimming.
/// An empty or unknown kind behaves like "any", so a stale UI value never turns
/// into a rejected request.
///
/// `senderUserId` restricts an in-chat search to messages sent by that user
/// (0 or a negative id means every sender). TDLib's global search has no sender
/// field, so `searchMessagesFilteredRequest` ignores it.
///
/// `minDate`/`maxDate` are inclusive Unix timestamps (0 means unbounded) and
/// only apply to global search, because `searchChatMessages` has no date range;
/// a caller that wants dates in a chat filters the returned page itself. Both
/// bounds are clamped to TDLib's int32 range, and a reversed range is swapped so
/// the request still describes a valid interval.
struct SearchFilter {
    QString kind = QStringLiteral("any");
    qint64 senderUserId = 0;
    qint64 minDate = 0;
    qint64 maxDate = 0;
};

/// Builds a TDLib `searchChatMessages` request for one chat with the sender and
/// attachment-type parts of `filter`. `limit` is clamped to 1..100, the range
/// the server accepts. topic_id is null (all topics), offset is 0 and the
/// filter is null for kind "any". `fromMessageId` is the cursor from the
/// previous page (0 for the first page).
[[nodiscard]] QJsonObject searchChatMessagesFilteredRequest(qint64 chatId, const QString& query,
                                                           const SearchFilter& filter,
                                                           qint64 fromMessageId, int limit);

/// Builds a TDLib `searchMessages` request across all non-secret chats with the
/// attachment-type and date parts of `filter`. chat_list and chat_type_filter
/// are null, `offset` is passed through verbatim (empty for the first page) and
/// `limit` is clamped to 1..100. min_date/max_date are 0 when the filter has no
/// bound. `senderUserId` is ignored: TDLib's global search has no sender field.
[[nodiscard]] QJsonObject searchMessagesFilteredRequest(const QString& query,
                                                        const SearchFilter& filter,
                                                        const QString& offset, int limit);

} // namespace beygram::telegram

Q_DECLARE_METATYPE(beygram::telegram::SearchFilter)
