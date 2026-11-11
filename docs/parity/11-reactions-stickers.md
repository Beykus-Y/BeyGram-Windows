# 11 - Reactions, custom emoji, stickers and GIFs: Android → Windows parity audit

Status: read-only audit. No build, no test run, no GUI session, no git operation and
no code change was performed for this document. Every statement below comes from
reading the sources listed in §1 and §3.1.

Reference client: the Android sources kept in `app/` (Jetpack Compose, TDLib Java
bindings). Target client: the Qt/QML Windows client in `src/`, `qml/`, `tests/`.

Scope of this slice: message reactions (emoji, custom emoji, paid/Stars), reaction
picker and live reaction updates, custom emoji rendering, sticker and GIF catalogs
(recent, favorites, installed sets, set browsing, search), the emoji/sticker/GIF
panel in the composer, sticker messages, and animated sticker playback.

---

## 1. Android files reviewed

### Data / TDLib layer

| File | What it implements |
|---|---|
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdMessageReactions.kt` | Reaction parsing (`messageReactions`, `messageReactionsFrom`), picker option assembly (`pickerOptions`), type mapping (`optionOf`, `reactionTypeOf`), limit overflow (`overflowReactions`), paid count (`paidReactionCount`) |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdConversationActions.kt` | `availableReactions` (lines 678–682), `toggleReaction` (690–711), `maxReactionCount` (713–721), per-message `KeyedMutex` (line 29), `activeEmojiReactions` (32–33) |
| `app/src/main/java/com/beykus/beygram/data/tdlib/ReactionReplacement.kt` | Displace-then-add transaction with non-cancellable rollback (11–32) |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdCustomEmojis.kt` | `GetCustomEmojiStickers` lookup, per-id `StateFlow` cache, retry budget (31–61, 63–69) |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdPaidReactions.kt` | Pending → commit → rollback Stars flow, single-flight, uncertain-result classification, error text mapping (19–52, 58–72, 78–100) |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdStickerCatalog.kt` | Recent/favorite stickers, installed sets, `getStickerSet` with LRU cache, `changeStickerSet`, sticker search paging, saved/recent/search GIFs, premium check, TDLib→model mapping (33–195, 198–265) |
| `app/src/main/java/com/beykus/beygram/data/tdlib/OutgoingMediaContent.kt` | `InputMessageSticker` (63–69) and `InputMessageAnimation` (70–82) construction |
| `app/src/main/java/com/beykus/beygram/domain/model/StickerPicker.kt` | `StickerItem`, `StickerSetSummary`, `StickerSetContent`, `StickerSearchPage`, `AnimationItem`, `AnimationPage`, stable `key`, `toOutgoingMedia()` (14–113) |
| `app/src/main/java/com/beykus/beygram/domain/model/PaidReaction.kt` | Paid reaction model, presets, amount blocking rules |

### UI layer

| File | What it implements |
|---|---|
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/MessageReactions.kt` | Chips under a bubble (66–116), glyph rendering incl. custom emoji and Stars (119–142), reaction picker strip (154–186) |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ChatScreen.kt` | `setReaction`/`react` (465–475), `availableReactions` load (1043–1046), picker wiring (1587–1590), chips in bubble (2863–2866), sticker tap → set sheet (246–248, 2697–2718, 1791) |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/StickerPanel.kt` | Composer entry button (132–163), send host (171–258), unified panel state and loaders (261–480), header/tabs (776–914), section strip and pack chips (916–1087), content routing (1090–1236), grids (1238–1458), overlay previews (1543–1856) |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/StickerPanelLogic.kt` | Tabs/sections state machine, saver, load states, favorite list merging (7–132) |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/StickerPicker.kt` | Sticker grid, cell, premium badge, artwork drawing (49–152) |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/StickerPickerLogic.kt` | Page merge with de-duplication (12–16) |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/StickerSetLogic.kt` | Set content state (`Unavailable`/`Empty`/`Ready`, 13–17) |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/StickerSetSheet.kt` | Set browser/sheet, install/uninstall toggle, per-cell send, read-only variant (61–110, 119–303) |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/StickerViews.kt` | `StickerPreview` with auto-download policy (56–116), TGS via Lottie (123–148), `CustomEmojiImage` (155–178) |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/EmojiCatalog.kt` | Eight emoji groups, recent list, `RecentEmojiStore` in `SharedPreferences` (8–205) |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/EmojiPanel.kt` | Emoji tab grid, group strip, backspace (54+) |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/FormattedMessageText.kt` | Inline custom emoji in message text (73–264, 409–425) |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/PaidReactionDialog.kt`, `PaidReactionStore.kt` | Stars confirmation dialog, presets, balance, in-flight/uncertain state |

### Android tests reviewed (as behaviour documentation)

