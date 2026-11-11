# 03 — Message history, paging, merge, revisions and deleted messages

Read-only parity audit. Android (Kotlin/Compose, the source of truth) vs. the
Windows port (`src/telegram`, `qml/pages/ChatsPage.qml`).

No build, CMake configure, GUI run, instrumentation test or `git` command was
executed for this document. Every statement below is derived from reading
source; nothing here is a claim that the Windows side builds, renders or
passes tests.

Companion documents: [../conversation-architecture.md](../conversation-architecture.md)
(Android contract prose), [../PORTING_AUDIT.md](../PORTING_AUDIT.md) §2.2,
[../PORTING_ROADMAP.md](../PORTING_ROADMAP.md) Phase 2 item 3.

---

## 1. Android files reviewed

| File | What it defines for this slice |
|---|---|
| `app/src/main/java/com/beykus/beygram/data/tdlib/MessageHistoryStore.kt` | The whole history state machine: per-chat `ChatHistory`, window epochs, revision counter, changed-message log, retired (deleted) ids, outside-window messages, LRU eviction, merge/sort. |
| `app/src/main/java/com/beykus/beygram/domain/model/ChatMessage.kt` | `ChatMessage`, `ChatHistory` (`messages`, `isLoading`, `hasOlderMessages`, `hasNewerMessages`, `error`, `lastReadInboxMessageId`, `lastReadOutboxMessageId`, `liveArrivals`, `liveArrivalSequence`, `recentReplacements`, `outsideWindowMessages`), `LiveMessageArrival`, `MessageIdReplacement`, `MessageContext`. |
| `app/src/main/java/com/beykus/beygram/domain/repository/ConversationRepository.kt` | UI port: `observeChatHistory`, `openChat`/`closeChat`, `loadOlderMessages`, `loadMessagesAround`, `loadLatestMessages`, `newerMessages`, `observeDeletedMessages`, `messagesVisible`, forum-topic equivalents, `messageContext`. |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdLibTelegramRepository.kt` | Paging driver (`requestHistory`, 3×50 pages), jump windows, forum-topic store, read markers, every update handler that mutates history. |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdConversationActions.kt` | `window()` (offset `-15`, limit `40`, plus `getMessage` with 404 tolerance), `newerMessages()` (offset `1`, limit `50`), `context()`, `hydrateReply()`. |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdConversationEffects.kt` | Read-receipt gateway: dedup per chat session, `force_read=false`, policy/foreground/opened-chat gate, bounded retries. |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ChatScreen.kt` | List position restore, viewport-based receipts (350 ms debounce), prefetch trigger, unread opening, jump/highlight, `jumpToFirstUnread`, focus-from-outside. |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ChatUnread.kt` | Unread separator placement, `needsUnreadWindow`, `unreadOpenStep`. |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/HistoryAutoLoad.kt` | `shouldAutoLoadOlderMessages` with `HISTORY_PREFETCH_DISTANCE = 6`. |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ChatScrollPositionViewModel.kt` | Per-chat/per-topic visit state: index+offset, `readBoundaryId`, `readBoundaryChecked`, `readBoundaryWindowRequested`, `focusClaimed`, `seenNewestId`, `visitContinues`, `endVisit()`. |
| `app/src/test/java/com/beykus/beygram/data/tdlib/MessageHistoryStoreTest.kt` | 26 executable expectations for the invariants in §2. |
| `app/src/test/java/com/beykus/beygram/ui/screens/chat/HistoryAutoLoadTest.kt`, `ChatUnreadTest.kt`, `ChatScrollPositionViewModelTest.kt` | Prefetch / unread-opening / visit-reset expectations. |
| `docs/conversation-architecture.md` | Written contract: read receipts, replies/edit/delete, revision protection, live arrivals. |

---

## 2. Android behavior contract

### 2.1 Paging windows

- Opening a chat calls `openChat` → `restartFromLatest(chatId)` (drops any jump
  window, keeps unsent messages, sets `isLoading`, clears the changed log,
  bumps the window epoch) and then `requestHistory(fromMessageId = 0,
  remaining = 50, pagesLeft = 3)`.
- Each TDLib page is `getChatHistory(chatId, from, offset 0, limit 50,
  onlyLocal=false)`. The request repeats while the page is full, the cursor
  advances, fewer than 50 rows remain in the budget, and pages are left — so
  the initial load is up to 150 messages in at most 3 requests.
- The cursor row itself is filtered out (`filterNot { it.id == fromMessageId }`)
  and the next cursor is the last row of the page, not the requested cursor.
- Older paging: `loadOlderMessages` takes `messages.first().id` (the oldest
  loaded id) and runs the same 50×3 loop. `beginLoad` returns `null` while a
  load is running, so duplicate requests are structurally impossible.
- `hasOlderMessages` is set from page fullness, not from a count. An empty page
  or a non-advancing cursor ends paging.
- Forum topics use a second, independent `MessageHistoryStore` per
  `(chatId, forumTopicId)` with `getForumTopicHistory` and the same
  epoch/version rules.
- The UI prefetches when the last visible row is within 6 rows of the loaded
  edge, and only after the opening position is settled, not while loading, and
  not after an error (retry is explicit).

### 2.2 Duplicate and merge rules

- One identity per message id. `merge()` is
  `(current + incoming).filterNot { retired }.associateBy { id }.values`
  sorted by `dateSeconds`, then `sendState == Sending` last, then `id`.
  Later input wins on id collision, so a live copy beats a page copy.
- `put(message, replaces)`:
  - retires `replaces` when it differs from the new id (temporary → server);
  - refuses to insert a confirmed incoming message while
    `hasNewerMessages` (a jump window must not gain a hole);
  - records a `LiveMessageArrival` only when the id is genuinely new and no
    visible temporary row was replaced;
  - keeps `recentReplacements` (bounded to 64) for the composer launch.
- `finishLoad(..., requestVersion, epoch)`:
  - drops the page entirely if `epoch` is stale (window replaced meanwhile);
  - for each incoming row, if the row was changed after `requestVersion`
    (`changed[id] > requestVersion`), keeps the in-window copy or the kept
    outside-window copy instead of the older page copy.
- `update(chatId, id, transform)` marks the id as changed while a load is in
  flight and patches both `messages` and `outsideWindowMessages`.
- Deletion is permanent and monotonic: `retiredIds` filters every future merge,
  so a late page cannot resurrect a deleted row.

### 2.3 Edits and content updates

- `UpdateMessageContent` rebuilds text/HTML/entities/media/link preview from
  `newContent` and patches the row in place (`update`), without changing its
  position, id or read state.
- `UpdateMessageEdited` sets `isEdited = editDate > 0` and refreshes the inline
  keyboard.
- `UpdateMessageInteractionInfo` / `UpdateMessageReactions` patch reactions and
  `replyCount`.
- `UpdateMessageIsPinned` patches `isPinned`.
- All of the above also apply to the forum-topic store and to kept
  outside-window rows.
- Because the patch path is `update()`, an in-flight page older than the edit
  cannot roll the row back (§2.2).

### 2.4 Deleted messages

- `UpdateDeleteMessages` with `isPermanent`:
  - `messageHistory.delete()` retires the ids and removes them from both
    `messages` and `outsideWindowMessages`;
  - forum-topic stores for that chat are deleted too;
  - `observeDeletedMessages(chatId)` publishes the ids so thread screens can
    drop rows that a window no longer contains;
  - pins are invalidated.
- Non-permanent deletes (local delete of a scheduled/failed message) do not
  produce tombstones.
- `retiredIds` is trimmed to 256 per chat, oldest first, and trimming is
  deferred while a load is in flight so the running page cannot resurrect a
  just-deleted row.
- A read watermark can outlive its message: `window()` tolerates a 404 from
  `getMessage` and returns the surrounding page, so an unread-boundary jump to
  a deleted message still lands somewhere valid.

### 2.5 Read markers

- Inbox/outbox markers live in `ChatHistory` and are raised monotonically with
  `maxOf`, from the initial `Chat`/topic and from `UpdateChatReadInbox` /
  `UpdateChatReadOutbox`.
- Receipts are sent only for rows that are at least half visible (or 64 dp,
  whichever is smaller), stable for 350 ms, while the lifecycle is RESUMED and
  the window focused, excluding pending/failed rows, headers and the separator.
- `TdConversationEffects.visible()` dedups per chat session, takes at most 100
  ids, uses `force_read=false`, retries 3× with backoff, and re-checks
  authorization/foreground/opened-chat/policy immediately before each send.
- Read marks are held until the opening position is decided
  (`readBoundaryChecked`), so the boundary cannot move under the separator.
  Loading history, hydrating a reply or opening a profile never sends receipts.
- Unread opening: if the read boundary is loaded, scroll to the separator
  (`UNREAD_SEPARATOR_VIEWPORT_FRACTION = 0.7` from the bottom); if not, request
  the boundary window once; if the window holds no unread incoming row (all
  outgoing), walk newer pages (`newerMessages`, offset `1`, 50 at a time) to
  the first unread incoming row and open its window.
- A chat with no read watermark loads older pages until the beginning to find
  the first incoming message.

### 2.6 Scroll position

- Position is `(firstVisibleItemIndex, firstVisibleItemScrollOffset)` in a
  reverse `LazyColumn`, saved in a per-`(account, chat, topic)` ViewModel so a
  detour to chat info restores it. `visitContinues` distinguishes that detour
  from leaving the chat; `endVisit()` resets boundary state, focus claim and
  `seenNewestId`.
- A newly arrived outgoing message or a pending video note scrolls to the
  bottom (index 0) when the user is already within 2 rows of it.
- Opening with unread messages positions at the separator, not at the newest
  message.
- An opening focused from outside (profile media, link) claims the position;
  the unread opening then neither requests a window nor scrolls.

### 2.7 Jump-to-message

- `jumpToMessage(id)`: if the row is loaded, `animateScrollToItem` + highlight
  for 2.2 s; otherwise `loadMessagesAround(id)`, then wait for the row
  (`pendingJumpId`) and scroll. A failed or superseded window releases the
  opening instead of holding receipts forever.
- `loadMessagesAround` is `window()`: `getChatHistory(offset -15, limit 40)` +
  `getMessage` (404 tolerated), deduped by id, oldest first. The result
  **replaces** the window (`replaceWithWindow`), keeping unsent rows, setting
  `hasNewerMessages = !reachesLatest`, and invalidating in-flight loads via the
  epoch.
- Sending from a jump window first calls `loadLatestMessages` so the sent
  message does not create a gap.
- A newer jump or a latest load supersedes an older one via
  `historyRequests[chatId]`; a superseded result returns `false` and is not
  applied.
- Reply/pinned/search targets that are not loaded use `loadMessagesAround`;
  distant reply originals use `messageContext()` (separate window, never
  appended to the main list) to avoid a pagination gap.

### 2.8 Cache and outside-window rows

- At most 16 chats keep messages (LRU). Open chats, chats with a load in flight
  and chats with unsent rows are never evicted; read markers survive eviction.
- Chats with no messages hold no cache slot.
- `outsideWindowMessages` (e.g. older thread comments) receive updates and
  deletions but never enter `messages`, so timeline paging is unaffected.
- `mediaFileIds()` keeps files of cached + open chats referenced, including
  dice animation frames.

---

## 3. Current Windows status

Windows has a working but much smaller model: a single flat
`QVector<MessageData>` behind `MessageListModel`, a `bool`-based paging cursor in
`TelegramRepository`, and no revision, epoch, tombstone or per-chat history
store.

| Area | Windows today | Evidence |
|---|---|---|
| History ownership | One `MessageListModel` for the currently open chat only; no per-chat store, no cache. `openChat` clears the model. | `TelegramRepository.cpp:278-312`, `MessageListModel.cpp:97-114` |
| Paging window | One request per action, `limit 50`, `offset 0`, `only_local=false`; no 50×3 loop. | `TelegramRepository.cpp:31`, `345-355` |
| Cursor | `m_oldestMessageId = firstMessageId()` after a page; `hasMore` ends when a page is shorter than 50 or empty. No next-cursor-from-last-row logic. | `TelegramRepository.cpp:400-406` |
| Duplicate handling | `addOrUpdate` dedupes by id for older pages; `setMessages` (initial/latest page) does **not** dedupe and **resets** the model, discarding any live row received while the page was in flight. | `MessageListModel.cpp:107-124`, `126-144` |
| Merge ordering | Sorted by `id` then `date`; no `dateSeconds`-then-sending ordering, no `associateBy` merge. | `MessageListModel.cpp:292-300` |
| Revision guard | Only a job-id equality check (`jobId != m_historyJob`). No `version()`, no changed-message log, so a late page can roll back an edit applied while it was in flight. | `TelegramRepository.cpp:375-414` |
| Window epoch | None. No jump window, so nothing to invalidate. | — |
| Deleted messages | `updateDeleteMessages` removes ids from the current model only. No tombstones, no retired-id filter, no `observeDeletedMessages`; a late page can re-add a deleted row. Deletes for non-current chats are ignored. | `TelegramRepository.cpp:1111-1120` |
| Edits | `updateMessageContent` rewrites text/HTML/type and optionally `edited`; `updateMessageEdited` only flips the flag. Only for `m_currentChatId`; no entity/media/link-preview rebuild on content update. | `TelegramRepository.cpp:1095-1109`, `MessageListModel.cpp:188-209` |
| Read markers | No inbox/outbox markers in the history model. `updateChatReadInbox` updates only the chat-list unread count. `updateChatReadOutbox` is coalesced but never processed. | `TelegramRepository.cpp:995-1003`, `TelegramUpdateScheduler.cpp:80-92` |
| Receipts | `markCurrentChatRead()` sends `viewMessages` for the last 20 rows on initial load, on every new message and after every send. No visibility, focus, foreground, policy or dedup gate; `m_lastReadMessageId` is only a "newest already sent" guard. | `TelegramRepository.cpp:851-873`, `1056`, `1072` |
| Scroll position | `ListView` with `positionViewAtEnd` on `initialHistoryLoaded` and on `countChanged` when not loading. No per-chat restore, no anchor by message id, no unread separator. | `ChatsPage.qml:505-508`, `889-893` |
| Older paging trigger | Explicit "Load older messages" header button only; no proximity prefetch, no auto-load on scroll. | `ChatsPage.qml:440-458` |
| Jump-to-message | None. Search-result click just closes the panel. Pinned banner activation is a no-op. No `getMessage`, no offset window, no highlight. | `ChatsPage.qml:424`, `559-565` |
| Reply previews | Resolved only from rows already loaded; a reply to an unloaded message keeps an empty preview. No context window. | `MessageListModel.cpp:242-261` |
| Outside-window rows | No concept. Thread/reply-context messages cannot receive later edits/deletes. | `MessageListModel.h:45-76` |
| Parse thread | Only reverses TDLib's newest-first array and maps JSON; no merge, dedupe, revision or ordering decisions. | `TelegramParseWorker.cpp:24-39` |
| Message shape | `MessageData` has no service-text, album id, send-state enum, pinned flag, forum topic id or self-destruct fields. | `TelegramTypes.h:30-59` |

---

## 4. Gap list

Priority: **P0** = correctness (duplication, resurrection, rollback);
**P1** = visible behavior parity; **P2** = depth/edge cases.

| ID | Gap | Android | Windows | Impact | Priority |
|---|---|---|---|---|---|
| H-01 | No revision guard | `version()` + `changed` map + `requestVersion` in `finishLoad` | job id only | A page in flight silently reverts a live edit | P0 |
| H-02 | No deleted-id tombstones | `retiredIds` + deferred trim + merge filter | `removeIds` only | Deleted messages reappear after an older page arrives | P0 |
| H-03 | Initial page resets the model | `finishLoad` merges | `setMessages` reset | Live messages received during the first page are lost | P0 |
| H-04 | No epoch/window replacement | `windowEpochs` + `replaceWithWindow` | none | Prerequisite for jump windows | P0 |
| H-05 | No jump-to-message | `loadMessagesAround`/`window()`/`pendingJumpId` | none | Search, pinned, reply and link targets cannot be opened | P0 |
| H-06 | Single-page fetch | 50×3 loop, cursor from last row | one 50-row request | Slow first paint; short chats need several taps | P1 |
| H-07 | No prefetch | 6-row threshold, gated by opening/loading/error | explicit button | Scrolling stops at the loaded edge | P1 |
| H-08 | No per-chat history cache | 16-chat LRU, open/loading/unsent pinned | clear on open/close | Reopening a chat always refetches; no instant back-navigation | P1 |
| H-09 | No read markers in history | `lastReadInbox/OutboxMessageId` in `ChatHistory` | none | No unread separator, no "first unread" jump, no per-row read state | P1 |
| H-10 | Receipts ignore visibility | viewport + focus + 350 ms debounce + dedup + policy | last 20 rows on load/new/send | Marks read messages the user never saw; sends while unfocused | P1 |
| H-11 | Scroll position not preserved | per-chat ViewModel, index+offset, visit flag | `positionViewAtEnd` | Detours and chat switches lose position; no anchor stability | P1 |
| H-12 | No unread opening | separator at 0.7 viewport, boundary window, newer-page walk | none | Opening a chat with unread messages jumps to the newest | P1 |
| H-13 | Edits do not rebuild content | full content/media/entities/link-preview patch | text/HTML/type only | A media or caption edit can leave stale fields | P1 |
| H-14 | `updateChatReadOutbox` unused | raises outbox marker | coalesced then dropped | No two-check read state on outgoing rows | P2 |
| H-15 | No outside-window store | `outsideWindowMessages` | none | Thread comments cannot receive edits/deletes | P2 |
| H-16 | No deleted-message signal | `observeDeletedMessages` | none | Thread/context surfaces cannot drop deleted rows | P2 |
| H-17 | No context window for distant replies | `messageContext()` | none | Reply targets outside the window stay unresolved | P2 |
| H-18 | Merge ordering differs | date, sending-last, id | id, date | Pending rows can sort above confirmed rows | P2 |
| H-19 | `MessageData` too thin for merge parity | send state, album id, pinned, topic | bools only | Blocks H-18 and album-aware dedupe | P2 |
| H-20 | Album grouping not used by the feed | `groupTelegramMediaAlbums` in the item list | `AlbumGrouper` exists, unused in `ChatsPage.qml` | Albums render as separate bubbles | P2 |

---

## 5. Implementation plan

Suggested order: H-01…H-05 as one correctness slice, then H-06…H-12 as the
paging/reading slice, then the P2 depth items. Each step keeps
`TelegramRepository` the only owner of TDLib state and keeps QML free of
protocol calls.

### Step 1 (P0) — `MessageHistoryStore` in C++

New files:

- `src/telegram/MessageHistoryStore.h` / `.cpp`
- `src/telegram/ChatHistoryData.h` (or extend `TelegramTypes.h`)

API (mirrors the Kotlin class, Qt naming):

```cpp
namespace beygram::telegram {

struct ChatHistoryData {
    QVector<MessageData> messages;
    QVector<MessageData> outsideWindowMessages;
    bool isLoading = false;
    bool hasOlderMessages = true;
    bool hasNewerMessages = false;
    QString error;
    qint64 lastReadInboxMessageId = 0;
    qint64 lastReadOutboxMessageId = 0;
};

class MessageHistoryStore final : public QObject {
    Q_OBJECT
public:
    explicit MessageHistoryStore(int maxCachedChats = 16, QObject* parent = nullptr);

