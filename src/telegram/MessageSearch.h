#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One message found by an in-chat or global search.
///
/// chatTitle and senderName are only filled when the response itself carries
/// them (an optional "chat_title" key, or the message "author_signature" for
/// senderName); they are never derived from identifiers. A caller that wants
/// chat titles joins them from its chat cache by chatId.
struct SearchHit {
    qint64 chatId = 0;
    qint64 messageId = 0;
    qint64 date = 0;
    QString chatTitle;
    QString senderName;
    QString text;
    QString textHtml;
};

/// Builds a TDLib searchChatMessages request for one chat. The limit is
/// clamped to 1..100, the range the server accepts. topic_id, sender_id and
/// filter are always null, so the search covers all topics, all senders and
/// all message types. The query is passed through verbatim and is never
/// logged.
[[nodiscard]] QJsonObject searchChatMessagesRequest(qint64 chatId, const QString& query,
                                                    qint64 fromMessageId, int limit);

/// Builds a TDLib searchMessages request across all non-secret chats. The
/// limit is clamped to 1..100. chat_list, filter and chat_type_filter are
/// null, and min_date/max_date are 0 (no date restriction).
///
/// fromMessageId is accepted for symmetry with the in-chat builder but the
/// current TDLib schema has no from_message_id field for global search: the
/// next page is requested with the opaque "next_offset" string that
/// searchResponseHasMore()/the response carries.
[[nodiscard]] QJsonObject searchMessagesRequest(const QString& query, qint64 fromMessageId,
                                                int limit);
[[nodiscard]] QJsonObject searchMessagesRequest(const QString& query, const QString& offset,
                                                int limit);

/// Converts a FoundChatMessages or FoundMessages response (or any object with
/// a "messages" array) into search hits. Entries may be full message objects
/// or nested found* containers; malformed entries are skipped rather than
/// reported. Plain text comes from messageFromJson() and textHtml from
/// formatTdlibText(), so the HTML is already escaped for Qt Quick RichText.
[[nodiscard]] QVector<SearchHit> searchHitsFromResponse(const QJsonObject& response);

/// True when the response advertises another page: next_from_message_id != 0
/// for FoundChatMessages, or a non-empty next_offset for FoundMessages.
[[nodiscard]] bool searchResponseHasMore(const QJsonObject& response);

} // namespace beygram::telegram