`app/src/test/.../data/tdlib/MessageReactionsTest.kt`,
`ReactionReplacementTest.kt`, `TdCustomEmojisTest.kt`, `TdPaidReactionsTest.kt`,
`StickerInputMappingTest.kt`; `app/src/test/.../ui/screens/chat/StickerPanelLogicTest.kt`,
`StickerPickerLogicTest.kt`, `StickerSetLogicTest.kt`, `EmojiCatalogTest.kt`,
`PaidReactionStoreTest.kt`, `EmojiPresentationTest.kt`;
`app/src/androidTest/.../chat/MessageReactionsTest.kt`,
`MessageReactionBubblePlacementTest.kt`.

---

## 2. How Android behaves (the specification to port)

### 2.1 Reaction chips on a bubble

- `FlowRow` with `widthIn(max = 292.dp)`, 2 dp gaps, chip = rounded pill, 18 dp glyph +
  13 sp count, min touch target 48×40 dp.
- Chosen reaction: `accentSoft` fill and a 1 dp accent border; other chips `surface2`.
  Under the Glass design style the chip is a tint of the bubble with no outline.
- Glyph source by priority: paid → Stars icon tinted `#FFB300`; custom emoji → its
  sticker, with `🙂` placeholder while TDLib resolves it and if the id is gone; emoji →
  the emoji text through `emojiPresentation`.
- Accessibility: content description differs for paid (`chat_reaction_paid_description`),
  custom (`chat_reaction_custom_description`) and emoji (`chat_reaction_description`),
  and `selected` mirrors `isChosen`.
- A chip tap passes the whole `ReactionOption` upward; the screen decides add vs remove.

### 2.2 Reaction picker

- Opened from the message action menu ("React"), rendered as a 2-row horizontal grid,
  48 dp cells, 106 dp total height, above a divider, inside the bubble column.
- Options come from `GetMessageAvailableReactions(chatId, messageId, 25)`; when TDLib
  reports an unavailability reason the list is empty and the entry point is hidden.
- Order: optional Stars reaction first, then `topReactions` + `recentReactions` +
  `popularReactions`, then — for Premium accounts only — the chat's own list
  (`ChatAvailableReactionsSome`) or every active emoji (`ChatAvailableReactionsAll`).
  Non-Premium users never see `needsPremium` reactions and never see custom emoji.
- De-duplication key ignores U+FE0F so the same emoji never appears twice.
- Chosen options are outlined (`accentSoft` circle); tapping a chosen one removes it.

### 2.3 Send / remove / limit handling

- `toggleReaction` runs under a per-`(chatId, messageId)` mutex.
- It re-reads the message, collects the chosen non-paid reactions, and if the picked
  option is among them calls `RemoveMessageReaction` with that **concrete** reaction
  type; otherwise it adds.
- When the message already holds the chat's maximum number of reactions
  (`ChatAvailableReactionsAll/Some.maxReactionCount`, at least 1), the first chosen
  reaction in server order is removed first, and restored if the add fails
  (`replaceReactions`, rollback runs non-cancellably).
- `addToRecent` is true only for a reaction picked from the extended menu; chip taps
  and rollback restores do not pollute recent reactions.
- After the request the message is re-read and the stored history row updated, so the
  UI shows the server's counts, not an optimistic guess.

### 2.4 Live updates

- `UpdateMessageReactions` → `messageReactionsFrom(update.reactions)` and replace the
  row.
- Message updates carrying `interaction_info` also replace the reactions.
- `UpdateActiveEmojiReactions` refreshes the pool used for `ChatAvailableReactionsAll`.

### 2.5 Custom emoji

- `GetCustomEmojiStickers([id])` once per id, cached in a `ConcurrentHashMap<Long,
  MutableStateFlow<CustomEmoji?>>`; observers subscribe per reaction chip / per inline
  run.
- Retry policy: `Missing` retries with pauses 2 s, 5 s, 15 s, 30 s then gives up;
  `Unanswered` (network) keeps retrying until answered. A chip never collapses to an
  empty box.
- Rendering: TGS → Lottie from `readTgsJson`; WebP → image; WebM → not downloaded and
  not drawn (documented limitation), emoji text fallback instead.
- Inline custom emoji in message text use the same lookup and the same fallbacks.

### 2.6 Paid (Stars) reactions

- Tapping a paid option never sends directly; it opens a confirmation with the current
  Stars balance, presets and a max from `paid_reaction_star_count_max`.
- Send = `AddPendingPaidMessageReaction` → `CommitPendingPaidMessageReactions`; any
  failure removes the pending reaction (non-cancellable) and the original error is
  rethrown with a suppressed rollback error.
- One send in flight at a time; never retried (a repeated commit could spend Stars
  twice). A failure after the commit was sent is reported as *uncertain*, not failed.
- Anonymous vs regular reactor type is a user choice.
- Errors are mapped: balance too low, flood wait, message unavailable, reaction
  invalid/forbidden, timeout, generic.

### 2.7 Recent and favorite stickers

- `GetRecentStickers(false)` for the Recent section, `GetFavoriteStickers()` for
  Favorites.
- Favorite toggles are applied locally first and rolled back on failure; identity is
  `setId:stickerId` (falling back to the file id), so the same sticker reached through
  search is not added twice.
