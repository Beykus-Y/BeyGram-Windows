# 02 — Chat list: folders, archive, pins, mute, unread and ordering

Parity audit of the Android chat list surface against the Windows (Qt/QML)
implementation. Read-only audit: no code was changed, nothing was built, no
test or application run was performed. Every claim below comes from reading the
listed sources.

Companion documents: [../PORTING_ROADMAP.md](../PORTING_ROADMAP.md) (Phase 2),
[../PORTING_AUDIT.md](../PORTING_AUDIT.md).

---

## 1. Android files reviewed

### Domain model

| File | What it defines |
|---|---|
| `app/src/main/java/com/beykus/beygram/domain/model/ChatPreview.kt` | Row model plus `orderedMainChats`, `orderedArchivedChats` |
| `app/src/main/java/com/beykus/beygram/domain/model/TelegramChatFolders.kt` | `FolderChatPosition`, `TelegramChatFolder`, `TelegramChatFolders`, `TelegramFolderDraft`, `TelegramFolderLoadState`, `orderedTelegramFolderTabs`, `validTelegramFolderSelection`, `orderedTelegramFolderChats` |
| `app/src/main/java/com/beykus/beygram/domain/model/ArchiveSettings.kt` | Archive screen switches (`returnOnNewMessage`, `archiveSound`) |
| `app/src/main/java/com/beykus/beygram/domain/repository/TelegramRepository.kt` (lines 56–82) | Chat-list API surface: `chats`, `archivedChats`, `chatFolders`, `folderChats`, `folderLoadStates`, `loadMoreFolderChats`, `setFolderChatPinned`, folder CRUD |

### data/tdlib

| File | What it does |
|---|---|
| `data/tdlib/TdLibTelegramRepository.kt` | Chat cache, publisher, every chat-list update handler, all chat-list actions (see line references in §2) |
| `data/tdlib/ChatMuteState.kt` | Mute resolution: per-chat setting vs. per-scope default (`useDefaultMuteFor`) |
| `data/tdlib/TdChatFolders.kt` | `ChatFolderInfo.name`/`ChatFolderName` compatibility adapter, `toFolderOverview` |
| `data/tdlib/CoalescedPublisher.kt` | Coalesced chat-list publication |
| `data/tdlib/TdRequests.kt` | `markChatUnreadRequest`, `markChatReadToggleRequest`, `TdRequestError` (404 handling) |
| `data/tdlib/TdNotifications.kt` | Archived-chat notification routing (reads `archiveOrder` from the cache) |

### UI

| File | What it does |
|---|---|
| `ui/screens/chats/ChatsScreen.kt` | Folder chip strip, pinned section, archive row, empty states, unread badge/dot, muted icon, swipe actions, mark-all-read |
| `ui/screens/chats/ChatsScreenState.kt` | Selected folder, query, per-folder scroll position, saver |
| `ui/screens/chats/SwipeChatRow.kt` | Reveal/dismiss gesture, `ChatSwipeAction` |
| `ui/screens/chats/ChatActionsSheet.kt` | Pin/unpin, mute/unmute, copy title, mark unread, delete |
| `ui/screens/chats/FolderManagerScreen.kt` | Create/edit/reorder/delete server folders |
| `ui/screens/chats/AdaptiveChatsLayout.kt` | Phone vs. two-pane list hosting |
| `ui/screens/archive/ArchiveScreen.kt` | Archive list, unarchive/read swipes, archive settings |
| `BeyGramApp.kt` (lines 1697–1726) | Route wiring for the chats tab and the archive route |

### Tests (Android, source of expected values)

- `app/src/test/java/com/beykus/beygram/domain/model/TelegramChatFoldersTest.kt`
  — Main tab at TDLib `mainChatListPosition`; deleted folder invalidates
  selection; folder shows chats absent from Main; folder pinned order is
  independent; `order == 0` means "not a member"; Main excludes folder-only chats.
- `app/src/test/java/com/beykus/beygram/domain/model/TelegramFolderEditorTest.kt`
- `app/src/test/java/com/beykus/beygram/data/tdlib/TdChatFoldersNameTest.kt`

---

## 2. Exact Android behavior

### 2.1 Row model and ordering

`ChatPreview` carries four independent ordering inputs
(`ChatPreview.kt:35–49`):

- `order` — TDLib `ChatListMain` position; `0` means "not in Main".
- `archiveOrder` — TDLib `ChatListArchive` position; `0` means "not archived".
- `isPinned` — Main-list pinned flag only.
- `folderPositions: Map<Int, FolderChatPosition>` — one `(order, pinned)` pair
  per server folder the chat belongs to; `0` order entries are dropped when the
  map is built, so membership is exactly "key present".

