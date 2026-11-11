# 08 — Search parity: Android → Windows

Read-only parity audit of **global search** (all chats), **in-chat search**, filters, paging,
result grouping and result navigation/highlight.

Method: static reading of the Android Kotlin sources and the Windows C++/QML sources only.
No build, CMake configure, test run, GUI run, device interaction or `git` command was executed for
this document, so every statement below describes *code as written*, not observed behaviour.
Nothing in this audit was verified against a live TDLib instance.

Reference items: `docs/PORTING_AUDIT.md` — `M04 | Chats, archive, folders, search` (priority
**P1**), `docs/PORTING_ROADMAP.md` §"Archive, global-search wiring … remain open", and
[03-history-paging.md](03-history-paging.md) for the history window/jump primitives that Phase A
builds on.

---

## 1. Files reviewed

### Android (Kotlin)

| File | What it owns |
| --- | --- |
| `app/src/main/java/com/beykus/beygram/data/tdlib/ChatSearchRequest.kt` | `SearchChatMessages` builder, `SEARCH_PAGE_SIZE = 50`, `SharedMediaKind → SearchMessagesFilter` |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdConversationActions.kt` (§`search`, `searchGlobal`, `shared`, `window`, `context`) | Actual TDLib calls, opaque `nextOffset`, `nextFromMessageId`, client-side date filter, `GetChatHistory` window for jumps |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdLibTelegramRepository.kt` (§`searchGlobalChats`, `searchGlobalContacts`, `searchGlobalPublicChats`, `searchGlobalMessages`, `searchMessages`) | Source composition, `GetChat` metadata enrichment, total counts |
| `app/src/main/java/com/beykus/beygram/domain/repository/ConversationRepository.kt` | Repository contract (`searchMessages`, `searchForumTopicMessages`, `searchGlobalMessages`, `searchGlobalChats`, `searchGlobalContacts`, `searchGlobalPublicChats`) |
| `app/src/main/java/com/beykus/beygram/domain/search/SearchPaging.kt` | `appendDistinctBy`, `isCurrentSearch`, `isCurrentChatSearch` |
| `app/src/main/java/com/beykus/beygram/domain/search/GlobalSearchMerge.kt` | `GlobalSearchRequest(query, generation)`, `isStaleGlobalSearchRequest`, merge by chat id / user id |
| `app/src/main/java/com/beykus/beygram/domain/search/GlobalSearchDiscovery.kt` | `parseGlobalSearchQuery` (`@name`, `t.me/name`), `networkSearchAction`, section/source/scope enums |
| `app/src/main/java/com/beykus/beygram/domain/model/ChatSearchFilter.kt` | In-chat filter semantics, day-boundary helpers, client-side `matchesDate` |
| `app/src/main/java/com/beykus/beygram/domain/model/GlobalSearchFilter.kt` | Global filter (kind, chat type, period → `minDate`) |
| `app/src/main/java/com/beykus/beygram/domain/model/GlobalSearchPeople.kt` | `GlobalSearchPage<T>(items, totalCount)`, `PublicChatOpenResult` |
| `app/src/main/java/com/beykus/beygram/domain/model/ChatMessage.kt` (§`MessageSearchPage`, `GlobalMessagePage`) | Page envelopes, `hasMore`, `chats` map, `forumTopicId` |
| `app/src/main/java/com/beykus/beygram/ui/screens/chats/GlobalSearchScreen.kt` | Four sections, per-source effects, chips, load-more, states |
| `app/src/main/java/com/beykus/beygram/ui/screens/chats/GlobalSearchSession.kt` | Hoisted session: generation, per-source limits/`hasMore`, saver |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ChatSearch.kt` | Search bar, filter dialog, active-filter pills, results list, `snippet()`, `highlightMatches()` |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ChatScreen.kt` (§search effects, `openSearchResult`, `jumpToMessage`, `highlighted`) | Debounce, generation guard, jump + 2.2 s highlight |
| `app/src/main/java/com/beykus/beygram/ui/screens/chats/ChatsScreen.kt` (§local filter, `networkSearchAction`) | Local chat filter + "Find in Telegram" affordance |
| `app/src/main/java/com/beykus/beygram/BeyGramApp.kt` (§`AppRoute.GlobalSearch`) | Route wiring, `chatFocusMessageId`, public-chat open/join |
| Tests: `SearchPagingTest`, `GlobalSearchMergeTest`, `GlobalSearchDiscoveryTest`, `GlobalSearchFilterTest`, `GlobalSearchContextTest`, `ChatSearchFilterTest`, `ChatSearchSendersTest`, `ChatSearchRequestTest` | Behavioural specs mirrored below |