- Masks and custom-emoji "stickers" are filtered out of every picker list.

### 2.8 Sticker sets

- `GetInstalledStickerSets(StickerTypeRegular)` drives the horizontal pack strip.
- `GetStickerSet(setId)` is cached in a 12-entry access-ordered map; `ChangeStickerSet`
  drops the cache entry **before** the request and patches the installed flag after, so
  a cancelled mutation cannot leave a stale install button.
- The set sheet shows cover, title, size, install/uninstall, a grid of the set, and a
  read-only mode when the caller has no send target. A set id of 0 or a set with no
  regular stickers shows a placeholder instead of an endless spinner.
- Tapping a sticker message opens its set (`openStickerSetId`), not the media viewer.

### 2.9 Panel structure, search, GIFs

- One composer entry button opens an inline panel with tabs `Emoji`, `Stickers`, `Gifs`
  (default tab: Stickers) and sections `Recent`, `Favorites`, `Pack`.
- Back gesture: clear search → leave pack/favorites → close panel; state is saved by
  tab **names**, not ordinals.
- Search is debounced 350 ms; stickers page at 60 per request, GIFs at 40 with up to 3
  history pages per load; appended pages are de-duplicated.
- GIF recent list is derived from the account's message history
  (`SearchMessages` + `SearchMessagesFilterAnimation`) because TDLib has no recent-GIF
  list; global GIF search uses the inline animation bot
  (`animation_search_bot_username`, fallback `gif`), with a `history:` cursor prefix so
  a history cursor is never sent to the bot.
- Sending: sticker tap sends immediately (premium sticker blocked for non-Premium with
  a notice); long press offers send-now / silent / scheduled; GIFs can be staged with a
  caption.

### 2.10 Emoji tab

- Eight groups (smileys, animals, food, activity, travel, objects, symbols, flags) with
  a jump strip and a backspace key; emoji are inserted into the draft and the panel
  stays open.
- Recent emoji (max 32, newest first) live in `SharedPreferences` under
  `emoji_panel` / `recent`, shared app-wide, and lead the list once non-empty.

### 2.11 Animated playback

- TGS: `readTgsJson` (gunzip to Lottie JSON) then Lottie, infinite iterations, stops
  off-screen, falls back to the static thumbnail and then to the label.
- WebP: drawn as an image (animated WebP through `LocalMediaImage`).
- WebM: only the still thumbnail is shown, by design, because the platform player may
  not composite the alpha channel — this is an accepted Android limitation, not a
  target to exceed.
- Sticker bubbles are 160 dp, frameless, auto-downloaded through the auto-download
  policy (`MediaKind.Sticker`), with explicit download on viewer actions.

---

## 3. Current Windows status

### 3.1 What exists

| Area | Windows implementation | State |
|---|---|---|
| Reaction parsing | `src/telegram/MessageReactions.{h,cpp}` — `ReactionCount`, `MessageReactions`, `reactionsFromMessage` accepting `reactions`, bare `updateMessageReactions` list and `interaction_info.reactions` | implemented, **only used by tests** |
| Reaction request builders | `sendReactionRequest(chatId, messageId, emoji\|customEmojiId, big)`, `clearReactionRequest` | implemented, `clearReactionRequest` emits a **null** `reaction_type` and its header admits the caller must patch it |
| Reaction sending | `TelegramRepository::sendReaction(chatId, messageId, emoji)` (`TelegramRepository.cpp:721-736`) → `AppController::sendReaction` (`AppController.h:282`) | emoji only, fire-and-forget, no model refresh |
| Reaction chips | `qml/components/ReactionBar.qml` | implemented, `canReact: false` from the bubble, no custom-emoji image, star for custom emoji |
| Reaction picker | `qml/components/ReactionPicker.qml` | implemented, **not instantiated anywhere**, static 24-emoji list |
| Sticker metadata | `src/telegram/StickerInfo.{h,cpp}` — ids, emoji, size, thumbnail/full file, TGS/WebM/WebP format, mask, premium | implemented |
| Sticker set request | `getStickerSetRequest(setId)` (`StickerInfo.cpp:96-102`) | request builder only |
| Sticker set model | `src/telegram/StickerSetModel.{h,cpp}` — roles `stickerId, emoji, thumbnailFileId, fullFileId, isAnimated, isVideo` | implemented, no consumer |
| Sticker picker UI | `qml/components/StickerPicker.qml` | implemented, **not instantiated anywhere** (only listed in `CMakeLists.txt:96`) |
| Sticker bubble | `qml/components/StickerBubble.qml` + `MessageBubble.qml:215-227` | static image only; `fileId: 0`, `animated: false` are hard-coded at the call site |
| Sticker message data | `TelegramTypes.cpp:124-131, 229-231, 269-270, 314, 498-499`; `MessageListModel` `contentType`/`mediaFileId`/`mediaFullFileId` | thumbnail + label only |

### 3.2 Behaviour of the Windows code today

