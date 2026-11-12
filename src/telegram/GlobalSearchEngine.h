#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

// Global-search request builders and the merge that turns three independent
// result lists into one page.
//
// The engine is pure: it owns no session state, performs no I/O and never logs
// the query. The caller issues the three requests and hands the parsed rows to
// mergeSearchHits().
//
// These types live in beygram::telegram::globalsearch rather than directly in
// beygram::telegram because MessageSearch.h already defines a SearchHit (an
// in-chat message row) and ContactSearch.h already declares
// searchContactsRequest(). A distinct namespace keeps both search modules
// includable from one translation unit.

namespace beygram::telegram::globalsearch {

/// Which request produced a hit. The declaration order is the interleaving
/// order used by mergeSearchHits().
enum class SearchSource {
    Chats,
    Messages,
    People,
};

/// One row of the merged global-search page.
///
/// chatId identifies the chat of a Chats or Messages row, messageId is set for
/// Messages rows and userId for People rows. title and subtitle are display
/// strings supplied by the caller and are never derived here. date is a Unix
/// timestamp; score is the ranking value, higher first.
struct SearchHit {
    SearchSource source = SearchSource::Messages;
    qint64 chatId = 0;
    qint64 messageId = 0;
    qint64 userId = 0;
    QString title;
    QString subtitle;
    qint64 date = 0;
    int score = 0;
};

/// Builds a TDLib `searchChatsOnServer` request. The query is trimmed and a
/// query that is empty after trimming produces an empty object, so no request
/// is sent. `limit` is clamped to 1..100.
[[nodiscard]] QJsonObject searchChatsOnServerRequest(const QString& query, int limit);

/// Builds a TDLib `searchMessages` request across all non-secret chats. The
/// query is trimmed and a blank query produces an empty object. `limit` is
/// clamped to 1..100. chat_list, filter and chat_type_filter are null,
/// min_date/max_date are 0 (no restriction) and offset is empty (first page).
[[nodiscard]] QJsonObject searchMessagesOnServerRequest(const QString& query, int limit);

/// Builds a TDLib `searchContacts` request. The query is trimmed and a blank
/// query produces an empty object. `limit` is clamped to 1..100.
[[nodiscard]] QJsonObject searchContactsRequest(const QString& query, int limit);

/// Merges the three independent result lists into one page.
///
/// Each source is cleaned on its own:
///  - rows are de-duplicated by source and its stable identity: chatId for
///    Chats, chatId plus messageId for Messages, and userId for People (a
///    person stays one user even if different chats surfaced them);
///  - a duplicate keeps the copy with the higher score; a tie keeps the first
///    occurrence, so the caller's order stays stable;
///  - the source is sorted by score descending, then date descending, then
///    SearchSource order (std::stable_sort, so equal ranks keep input order);
///  - only the first `maxPerSource` rows of each source survive.
///
/// The surviving lists are then interleaved round-robin in SearchSource order,
/// so a page starts chat, message, person, chat, ... and never shows one source
/// alone while the others wait. A source that runs out is simply skipped.
///
/// `maxPerSource` <= 0 returns an empty page. The inputs are not modified.
[[nodiscard]] QVector<SearchHit> mergeSearchHits(const QVector<SearchHit>& chats,
                                                 const QVector<SearchHit>& messages,
                                                 const QVector<SearchHit>& people,
                                                 int maxPerSource);

} // namespace beygram::telegram::globalsearch