Ordering rules:

```kotlin
// ChatPreview.kt:52
orderedMainChats = chats.filter { order > 0 }
    .sortedWith(pinned desc, order desc, id desc)

// ChatPreview.kt:57
orderedArchivedChats = chats.filter { archiveOrder > 0 }
    .sortedWith(archiveOrder desc, id desc)   // archive has no pinned section

// TelegramChatFolders.kt:101
orderedTelegramFolderChats(chats, folderId) =
    chats.mapNotNull { chat ->
        val p = chat.folderPositions[folderId] ?: return null
        if (p.order <= 0) return null
        chat.copy(isPinned = p.pinned, order = p.order)   // folder-local pin/order
    }.sortedWith(pinned desc, order desc, id desc)
```

Consequences that matter for parity:

- Pins are a **per-list** property. A chat pinned in folder 7 is not pinned in
  Main, and a folder's pinned order never touches Main.
- A folder can list chats that are **not** in Main (included-only members) and
  can omit chats that are in Main.
- Archived chats are never in `chats`; Main and Archive are disjoint flows.

### 2.2 Folder tabs and folder metadata

- `updateChatFolders` is authoritative (`TdLibTelegramRepository.kt:3026–3060`):
  - folders are deduplicated by id, mapped through `toFolderOverview`
    (`TdChatFolders.kt:26`), title from `ChatFolderName.text.text`, blank title
    falls back to `"📁"`, icon from `icon.name`;
  - removed folder ids are dropped from `folderUnreadCount`, `folderPageLocks`,
    `_folderLoadStates`, **and** from every cached chat's `folderPositions` so
    stale folder-only chats cannot linger;
  - `_chatFolders` keeps `mainChatListPosition` from TDLib;
  - a `LoadChats(ChatListFolder, 100)` page is started for every newly appeared
    folder.
- Tab order is TDLib's, with Main inserted at `mainChatListPosition`
  (`TelegramChatFolders.kt:86–91`): `folders` order is the server order, Main is
  not hard-wired first.
- `initialized == false` means "folders not received yet". The chip strip is not
  drawn at all in that state (`ChatsScreen.kt:514`), so an uninitialized account
  is never shown as "zero folders".
- Selection is by server id, never by index or local category name
  (`ChatsScreen.kt:250–256`); a selection that no longer exists is reset to 0
  (`validTelegramFolderSelection`, `TelegramChatFolders.kt:94`).
- `ChatsScreenState` remembers the scroll position **per folder**
  (`ChatsScreenState.kt:59–71`) and restores it after rotation/process death.
- Each folder chip shows its own unread badge from
  `TelegramChatFolder.unreadChats`; the Main chip passes `null` (no badge).

### 2.3 Folder content loading

- Main and Archive are fully paginated at startup
  (`TdLibTelegramRepository.kt:2981–3094`): `LoadChats(list, 100)` is re-issued
  on every `Ok` and stops on 404 ("list fully loaded").
- Folder content is **not** derived from Main: `_folderChats` is built from
  `chatCache` entries that have a position for that folder
  (`TdLibTelegramRepository.kt:1818–1825`), and the first page is fetched with
  `LoadChats(ChatListFolder(folderId), 100)`.
- `loadMoreFolderChats` (line 668) is per-folder mutex-guarded, keeps
  `TelegramFolderLoadState(loading, hasMore, error)`, treats 404 as
  `hasMore = false`, and ignores results for a folder that was removed while the
  page was in flight (line 696).

### 2.4 Archive

- Archive is a first-class chat list: `LoadChats(ChatListArchive)` at startup,
  `_archivedChats` published alongside `_chats`.
- Entry point is a navigation row above the pinned section
  (`ChatsScreen.kt:576–577`), shown only when all of these hold: Main list
  selected, `archivedChats` non-empty, filter `All`, query blank
  (`ChatsScreen.kt:321`). The row shows the chat count and the summed unread
  count (`ChatsScreen.kt:877–892`).
- The row can be hidden (`chats_archive_hide`); while hidden, pulling down at the
  top of the list past a 104 dp threshold re-reveals it (`ChatsScreen.kt:344–382`,
  `archivePullThreshold`).
- `ArchiveScreen` lists archived chats in TDLib archive order with **no** pinned
  section, swipe-to-mark-read and swipe-to-unarchive, plus two account-level
  switches (`ArchiveScreen.kt:57–136`).
- Archiving is `AddChatToList(chatId, ChatListArchive())`, unarchiving
  `AddChatToList(chatId, ChatListMain())` (`TdLibTelegramRepository.kt:521–524`).