    [[nodiscard]] quint64 version() const;
    void opened(qint64 chatId);
    void closed(qint64 chatId);
    void put(const MessageData& message, qint64 replaces = 0);
    void replaceWithWindow(qint64 chatId, const QVector<MessageData>& messages, bool reachesLatest);
    [[nodiscard]] quint64 restartFromLatest(qint64 chatId);
    [[nodiscard]] std::optional<quint64> beginLoad(qint64 chatId);
    bool finishLoad(qint64 chatId, const QVector<MessageData>& page, bool hasOlder,
                    bool continueLoading = false,
                    std::optional<quint64> requestVersion = std::nullopt,
                    std::optional<quint64> epoch = std::nullopt);
    void recordOutsideWindow(qint64 chatId, const QVector<MessageData>& messages);
    bool update(qint64 chatId, qint64 messageId,
                const std::function<void(MessageData&)>& transform);
    void readInbox(qint64 chatId, qint64 messageId);
    void readOutbox(qint64 chatId, qint64 messageId);
    void fail(qint64 chatId, const QString& error, bool endLoading = true,
              std::optional<quint64> epoch = std::nullopt);
    void remove(qint64 chatId, const QVector<qint64>& ids);

signals:
    void historyChanged(qint64 chatId);

private:
    // merge(), markChanged(), retire(), trimRetired(), endLoad(), keepCached(), epochOf()
    // constants: MaxRetiredIdsPerChat = 256, MaxCachedChats = 16
};
}
```

Required semantics (copy the Kotlin invariants verbatim):

- `merge()` = concatenate, drop retired ids, last-wins by id, sort by
  `date`, then `pending` last, then id.
- `put()` refuses a confirmed insert while `hasNewerMessages`; sets `replaces`
  retirement before merging.
- `finishLoad()` drops a stale epoch, then per row honours
  `changed[id] > requestVersion`.
- `remove()` retires ids and defers trimming while `isLoading`.
- `keepCached()` never evicts open, loading or unsent chats; read markers
  survive eviction.

Repository changes (`TelegramRepository.h/.cpp`):

- Replace `m_oldestMessageId`, `m_hasMoreHistory`, `m_historyLoading`,
  `m_lastReadMessageId` with `MessageHistoryStore m_history` and per-chat
  `QHash<qint64, quint64> m_historyRequests`.
- `openChat` → `m_history.opened()` + `restartFromLatest()` + paged request.
- `closeChat` → `m_history.closed()`.
- `applyHistory` → `m_history.finishLoad(...)`, then push the chat's snapshot to
  `MessageListModel` via a new `applyHistory(const ChatHistoryData&)`.
- New signals: `historyChanged(qint64)`, `deletedMessagesChanged(qint64)`.

Model changes (`MessageListModel.h/.cpp`):

- Add `void applyHistory(const ChatHistoryData&)`: incremental diff by id
  (insert/remove/dataChanged) instead of `beginResetModel`, so scroll position
  survives a page merge. Keep `setMessages` for tests/initial fill only.
- Add `dedupeById()` inside `sortMessages` as a safety net.
- Fix the `addOrUpdate` insertion predicate to use the same comparator as
  `sortMessages`.

Tests: `tests/tst_message_history_store.cpp` (registered in
`tests/CMakeLists.txt`), one slot per Android test:

`successReplacesTemporaryMessageAndLateResponseCannotRestoreIt`,
`overlappingHistoryRetainsLiveMessagesWithoutDuplicates`,
`failedSendUpdatesExistingMessage`,
`deletedMessageCannotReturnFromAnOlderHistoryRequest`,
`jumpWindowReplacesLoadedMessagesButKeepsUnsentOnes`,
`windowThatReachesTheNewestMessagesIsNotMarkedAsGapped`,
`incomingMessageDoesNotJoinAJumpWindow`,
`sentConfirmationReplacesUnsentMessageInsideAJumpWindow`,
`loadStartedBeforeJumpWindowCannotMergeIntoIt`,
`restartFromLatestSupersedesOlderLoadRunningInTheJumpWindow`,
`snapshotRequestedBeforeAnUpdateCannotRollItBack`,
`coldChatsBeyondTheLimitLoseMessagesWhileOpenUnsentAndLoadingChatsKeepTheirs`,
`loadingChatIsNotEvicted`,
`recentlyUsedColdChatsSurviveWhileTheLeastRecentlyUsedGoes`,
`closedChatIsEvictedLastBecauseItWasUsedLast`,
`chatsWithoutMessagesDoNotTakeACacheSlot`,
`evictionKeepsReadMarkers`,
`retiredIdsOfABulkDeleteStayUntilTheRunningLoadEnds`,
`oldestRetiredIdsAreForgottenWhenTheLimitIsReached`,
`failedLoadAlsoEndsTheRetiredIdsWindow`,
`outsideWindowCommentsTakeUpdatesWithoutJoiningTheWindow`,
`olderPageCannotRollBackAnOutsideWindowUpdate`,
`outsideCopyUpdatedWhileAPageIsInFlightIsNotRolledBackByThePage`,
`windowLoadsAndDeletionsLeaveOutsideWindowCommentsConsistent`,
`liveCopyOfAnOutsideWindowCommentRefreshesItsKeptCopy`,
`deletedOutsideWindowCommentCannotBeRecordedAgain`.

### Step 2 (P0) — Paging loop and cursor

`TelegramRepository.cpp`:

```cpp
void requestHistory(qint64 chatId, qint64 fromMessageId, int remaining,
                    int pagesLeft, quint64 epoch);