- The bubble parses reactions **again in QML** (`MessageBubble.qml:34-59`) from
  `reactionsJson`, a compact TDLib JSON blob produced in `TelegramTypes.cpp:459-469`.
  The C++ `reactionsFromMessage` parser is dead production code — two parsers, one
  contract.
- `ReactionBar` is rendered only when `reactionList().length > 0`
  (`MessageBubble.qml:298-304`), so a message with no reactions has no strip and no
  entry point. With `canReact: false` the trailing “+” affordance never appears either.
  A user can therefore only toggle a reaction that already exists.
- `ReactionBar` emits `customReactionToggled(int)` for custom emoji, but
  `MessageBubble` only handles `onReactionToggled`, so custom-emoji chips are inert.
- `ReactionPicker.qml` exists but is never opened, so the 24-emoji fallback list is
  unreachable. There is no `getMessageAvailableReactions` request anywhere in `src/`.
- Removing a reaction is impossible: `clearReactionRequest` is never called and its
  payload is invalid as built (null `reaction_type`), and the repository has no remove
  path. Toggling an already-chosen emoji therefore sends `addMessageReaction` again.
- There is no reaction-limit handling (Android's `maxReactionCount` +
  displacement/rollback) and no `addToRecent` distinction.
- After a successful send nothing refreshes the bubble: `sendReaction` only records an
  error (`TelegramRepository.cpp:727-735`), and `updateMessageReactions` /
  `updateMessageInteractionInfo` are not handled in
  `TelegramRepository::handleUpdate` (`TelegramRepository.cpp:894-1133`), so counts
  stay stale until the chat is reloaded.
- Custom emoji reactions render as `★`, the same glyph used for paid reactions
  (`ReactionBar.qml:31-40`), so paid and custom reactions are visually
  indistinguishable. No `getCustomEmojiStickers` request exists.
- No paid/Stars reaction path: no pending/commit/rollback builders, no dialog, no
  balance, no single-flight guard.
- `MediaImageProvider` only decodes through `QImageReader`
  (`src/ui/MediaImageProvider.cpp:42-59`), so TGS and WebM sticker files cannot be
  drawn at all; the bubble is stuck on the thumbnail.
- `OutgoingMediaKind` is `Photo | Video | Document` (`src/telegram/MediaSend.h:10`);
  there is no `inputMessageSticker` builder, so **sending a sticker is impossible**.
- No sticker catalog: no `getRecentStickers`, `getFavoriteStickers`,
  `getInstalledStickerSets`, `changeStickerSet`, `searchStickers`,
  `getSavedAnimations` or GIF search. `StickerSetModel` has no consumer, so
  `StickerPicker.qml` would show an empty popup even if it were opened.
- No emoji tab, no emoji picker in the composer, no recent-emoji store, no GIF tab.
- `StickerBubble` is a static thumbnail with an “ANIM” badge that starts no animation
  (`StickerBubble.qml:4-8`), and the call site always passes `animated: false`.

### 3.3 Tests that exist on Windows

`tests/tst_message_reactions.cpp` (parse shapes, emoji/custom/paid, malformed input,
emoji request, custom-emoji request, clear request), `tests/tst_sticker_info.cpp`
(parsing, formats, masks, premium, oversized file ids, `getStickerSetRequest`),
`tests/tst_sticker_set_model.cpp` (roles, counts, reset, invalid index, role names).
All three are registered in `tests/CMakeLists.txt`. They cover the model layer only —
no test drives a repository path, an update, or a UI component.

---

## 4. Parity matrix

Legend: **— missing**, **~ partial**, **✓ present** (present = exists in code; nothing
here was executed).

| Capability | Android | Windows | Gap |
|---|---|---|---|
| Reaction chips with counts | ✓ | ✓ | — |
| Chosen-state styling | ✓ | ✓ | — |
| Chip tap toggles | ✓ | ~ | sends add only, never removes |
| Add a first reaction to a message | ✓ | — | strip hidden when empty; no picker |
| Reaction picker with TDLib options | ✓ (25, premium-gated) | — | `ReactionPicker.qml` orphaned, static list |
| Remove chosen reaction | ✓ (concrete type) | — | builder invalid, never called |
| Reaction limit / displacement + rollback | ✓ | — | absent |
| Recent-reaction behaviour (`addToRecent`) | ✓ | — | absent |
| Live reaction updates | ✓ | — | `updateMessageReactions` unhandled |
| Refresh after own reaction | ✓ (re-read message) | — | absent |
| Custom emoji reaction glyph | ✓ (sticker, TGS/WebP) | ~ | `★` placeholder |
| Custom emoji lookup + retry | ✓ | — | no request |
| Custom emoji inline in text | ✓ | — | `TextFormatting.cpp:187` keeps literal text |
| Paid/Stars reaction chips | ✓ | ~ | `★` glyph, no semantics |
| Paid reaction send flow | ✓ (pending/commit/rollback) | — | absent |
| Recent stickers | ✓ | — | absent |
| Favorite stickers | ✓ | — | absent |
| Installed set strip | ✓ | — | absent |
| Set browsing sheet | ✓ | — | `getStickerSetRequest` only |
| Install/uninstall set | ✓ | — | absent |
| Sticker search + paging | ✓ | — | absent |
| Sticker send (`inputMessageSticker`) | ✓ | — | no builder, no `OutgoingMediaKind::Sticker` |
| Sticker message → open set | ✓ | — | tap opens media viewer |
| Premium sticker gating | ✓ | — | absent |
| TGS animated playback | ✓ (Lottie) | — | provider cannot decode TGS |
| WebM sticker handling | ~ (thumbnail only) | — | no format branch at all |
| GIF tab (recent/saved/search) | ✓ | — | absent |
| Emoji tab + recents | ✓ | — | absent |
| Composer entry for emoji/sticker/GIF | ✓ | — | composer has Attach + text + Send only |