### Windows (C++ / QML)

| File | What it owns today |
| --- | --- |
| `src/telegram/MessageSearch.h/.cpp` | `SearchHit`, `searchChatMessagesRequest`, two `searchMessagesRequest` overloads, `searchHitsFromResponse`, `searchResponseHasMore` |
| `src/telegram/SearchResultModel.h/.cpp` | Flat in-chat result list (`chatId, messageId, date, chatTitle, senderName, text, textHtml`), `hitAt()` |
| `src/telegram/GlobalSearchModel.h/.cpp` | Paged global list, `setHits/appendHits/setChatTitles/clear`, `hasMore`, dedupe by `(chatId, messageId)` |
| `src/telegram/TelegramRepository.cpp` (§`searchInChat`, `clearSearch`, `searchGlobal`, `loadMoreGlobal`) | Request orchestration, query guards, chat-title cache join |
| `src/telegram/ContactSearch.h/.cpp` | `searchContacts` request/parse — **pure helper, not wired to any UI** |
| `src/telegram/ChatFilterModel.cpp` | Local chat-list filter: title + preview, case-insensitive `contains` |
| `src/ui/AppController.cpp/.h` | `searchResults`, `searchLoading`, `globalSearchResults`, `globalSearchLoading`, `searchGlobal`, `loadMoreGlobalSearch`, `clearGlobalSearch`, `searchInChat`, `clearSearch` |
| `qml/pages/GlobalSearchPage.qml` | 320 ms debounce, flat result list, "Load more", idle/no-results states |
| `qml/pages/ChatsPage.qml` | In-chat search bar, 320 ms debounce, overlay panel, local chat filter field |
| `qml/Shell.qml` | Global search as nav section 5; `onResultActivated` opens chat and **drops `messageId`** |
| `qml/components/MessageBubble.qml` | No highlight/focus property |
| Tests: `tests/tst_message_search.cpp`, `tests/tst_search_model.cpp`, `tests/tst_global_search_model.cpp`, `tests/tst_contact_search.cpp` | Existing unit coverage (registered in `tests/CMakeLists.txt`) |

---

## 2. Query semantics

| Aspect | Android | Windows | Parity |
| --- | --- | --- | --- |
| Trim before search | `query.trim()` for both scopes; blank ⇒ clear/idle | `query.trimmed()` in repository for both | ✅ |
| Empty query in-chat | Clears results, shows "type to search" | Clears model, panel hides | ✅ |
| Empty query global | Idle text; session reset | `searchGlobal("")` clears model + offset | ✅ |
| Blank-but-active in-chat (filter only) | Allowed: `active = needle.isNotEmpty() \|\| filter.isActive`, `query = ""` | Not possible (no filters) | ❌ |
| Username normalisation | `parseGlobalSearchQuery`: `@name`, `name`, `t.me/name`, `https://www.t.me/name/`, `?single` suffix | None | ❌ |
| What public lookups receive | `publicText` (bare username) while message search keeps `raw` | n/a (no chat/contact/public sources) | ❌ |
| Debounce | 400 ms global, 300 ms in-chat | 320 ms both | ~ |
| Stale-answer guard | `GlobalSearchRequest(query, generation)`; in-chat `searchGeneration` + `isCurrentChatSearch(query, filter)` | Global: `body != m_globalQuery`; in-chat: `chatId != m_currentChatId \|\| body != m_searchQuery` | ~ (no generation, no account dimension) |
| Whitespace-insensitive guard | `isCurrentSearch` compares `trim()`ed values | Compares already-trimmed strings | ✅ |
| Case sensitivity | TDLib decides; local date/sender/type checks are exact | TDLib decides | ✅ |
| Query in logs | Not logged (Android) | Not logged; `MessageSearch.h` states it explicitly | ✅ |
| Rich text in hits | `ChatMessage` text + entities; results render plain text with manual highlight | `messageFromJson` + `formatTdlibText` (HTML-escaped for `RichText`), QML shows `text` only | ~ |

---

## 3. Paging and offsets

### Android — in-chat

- `SEARCH_PAGE_SIZE = 50`; `offset = 0` always; cursor is `fromMessageId`.
- Response: `MessageSearchPage(messages, nextFromMessageId, totalCount)`; `hasMore = nextFromMessageId != 0L`.
- `totalCount` is TDLib's pre-date-filter count; the UI suppresses it when a date range is active
  (`searchTotalCount = if (hasDateRange) 0 else page.totalCount`).
