# Forwarding and selection — Android → Windows parity audit

Status: read-only static audit of the repository at `F:\BeyGram Windows`.
Date: 2026-11-11.
Nothing in this document was produced by building, running, or testing either
client. Every statement below comes from reading the sources named in §1 and §3.

Companion documents: [PORTING_AUDIT.md](../PORTING_AUDIT.md),
[PORTING_ROADMAP.md](../PORTING_ROADMAP.md) (forwarding UI is listed as still
open in Phase 2), [WINDOWS_TDLIB.md](../WINDOWS_TDLIB.md) (pinned TDLib
revision).

Method: the Android client is the functional reference and is read first
(§1–§2). The Windows tree is then read to establish what actually exists (§3).
Only then are gaps and a plan written (§4–§5). Where Android and Windows
disagree, Android behaviour is the specification unless the Windows deviation is
clearly deliberate and documented.

Scope: entering selection mode, selection-set semantics (album groups, anchor,
limit, pruning), the selection action bar, forwarding (single and batch,
destination picker, captions/comments, copy-without-author), copy formatting,
delete confirmation and scope, batch save, and share. Single-message context
actions are covered only where they share code paths with selection.

---

## 1. Android files reviewed

| File | What it defines |
|---|---|
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/MessageSelection.kt` | Pure selection logic: `BatchDeleteRights`, `batchDeleteRights()`, `selectionGroupIds()`, `forwardIdsForSelection()`, `selectedText()` |
| `.../chat/SelectionActionBar.kt` | `SelectionActionBar` (count, forward/copy/save/delete/more/close), `SelectionDeleteDialog`, `DeleteChoice`, `SelectionCheckBadge` |
| `.../chat/ForwardChatDialog.kt` | `ForwardTarget`, full-screen destination picker: multi-target, forum topics, search, comment field, copy mode, retry-only-failed |
| `.../chat/MessageSelectionMedia.kt` | `saveSelectedMediaToDevice()` — per-file MediaStore export used by batch save |
| `.../chat/ChatScreen.kt` | All selection state and orchestration (lines 251–272, 628–637, 771–921, 1480–1491, 1880–1909, 2237–2299, 2665–2760) |
| `.../chat/MessageActions.kt` | `MessageActionsBar` (single-message actions incl. Forward, Forward-without-author, Select), `MessageLinkCopyDialog`, `DeleteConfirmPanel` |
| `.../chat/MessageContextDialog.kt` | Reply-context window; reuses per-message capabilities and copy dialogs (tangential) |
| `.../chat/SelectionMenuProvider.kt` | Compose 1.10 text-selection toolbar override (clipboard menu), not message selection (tangential) |
| `.../chat/MediaViews.kt` | Media viewer `shareMedia()` (system `ACTION_SEND`) and `saveToDevice()` (lines 1674–1700, 1004) |
| `.../profile/SharedMediaSection.kt` | Reuses `ForwardChatDialog` for a single media item, `allowComment = false` (lines 221–241) |
| `domain/model/ForwardedMessages.kt` | `MAX_MESSAGE_BATCH_SIZE = 100`, `ForwardedMessages(forwardedIds, failedIds).isComplete` |
| `domain/repository/ConversationRepository.kt` | `forwardMessage`, `forwardMessages`, `deleteMessages`, `messageCapabilities` contracts (lines 215–239) |
| `data/tdlib/TdConversationActions.kt` | `deleteMessages()` (per-message scope check, one request), `forwardMessages()` (sorted ids, 100 cap, per-message `canBeForwarded`, partial results) |
| `app/src/test/java/.../ui/screens/chat/MessageSelectionTest.kt` | Unit tests for album groups, forward id order, copy text, batch delete rights, partial forward |
| `app/src/main/res/values/strings.xml` | All `message_selection_*`, `forward_*`, `message_action_forward*`, `message_delete_*` strings |

---

## 2. Android behaviour (the specification to port)

### 2.1 Entering selection mode

- Long-press opens the **single-message action bar** (`MessageActionsBar`), not
  multi-select. Multi-select is entered deliberately from that bar's
  `More` menu → "Select" (`ChatScreen.kt:797-804`, `MessageActions.kt:143-146`).
  `openActionsOnLongPress` records `selectionAnchorId = messageId` so the anchor
  survives the transition.
- `enterSelection(message)` closes the action bar, sets `selectionMode = true`,
  and adds the pressed message's **selection group** (album), not just the
  message (`ChatScreen.kt:771-783`).
- In selection mode a tap **toggles**; a long-press also toggles
  (`ChatScreen.kt:2750-2757`). Every bubble shows `SelectionCheckBadge` — a
  filled check circle when selected, an empty accent-ringed circle when not
  (`SelectionActionBar.kt:179-191`).
- Accessibility: bubble semantics expose `selected` plus a
  "selected"/"not selected" state description; click and long-click labels
  switch to "Select"/"Deselect" (`ChatScreen.kt:2720-2721, 2736-2757`).
- Back handling is layered: delete dialog → forward dialog → selection mode →
  then normal back (`ChatScreen.kt:1074-1088`).
- `exitSelection()` clears mode, ids, anchor, error, forward/delete dialog
  state (`ChatScreen.kt:784-794`). `selectionMode` and `selectionIds` are
  `rememberSaveable` per chat, so rotation keeps the selection
  (`ChatScreen.kt:258-262`).

### 2.2 Selection set, "ranges", anchor, limit

- The set is a `Set<Long>` of message ids; order of toggling never matters.
- **Album grouping** (`MessageSelection.kt:22-30`): tapping any member of a
  media album selects every *loaded* member; tapping a selected member clears
  the whole album. A message with `mediaAlbumId == 0` or no media is its own
  group. Unknown ids fall back to `setOf(messageId)`.
- **No shift-range selection exists on Android.** There is no from/to range
  gesture, no "select all", and no drag-select. What behaves like a range is
  only (a) the album group expansion above and (b) the batch cap below.
  Desktop `Ctrl+A` / `Shift+click` are therefore new behaviour, not parity.
- **Anchor** (`selectionAnchorId`) is only used to keep the single-message
  `More` action available while the selection still equals the anchor's own
  group: `singleActionTarget = anchorId.takeIf { selectionIds == selectionGroupIds(messages, it) }`
  (`ChatScreen.kt:805`, rendered at `1480-1491`).
- **Limit**: adding a group that would push the set past
  `MAX_MESSAGE_BATCH_SIZE` (100) is refused with an error string and the set is
  unchanged (`ChatScreen.kt:777-780, 809-812`). The cap matches TDLib's
  ForwardMessages/DeleteMessages batch limit (`ForwardedMessages.kt:4`,
  `TdConversationActions.kt:588, 643`).
- **Pruning**: while in selection mode, ids that disappear from the loaded
  window are dropped (`ChatScreen.kt:633-637`), so a stale selection cannot
  target a message that is no longer in history.
- Derived flags: `selectedMessages`, `selectionCanCopy` (any selected message
  has non-blank text), `selectionCanSave` (any selected message has media)
  (`ChatScreen.kt:628-630`).

### 2.3 Selection action bar

`SelectionActionBar` (`SelectionActionBar.kt:47-98`) replaces the chat top bar
while `selectionMode` is true (`ChatScreen.kt:1480-1491`):

| Element | Enablement |
|---|---|
| Close (X) | always; `exitSelection()` |
| Count label `"Selected: %1$d"` | always |
| `More` (single-message actions) | only when `onMore != null`, i.e. the selection equals the anchor's group; disabled while busy |
| Forward | `!busy && count > 0`; sets `forwardAsCopy = false`, opens forward dialog |
| Copy | `!busy && canCopy` (any non-blank text) |
| Save | `!busy && canSave` (any media) |
| Delete | `!busy && count > 0`; first loads per-message capabilities |
| Inline error line | shown under the bar, `errorAnnouncement()` for screen readers |

`busy = selectionBusy || forwardBusy`. Every action is a no-op re-entry guard
(`if (selectionBusy) return`).

### 2.4 Forward — destination, targets, topics, comment, copy mode

**Single message** (`ChatScreen.kt:2223, 2237-2241`): `forwardId` set from the
action bar or media viewer → `ForwardChatDialog` with `forwardIds = [id]`.
"Forward without author" is a separate action gated by a confirmation dialog
(`message_forward_copy_title` / `_warning` / `_confirm`,
`ChatScreen.kt:1867-1879`) that sets `forwardAsCopy = true`.

**Selection** (`ChatScreen.kt:2239`): `forwardIdsForSelection(messages, selectionIds)`
— history order, distinct, strictly increasing (TDLib requires it). Album
members are contiguous in history, so every album stays one contiguous run
(`MessageSelection.kt:36-42`, asserted in `MessageSelectionTest.kt:50-66`).
Batch forward always uses `asCopy = false`.

**Destination picker** (`ForwardChatDialog.kt`):

- Full-screen dialog over the chat list; title is "Forward" or "Forward without
  author" (`:179`).
- Tap **selects** a chat; nothing is sent until the explicit
  "Forward (N)" button (`:241-243`). Multi-target is supported.
- Search is a local case-insensitive title substring filter
  (`:162-165`); empty state `forward_no_results`.
- Forums: if a `loadTopics` loader is supplied and the chat `isGroup`, tapping
  loads writable topics (`canWrite`). If no writable topics, the chat itself is
  selected instead. Otherwise a topic list with a "General topic" row appears,
  each topic toggling independently as its own `ForwardTarget(chatId, title,
  topicId, topicTitle)` (`:122-146, 267-286`).
- Selected targets are shown as removable chips (`:201-217`); chip label is
  `title · topicTitle` when a topic is targeted.
- **Comment field** (`:218-234`): optional, max 3 lines, limited to Telegram's
  text limit in **code points**; over-limit is refused, never truncated. The
  comment is sent as a **separate message** in the destination chat/topic with
  `clearDraft = false` (`ChatScreen.kt:2267-2273`), so the destination's own
  draft is untouched. `allowComment = false` is used for media-only forwarding
  (`SharedMediaSection.kt:223`).
- **Captions are preserved**: the forward request never sets
  `remove_caption`; Android's comment is not a caption replacement.
- Send loop (`ChatScreen.kt:2248-2297`): one request per target; `forwardMessage`
  when exactly one id, `forwardMessages` otherwise. A target that fails to
  receive the messages, or whose comment fails after them, stays selected; the
  rest drop out, so a retry never duplicates a chat that already succeeded.
  Errors are concatenated with `forward_failed_chats` + last error.
- Success: closes the dialog, exits selection, and toasts
  `forward_sent_to_chats` (multi-target) / `message_selection_forwarded`
  (multi-id) / `message_forwarded_without_author` (copy) /
  `chat_message_forwarded` (`ChatScreen.kt:2278-2291`).

**Transport contract** (`TdConversationActions.kt:632-663`): ids are
`distinct().sorted()`, capped at 100, every id pre-checked with
`GetMessageProperties(...).canBeForwarded` (one blocked message refuses the
whole batch), one `forwardMessages` request, then `forwardedIds`/`failedIds`
derived from TDLib's returned array (null entries = failed). All-forwarded-empty
is an error.

### 2.5 Copy formatting

`selectedText(messages, selected)` (`MessageSelection.kt:45-50`):

- history order (oldest first, the list's order);
- `text.trim()` per message; blank entries are **skipped entirely**;
- joined with `"\n\n"` (one blank line between messages);
- captions count as text (TDLib puts the caption in `text`), so a captioned
  photo copies its caption; an uncaptioned photo contributes nothing.

`copySelection()` (`ChatScreen.kt:816-847`) first loads
`messageCapabilities` for every selected message in parallel. Any transport
failure surfaces `err_chat_actions_load_failed`; any message with
`canCopy != true` refuses the whole copy with `err_messages_cannot_copy`
(view-once and protected content are the usual causes). An all-blank selection
reports `message_selection_copy_empty` without touching the clipboard.
Success writes plain text with clip label `chat_message`, toasts
`message_selection_copied` (count), and exits selection mode.

Single-message copy (`ChatScreen.kt:1501`) is plain `message.text` with the
same clip label — no markdown, no entity markup.

### 2.6 Delete confirmation and scope

`requestSelectionDelete()` (`ChatScreen.kt:893-921`) loads capabilities for
every selected message, then:

- `batchDeleteRights()` (`MessageSelection.kt:11-16`) intersects the scopes:
  `canDeleteForSelf` = all messages allow self-delete, `canDeleteForAll` = all
  allow for-everyone. `canDelete = self || all`. Empty list → neither.
- If neither is allowed → `err_messages_cannot_delete`, no dialog.
- Default radio choice is for-everyone **only** when self-delete is impossible
  (`selectionDeleteEveryone = !rights.canDeleteForSelf`, `:914`).

`SelectionDeleteDialog` (`SelectionActionBar.kt:113-159`):

- Title "Delete selected messages?"; body repeats the count;
- only scopes supported by **every** selected message are offered, as radio
  rows (whole row is the radio target for accessibility);
- fixed note "Deletion cannot be undone";
- inline error; Cancel / Delete (danger); all controls disabled while busy;
  dismiss blocked while busy.

Confirm (`ChatScreen.kt:1889-1908`): ids `sorted()`, one
`deleteMessages(chatId, ids, scope)` request, toast
`message_selection_deleted` (count), `exitSelection()`. Failure keeps the
dialog open with the error. Transport (`TdConversationActions.kt:585-601`)
re-checks each message against the chosen scope before sending, because TDLib
applies one revoke flag to the whole batch; mixed batches must not silently
delete more or less than asked.

Single-message delete uses the inline `DeleteConfirmPanel` above the composer
with the same radio model (`MessageActions.kt:218-243`).

### 2.7 Batch save

`saveSelection()` (`ChatScreen.kt:848-892`): media-only targets; per message
`canSave` is re-checked and `media.isSecret` is skipped. For each file it waits
for the local path (requesting download, 60 s timeout), then
`saveSelectedMediaToDevice()` (`MessageSelectionMedia.kt:15-51`) writes into
MediaStore Pictures/Movies (or Downloads) under `BeyGram/`, with export
metadata resolution and a signature check. Result is reported as
saved / saved-partial / denied / failed, and a zero-saved run also sets the
inline selection error.

### 2.8 Share

- **There is no message-level "Share" action on Android.** `MessageActionsBar`
  and `SelectionActionBar` have no share entry, and no `message_action_share`
  string exists.
- The only share path is the **media viewer**: a per-file `ACTION_SEND`
  chooser for an already-downloaded, exportable file
  (`MediaViews.kt:1004, 1680-1685`), gated by `canExport` (per-message
  `canSave`, not secret, not view-once).
- Consequently "share" is *not* an Android message-selection feature. A desktop
  equivalent (copy file to clipboard, reveal in Explorer, OS share sheet) is an
  addition, and should be labelled as such rather than as parity.

---

## 3. Windows current status

| Capability | Current state | Evidence |
|---|---|---|
| Selection data structure | `MessageSelectionModel` exists: `count`, `active`, `selectedIds()` (sorted), `toggle/select/clear/selectAll`, ids ≤ 0 ignored, `selectionChanged` only on real change | `src/telegram/MessageSelectionModel.{h,cpp}` |
| Selection model wiring | **Not instantiated anywhere.** Not registered in QML; no owner, no property, no signal consumer | `src/app/main.cpp:71-76` registers only Theme, App, MediaViewer, Playback |
| Selection toolbar | `SelectionToolbar.qml` is complete-looking but **unused** (no reference outside its own file); has count, Forward, Copy, Delete, Clear; missing Save and More; `visible: false` by default | `qml/components/SelectionToolbar.qml` |
| Selection entry | None. No long-press, no "Select" menu item, no Ctrl/Shift click, no Esc-to-clear, no select-all | `qml/components/MessageBubble.qml:333-378`, `qml/pages/ChatsPage.qml` |
| Album grouping | `AlbumGrouper` (pure `groupAlbums`) exists and is unit-tested, but is **not called by any production code**; the model carries no grouped id | `src/telegram/AlbumGrouper.h`, `src/telegram/TelegramTypes.cpp:438-516`, `src/telegram/MessageListModel.h:15-42` |
| Batch limit / pruning / anchor | None of the three exists | — |
| Single forward | Works for one message, one destination; no copy mode, no topic, no comment, no result feedback beyond a generic error | `src/ui/AppController.cpp:1157-1163`, `src/telegram/TelegramRepository.cpp:792-808` |
| Forward request builder | Hardcodes `send_copy: false`, `remove_caption: false`, `topic_id: null`; only a `silently` flag; no `ForwardedMessages` result type | `src/telegram/ForwardMessages.{h,cpp}` |
| Forward destination picker | `ForwardDialog.qml`: single-target, closes on choose, local title filter, no comment, no copy mode, no topics, no busy/error state, no multi-select chips | `qml/components/ForwardDialog.qml`, `qml/pages/ChatsPage.qml:909-917` |
| Copy | Context menu copies raw `text` for one message; no selection copy, no capability check, no toast, no formatting helper | `qml/components/MessageBubble.qml:362-366`, `src/ui/AppController.cpp:1165-1171` |
| Delete | Always `revoke = true` (for everyone) for one message, no scope choice, no capability check, no batch delete, no count | `src/telegram/TelegramRepository.cpp:833-849`, `src/telegram/TelegramRequests.cpp:83-90`, dialog at `qml/pages/ChatsPage.qml:937-972` |
| Capabilities API | Absent: no `getMessageProperties`, no `canBeForwarded/canCopy/canBeSaved/canBeDeleted*` anywhere in `src/` | grep over `src/telegram`, `src/ui` |
| Batch save | Absent. Single-file "Save as" exists only for the open media viewer | `src/ui/AppController.cpp:807-810`, `qml/components/MediaViewerOverlay.qml:91-97` |
| Share | Absent entirely (no `ACTION_SEND` analogue, no clipboard-file path, no Explorer reveal) | grep over `src/`, `qml/` |
| Forum topics | No topic support at all in the Windows client | `ForwardMessages.h:12-16` documents the gap |
| Tests | `tst_message_selection.cpp` and `tst_forward_messages.cpp` exist and cover the pure model/builder only; nothing covers batch delete, copy formatting, or capabilities | `tests/CMakeLists.txt:37, 47` |

Summary: the *primitives* for selection and forward exist and are tested in
isolation, but no user-facing selection mode, no batch operation, no capability
gating, and no destination-picker parity is wired up. `docs/PORTING_ROADMAP.md`
already lists forwarding UI as open in Phase 2.

---

## 4. Gaps, ranked by risk

1. **Destructive: delete is always "for everyone".** `deleteMessage()` sends
   `revoke = true` unconditionally, with no `canBeDeletedForAllUsers` check and
   no "delete for me" option. On a channel/group this removes the message for
   other people on a single click, and the confirmation text promises exactly
   that. Highest-priority correctness gap.
2. **No selection mode at all**, so none of forward/copy/delete/save batching is
   reachable. `MessageSelectionModel` and `SelectionToolbar.qml` are dead code.
3. **No per-message capabilities**, so forward/copy/delete can be offered on
   protected content, view-once media, or messages the account may not touch;
   failures surface only as a generic repository error string.
4. **Forward builder cannot express parity**: no `send_copy`, no topic,
   `remove_caption` fixed false, no partial-result reporting, no per-target
   retry.
5. **Forward picker is single-target and one-shot**; no comment, no copy mode,
   no forum topics, no failure retention, no busy/error feedback.
6. **Copy formatting diverges**: no trimming, no blank skipping, no `\n\n`
   join, no caption/placeholder distinction. `MessageData.text` is filled with
   `"[Photo]"`/`"[Video]"`/… when a media message has no caption
   (`TelegramTypes.cpp:487-506`), so a naive batch copy would paste placeholder
   brackets where Android pastes nothing.
7. **No album-aware selection**: the model has no grouped id and `AlbumGrouper`
   is unused, so tapping one album member cannot select the album.
8. **No batch save and no share**, including no desktop equivalent for the
   media-viewer share that Android does have.
9. **No keyboard path**: no Esc, no Ctrl+A, no Shift/Ctrl click, no context-menu
   "Select" — desktop users expect these even though Android has no range
   selection.
10. **No user feedback**: forward/delete/copy emit no confirmation, and the
    toolbar has no busy/error surface.

---

## 5. Port plan

Ordered so that each step is independently shippable and testable. Names are
proposals consistent with the existing `beygram::telegram` / `beygram::ui`
split; nothing here is implemented yet.

### Step 1 — selection core (no UI)

- `src/telegram/MessageSelectionModel.{h,cpp}`:
  - `static constexpr int MaxBatchSize = 100;` (single source for the TDLib cap)
  - `Q_PROPERTY(qint64 anchorId READ anchorId NOTIFY selectionChanged)`,
    `Q_INVOKABLE void setAnchor(qint64)`
  - `Q_INVOKABLE bool toggleGroup(const QVector<qint64>& groupIds)` — returns
    false and emits nothing when the group would exceed `MaxBatchSize`
  - `Q_INVOKABLE void retain(const QSet<qint64>& loadedIds)` (pruning)
  - `Q_INVOKABLE QVector<qint64> orderedIds(const QVector<qint64>& historyOrder) const`
  - `Q_INVOKABLE void selectRange(const QVector<qint64>& historyOrder, qint64 fromId, qint64 toId)`
    (desktop-only addition; document it as new behaviour, not parity)
- `src/telegram/SelectionGroups.{h,cpp}`: pure `selectionGroupIds(messages, id)`
  equivalent, taking `QVector<QPair<qint64,qint64>>` (id, groupedId) so it
  reuses `groupAlbums()` and stays trivially testable.
- Model plumbing: add `GroupedIdRole` (+ `hasCaptionRole` or a
  `textIsPlaceholder` flag) to `MessageListModel.h:15-42`, populate
  `MessageData::groupedId` and `MessageData::textIsPlaceholder` in
  `messageFromJson` (`TelegramTypes.cpp:438-516`), and set
  `textIsPlaceholder = true` in the `data.text.isEmpty()` fallback block.

### Step 2 — capabilities and batch delete (safety first)

- `src/telegram/MessageCapabilities.{h,cpp}`: pure parser for a
  `getMessageProperties` response →
  `{canBeForwarded, canBeSaved, canBeCopied, canBeDeletedOnlyForSelf, canBeDeletedForAllUsers}`.
  Field names and the exact copy/save split must be confirmed against the
  TDLib schema the Windows client links before implementation; Android relies
  on the generated bindings, which this audit did not compare field by field.
- `src/telegram/TelegramRequests.{h,cpp}`: `getMessagePropertiesRequest(chatId, messageId)`.
- `src/telegram/TelegramRepository.{h,cpp}`:
  `void requestMessageCapabilities(qint64 chatId, QVector<qint64> ids)` emitting
  `capabilitiesReady(QVector<MessageCapabilities>)`, or a
  `BatchRights` aggregate built with the same all-of intersection rule as
  `batchDeleteRights`.
- `deleteMessages(chatId, ids, DeleteScope)` with `enum class DeleteScope { ForSelf, ForEveryone }`,
  per-message scope pre-check, one `deleteMessagesRequest(..., revoke)`, then
  `MessageListModel::removeIds`. Cap at `MaxBatchSize`.
- Replace the unconditional `revoke = true` call site in
  `TelegramRepository::deleteMessage` with `DeleteScope::ForEveryone` **only
  after** a successful `canBeDeletedForAllUsers` check.

### Step 3 — forward transport

- `src/telegram/ForwardMessages.{h,cpp}`: replace the boolean tail with
  `struct ForwardOptions { bool sendCopy = false; bool removeCaption = false;
  bool silently = false; qint64 topicId = 0; };` and emit `send_copy`,
  `remove_caption`, `topic_id` accordingly (keep the positive-and-unique id
  filter, keep ids sorted).
- `src/telegram/ForwardedMessages.{h,cpp}`: `forwardedIds` / `failedIds` /
  `isComplete`, mirroring the Android model.
- `TelegramRepository::forwardMessages(fromChatId, ids, toChatId, ForwardOptions)`
  returning/emitting `ForwardedMessages`; pre-check `canBeForwarded` per id
  (one blocked message refuses the batch); map TDLib's null entries to
  `failedIds`.

### Step 4 — selection UI

- `src/ui/SelectionController.{h,cpp}` (new `QObject`, registered in
  `src/app/main.cpp` next to `MediaViewerController`): owns
  `MessageSelectionModel`, exposes `active`, `count`, `anchorId`,
  `canCopy`, `canSave`, `busy`, `error`, `singleTarget`, and invokables
  `toggle(messageId)`, `toggleGroup(ids)`, `selectAll()`, `clear()`,
  `copy()`, `requestDelete()`, `confirmDelete(scope)`, `saveTo(dir)`,
  `forwardIds()`.
- `qml/components/SelectionToolbar.qml`: add `Save` and `More`, plus
  `busy`, `error`, `canCopy`, `canSave`, `singleTarget` properties and
  `saveRequested`/`moreRequested` signals; wire `enabled` exactly as in
  `SelectionActionBar.kt:83-86`; render the inline error line.
- `qml/components/MessageBubble.qml`: add `selectionMode`, `selected`,
  `canForward`, `canCopy`, `canDelete`, `canGetLink`; draw the check badge
  (empty ring / filled check) over the bubble; make click toggle in selection
  mode; emit `selectionRangeRequested(fromId, toId)` for Shift+click; add
  "Select" to the context menu and gate Forward/Copy/Delete on capabilities.
- `qml/pages/ChatsPage.qml`: own the mode via `BeyGramSelection`; replace the
  top bar with the toolbar; `Esc` clears, `Ctrl+A` selects loaded history,
  `Ctrl+C`/`Delete` route to the controller; prune selection when the message
  list model shrinks.

### Step 5 — forward destination picker

- `qml/components/ForwardDialog.qml` → multi-target picker: local selection
  list with removable chips, explicit "Forward (N)" button, optional comment
  field with code-point limit (refuse, never truncate), `asCopy` title/note,
  `busy`/`error` inputs, and a `failedTargets` re-open path so only failed
  destinations stay picked.
- Keep it model-agnostic: parent supplies the chat model and performs the send.
- Topic support only if/when Windows gains forum topics; until then keep
  `topicId` plumbed through the request builder but not offered in the UI, and
  say so in the dialog's header comment.

### Step 6 — copy, save, share, feedback

- `src/telegram/MessageTextExport.{h,cpp}`: pure
  `QString joinSelectedText(const QVector<ExportText>&)` implementing trim →
  drop blanks → drop placeholders → `"\n\n"` join in history order. Feed it
  from `MessageListModel` rows (`text`, `textIsPlaceholder`, `messageId`).
- `SelectionController::copy()`: capability gate, `QClipboard::setText`, status
  message, exit selection.
- `SelectionController::saveTo(QUrl dir)`: folder `FileDialog`, per-file
  `MediaFileStore` path wait, per-file `canSave` + not-secret, per-file
  success/skip/fail counters, one summary message.
- Share (desktop equivalent, explicitly *not* Android parity): clipboard file
  URL via `QMimeData::setUrls` + `QDesktopServices::openUrl` for "reveal in
  folder"; add the same to `MediaViewerOverlay.qml` so the existing single-file
  path gains what Android's viewer already has.
- Feedback: a transient status line/toast area in `ChatsPage.qml` driven by
  `SelectionController` (`error`, `status`), so forward/delete/copy/save stop
  being silent.

### Files touched (proposed)

```
src/telegram/MessageSelectionModel.{h,cpp}     extend
src/telegram/SelectionGroups.{h,cpp}           new
src/telegram/MessageCapabilities.{h,cpp}       new
src/telegram/MessageTextExport.{h,cpp}         new
src/telegram/ForwardMessages.{h,cpp}           extend (options struct)
src/telegram/ForwardedMessages.{h,cpp}         new
src/telegram/TelegramRequests.{h,cpp}          + getMessagePropertiesRequest
src/telegram/TelegramRepository.{h,cpp}        batch delete, forward result, capabilities
src/telegram/MessageListModel.{h,cpp}          + GroupedIdRole, placeholder flag
src/telegram/TelegramTypes.{h,cpp}             populate groupedId / placeholder
src/ui/SelectionController.{h,cpp}             new
src/ui/AppController.{h,cpp}                   expose controller, status text
src/app/main.cpp                               register BeyGramSelection
qml/components/SelectionToolbar.qml            Save/More/busy/error
qml/components/MessageBubble.qml               selection badge, gated menu, range
qml/components/ForwardDialog.qml               multi-target + comment + copy mode
qml/components/MediaViewerOverlay.qml          share/reveal
qml/pages/ChatsPage.qml                        mode, shortcuts, dialogs, status
src/telegram/CMakeLists.txt, tests/CMakeLists.txt  new sources/targets
```

### Tests to add (QtTest, `tests/`, registered with `beygram_add_test`)

- extend `tst_message_selection.cpp`: album group toggle adds/removes whole
  group; group that exceeds `MaxBatchSize` is refused and emits nothing; anchor
  survives toggle and clears on `clear()`; `retain()` drops unloaded ids;
  `orderedIds()` returns history order; `selectRange()` is inclusive and
  direction-agnostic.
- new `tst_selection_groups.cpp`: unknown id → singleton; zero grouped id →
  singleton; album member with null media → singleton (mirrors
  `MessageSelectionTest.kt:32-48`).
- new `tst_message_capabilities.cpp`: parser fields; batch intersection
  (all-of both scopes); empty batch → neither; single non-deletable message
  blocks for-everyone.
- new `tst_delete_messages.cpp`: request shape (`revoke` true/false), ids
  distinct+sorted, >100 rejected, per-message scope mismatch rejected before
  any request.
- extend `tst_forward_messages.cpp`: `send_copy` true/false, `topic_id`
  present/numeric, `remove_caption`, `silently`, ids sorted+deduped, partial
  result mapping (`ForwardedMessages.failedIds`).
- new `tst_message_text_export.cpp`: trim, blank skipping, placeholder
  skipping, `\n\n` join, captions included, empty → empty string.
- extend `tst_telegram_types.cpp` / `tst_chat_models.cpp`: `grouped_id` parsing
  into `MessageData`, placeholder flag for uncaptioned media, role exposure.
- No QML test harness exists in `tests/`; selection gestures (Ctrl/Shift click,
  Esc, badge rendering, toolbar enablement, retry-only-failed) need a manual QA
  checklist in this document's follow-up, not an automated suite.

### Priority

**P1.** It is the top of the still-open Phase 2 conversation slice
(`docs/PORTING_ROADMAP.md:95, 103`), the primitives are already written and
tested, and item 1 in the gap list is a destructive correctness bug that should
land before any selection UI is exposed. Suggested order: Step 1 → Step 2
(ship the delete-scope fix immediately after) → Step 3 → Step 4 → Step 5 →
Step 6.

### Risks and constraints

- TDLib applies one `revoke` flag per `deleteMessages` and one `send_copy` per
  `forwardMessages`, so batch pre-checks are mandatory, not defensive.
- Forward ids must be strictly increasing for TDLib; sorting in the builder is
  what keeps album members contiguous, so do not reorder by selection order.
- Windows has no forum-topic model, so `topicId` stays plumbing-only for now;
  do not show a topic picker that cannot work.
- Desktop range selection has no Android counterpart — document it as an
  intentional addition so a future parity check does not flag it as drift.
- `MessageData.text` placeholders must never reach the clipboard; the
  placeholder flag is the guard.

---

## 6. Verification status

Not verified by execution. No CMake configure, build, application launch,
emulator, device, or test run was performed, and no file other than this
document was created or modified. Claims about behaviour are claims about code
as written, not about observed runtime behaviour. In particular:

- no `ctest`/`tst_message_selection`/`tst_forward_messages` run;
- no Android unit-test run (`MessageSelectionTest.kt` was read, not executed);
- no QML scene was loaded, so toolbar/badge/menu rendering is unverified;
- TDLib field names used in §5 Step 2 are proposals pending schema confirmation.