- Automatic return from the archive: a non-outgoing message in an archived chat,
  with the chat's mute state *known* and *not muted*, and the account setting
  `returnOnNewMessage` on, unarchives the chat
  (`TdLibTelegramRepository.kt:3206–3216`). Muted archived chats stay archived,
  and a chat whose mute state is not yet known stays archived until it is
  (`ChatMuteState.isKnown`).
- The setting itself is read/written through
  `GetArchiveChatListSettings` / `SetArchiveChatListSettings.keepUnmutedChatsArchived`
  (inverted), `TdLibTelegramRepository.kt:1013–1021`, and is refreshed on
  opening the archive screen (`BeyGramApp.kt:1703–1709`).

### 2.5 Pins

- Main-list pin toggle: `ToggleChatIsPinned(ChatListMain(), chatId, pinned)`
  (`TdLibTelegramRepository.kt:702–705`).
- Folder pin toggle: `ToggleChatIsPinned(ChatListFolder(folderId), chatId, pinned)`
  (line 527), guarded by "folder still exists".
- UI: pinned chats are extracted into a section with a "Pinned" header, then a
  quiet separator, then the remaining chats (`ChatsScreen.kt:316–319`,
  `579–643`). This grouping only applies to the Main list with a blank query.
- Archive has no pinned grouping at all.

### 2.6 Unread counters

- Per-chat count comes from `TdApi.Chat.unreadCount` at `UpdateNewChat` time and
  is replaced by `UpdateChatReadInbox.unreadCount`
  (`TdLibTelegramRepository.kt:2553–2557`, `3325`).
- A chat is shown as unread when `unreadCount > 0 || isMarkedAsUnread`
  (`ChatsScreen.kt:1254`).
- Manual "mark as unread" (`UpdateChatIsMarkedAsUnread`) is tracked separately
  (line 2558) so a chat with zero messages still shows a marker.
- Rendering (`ChatsScreen.kt:1301–1324`):
  - unread → timestamp in accent, semibold; preview emphasised (unless muted);
  - `unreadCount > 0` → pill with the number, `99+` above 99;
  - `unreadCount == 0 && isMarkedAsUnread` → 10 dp dot;
  - muted chat → the badge/dot uses the secondary (grey) colour.
- Folder badges come from `UpdateUnreadChatCount` for `ChatListFolder`
  (lines 2532–2543), which updates both `folderUnreadCount` and the published
  `TelegramChatFolder.unreadChats`. Main/Archive do not display a total badge.
- Mark-all-read applies to every displayed chat with `unreadCount > 0 ||
  isMarkedAsUnread` (`ChatsScreen.kt:440`, `767`).
- Opening a chat clears both the read watermark and the explicit unread marker
  (`markChatRead`, `TdLibTelegramRepository.kt:1022–1034`).

### 2.7 Muted

- `ChatMuteState` resolves a chat's effective mute state
  (`ChatMuteState.kt:39–57`): if `useDefaultMuteFor` is set, the **scope**
  default (private / group / channel) decides; otherwise the chat's own
  `muteFor > 0`. Scope values are loaded with `GetScopeNotificationSettings`
  for all three scopes at startup (`TdLibTelegramRepository.kt:3011–3023`).
- Mute is applied at publish time (`withMute`, line 3300), so a scope change
  re-publishes only chats that inherit it.
- Per-chat mute writes reuse the existing `ChatNotificationSettings` and set
  `useDefaultMuteFor = false`, `muteFor = Int.MAX_VALUE` or `0`
  (`TdLibTelegramRepository.kt:707–715`).
- UI: muted rows show `NotificationsOff` before the preview
  (`ChatsScreen.kt:1305`), use the grey badge, and do not get the emphasised
  preview.

### 2.8 Live updates that move the list

| Update | Android effect |
|---|---|
| `UpdateNewChat` | cache insert, drafts, read markers, mute state, notification feed, publish |
| `UpdateChatPosition` | Main `order`/`isPinned`, Archive `archiveOrder`, folder map add/remove (`order <= 0` removes) |
| `UpdateChatLastMessage` | preview text/time/id/outgoing/sender/send-state **and** all positions |
| `UpdateChatDraftMessage` | positions are re-applied (drafts change ordering), publish |
| `UpdateChatReadInbox` | `unreadCount` |
| `UpdateChatReadOutbox` | `lastReadOutboxMessageId` (drives "read" tick) |
| `UpdateChatIsMarkedAsUnread` | `isMarkedAsUnread` |
| `UpdateChatNotificationSettings` | chat's own mute settings |
| `UpdateScopeNotificationSettings` | scope defaults, re-publish inheriting chats |
| `UpdateChatTitle` | title (Saved Messages keeps its localized title) |
| `UpdateChatPhoto` | avatar photo |
| `UpdateChatFolders` | tabs, membership cleanup, first folder page |
| `UpdateUnreadChatCount` | folder badge |