- Next page: `appendDistinctBy(searchResults, page.messages) { it.id }` (message ids are unique per
  chat, so `id` alone is a valid key here).
- A page fully hidden by the local date filter still exposes its cursor (`ChatSearchFilterTest`
  case `aPageEmptiedByTheDateFilterStillExposesTheNextCursor`).

### Android — global messages

- `SEARCH_PAGE_SIZE = 50`; first page sends `offset = ""`; the response's opaque `nextOffset` is
  echoed back verbatim (`TdConversationActions.searchGlobal`).
- Blank `nextOffset` ⇒ end of list (`ChatMessage.kt`: "`nextOffset` пустой, когда страницы кончились").
- Next page: `appendDistinctBy(hits, page.messages) { it.chatId to it.id }` — **pair key**, because
  message ids collide across chats (`SearchPagingTest.sameMessageIdInDifferentChatsIsNotDropped`).
- `chatContexts = chatContexts + page.chats` — metadata is merged, not replaced, across pages.

### Android — chats / contacts / public chats

- No offset in TDLib for these sources; the page size *grows* instead:
  `GLOBAL_SEARCH_PAGE_SIZE = 20`, `GLOBAL_SEARCH_PEOPLE_MAX = 50`,
  `nextLimit() = (limit + 20).coerceAtMost(50)`.
- `hasMore = max(totalCount, items.size) > limit && nextLimit() > limit`.
- Chats = `SearchChats` ∥ `SearchChatsOnServer` (server best-effort, failure tolerated), ids unioned
  and de-duplicated, `totalCount = max(local, server)`.
- Public chats: `SearchPublicChats(query, null)` — no limit/offset, so the section has
  `supportsMore = false`.

### Windows

| Aspect | Status |
| --- | --- |
| In-chat paging | ❌ Single request, `limit = 50`, `from_message_id = 0`, `next_from_message_id` never read back, no "load more" |
| Global paging | ✅ First page `offset = ""`, `m_globalOffset = response.next_offset`, `loadMoreGlobal()` guards on empty query/offset/loading, dedupe by `(chatId, messageId)` in `GlobalSearchModel::appendHits` |
| Global `hasMore` | ✅ `!m_globalOffset.isEmpty()`; also `searchResponseHasMore()` exists but the repository uses the offset string directly |
| Page size | 50 for both scopes (matches Android) |
| Limit clamping | `std::clamp(1, 100)` — documented and tested |
| People paging (growing limit) | ❌ Not implemented (no people sources) |

---

## 4. Filters

### Android — in-chat (`ChatSearchFilter`)

- Fields: `senderId: Long?`, `kind: SharedMediaKind?`, `fromDateSeconds: Long?` (inclusive day
  start), `toDateSeconds: Long?` (exclusive start of the day after the last chosen day), all in the
  **device time zone**.
- `isActive`, `hasDateRange`, `hasValidDateRange` (`from < to`), `matches(message, query)` (pure
  spec used by tests/previews), `matchesDate(message)` (the only part TDLib cannot do).
- Request mapping: `senderId > 0` → `MessageSenderUser`; `kind` → `SearchMessagesFilter`
  (Media→PhotoAndVideo, Files→Document, Links→Url, Voice→VoiceNote, Music→Audio); **date range is
  absent from the request on purpose** and applied per page after the response, so the cursor is
  untouched.
- UI: tune icon (accent when `isActive`), dialog with sender dropdown built from loaded history
  (`chatSearchSenders`: positive user ids only, distinct, sorted case-insensitively, name falls back
  to id), type chips (`Media, Files, Links, Voice` — `Music` has a label but is not offered),
  two date buttons opening `DatePickerDialog`, invalid-range warning, reset, apply disabled while
  invalid. Active criteria stay visible as pills under the field with a one-tap reset.
- Saved state: `ChatSearchFilterSaver` (primitives only), survives process recreation.

### Android — global (`GlobalSearchFilter`)

- `kind: SharedMediaKind?` → `SearchMessagesFilter`
- `chatType: GlobalChatType?` (`Private / Group / Channel`) → `SearchMessagesChatTypeFilter*`
- `period: GlobalSearchPeriod` (`Any / Week / Month`) → `minDate = now - period`, floored at 0;
  `maxDate = 0`.
- No sender filter — documented: TDLib `SearchMessages` does not accept one.
- UI: three chip rows (`All` + options) above the message section only; changing a filter re-runs
  the **message** source only. Filters are saved by `GlobalSearchSession.stateSaver`; search pages
  are deliberately reloaded after process death while query + filters are restored.