---

## 5. Gaps, in dependency order

- **G1 — Reaction writes are incomplete.** No remove path (`clearReactionRequest` emits
  null type), no toggle semantics, no limit handling, no `addToRecent` distinction, no
  post-write refresh.
- **G2 — No live reaction updates.** `updateMessageReactions` and
  `updateMessageInteractionInfo` are ignored, so any change made on another device (or
  by the user) does not appear.
- **G3 — No reaction picker in production.** `ReactionPicker.qml` is dead; there is no
  `getMessageAvailableReactions`, no premium/chat-list/active-emoji assembly, no
  Stars-first ordering, no U+FE0F de-duplication.
- **G4 — Custom emoji are not rendered.** Reaction chips show `★`; inline custom emoji
  in text stay literal; no lookup, cache or retry.
- **G5 — Paid reactions have no flow.** No pending/commit/rollback, no confirmation, no
  balance, no single-flight guard, no uncertain-result reporting.
- **G6 — No sticker catalog.** No recents, favorites, installed sets, set content,
  install/uninstall, search or GIF sources; `StickerSetModel` has no producer.
- **G7 — Stickers cannot be sent.** `OutgoingMediaKind` lacks `Sticker`, there is no
  `inputMessageSticker` builder, no premium gating.
- **G8 — The composer has no emoji/sticker/GIF entry.** `StickerPicker.qml` is
  unreachable; there is no emoji tab or recent-emoji store.
- **G9 — Animated stickers cannot render.** `MediaImageProvider` decodes only
  `QImageReader` formats; there is no TGS decoder and no WebM policy.
- **G10 — Sticker messages are second class.** Bubble passes `fileId: 0` and
  `animated: false`; tapping opens the media viewer instead of the set sheet.
- **G11 — Duplicate reaction parsing.** QML re-parses the JSON that
  `reactionsFromMessage` already models; one of the two must become the single source.
- **G12 — Accessibility drift.** Android exposes distinct descriptions for paid,
  custom and emoji chips and a selected state; Windows exposes `glyph + count` only,
  with `★` for both custom and paid.

---

## 6. Implementation plan

All steps are proposals. Nothing below has been built, run or measured.

### Stage A — Reaction correctness (P1)

**Files**

- `src/telegram/MessageReactions.{h,cpp}`
  - Add `ReactionKind`/`ReactionType` enum instead of the `QString type` tag, keeping a
    JSON converter for the existing model role.
  - `reactionTypeJson(const ReactionCount&)`.
  - `removeReactionRequest(chatId, messageId, const ReactionCount&)` — replaces the
    null-typed builder; keep the old overload deleted, not deprecated.
  - `availableReactionsRequest(chatId, messageId, rowSize = 25)`.
  - `mergePickerOptions(const QJsonObject& available, const QJsonObject& chat,
    const QStringList& activeEmojis, bool premium)` implementing Stars-first order,
    premium filtering, custom-emoji exclusion for non-Premium, `needsPremium` filter
    and the U+FE0F-insensitive de-duplication.
  - `overflowReactions(const QVector<ReactionCount>& chosen, int limit)`.
- `src/telegram/MessageListModel.{h,cpp}`
  - `setReactions(qint64 messageId, const MessageReactions&)` emitting `dataChanged`
    for `ReactionsRole` (and the derived `reactionsJson` role).
- `src/telegram/TelegramRepository.{h,cpp}`
  - `toggleReaction(chatId, messageId, ReactionCount, bool addToRecent)` with a
    per-message serialisation guard (mirror `KeyedMutex`), remove-if-chosen, limit
    displacement and restore-on-failure, then re-read the message and update the model.
  - `requestAvailableReactions(chatId, messageId)` exposing the merged option list.
  - Handle `updateMessageReactions`, `updateMessageInteractionInfo` and
    `updateActiveEmojiReactions` in `handleUpdate`.
- `src/ui/AppController.{h,cpp}`
  - `Q_INVOKABLE void toggleReaction(qint64 chatId, qint64 messageId, const QString&
    emoji, bool chosen)`; `Q_INVOKABLE QVariantList availableReactions(...)`;
    `reactionOptionsChanged` notification.