Publishing is coalesced (`CoalescedPublisher`, ~`CHAT_LIST_PUBLISH_DELAY_MS`)
and always recomputed from `chatCache`, so the list is a pure function of the
cache — no incremental reordering bugs are possible.

### 2.9 Empty, loading and error states

- Folder still loading, list empty → centred spinner
  (`ChatsScreen.kt:627–629`).
- Folder loaded, no chats → `chat_folder_empty` text (line 630–632).
- Folder page failed / more available → trailing row with spinner, error text,
  and a load-more / retry button (`ChatsScreen.kt:675–690`).
- Main list empty (or query/filter empty) → `EmptyState` with an icon and a
  message chosen by filter and query (`ChatsScreen.kt:1170–1190`).
- Archive empty → `archive_empty` (`ArchiveScreen.kt:96–105`).
- Folders not initialized → chip strip absent rather than empty.

### 2.10 List actions

- Swipe left reveals **Mark read** and the primary action (Archive in Main,
  Unarchive in Archive); a full swipe performs the primary
  (`SwipeChatRow.kt:87–100`, `164–191`). Only one row is open at a time.
- Long press opens `ChatActionsSheet`: pin/unpin, mute/unmute, copy title,
  mark unread (only when the chat is currently read), delete with confirmation.
- Folder management (create/edit/reorder/delete) lives in `FolderManagerScreen`,
  reached from the chip strip (`ChatsScreen.kt:546`), and is blocked while a
  folder draft is open.

---

## 3. Current Windows status

### 3.1 Files and what they actually do