### Windows

| Filter | Android | Windows |
| --- | --- | --- |
| In-chat sender | ✅ `sender_id` | ❌ always `null` (`MessageSearch.cpp:76`) |
| In-chat attachment type | ✅ `filter` | ❌ always `null` (`:80`) |
| In-chat date range | ✅ client-side per page | ❌ |
| In-chat forum topic scope | ✅ `topic_id` | ❌ always `null` (`:74`) |
| Global attachment type | ✅ | ❌ always `null` (`:92`) |
| Global chat type | ✅ `chat_type_filter` | ❌ always `null` (`:93`) |
| Global period | ✅ `min_date` | ❌ always `0` (`:94`) |
| Global sender | n/a | n/a |
| Filter persistence | ✅ savers | n/a |

---

## 5. Result grouping and row content

### Android

Fixed order, one header per section, each with a count and its own loading/retry state:

1. **Chats** (`LocalChat`) — local + server chat search, scope label "Local", type label.
2. **People and bots** (`LocalContact`) — contact book, "Contact" / "Bot" label + subtitle.
3. **Public results** (`PublicResult`) — `SearchPublicChats`, accent-coloured scope label; tap goes
   through the explicit `openPublicChat` (join / join-request) path, not the local open path.
4. **Global messages** (`GlobalMessage`) — filter chips, count header, avatar + chat title + type,
   sender name, 2-line preview (`attachment` placeholder when text is blank), date + time.

- Row keys are namespaced: `chat:<id>`, `contact:<id>`, `public:<id>`, `message:<chatId>:<id>`.
- Message hits carry a resolved `ChatPreview` map (`chatContexts`); a hit whose chat cannot be
  resolved still renders (`global_search_chat_unknown`, `Avatar("?")`), per
  `GlobalSearchContextTest.unavailableChatDoesNotHideSearchHit`.
- Chat metadata for hits is fetched lazily: one `GetChat` per distinct uncached chat id, bounded to
  6 concurrent requests, timeouts and failures tolerated, results cached in `chatCache` so the next
  page and the later open reuse them.

### Windows

- Global search is a **single flat list of messages**, no sections, no chat/contact/public sources.
- Row: `chatTitle · senderName` (meta line) + right-aligned `dd.MM.yyyy hh:mm` + one elided line of
  text. `[media]` placeholder exists only in the in-chat panel, not in the global page.
- `chatTitle` resolution: the response's `chat_title` key (TDLib does **not** return it) else the
  `m_chats` cache. Chats outside the loaded list therefore render with an empty title.
- `senderName`: response `sender_name` else `author_signature` — never derived from ids
  (`MessageSearch.h` doc comment, tested by `leavesMissingTitlesEmpty`).
- In-chat panel rows: text or `[media]`, plus date; no sender, no count.

---

## 6. Result navigation (jump) and highlight

This is the largest functional gap.

### Android — in-chat

`ChatScreen.kt`:

1. Tap → `openSearchResult(message)`:
   - find the row index in `messageListItems`; if present →
     `animateScrollToItem(index)` then `highlighted = message.id`;
   - else → `contextChatId/contextMessageId` set, which opens the message-context viewer.
2. `closeSearch()` runs first, so the results overlay disappears and the conversation is visible.
3. `highlighted` renders a **2 dp accent border** on the bubble (`ChatScreen.kt:2733`), including the
   album case (`listItem.messages.any { it.id == highlighted }`), and is cleared by
   `LaunchedEffect(highlighted) { delay(2200); highlighted = null }`.
4. Result rows themselves highlight the match: `snippet(text, query)` windows the text 40 chars
   before the first case-insensitive match (surrogate-safe, `…` prefix), then
   `highlightMatches()` applies accent colour + SemiBold spans to **every** occurrence.
5. Rows also show `search_you` for outgoing hits, `message.summary()` when text is blank.

### Android — global

1. Tap → `onOpenMessage(chatId, forumTopicId, messageId)`.
2. `BeyGramApp` awaits `loadChat(chatId)`, re-checks the selected account, then sets
   `selectedChatId`, `selectedForumTopicId`, **`chatFocusMessageId = messageId`**,
   `chatBackRouteName = GlobalSearch`, and navigates to `AppRoute.Chat`.