- `qml/components/ReactionBar.qml`
  - `canReact` defaults true; emit `addReactionRequested` from the strip even when the
    list is empty (the bubble must render the strip whenever the user may react).
- `qml/components/ReactionPicker.qml`
  - Accept a `model` of `{ emoji, customEmojiId, isPaid, chosen }` and keep the static
    list only as an explicit offline fallback; add `isPaid` and chosen styling.
- `qml/components/MessageBubble.qml`
  - Use the C++-provided list (drop the local JSON parser, see G11), handle
    `customReactionToggled` and `addReactionRequested`, expose `canReact`.
- `qml/pages/ChatsPage.qml`
  - Add a “React” context-menu item that opens `ReactionPicker` anchored to the bubble;
    route selections to `toggleReaction`.

**TDLib APIs:** `getMessageAvailableReactions`, `addMessageReaction`,
`removeMessageReaction`, `getChat` (`available_reactions.max_reaction_count`), `getMe`
(`is_premium`), `getMessage`.

**Tests (to be written, not run):**
`tests/tst_message_reactions.cpp` — remove builder carries the concrete type; picker
assembly order (Stars first, top/recent/popular, chat list, active emoji); premium
filtering; custom emoji excluded for non-Premium; FE0F de-duplication; overflow
selection order.
`tests/tst_chat_models.cpp` — `setReactions` emits one `dataChanged` for the right row
and roles, ignores unknown ids, and keeps `reactionsJson` consistent.

### Stage B — Sticker catalog and panel v1 (P1)

**Files**

- `src/telegram/StickerCatalog.{h,cpp}` (new): request builders plus a small cache —
  `getRecentStickers(false)`, `getFavoriteStickers`, `addFavoriteSticker` /
  `removeFavoriteSticker`, `getInstalledStickerSets(stickerTypeRegular)`,
  `getStickerSet` with a 12-entry access-ordered cache, `changeStickerSet(setId,
  installed, false)` invalidating before the request, `searchStickers(type, query,
  offset, 60)`.
- `src/telegram/StickerInfo.{h,cpp}`: extend parsing to the full `StickerItem` shape
  (`fileId`, `thumbnailWidth/Height`, `thumbnailIsStatic`, `format` enum, `premium`,
  `key()`), keeping `stickerFromMessage` behaviour compatible.
- `src/telegram/StickerSetModel.{h,cpp}`: add `FileIdRole`, `FormatRole`,
  `PremiumRole`, `SetIdRole`, `ThumbnailWidthRole`, `ThumbnailHeightRole`.
- `src/telegram/StickerListModel.{h,cpp}` (new): flat recent/favorites/search model
  with the same roles plus de-duplicating `append`.
- `src/telegram/StickerSetListModel.{h,cpp}` (new): installed-set strip
  (`id, title, size, installed, coverFileId`).
- `src/telegram/MediaSend.{h,cpp}`: `OutgoingMediaKind::Sticker` and
  `sendStickerMessageRequest(chatId, stickerFileId, emoji, width, height,
  replyToMessageId)` building `inputMessageSticker` with `inputFileId` and an
  `inputThumbnail` only when the thumbnail is a static image.
- `src/telegram/TelegramRepository.{h,cpp}`: `requestRecentStickers`,
  `requestFavoriteStickers`, `setStickerFavorite`, `requestInstalledStickerSets`,
  `requestStickerSet`, `changeStickerSet`, `searchStickers`, `sendSticker`.
- `qml/components/StickerPicker.qml`: tabs (`Emoji` placeholder in this stage,
  `Stickers`), sections (`Recent`, `Favorites`, pack), pack strip with covers, search
  field with 350 ms debounce, paging footer, per-cell premium badge and
  thumbnail-first loading; keep the existing `stickerSelected` contract and add
  `stickerOptionsRequested` for long press.
- `qml/components/StickerSetSheet.qml` (new): cover, title, size, install/uninstall,
  grid, read-only mode, empty/unavailable placeholder.
- `qml/pages/ChatsPage.qml`: composer entry button that toggles the picker above the
  input row; send path calls `sendSticker`; long press offers send/silent/schedule
  reusing the existing send-options dialog.

**TDLib APIs:** `getRecentStickers`, `getFavoriteStickers`, `addFavoriteSticker`,
`removeFavoriteSticker`, `getInstalledStickerSets`, `getStickerSet`,
`changeStickerSet`, `searchStickers`, `sendMessage` with `inputMessageSticker`.

**Tests:** `tests/tst_sticker_catalog.cpp` (new) — request shapes, LRU eviction and
cache invalidation order for `changeStickerSet`, page merge de-duplication, mask and
custom-emoji filtering; extend `tests/tst_sticker_info.cpp` for the sticker send
builder (reply target only when positive, thumbnail only when static, emoji preserved);
extend `tests/tst_sticker_set_model.cpp` for the new roles.

### Stage C — Custom emoji (P2)

**Files**