| File | Behavior today |
|---|---|
| `src/telegram/TelegramTypes.h/.cpp` | `ChatData` has `order`, `unreadCount`, `pinned`, `muted`, `inMainList`, `isChannel`, photo fields. `applyChatPosition` (`TelegramTypes.cpp:423–436`) **ignores every list except `chatListMain`**; `chatFromJson` only applies Main positions. No archive order, no per-folder positions, no marked-as-unread flag. |
| `src/telegram/ChatListModel.h/.cpp` | Roles: chatId, title, preview, unreadCount, pinned, muted, lastDate, initials, accentIndex, isChannel, chatType, photo*. `setChats` diffs in place when the id order is unchanged, otherwise resets. No archive/folder roles. |
| `src/telegram/ChatFilterModel.h/.cpp` | Case-insensitive substring match on title **or** preview only. No filter categories, no server folders. |
| `src/telegram/ChatFolders.h/.cpp` | Parses `updateChatFolders` into `ChatFoldersState`: Main **always prepended as row 0**, custom folders sorted by `position`, `has_archive` parsed, `archiveChatListId` hard-coded to 0. Reads `unread_count` from each folder object (a field TDLib's `chatFolderInfo` does not carry). `getChatFoldersRequest` / `getChatFolderRequest` are defined but **never called**. |
| `src/telegram/ChatFolderListModel.h/.cpp` | Renders the folder strip: Main first, dedupe by id, sort by `position` only when the parser order is not ascending. Emits `countChanged` only when the count changes. |
| `src/telegram/FolderChats.h/.cpp` | Request builders for `getChats`/`loadChats` on Main (null list) and `chatListFolder`, plus `chatIdsFromResponse` (dedupe, numeric-string ids, skip 0). `loadChatsForFolderRequest` is **never called**. |
| `src/telegram/ArchiveChats.h/.cpp` | `getArchivedChatsRequest`, `loadArchivedChatsRequest`, `setChatArchivedRequest` (`addChatToList`), `responseIsArchivedChats`. **None of these are called from the repository or the UI.** |
| `src/telegram/TelegramRepository.cpp` | Main list only: `requestChats` (`getChats` null list + `loadChats` null list, lines 138–211); `rebuildChatListNow` filters `inMainList && order != 0` and sorts `order desc`, then `lastMessageDate desc`, then title (lines 220–246); `selectFolder` fetches `getChats(chatListFolder, 100)` once and stores an id set (lines 601–652). |
| `src/ui/AppController.h/.cpp` | Exposes `chatList` (the filter proxy), `chatFolders`, `currentFolderId`, `selectFolder`, `chatFilter`, `chatLoading`, `chatError`, `requestChatPhoto`. No archive, pin, mute, mark-unread or mark-all-read invokables. |
| `qml/pages/ChatsPage.qml` | Folder strip + local search + `ListView` of `ChatRow`; header shows the row count only; one generic empty state; no archive row, no pinned header, no folder loading/load-more state, no context menu. |
| `qml/components/ChatFolderTabs.qml` | Horizontal pills with `folderId`, `title`, `unreadCount`, `isMain`; badge `99+`; no Main-position support, no manage entry, no scroll-to-selected. |
| `qml/components/ChatRow.qml` | Avatar, title, preview, time, numeric unread badge. **`pinned` and `muted` are received but never rendered**; no dot for mark-unread, no send-status icon. |
| `qml/pages/ArchivePage.qml` | Complete archive browser UI, but **not instantiated anywhere** (`Shell.qml` has six sections, none is the archive). |
| `qml/components/ChatNotificationSettings.qml` | Complete per-chat notification dialog, **not instantiated anywhere**. |

### 3.2 Update coverage actually implemented

Handled in `TelegramRepository::processUpdate`: `updateChatFolders`,
`updateNewChat`, `updateChatTitle`, `updateChatPosition` (Main only),
`updateChatLastMessage`, `updateChatReadInbox`, `updateChatNotificationSettings`,
`updateChatAddedToList`/`updateChatRemovedFromList` (Main list only),
`updateNewMessage`, send succeeded/failed, message content/edited/deleted,
`updateChatAction`, `updateUser`.

Not handled at all: `updateChatPhoto`, `updateChatDraftMessage`,
`updateUnreadChatCount`, `updateChatIsMarkedAsUnread`, `updateChatReadOutbox`,
`updateChatUnreadMentionCount`, `updateChatHasScheduledMessages`. The scheduler
already coalesces `updateChatIsMarkedAsUnread` and `updateChatReadOutbox`
(`TelegramUpdateScheduler.cpp:77–99`), so those updates reach the repository and
are then dropped.

---

## 4. Gaps

Severity: **S1** = user-visible wrong data or missing core Telegram behavior;
**S2** = feature missing but list remains usable; **S3** = polish.

| # | Gap | Android evidence | Windows evidence | Sev |
|---|---|---|---|---|
| G1 | No archive at all: archived chats are fetched by nobody, so they never appear and cannot be unarchived | `LoadChats(ChatListArchive)`, `_archivedChats`, `ArchiveScreen` | `ArchiveChats.*` unused; `applyChatPosition` ignores `chatListArchive`; no archive route in `Shell.qml` | S1 |
| G2 | Main list ordering uses `lastMessageDate`/title as tie-breakers and has no explicit pinned grouping | `orderedMainChats`: pinned desc, order desc, id desc | `rebuildChatListNow`: order desc, date desc, title | S1 |
| G3 | Pinned and muted are never drawn in a row; no pin/mute toggle anywhere | `NotificationsOff` icon, pinned section, `ChatActionsSheet` | `ChatRow.qml` ignores `pinned`/`muted`; no invokables in `AppController` | S1 |
| G4 | Manual "mark as unread" is not modelled: no dot, no update handling, no action | `isMarkedAsUnread`, dot at 10 dp, `markChatUnread` | no field in `ChatData`, `updateChatIsMarkedAsUnread` dropped | S1 |
| G5 | Folder badges are always 0: `unread_count` is read from `chatFolderInfo`, which TDLib does not send; `updateUnreadChatCount` is ignored | `UpdateUnreadChatCount` per `ChatListFolder` | `ChatFolders.cpp:84`; no handler | S1 |
| G6 | Folder membership/order is a snapshot taken when the folder is selected; folder positions and folder pins are not modelled, so reorders and pin changes while a folder is open never reach the UI | `folderPositions`, `handleChatPosition` folder branch | `m_folderChatIds` id set; `applyChatPosition` returns early for `chatListFolder` | S1 |
| G7 | Folder content is fetched with `getChats` only (one page, 100 max) and never with `loadChats`; a cold cache can show an empty folder with no error and no retry | `LoadChats(ChatListFolder)` + `loadMoreFolderChats` + load state | `FolderChats.cpp:72` builder unused; `selectFolder` single `getChats` | S1 |
| G8 | Main is hard-wired as the first tab; TDLib `main_chat_list_position` is parsed but unused | `orderedTelegramFolderTabs` | `ChatFolderListModel.cpp:70–74`, `ChatFolders.cpp:130–138` | S2 |
| G9 | Mute semantics ignore `useDefaultMuteFor` and per-scope defaults, so a chat inheriting a muted scope is shown as unmuted (and Windows would notify) | `ChatMuteState` | `TelegramTypes.cpp:385–389`, `TelegramRepository.cpp:1005–1017` | S1 |
| G10 | No per-folder empty state, loading state, load-more or error row; a failed folder load looks like an empty folder | `chat_folder_empty`, folder footer | `ChatsPage.qml:250–275` generic text | S2 |
| G11 | `ArchivePage.qml` and `ChatNotificationSettings.qml` are orphaned components | — | not referenced from `Shell.qml`/`ChatsPage.qml` | S2 |
| G12 | No chat-list actions: pin, mute, archive, mark unread, mark all read, delete; no context menu and no swipe | `ChatActionsSheet`, `SwipeChatRow` | `ChatRow.qml` exposes only `clicked()` | S2 |
| G13 | Avatars do not refresh on `updateChatPhoto`; draft changes do not reorder the list or show a draft preview | `UpdateChatPhoto`, `UpdateChatDraftMessage` | `processUpdate` has no branch for either | S2 |
| G14 | No folder management (create/edit/reorder/delete, shareable folders, rule validation) | `FolderManagerScreen`, `TelegramFolderDraft` | none | S2 |
| G15 | Selection is not validated against a folder disappearing except through `updateChatFolders`; `m_folderChatIds` entries for removed folders are never pruned | `handleChatFolders` removes membership | `TelegramRepository.cpp:899–913` resets `m_currentFolderId` only | S3 |
| G16 | No per-folder scroll restore; switching folders always starts at the top | `ChatsScreenState.folderScrollPositions` | `ChatsPage.qml` single `ListView` | S3 |
| G17 | Read receipts (`lastReadOutboxMessageId`) are not modelled, so no "read" tick in the list | `UpdateChatReadOutbox` | dropped | S3 |
| G18 | Header shows the row count, not the folder/list unread total; folder tab strip has no manage entry and no scroll-to-selected | `unreadTotal`, manage button | `ChatsPage.qml:155–159`, `ChatFolderTabs.qml` | S3 |

Deliberate non-gaps (do **not** port): the local `All / People / Groups / Unread`
chips. Android removed them in favour of server folders and now hard-codes
`selectedFilter = ChatFilter.All` (`ChatsScreen.kt:248`); only the empty-state
wording still references the enum. Windows therefore does not need local filter
chips, but it does need the enum-shaped empty-state messages.

---

## 5. Implementation plan

Ordering is dependency-first. Each step is independently shippable and testable.
No step requires a running TDLib session for its unit tests.

### Step 1 (P0) — Model the three lists properly

**Files:** `src/telegram/TelegramTypes.h/.cpp`, `src/telegram/ChatListModel.h/.cpp`

- Add to `ChatData`: `archiveOrder`, `markedAsUnread`, `lastReadOutboxMessageId`,
  `folderPositions: QHash<qint64, ChatPosition>` where
  `struct ChatPosition { qint64 order; bool pinned; }`.
- Replace the `chatListMain`-only `applyChatPosition` with a list-aware
  `applyChatPositions(ChatData&, const QJsonArray&)` that fills Main, Archive and
  every `chatListFolder` entry, removing folder keys whose order is `0`.
- Add `orderedMainChats`, `orderedArchivedChats`, `orderedFolderChats(folderId)`
  as free functions mirroring the Kotlin ones exactly (pinned desc, order desc,
  id desc; Archive without pinned; folder uses the folder-local pin/order).
- Add roles: `MarkedAsUnreadRole`, `ArchiveOrderRole`, `LastReadOutboxRole` if
  the UI needs them.

**Tests** (`tests/tst_chat_ordering.cpp`, register in `tests/CMakeLists.txt`):
port `TelegramChatFoldersTest.folderShowsChatsNotInMainAndKeepsIndependentPinnedOrder`,
`mainListDoesNotIncludeFolderOnlyChat`, `zeroOrderIsRemovedAndOrdersFollowTdlib`,
plus new cases for pinned-before-unpinned, id tie-break, and Archive ordering.

### Step 2 (P0) — Archive end to end

**Files:** `src/telegram/TelegramRepository.h/.cpp`, `src/telegram/ArchiveChats.*`
(already correct), `src/ui/AppController.h/.cpp`, `qml/Shell.qml`,
`qml/pages/ArchivePage.qml`

- Startup: after the Main `loadChats`, also issue
  `loadArchivedChatsRequest(100)` and re-issue on `Ok` until 404, exactly like
  `loadChatList` in Android.
- Track `m_archivedChatIds` and publish through a second model
  (`ArchiveChatListModel` or a `setMode()` on `ChatListModel`); simplest is a
  second `ChatListModel` instance owned by the repository.
- Handle `updateChatAddedToList`/`updateChatRemovedFromList` for **all** list
  types (`chatListMain`, `chatListArchive`, `chatListFolder`) instead of
  returning early on non-Main.
- `AppController`: `Q_PROPERTY(QAbstractItemModel* archivedChats ...)`,
  `Q_INVOKABLE void archiveChat(qint64, bool)`, `archiveVisible`,
  `archiveUnreadCount`.
- QML: instantiate `ArchivePage` as a real route (Shell section or an overlay
  inside `ChatsPage`), add the archive navigation row above the pinned section,
  and show the row only under the same conditions as Android.
- Optional but cheap: the "return on new message" setting
  (`GetArchiveChatListSettings` / `SetArchiveChatListSettings`).

**Tests** (`tests/tst_repository_archive.cpp`): request shapes for
`getChats`/`loadChats` on `chatListArchive` (already covered by
`tst_archive_chats`), plus new coverage for `addChatToList` round trip,
`chatListArchive` position parsing into `archiveOrder`, and Main/Archive
disjointness after an add/remove pair.

### Step 3 (P0) — Unread, marked-unread and mute correctness

**Files:** `src/telegram/TelegramRepository.cpp`, `src/telegram/ChatMuteState.*`
(new), `src/telegram/ChatListModel.cpp`, `qml/components/ChatRow.qml`

- New `ChatMuteState` (port of `data/tdlib/ChatMuteState.kt`): per-chat
  `(scope, useDefaultMuteFor, muteFor)` plus per-scope defaults; `isMuted`,
  `isKnown`, `updateScope` returning the affected chat ids.
- Load the three scopes once after authorization with
  `getScopeNotificationSettings` and handle `updateScopeNotificationSettings`.
- Handle `updateChatIsMarkedAsUnread`, `updateChatReadOutbox`,
  `updateChatUnreadMentionCount`; apply `muted` from `ChatMuteState` at publish
  time rather than from `mute_for` alone.
- `ChatRow.qml`: render `NotificationsOff` for muted, a 10 dp dot when
  `unreadCount == 0 && markedAsUnread`, grey badge when muted, and accent
  timestamp when unread.
- Add `markChatUnread` / `markChatRead` invokables
  (`toggleChatIsMarkedAsUnread`, `viewMessages`) so the action exists even
  before the context menu lands.

**Tests** (`tests/tst_chat_mute_state.cpp`): scope inheritance, unknown state,
scope change republish list; (`tests/tst_chat_models.cpp`) add marked-unread dot
role and muted badge colour inputs.

### Step 4 (P0) — Folder tabs as server lists

**Files:** `src/telegram/ChatFolders.h/.cpp`, `src/telegram/ChatFolderListModel.*`,
`src/telegram/FolderChats.*`, `src/telegram/TelegramRepository.*`,
`qml/components/ChatFolderTabs.qml`

- Call `getChatFoldersRequest()` after authorization (the builder exists) rather
  than relying on a push only.
- Store `mainChatListPosition` in `ChatFoldersState` and honour it in
  `ChatFolderListModel` (insert Main at the clamped position instead of row 0).
- Drop `unread_count` from the folder object parse; add `updateUnreadChatCount`
  handling for `chatListFolder` and republish folder badges.
- Keep a per-folder `QSet<qint64>` **plus** order/pinned via `folderPositions`
  (Step 1), so `orderedFolderChats(folderId)` replaces the membership-set filter.
- Add `loadChatsForFolderRequest` paging with a per-folder
  `FolderLoadState { loading, hasMore, error }`, mutex-guarded, 404 → no more.
- Prune `m_folderChatIds` / load states / unread counts for removed folders in
  the `updateChatFolders` handler.

**Tests:** extend `tests/tst_chat_folders.cpp` (main position, no
`unread_count` dependence), `tests/tst_chat_folder_model.cpp` (Main inserted at
`mainChatListPosition`, removal prunes membership), new
`tests/tst_folder_load_state.cpp` (paging, 404, error, removed-folder guard).

### Step 5 (P1) — List actions and empty/loading states

**Files:** `qml/pages/ChatsPage.qml`, `qml/components/ChatRow.qml`,
`qml/components/ChatFolderTabs.qml`, new `qml/components/ChatActionsMenu.qml`,
`src/ui/AppController.*`

- Invokables: `setChatPinned(chatId, pinned)` →
  `toggleChatIsPinned(chatListMain|chatListFolder)`,
  `setChatMuted(chatId, muted)` → `getChat` then `setChatNotificationSettings`
  (`useDefaultMuteFor = false`, `muteFor = INT_MAX | 0`),
  `archiveChat`, `markChatUnread`, `markAllRead`, `deleteChat`.
- Context menu on the row (right-click on desktop; keep the mouse path, skip the
  touch swipe gesture unless it is wanted on touch laptops).
- Pinned section header + separator; pinned chats are only grouped in the Main
  list with an empty query, matching Android.
- Per-folder empty text, spinner while the first page loads, and a trailing
  "load more / retry" row bound to `FolderLoadState`.
- Header unread total for the visible list.

**Tests:** QML-level state tests for the empty/loading/error branches
(`tests/tst_chat_models.cpp` already covers the model side), plus request-shape
tests for the new invokables in `tests/tst_telegram_requests.cpp`.

### Step 6 (P2) — Folder management and remaining polish

- Folder CRUD: `getFolderDraft`-equivalent (`getChatFolder`), `saveFolderDraft`
  with a baseline fingerprint (TDLib has no optimistic-concurrency field, so the
  fingerprint must be recomputed from the folder's rule fields, as Android does
  in `folderStamp()`), `deleteChatFolder`, `reorderChatFolders`.
- Per-folder scroll restore in `ChatsPage.qml`.
- `updateChatPhoto` and `updateChatDraftMessage` handling.
- `ChatNotificationSettings.qml` wired into the chat actions menu.

---

## 6. Test plan summary

| Layer | New/changed file | Covers |
|---|---|---|
| Ordering pure functions | `tests/tst_chat_ordering.cpp` | Main/Archive/folder ordering, pinned independence, id tie-break, `order == 0` |
| Folder metadata | `tests/tst_chat_folders.cpp` | Main position, folder removal, archive flag |
| Folder model | `tests/tst_chat_folder_model.cpp` | Main insertion position, pruning, no-op updates |
| Folder paging | `tests/tst_folder_load_state.cpp` | `loadChats` pages, 404, error, removed-folder guard |
| Mute | `tests/tst_chat_mute_state.cpp` | scope inheritance, unknown state, republish set |
| Archive | `tests/tst_repository_archive.cpp` | archive list load, add/remove to list, disjointness |
| Row roles | `tests/tst_chat_models.cpp` | marked-unread dot, muted badge, archive roles |

Existing tests already cover the request builders and must keep passing:
`tests/tst_archive_chats.cpp` (8 cases), `tests/tst_folder_chats.cpp` (9 cases),
`tests/tst_chat_folders.cpp` (7 cases), `tests/tst_chat_folder_model.cpp`
(7 cases), `tests/tst_chat_models.cpp` (8 cases).

Register every new suite with `beygram_add_test(...)` in
`tests/CMakeLists.txt`; add new sources to `src/telegram/CMakeLists.txt`.

---

## 7. Priority

| Priority | Steps | Why |
|---|---|---|
| **P0** | 1, 2, 3, 4 | Archive is entirely absent, pins and mute are invisible, folder badges are always zero and folder content is a one-shot snapshot. These are wrong-data problems, not missing polish, and every one of them is user-visible on the first real account. |
| **P1** | 5 | Actions and empty/loading states. The list is readable without them, but a chat client that cannot pin, mute, archive or mark unread is not usable for daily driving. |
| **P2** | 6 | Folder CRUD, scroll restore, photo/draft refresh, notification dialog wiring. |

Suggested sequencing rationale: Step 1 unblocks 2, 3 and 4 (all three need
archive order, marked-unread and folder positions in the model). Step 4 depends
on Step 1 but not on Step 2. Step 3's mute state is also a prerequisite for the
archive "return on new message" rule, so it must land before that rule is
enabled — until then, keep archived chats archived unconditionally rather than
guessing the mute state.

---

## 8. Open questions for the owner

1. **Swipe vs. context menu.** Android's primary affordance is a swipe. On a
   desktop list, a right-click menu plus an inline hover button set is the
   conventional equivalent. Which is wanted, or both?
2. **Archive route.** Should the archive be a sidebar section, a modal overlay,
   or a mode of the chats page? `ArchivePage.qml` currently assumes a full page
   with a Back button.
3. **Folder management scope.** Full rule editing (contacts/groups/channels,
   exclusions, shareable folders) is a sizeable surface. Ship read-only folder
   tabs first and defer editing, or block on the editor?
4. **Pinned section in folders.** Android only groups pins in the Main list.
   Confirm the same for Windows (the folder order still puts pinned chats first
   because TDLib's folder order does).