3. `ChatScreen`'s `LaunchedEffect(focusMessageId)` calls `jumpToMessage(id, …)`:
   - already loaded → scroll + highlight;
   - not loaded → `loadMessagesAround` / `loadForumTopicMessagesAround` (a `GetChatHistory`
     window of −15/40 plus a separate `GetMessage`), then `pendingJumpId` scrolls + highlights once
     the row exists;
   - deleted target → the surrounding page is still usable (`TdConversationActions.window`
     tolerates a 404 from `GetMessage`), and the opening is released rather than hanging.
4. `onFocusHandled` clears the focus id so a later unrelated open cannot re-jump.

### Windows

| Step | Status |
| --- | --- |
| In-chat result click | ❌ `ChatsPage.qml:558` only calls `page.closeSearch()` — no jump, no highlight, the query is also cleared |
| Global result click | ⚠️ `Shell.qml:223` calls `BeyGramApp.openChat(chatId)` and ignores `messageId`; lands at the newest message |
| Jump to an arbitrary message | ❌ No repository API; `openChat` always `getChatHistory(chatId, 0, …)` (newest page) |
| Load a window around a message | ❌ No `loadMessagesAround`; no `getMessage` request builder at all (`TelegramRequests.h`) |
| Scroll to a loaded row | ⚠️ `MessageListModel::indexForId()` exists but is unused by any jump path; QML only ever `positionViewAtEnd()` |
| Bubble highlight | ❌ `MessageBubble.qml` has no `highlighted` property; no timer, no accent border |
| Match highlight in result rows | ❌ Plain `Text` with `ElideRight`; no snippet window, no span styling |
| Forum topic in the target | ❌ `SearchHit` has no topic id and in-chat search sends `topic_id: null` |

---

## 7. Empty / loading / error states

### Android — in-chat (`ChatSearchResults`)

| State | Presentation |
| --- | --- |
| No criteria | "type to search" note |
| Busy, no rows | Centred spinner |
| No rows, more pages | "no matches on this page" + "Show more" |
| No rows, filter active | "no results for these filters" + reset button |
| No rows, plain query | "nothing found" |
| Rows | Count header (`search_results_count` with `totalCount`, else `search_results_loaded`), list, "Show more"/"Loading more", error text with `errorAnnouncement()` |
| Error | `failure.userMessage(context, R.string.chat_search_failed)` shown inline, announced for a11y |
| Paging error | Same string, list kept |

### Android — global

| State | Presentation |
| --- | --- |
| Blank query | Idle hint |
| Offline (`WaitingForNetwork`) | Warning banner with `WifiOff` icon above the list |
| Per section | Header count + inline spinner; on failure a `Retry` button in the header |
| Per section empty | Section-specific text (chats / contacts / public / messages) |
| Per section failure | Section-specific failure text, or "source unavailable offline" when offline |
| Message section | Filters always visible, then empty/failure state |
| Load more | `Load more` button, disabled while loading, "Couldn't load more" note on failure |
| Accessibility | Section titles are `heading()`, failures use `errorAnnouncement()` |

### Windows

| State | Status |
| --- | --- |
| Global: idle | ✅ "Type a word or phrase to search messages in every chat." |
| Global: first-page spinner | ✅ `BusyIndicator` when `loading && count === 0` |
| Global: no results | ✅ "No results" (only when `appliedQuery` is non-empty) |
| Global: error | ❌ No error surface in the page. `TelegramRepository` emits `errorOccurred("Search failed: …")` → `AppController::m_chatError` → the banner inside `ChatsPage` — invisible while the Search section is showing |
| Global: load-more spinner | ✅ footer `BusyIndicator` + disabled "Load more" |
| Global: load-more error | ❌ Emits `stateChanged` only, no message, no retry |
| Global: offline | ❌ No banner |
| Global: result count | ❌ |
| In-chat: spinner | ✅ in the search bar (`searchLoading`) |
| In-chat: no matches | ✅ "No matches" |
| In-chat: error | ⚠️ Only via the shared `chatError` banner below the message list |
| In-chat: result count | ❌ |
| In-chat: load more | ❌ |

---

## 8. Gap summary