- `src/telegram/CustomEmojiStore.{h,cpp}` (new): `getCustomEmojiStickers([id])`,
  per-id cache with a `Found`/`Missing`/`Unanswered` classification and the Android
  retry budget (2/5/15/30 s for `Missing`, retry until answered for `Unanswered`);
  emits `customEmojiResolved(qint64 id)`.
- `src/ui/MediaImageProvider.cpp`: expose a `path://`-style lookup or a sibling
  provider so QML can hand a local path to a decoder instead of forcing
  `QImageReader`.
- `qml/components/CustomEmojiImage.qml` (new): WebP through `Image`, TGS through the
  Stage E decoder, emoji-text fallback with `🙂` when the id is gone, download only
  when the format can be drawn.
- `qml/components/ReactionBar.qml`, `MessageBubble.qml`, `ReactionPicker.qml`: use
  `CustomEmojiImage` for `customEmojiId > 0`; stop rendering `★` for custom emoji.
- `src/telegram/TextFormatting.{h,cpp}`: emit custom-emoji runs (offset + id) instead
  of keeping literal text (`TextFormatting.cpp:187`), and render them in
  `qml/components/MessageBubble.qml` through a rich-text path or an inline image
  overlay.
- Premium gating: hide custom-emoji options for non-Premium in the picker and show the
  Android-style notice when a premium sticker is tapped.

**Tests:** `tests/tst_custom_emoji_store.cpp` (new) — cache hit/miss, `Missing` budget
stops after four pauses, `Unanswered` keeps retrying, concurrent observers share one
request; `tests/tst_text_formatting.cpp` — custom-emoji runs preserve offsets and
ordering with other entities.

### Stage D — Paid (Stars) reactions (P2)

**Files**

- `src/telegram/PaidReactions.{h,cpp}` (new):
  `addPendingPaidMessageReactionRequest`, `commitPendingPaidMessageReactionsRequest`,
  `removePendingPaidMessageReactionsRequest`, `paidReactionTypeJson(anonymous)`, and a
  state machine mirroring `TdPaidReactions`: single in-flight send, no retry,
  non-cancellable rollback, `sent` / `failed` / `uncertain` result classification.
- `src/telegram/TelegramRequests.{h,cpp}`: `getStarCount`/balance request and the
  `paid_reaction_star_count_max` option read.
- `src/ui/AppController.{h,cpp}`: `paidReactionBalance`, `openPaidReaction(...)`,
  `sendPaidReaction(starCount, anonymous)`, `paidReactionState` notifications.
- `qml/components/PaidReactionDialog.qml` (new): balance, presets, max, blocked
  amounts, busy and uncertain states.
- `qml/components/ReactionPicker.qml`: Stars option first, opens the dialog instead of
  toggling.
- `qml/components/ReactionBar.qml`: Stars glyph plus count with a paid-specific
  accessible name.

**Tests:** `tests/tst_paid_reactions.cpp` (new) — request shapes, one-send-at-a-time
guard, rollback ordering and suppression, post-commit failure reported as uncertain,
error-code mapping (balance low / flood wait / message invalid / forbidden).

### Stage E — Animated playback (P3)

**Decision needed before code.** The Windows tree has no TGS renderer today. Two
options must be evaluated against the installed Qt kit and the licensing policy:

1. Qt's Lottie module (`Qt.labs.lottieqt`, if present in the shipped Qt) — no new
   third-party dependency, but its API and availability must be verified on the target
   kit.
2. `rlottie` under `third_party/` with a small `QQuickItem`/`QImage` wrapper — more
   control, one more native dependency.

Neither is confirmed available in this repository; this is an investigation item, not
an assumption.

**Files**

- `src/media/TgsDecoder.{h,cpp}` (new): gunzip `.tgs` → Lottie JSON, matching Android's
  `readTgsJson` contract (reject corrupt archives with a fallback, never crash).
- `src/ui/TgsImageProvider.cpp` (new) or a `QQuickItem` in
  `qml/components/TgsSticker.qml`: frame-clock driven playback, paused when the item
  leaves the viewport, thumbnail fallback.