```

- Page size 50, cursor from the page's last row, cursor row filtered out.
- Continue while `page.size() == 50`, `nextCursor != cursor`,
  `remaining > page.size()`, `pagesLeft > 1`.
- `hasOlder` from page fullness, not from a total.
- `loadOlder()` takes the store's oldest id and `beginLoad()` returns
  `std::nullopt` when a load is already running.
- Keep `historyLoading`/`hasMoreHistory` as QML-facing properties fed from the
  store snapshot.

Tests: `tests/tst_history_paging.cpp` (fake transport):

`initialLoadFetchesThreePagesAndStopsAtShortPage`,
`cursorRowIsNotDuplicatedAcrossPages`,
`duplicateLoadOlderIsIgnoredWhileLoading`,
`emptyPageEndsPaging`,
`hasOlderIsFalseAfterShortPage`.

### Step 3 (P0) — Jump-to-message and window replacement

New request helper in `TelegramRequests.cpp`:

```cpp
QJsonObject getMessageRequest(qint64 chatId, qint64 messageId);
QJsonObject getChatHistoryWindowRequest(qint64 chatId, qint64 fromMessageId,
                                        int offset, int limit);
```

`TelegramRepository`:

```cpp
void loadMessagesAround(qint64 chatId, qint64 messageId); // async, emits result
void loadLatestMessages(qint64 chatId);
void newerMessages(qint64 chatId, qint64 afterMessageId); // offset 1, limit 50
```

- `window` = `getChatHistory(offset -15, limit 40)` + `getMessage`
  (tolerate error code 404), dedupe by id, sort ascending.
- Apply through `replaceWithWindow(..., reachesLatest)` only when
  `m_historyRequests[chatId]` still matches; otherwise discard.
- `leaveJumpWindow()` before every send path so a send never creates a hole.

QML (`qml/pages/ChatsPage.qml`):

- Search-result click → `BeyGramApp.openMessageAt(messageId)` instead of
  `closeSearch()` (currently line 564).
- Pinned banner `onActivated` → the same call (currently a no-op, line 424).
- Highlight the target row for ~2 s (new property on `MessageBubble`).
- Wait for the row via a `pendingJumpId` property, then
  `positionViewAtIndex(index, ListView.Center)`.

Controller: `AppController::openMessageAt(qint64)` in `AppController.h/.cpp`,
forwarding to the repository and exposing `Q_INVOKABLE`.

Tests: extend `tests/tst_history_paging.cpp`:

`jumpWindowReplacesLoadedRowsAndKeepsUnsent`,
`supersededJumpIsDiscarded`,
`jumpToDeletedMessageStillOpensSurroundingWindow`,
`sendFromJumpWindowReturnsToLatest`,
`jumpTargetIsCentredAndHighlighted` (QML test).

### Step 4 (P1) — Read markers, unread separator and receipts

- Add `UpdateChatReadOutbox` handling next to `updateChatReadInbox`
  (`TelegramRepository.cpp:995`): `m_history.readOutbox(...)`, chat-list
  `lastReadOutboxMessageId`.
- New `TelegramRepository::messagesVisible(qint64 chatId, const QVector<qint64>& ids)`
  with dedup per chat session and a bounded retry, replacing
  `markCurrentChatRead()` on the load/new/send paths.
- QML sends visible ids after a 350 ms settle, only when the window is active
  and the list is not searching/thread-open, matching Android's half-visible or
  64 dp rule (computed in C++ from `contentY`, row heights and viewport).
- `ChatsPage.qml`: add an `unreadSeparator` delegate inserted before the first
  incoming row with `id > lastReadInboxMessageId`, and an
  `onOpenAtUnread()` that positions at 0.7 viewport from the bottom.

Tests: `tests/tst_read_markers.cpp`:

`inboxAndOutboxMarkersOnlyGrow`,
`visibleIdsAreDeduplicatedPerChat`,
`pendingAndFailedRowsAreNotReported`,
`receiptsAreSuppressedWhileUnfocused`,
`separatorIsPlacedBeforeFirstUnreadIncoming`,
`noSeparatorWhenBoundaryIsNotLoaded`.

### Step 5 (P1) — Scroll position and prefetch

- New `src/ui/ChatViewState.h/.cpp` (or QML `Settings`/`QSettings` store):
  `contentY`, `anchorMessageId`, `anchorOffset`, `readBoundaryId`,
  `readBoundaryChecked`, `seenNewestId`, `focusClaimed`, keyed by
  `accountId/chatId` (and topic id later).
- Restore by anchor id + offset, not by pixel position, so a page merge does
  not move the viewport.
- Add the prefetch predicate as a small testable C++ helper
  (`shouldAutoLoadOlder(lastVisibleRow, rowCount, hasOlder, isLoading,
  hasError, openingSettled)`, distance 6) and call it from
  `ChatsPage.qml` on `contentY` changes instead of relying on the header
  button. Keep the button as an explicit retry.

Tests: `tests/tst_chat_view_state.cpp`:

`anchorSurvivesPagePrepend`,
`anchorFallsBackToNearestIdWhenRemoved`,
`prefetchTriggersSixRowsBeforeEdge`,
`prefetchWaitsForOpeningAndSkipsErrors`.

### Step 6 (P2) — Outside-window rows, tombstones, context window

- `MessageHistoryStore::recordOutsideWindow` + `remove` + `update` wiring for
  thread comments.
- `TelegramRepository::observeDeletedMessages(chatId)` as a
  `QSet<qint64>`-valued signal/property, populated from permanent
  `updateDeleteMessages`.
- `TelegramRepository::messageContext(chatId, messageId)` for distant reply
  originals; render in a separate panel, never appended to the feed.

Tests: `tst_message_context.cpp` (`contextWindowDoesNotJoinTheTimeline`,
`deletedTombstoneIsPublishedOnce`, `contextToleratesDeletedTarget`).

### Step 7 (P2) — Message shape and album-aware merge

- Extend `MessageData` with `sendState` (`Sent/Sending/Failed`), `groupedId`,
  `isPinned`, `forumTopicId`, `serviceText`, `isChannelPost`, `replyCount`.
- Switch merge ordering to `date → pending-last → id`.
- Feed the list through `groupAlbums()` (already implemented in
  `src/telegram/AlbumGrouper.*`, currently unused) so album members merge and
  paginate as one unit, matching Android's `groupTelegramMediaAlbums`.

Tests: extend `tests/tst_chat_models.cpp`
(`mergeSortsPendingLast`, `duplicateIdsAreDroppedOnSetMessages`) and
`tests/tst_album_grouper.cpp` (`albumSplitAcrossPagesRegroups`).

---

## 6. Suggested commit boundaries

1. `MessageHistoryStore` + `tst_message_history_store` (no behavior change yet;
   repository still writes the model directly).
2. Repository paging loop and store wiring + `tst_history_paging`.
3. Jump-to-message (requests, controller, QML) + highlight.
4. Read markers, receipts and unread separator + `tst_read_markers`.
5. Scroll anchor and prefetch + `tst_chat_view_state`.
6. P2 depth items, one commit per item.

Each commit should keep `TelegramRepository` as the sole TDLib owner, keep QML
free of request construction, and add tests in `tests/CMakeLists.txt` in the
same change. This document makes no statement about whether any of these
builds or passes; verification is a separate, explicit step.