| # | Gap | Severity | Notes |
| --- | --- | --- | --- |
| G1 | Global result tap discards `messageId` — no jump | **High** | `Shell.qml:223`; the data is already in the model |
| G2 | No message highlight after any navigation | **High** | `MessageBubble.qml` lacks the property; Android uses a 2.2 s accent border |
| G3 | No load-window-around-message API | **High** | Blocking for G1 on any message outside the newest page; needs `getMessage` + `getChatHistory` with `from_message_id` |
| G4 | In-chat results never jump (click closes the search) | **High** | `ChatsPage.qml:558` |
| G5 | In-chat search has no paging | **High** | `next_from_message_id` is discarded; only 50 hits reachable |
| G6 | No in-chat filters (sender / type / date) | Medium | Request fields are hardcoded `null` |
| G7 | No global filters (type / chat type / period) | Medium | Request fields hardcoded `null`/`0` |
| G8 | Global search has only messages — no chats, people, public results | Medium | `ContactSearch` helper exists but is unwired; `searchChats`/`searchPublicChats` builders absent |
| G9 | Hit chat titles depend on the loaded chat cache | Medium | Android resolves uncached chats with bounded `GetChat`; Windows shows an empty meta line |
| G10 | Search errors are not shown where the search happens | Medium | Errors land in the Chats-page banner only |
| G11 | No request generation / account dimension in guards | Medium | String comparison only; a late page can survive an account switch |
| G12 | No match highlight or snippet in result rows | Low | Android windows the text around the first match and bolds every occurrence |
| G13 | No result count / total | Low | Android shows total, or loaded count when a date filter hides rows |
| G14 | No forum-topic-scoped search or topic-aware jump | Low | `topic_id: null`, `SearchHit` has no topic |
| G15 | No "Find in Telegram" affordance from the chat list | Low | Windows global search is a separate nav section |
| G16 | Local chat filter ignores `@username` / `t.me/name` forms | Low | `ChatFilterModel` compares raw text against title+preview |
| G17 | `searchGlobal` error path leaves the stale offset | Low | Model cleared, `m_globalOffset` kept; next `loadMoreGlobal` is unreachable anyway because the query is unchanged |

---

## 9. Port plan

Ordered so each phase is independently shippable and testable. All paths are Windows-side.

### Phase A — Result navigation + highlight (closes G1, G2, G3, G4) — **P1**

1. `src/telegram/TelegramRequests.h/.cpp`
   - `getMessageRequest(qint64 chatId, qint64 messageId)`
   - `getChatHistoryAroundRequest(qint64 chatId, qint64 messageId, int limit = 40)`
     (`from_message_id = messageId`, negative `offset = -15`, `only_local = false`)
   - reuse `getChatHistoryRequest` for the plain older-page case.
2. `src/telegram/TelegramRepository.h/.cpp`
   - `void jumpToMessage(qint64 chatId, qint64 messageId)` — open the chat if needed, fetch the
     window, merge into `MessageListModel` (`setMessages` for a replaced window, `prependMessages`
     only when the window is strictly older), then emit `focusMessageRequested(messageId)`.
   - `void loadMessagesAround(qint64 chatId, qint64 messageId)` returning through the same path.
   - `qint64 focusMessageId() const` + `void clearFocusMessage()`, plus
     `signal void focusMessageRequested(qint64 messageId)`.
   - Guard: if `getMessage` returns 404, keep the surrounding page and still emit the focus request
     (mirror `TdConversationActions.window`).
3. `src/ui/AppController.h/.cpp`
   - `Q_INVOKABLE void openSearchResult(qint64 chatId, qint64 messageId)` — one entry point for both
     scopes (the model already carries both ids).
   - `Q_PROPERTY(qint64 focusMessageId … NOTIFY chatStateChanged)` and
     `Q_INVOKABLE void clearFocusMessage()`.
4. QML
   - `qml/pages/ChatsPage.qml`: in-chat delegate `onClicked: BeyGramApp.openSearchResult(chatId,
     messageId)` instead of `page.closeSearch()`; after the model changes, scroll with
     `messageListView.positionViewAtIndex(BeyGramApp.messageIndexForId(id), ListView.Center)`.
   - `qml/Shell.qml`: `onResultActivated: (chatId, messageId) => BeyGramApp.openSearchResult(chatId,
     messageId)`.
   - `qml/components/MessageBubble.qml`: `property bool highlighted: false`; accent 2 px border when
     true (keep `outgoing`/`pending` styling precedence).
   - `qml/pages/ChatsPage.qml`: `highlighted: messageId === BeyGramApp.highlightedMessageId` on the
     `MessageBubble` delegate, plus a `Timer { interval: 2200 }` in the controller/page that clears
     it.
5. New `AppController` property `highlightedMessageId` (or keep it page-local and drive it from the
   `focusMessageRequested` signal — page-local is simpler and avoids one more `chatStateChanged`
   storm).

### Phase B — In-chat paging + filters (closes G5, G6, G14) — **P1/P2**