- `qml/components/StickerBubble.qml`: `format` property (`webp`/`tgs`/`webm`), TGS
  animation, WebM → thumbnail only (Android's accepted limitation), badge only when
  animation is actually playing.
- `qml/components/StickerPicker.qml`, `StickerSetSheet.qml`: animate TGS cells only
  while visible; never download WebM full files for the grid.
- `qml/components/MessageBubble.qml`: pass `fileId`, `format` and `animated` from the
  model instead of the hard-coded `0`/`false`.

**Tests:** `tests/tst_tgs_decoder.cpp` (new) — valid gzip payload decodes, truncated
and non-gzip input returns a failure without throwing; media-kind routing test for
`webp`/`tgs`/`webm`.

### Stage F — Emoji tab and GIFs (P4)

**Files**

- `qml/components/EmojiPanel.qml` (new) + `qml/components/EmojiCatalog.js` (new, data
  only): the eight Android groups, jump strip, backspace, insertion into the composer
  at the cursor.
- `src/ui/RecentEmojiStore.{h,cpp}` (new, `QSettings`): newest-first list capped at 32,
  shared app-wide, tolerant of a corrupt value (empty list, never a crash).
- `src/telegram/StickerCatalog.{h,cpp}`: `getSavedAnimations`, recent GIFs via
  `searchMessages` + `searchMessagesFilterAnimation` (up to 3 pages per load, 40 per
  page), global GIF search through `getInlineQueryResults` with the
  `animation_search_bot_username` option (fallback `gif`) and the `history:` cursor
  prefix rule.
- `qml/components/GifPicker.qml` (new): Recent/Saved sections, search, paging,
  tap-to-send, stage-with-caption.
- `src/telegram/MediaSend.{h,cpp}`: `OutgoingMediaKind::Animation` and an
  `inputMessageAnimation` builder.

**Tests:** `tests/tst_emoji_catalog.cpp` (new) — recent ordering, cap of 32, duplicate
collapse, corrupt settings value; `tests/tst_sticker_catalog.cpp` — GIF cursor prefix
handling and page merge; `tests/tst_media_send.cpp` — animation builder shape.

---

## 7. Priority

| Order | Work | Why first |
|---|---|---|
| P1 | Stage A (reactions: remove, picker, limit, live updates) | Reactions are the only shipped feature of this slice, and today they are write-once, stale and partly inert. Cheapest high-visibility correctness win. |
| P1 | Stage B (sticker catalog, panel, sticker send) | Largest user-visible absence: the client cannot send a sticker at all, and `StickerPicker.qml`/`StickerSetModel` are already written but unconnected. |
| P2 | Stage C (custom emoji + premium gating) | Required by Stage A's chips to stop lying about custom reactions, and by message text rendering. |
| P2 | Stage D (paid reactions) | Money path: only worth adding with the full pending/commit/rollback and uncertain-result handling, never a shortcut. |
| P3 | Stage E (animated playback) | Needs a renderer decision and a new dependency; static stickers are usable without it. |
| P4 | Stage F (emoji tab, GIFs) | Nice-to-have breadth; the emoji tab is cheap, GIF search is the most expensive remaining piece. |

Suggested split into shippable slices: A → (C-part-1: custom emoji chips) → B →
F-emoji → C-rest → D → E → F-gifs. Each slice ends with model-level tests and a manual
chat check.

---

## 8. Manual verification gates (not executed)

For each stage, on a real account, without touching the Dev package's data:

1. React to a message that has no reactions yet (entry point appears), react again
   (chip disappears), reach the chat's reaction limit (oldest chosen reaction makes
   room and returns if the add fails).
2. React from a second device and confirm the count changes here without reopening the
   chat.
3. Open the picker as a non-Premium account: no premium-only emoji, no custom emoji; as
   Premium: chat list or all-active-emoji appear.
4. Custom-emoji reaction renders its sticker; a deleted custom emoji id falls back to
   `🙂` and never collapses the chip.
5. Send a WebP, a TGS and a premium sticker; confirm the bubble type is a sticker (not a
   document) and the premium one is refused for a non-Premium account.
6. Recent list reflects the last sent stickers; favorite toggle survives a restart and a
   failure rolls the star back.
7. Open a set from a sticker message, install/uninstall it, and confirm the button state
   after a cancelled request.
8. Paid reaction: confirm amount, see the balance, cancel before commit (nothing
   spent), and simulate a post-commit failure (reported as uncertain, not as failed).

Instrumentation and any install workflow must stay off the user's Dev/Prod package per
the repository rules; device checks for this slice belong on an emulator or a separate
test application id.

---

## 9. Risks and open decisions

- **TGS renderer choice** (§6 Stage E) is the only item that may add a native
  dependency; it must be decided before promising animated stickers.
- **WebM alpha** is an accepted Android limitation; Windows should match it (thumbnail
  only) rather than attempt alpha compositing.
- **Reaction limit** is per chat and comes from `getChat.available_reactions`; caching
  it risks stale limits after a chat's settings change. Read it at toggle time, as
  Android does.
- **Paid reactions** must never be retried and must never be optimistic; a repeated
  commit can spend Stars twice.
- **Single source for reaction parsing** (G11): removing the QML parser is a
  behaviour-visible refactor, because the QML parser also drops zero-count entries and
  coerces types. Keep a test asserting the C++ path yields the same list for the same
  JSON before deleting the QML one.
- **Duplicate requests on rapid taps** are already an Android problem solved by a
  per-message mutex; Windows must add the equivalent or the limit/displacement logic
  will race with itself.

---

## 10. Out of scope

Sticker/lottie rendering of *premium* sticker sets beyond the send guard, story
reactions, channel statistics reaction graphs, bot inline results, and reaction-based
message search filters. Those belong to their own slices.

---

## 11. Verification status

This document is based only on reading the files listed in §1 and §3.1 plus
`docs/PORTING_AUDIT.md` and `docs/PORTING_ROADMAP.md` for project conventions. No
CMake configure, build, `ctest`, GUI launch, device command or git command was run, and
no claim is made that any of the described Windows code works at runtime.