1. `src/telegram/MessageSearch.h/.cpp`
   - `struct SearchPage { QVector<SearchHit> hits; qint64 nextFromMessageId; int totalCount; }`
   - `SearchPage searchPageFromResponse(const QJsonObject&)`; keep `searchHitsFromResponse` as a
     thin wrapper so the existing tests stay valid.
   - Extend the in-chat builder:
     `searchChatMessagesRequest(chatId, query, fromMessageId, limit, senderId = 0, MessageSearchKind kind = Any, qint64 topicId = 0)`.
2. New `src/telegram/ChatSearchFilter.h/.cpp` (mirror of `ChatSearchFilter.kt`)
   - `senderId`, `kind`, `fromDateSeconds`, `toDateSeconds`, `isActive`, `hasDateRange`,
     `hasValidDateRange`, `matchesDate(date)`, `matches(MessageData, query)`.
   - Free helpers `startOfDayEpochSeconds(QDate, QTimeZone)`,
     `exclusiveEndOfDayEpochSeconds(QDate, QTimeZone)`, `searchBoundaryDate(...)`.
3. `src/telegram/SearchResultModel.h/.cpp`
   - `appendHits(const QVector<SearchHit>&, bool hasMore)`, `nextFromMessageId()`, `totalCount()`,
     `hasMore()`, dedupe by `(chatId, messageId)` (message ids are chat-local).
4. `src/telegram/TelegramRepository`
   - `m_searchOffset`, `m_searchHasMore`, `m_searchTotalCount`, `m_searchFilter`;
     `void setSearchFilter(ChatSearchFilter)`, `void loadMoreSearch()`.
   - Apply `matchesDate` to each page **after** parsing; keep `next_from_message_id` untouched
     (the Android invariant).
   - Suppress `totalCount` in the exposed property when a date range is active.
5. QML
   - `qml/pages/ChatsPage.qml`: filter row (type chips + sender combo populated from the loaded
     senders + from/to date popups), active-filter pills, "Load more" footer in the panel.
   - `qml/components/` new `SearchFilterBar.qml` + `SearchDatePicker.qml`
     (`MonthGrid`/`DayOfWeekRow` based — Qt Quick Controls has no `DatePicker`; decide between a
     `MonthGrid` popup and two `SpinBox` rows before implementing).

### Phase C — Global source parity (closes G7, G8, G9) — **P2**

1. `src/telegram/MessageSearch.h/.cpp`
   - Extend the global builder:
     `searchMessagesRequest(query, offset, limit, GlobalSearchFilter)` with `filter`,
     `chat_type_filter`, `min_date` (from period), `max_date = 0`.
   - New builders: `searchChatsRequest(query, limit)`, `searchChatsOnServerRequest(query, limit)`,
     `searchPublicChatsRequest(query)`, `searchContactsRequest` (already exists in `ContactSearch`).
2. New `src/telegram/GlobalSearchFilter.h/.cpp` (mirror of `GlobalSearchFilter.kt`):
   `SharedMediaKind kind`, `GlobalChatType chatType`, `GlobalSearchPeriod period` with
   `minDate(nowSeconds)`.
3. New models: `GlobalChatResultModel`, `GlobalContactResultModel` — or one
   `GlobalSearchSectionModel` exposing a `section` role, so `GlobalSearchPage.qml` can render four
   `ListView` sections from one object graph.
4. `src/telegram/TelegramRepository`
   - `searchGlobalChats(query, limit)`, `searchGlobalContacts(query, limit)`,
     `searchGlobalPublicChats(query)`, `openPublicChat(chatId)` (join / join-request result).
   - Growing-limit paging: `limit += 20`, cap 50; `hasMore = max(totalCount, count) > limit`.
   - `enrichGlobalHits()`: one `getChat` per distinct uncached chat id, bounded to ~6 in flight,
     failures tolerated, cached in `m_chats` so the next page and the later jump reuse it
     (mirror of `TdLibTelegramRepository.searchGlobalMessages`).
5. QML
   - `qml/pages/GlobalSearchPage.qml`: four sections with headers/counts/per-section states and a
     "Load more" row per source; three chip rows above the message section.
   - `qml/pages/ChatsPage.qml`: "Find in Telegram" row under the chat filter when the query is
     non-blank (`networkSearchAction` equivalent: `parseGlobalSearchQuery` in C++ or a small QML
     normaliser).

### Phase D — Polish (closes G10–G13, G15–G17) — **P3**

- Per-page error surfaces (`searchError`, `globalSearchError` properties) instead of the Chats-page
  banner; offline banner when the session reports `WaitingForNetwork`.
- Match highlight + snippet in result rows: precompute `textHtml` with `<b>` spans and an ellipsis
  window in C++ (`MessageSearch`), so QML stays declarative.
- Result counts (`searchTotalCount`, per-section counts).
- Request generation counter in `TelegramRepository` for both scopes; invalidate on account switch.
- Reset `m_globalOffset` on the global error path.
- Local chat filter: normalise `@name` / `t.me/name` before matching (`ChatFilterModel`).

---

## 10. Tests to add / extend

Qt Test, guiless, in `tests/` and registered in `tests/CMakeLists.txt`. None of these exist or were
run today; this is the proposed set.

| Test file | Cases (mirroring the Android specs) |
| --- | --- |
| `tst_message_search.cpp` (extend) | `searchPageFromResponse` exposes `nextFromMessageId` + `totalCount`; a date-hidden page keeps its cursor; in-chat request carries `sender_id`/`filter`/`topic_id`; global request carries `filter`/`chat_type_filter`/`min_date` |
| `tst_chat_search_filter.cpp` (new) | default filter matches everything; inclusive start / exclusive end; one-sided bounds; combined text+sender+type+date; attachment kinds map to the shared-media tabs; same-day range valid, reversed invalid; time-zone boundaries; `matchesDate` rejects `date <= 0` |
| `tst_global_search_filter.cpp` (new) | `Any` has no lower bound; week/month step back from "now"; lower bound floors at 0; default filter has no restrictions |
| `tst_search_paging.cpp` (new) | `appendDistinctBy` skips overlap, keeps first-page order, drops in-batch duplicates, keeps the same message id in a different chat; query-trim staleness; filter-aware staleness |
| `tst_search_model.cpp` (extend) | `appendHits` dedupe, `hasMore` transitions, `totalCount` suppression with a date filter, `clear()` resets cursor |
| `tst_global_search_model.cpp` (extend) | pair-key dedupe across chats; `setChatTitles` announcement already covered; section role if a unified model is added |
| `tst_global_search_merge.cpp` (new) | chat merge by id, contact merge by user id, generation/query staleness |
| `tst_global_search_discovery.cpp` (new) | `@name`, plain name, `t.me/name` with scheme/www/trailing slash, `?single` suffix, message link keeps the raw query, blank query has no action |
| `tst_search_navigation.cpp` (new, repository-level) | jump request JSON (`getMessage`, `getChatHistory` with negative offset), 404 target keeps the window, focus id cleared after handling |

UI-level checks are out of scope here (no GUI/build run in this audit).

---

## 11. Priority and sequencing

| Phase | Closes | Priority | Why |
| --- | --- | --- | --- |
| A — jump + highlight | G1–G4 | **P1** | Search is currently read-only in practice: a result cannot be inspected, so both scopes are half-features |
| B — in-chat paging + filters | G5, G6, G14 | **P1/P2** | Without paging only the newest 50 hits exist; filters are the main discovery tool for old conversations |
| C — global source parity | G7–G9 | **P2** | Chat/people/public discovery is a separate feature surface; message search already works |
| D — polish | G10–G13, G15–G17 | **P3** | Quality-of-life; no blocked workflow |

Sequencing constraints:

- A must land before B's "load more" is useful (both need `next_from_message_id` plumbing, but A is
  the visible payoff and B depends on the same response parsing change — implement
  `searchPageFromResponse` once in A).
- C's `enrichGlobalHits()` also improves A for global hits (chat title + avatar available on jump).
- B's `ChatSearchFilter` and C's `GlobalSearchFilter` are independent and can be parallelised.

Risks / decision points:

- **Window replacement semantics.** `openChat` currently replaces the message model. A jump into a
  window must not silently discard a history the user can still scroll to; decide whether
  `loadMessagesAround` sets a fresh window (with `hasMoreHistory` recomputed) or merges into the
  existing list. Android replaces via `loadMessagesAround` and keeps read-mark logic separate.
- **Highlight lifetime.** 2.2 s matches Android; a page-level `Timer` is the cheapest QML shape, but
  it must not re-arm on unrelated `chatStateChanged` emissions.
- **Date picker.** Qt Quick Controls has no `DatePicker`; pick `MonthGrid` vs. numeric fields before
  Phase B starts, because the filter popup shape drives the QML layout.
- **Topic-aware search.** Adding `topic_id` to `SearchHit` is cheap but the Windows chat UI has no
  forum-topic model yet; keep it as a field that is simply carried until forums land.
