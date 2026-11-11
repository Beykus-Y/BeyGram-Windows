# Parity 04 — Composer, drafts, replies, edit mode, send/typing, scheduled/silent messages

Read-only audit. No GUI, CMake, Gradle or build step was executed, and no test was run.
Everything below comes from reading the source on both sides. Nothing here is a
build or test result.

Scope: the Android chat composer and everything it owns — per-chat drafts, reply
targets and quotes, edit mode, send and send failure, typing indicators, scheduled
and silent sends, and keyboard/composer input behaviour — compared with the Windows
Qt/QML client (`DraftStore`, `ReplyInfo`, `AppController`, `ChatsPage.qml`,
`MessageBubble.qml`).

---

## 1. Android files reviewed

| File | What it owns |
| --- | --- |
| `app/src/main/java/com/beykus/beygram/domain/model/MessageDraft.kt` | `MessageDraft(text, entities, replyToMessageId)`, `StoredMessageDraft(value, updatedAtMillis, synced)`, `MessageDraftSync.keepsUnsentLocal`, `MessageDraftCodec` (`v2` local format) |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdChatDrafts.kt` | Local-first drafts per account and per forum topic; `SharedPreferences` backup; debounced `SetChatDraftMessage`; conflict rules; `closed()` / `connectionReady()` / `signedOut()` |
| `app/src/main/java/com/beykus/beygram/domain/model/MessageSendOptions.kt` | `disableNotification`, `scheduledAtSeconds`, `viewOnce`, `sendNowConsent`, `MIN_SCHEDULE_LEAD_SECONDS = 10`, `scheduledTimeIsFuture()` |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ChatScreen.kt` | The composer: draft observation, reply banner, edit banner, `sendCurrentDraft(options)`, `typedDraft`, send-options long-press, scheduled-messages entry, keyboard/IME layout, `SwipeToReply` wiring, reply `jump()` |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ScheduledMessages.kt` | `SendOptionsDialog` (send now / silently / later + date-time picker), `ScheduledMessagesDialog` (5 s server refresh, edit/reschedule/send now/retry/cancel), `ScheduledMessageEditDialog` |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/SwipeToReply.kt` | Swipe-to-reply gesture: 56 dp trigger, 96 dp max pull, 0.35 resistance past trigger, haptic at arming |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/MessageActions.kt` | `EditBanner(original, isCaption, count, limit, error, onClose)` |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ChatPresence.kt` | `typingLabel(names)`, `AnimatedTypingRow`, `TypingRow` — named typists, "and N more" |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/FormattingTextField.kt` | Multi-line field (`maxLines = 5`), `KeyboardCapitalization.Sentences`, `focusRequest` re-focus on reply start, cursor adoption |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ComposerTextEdits.kt` | Emoji/backspace edits that also adjust text entities |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/DelayedSendStrip.kt` | Local (on-device) delayed-send controller + strip above the composer |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ComposerExtras.kt`, `ComposerExtraPickers.kt`, `MediaComposer.kt`, `AttachmentMenu.kt` | Extra composer modes (poll/location/contact/dice/music), attachments with the shared draft as caption, one-shot send options for stickers/extras |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/StealthSendGuard.kt` | Consent prompt when hidden-presence mode schedules sends |
| `app/src/main/java/com/beykus/beygram/domain/repository/ConversationRepository.kt` | UI port: `observeMessageDraft`, `updateMessageDraft`, `observeForumTopicDraft`, `updateForumTopicDraft`, `sendMessage(..., clearDraft, options)`, `editMessage(..., entities)`, `scheduledMessages`, `deleteScheduledMessage`, `rescheduleMessage`, `sendScheduledNow`, `retryMessages`, `observeTyping`, `startChatActivity` / `endChatActivity` |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdConversationActions.kt` | `sendText` + private `send()` (reply permission check, reply `topicId` inheritance, `sendOptions`, scheduled-state), `edit` (text vs caption, no-op guard), `scheduledMessages` / `deleteScheduledMessage` / `rescheduleMessage` / `sendScheduledNow`, `requireSchedulingEditable`, `hydrateReply` (`GetRepliedMessage`), `sendThreadReply` |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdConversationEffects.kt` | Outgoing `SendChatAction` loop: re-send every 4 s, typing lifetime 5 s, `ChatActionCancel` on end/policy change, upload activities |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdProfiles.kt` | Incoming `UpdateChatAction`: self-filter, per-sender map, 6 s expiry, name resolution, publish per chat |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdLibTelegramRepository.kt` | Wiring: `messageDrafts.receive/closed/connectionReady/signedOut`, `UpdateChatDraftMessage`, `UpdateChatAction` → typing flow, open-chat draft read |
| `app/src/main/java/com/beykus/beygram/data/delayed/DelayedSendPolicy.kt`, `DelayedSendQueue.kt`, `DelayedSendOutbox.kt` | On-device delayed queue (24 h TTL, 50 entries, `Queued/Dispatching/Failed/Uncertain`) |
| `docs/conversation-architecture.md` | Android's own description of replies, edit/delete, typing gateway and stealth/scheduled sends |

Supporting Android tests read for intent (not executed): `ui/screens/chat/*` instrumentation
tests around bubbles/composer, `StealthBoundaryTest` references in the architecture doc.

## 2. Windows files reviewed

| File | What it owns today |
| --- | --- |
| `src/telegram/DraftStore.h` / `.cpp` | One JSON file (`state/drafts.json`), `Draft{chatId, replyToMessageId, text, updatedAt}`, max 500, `QSaveFile`, trimmed-empty clears, `draftChanged` signal |
| `src/telegram/ReplyInfo.h` / `.cpp` | `ReplyTarget{None, Message, ExternalMessage, Story}`, `replyTargetFromMessage` (quote text), `inputReplyToMessageRequest` |
| `src/telegram/TelegramRequests.cpp` | `sendTextMessageRequest` (`clear_draft: true`, `reply_to`, `topic_id: null`), `editTextMessageRequest` |
| `src/telegram/TelegramRepository.cpp` | `sendText`, `sendMedia`, `editMessage`, `sendTyping`, `updateChatAction`, `updateMessageSendSucceeded/Failed`, `updateMessageContent`, `updateMessageEdited`, `openChat` / `closeChat` |
| `src/telegram/MessageListModel.cpp` / `.h` | `pending` / `failed` / `edited` / `replyToMessageId` / `replyPreview` roles, `markFailed`, `markEdited`, `updateContent`, `resolveReplyPreviews` (in-window only) |
| `src/telegram/TypingStatus.h` / `.cpp` | `sendTypingRequest`, `chatActionLabel`, unused `sendUploadPhotoRequest` / `sendUploadDocumentRequest` |
| `src/telegram/TelegramTypes.cpp` | `messageFromJson`: reply target, `sending_state` → pending/failed |
| `src/ui/AppController.h` / `.cpp` | `draftText`, `draftReplyToMessageId`, `saveDraft`, `clearDraft`, `beginReply`, `cancelReply`, `sendChatMessage`, `editChatMessage`, `notifyTyping`, `replyToMessageId`, `replyPreview`, `typingText`, `chatError` |
| `qml/pages/ChatsPage.qml` | `flushDraft` / `scheduleDraft` / `loadDraft` / `resetComposer` / `beginEdit` / `submitComposer`, 450 ms `draftTimer`, edit banner, reply banner, composer `TextArea`, Enter-to-send, Send/Save button, error row, attach-with-caption |
| `qml/components/MessageBubble.qml` | Quoted block, Reply/Edit/Copy/Forward/Delete menu, `sending...` / `Not sent` / `edited` labels |
| `qml/components/TypingIndicator.qml` | Presentational dots + label driven by `BeyGramApp.typingText` |
| `tests/tst_draft_store.cpp`, `tests/tst_reply_info.cpp`, `tests/tst_typing_status.cpp`, `tests/tst_chat_models.cpp`, `tests/tst_media_send.cpp` | Existing unit coverage for the pure parts |

---

## 3. Behaviour, side by side

### 3.1 Draft per chat

**Android.** A draft is a first-class object, not a text buffer:

- `MessageDraft(text, entities, replyToMessageId)` — plain text, rich-text entities and the reply target are one unit. `ChatScreen` reads it from `conversation.observeMessageDraft(chatId)` and every keystroke calls `updateDraft { it.copy(text = …) }`.
- `TdChatDrafts.update()` writes the `SharedPreferences` backup **synchronously** on each change (`StoredMessageDraft(next, now)`), marks the chat dirty, bumps a revision and schedules the **cloud** write with a 400 ms debounce (`WRITE_DELAY_MS`).
- Cloud write is `SetChatDraftMessage(chatId, topicId = null, DraftMessage{date, DraftMessageContentText(formattedText(text, entities)), InputMessageReplyToMessage(replyId)})`; an empty draft sends `draftMessage = null`, i.e. an explicit clear.
- Conflict rules (`receive()` + `MessageDraftSync.keepsUnsentLocal`): a **synced** backup always yields to TDLib (so a draft cleared on another device is never resurrected); an **unsynced** local edit wins over a cleared or not-newer remote draft; while the editor is open (`editedWhileOpen`) remote updates are ignored; `closed()` re-reads the cloud draft once if nothing was dirty.
- Drafts are **per account** (`chat_message_drafts` or `chat_message_drafts_<accountId>`) and **per forum topic** (`observeTopic` / `updateTopic` / `receiveTopic`, key `topic_<chatId>_<topicId>`), and survive process restart. Only `signedOut()` clears them.
- The draft is cleared only after a successful send (`updateDraft { MessageDraft() }` on `result.isSuccess`), and TDLib is told `clear_draft = true`. On failure the text stays in the composer.
- There is no draft cap and no "Draft:" badge in the Android chat list either.

**Windows.** `DraftStore` keeps `Draft{chatId, replyToMessageId, text, updatedAt}` in one JSON file (`state/drafts.json`, `version: 1`), trims to the 500 most recently edited, writes atomically with `QSaveFile`, and clears a chat when the trimmed text is empty. `AppController` exposes `draftText`, `draftReplyToMessageId`, `saveDraft`, `clearDraft`. QML drives it:

- `composer.onTextChanged` → `scheduleDraft()` → `draftTimer` (450 ms) → `BeyGramApp.saveDraft(chatId, text, BeyGramApp.replyToMessageId)`; empty text → `clearDraft`.
- `loadDraft()` on chat switch sets `composer.text = BeyGramApp.draftText(id)`.
- `resetComposer()` (called from the chat row before `openChat`, and when `currentChatId` becomes 0) flushes first, so switching via the chat list usually saves.
- `submitComposer()` calls `clearDraft()` **before** `sendChatMessage(text)`.

**Gaps**

1. **Account scoping.** Windows keeps one global file for all accounts. TDLib chat ids are per-account, so two accounts can collide on the same numeric id and show each other's text. Android is per account.
2. **Cloud drafts are never read or written.** No `SetChatDraftMessage`, no `draft_message` parsing, no `UpdateChatDraftMessage` handling anywhere in `src/`. Drafts never appear on the user's other devices and drafts written elsewhere never appear on Windows.
3. **Loss window on chat switch.** `GlobalSearchPage.onResultActivated` calls `BeyGramApp.openChat(chatId)` without `resetComposer()`, and `AppController::openChat` clears the reply target first. Text typed within the 450 ms debounce is dropped; `draftTimer.onTriggered` then refuses to save because `chatId !== currentChatId`.
4. **Reply target is stored but never restored** (see 3.2).
5. **No entities.** Formatting (bold/italic/code, custom-emoji entities) is lost on save; Windows has no formatting UI yet, so this is latent but blocks parity once formatting lands.
6. **No forum topics.** Topic drafts do not exist; Windows has no topic support at all.
7. **`removeChat` is dead code** — drafts for deleted/left chats linger until the 500-cap evicts them.
8. **No unsent/synced state**, so there is no way to tell "local edit not yet in Telegram" from "mirror of Telegram".
9. Windows treats a whitespace-only draft as "clear" (`text.trimmed().isEmpty()`) and trims again before sending, while the stored draft keeps its raw text; Android keeps the raw text in the draft and bounds only by code points, trimming only at send.

### 3.2 Reply preview and quote

**Android.**

- The reply target lives **inside the draft** (`replyToMessageId`), so it survives restart and is synced to the cloud draft. `clearReply()` is `updateDraft { it.copy(replyToMessageId = null) }`.
- Entry points: message action bar `onReply`, swipe-to-reply (`SwipeToReply`, enabled only when the composer is idle: not searching/deleting/selecting/editing/recording/sending, and only for `MessageSendState.Sent` messages), and a bot's `forceReply` keyboard.
- Swipe and action-bar reply **ask the server first**: `GetMessageProperties(chatId, id).canBeReplied`, otherwise `err_message_cannot_reply`. A slower check for an older swipe is discarded (`swipedReplyId`).
- The reply banner shows `chat_reply_label · sender` plus a 2-line `summary()` of the original, with a Close button that is disabled while `sending`; starting a reply bumps `replyFocusRequest` so the field takes focus and the keyboard opens.
- In the bubble, the quoted block shows `reply.senderName` + up to 2 lines of `reply.text`; **tapping it jumps** to the original (`jump()`: scroll + highlight if loaded, otherwise open the context window `contextChatId`/`contextMessageId`). Unavailable originals are explicit (`err_tdlib_message_unavailable`, `isUnavailable`).
- Reply previews for distant messages are hydrated with `GetRepliedMessage` (`hydrateReply`), once per message; the quote text from TDLib wins when present.
- Sending uses `InputMessageReplyToMessage(messageId, pollOptionId = "")` and inherits the original's `topicId`; `forumTopicId` is used only when there is no reply target. After a successful send the reply target is cleared only if it is still the one that was sent (`onReplyConsumed`).

**Windows.**

- `beginReply(messageId, preview)` stores `m_replyToMessageId`, `m_replyChatId`, `m_replyPreview` in memory only; `cancelReply()` clears them; `openChat` clears them whenever the chat changes; `closeChat` and `resetRepository` clear them.
- The banner is a single line: `"Replying: " + BeyGramApp.replyPreview` with a Cancel button. No sender name, no two-line summary, no focus request.
- `sendChatMessage` / `sendChatMedia` capture `m_replyToMessageId` and clear the target immediately (before the request completes).
- `ReplyInfo` parses `reply_to` (`messageReplyToMessage`, `messageReplyToExternalMessage`, `messageReplyToStory`), keeps `quote.text`, and `inputReplyToMessageRequest` builds `inputMessageReplyToMessage` with `quote: null`, `checklist_task_id: 0`, `poll_option_id: ""`.
- In the bubble, `replyPreview` renders in a tinted block with an accent bar; **tapping the quote starts a reply to that message** (`replyRequested(messageId, replyPreview)`), it does not jump.
- `MessageListModel::resolveReplyPreviews()` fills a preview only from a message already in the loaded window, only when `replyPreview` is empty, formatted `"sender: text"`. There is no `GetRepliedMessage` call, so replies to messages outside the loaded page render **no quote at all**.

**Gaps**

1. Reply target is not restored with the draft: `loadDraft()` sets only text, and `AppController::openChat` clears the reply. The first `saveDraft` after reopening then writes `replyToMessageId = 0`, erasing it from disk.
2. No server-side reply permission check (`canBeReplied`); the reply menu is always enabled and TDLib rejects at send time with a generic error.
3. No swipe-to-reply gesture.
4. Quote block semantics differ: tap = reply-to-that-message on Windows vs jump-to-original on Android. Android also has cross-chat/context-window fallback.
5. No reply hydration for messages outside the loaded window; no "unavailable original" state; no `story` / `external` reply rendering (parsed but never displayed).
6. Banner is text-only: no sender name, no truncation policy (full bubble text is passed as the preview), no "Replying" label styling from the design system.
7. Reply is cleared optimistically before the send result, so a failed send loses the target as well as the text.
8. No `topicId` inheritance (no topics yet) and no `pollOptionId` handling for poll-option replies.

### 3.3 Edit mode — cancel and save

**Android.**

- State: `editId`, `editText`, `editEntities`, `editBusy`, `editError` (all `rememberSaveable` per chat except busy/error); `editEntities` is seeded from the message (`LaunchedEffect(editId)`), and `closeEdit()` clears id/text/error.
- Entry: message action bar `onEdit` (gated by `GetMessageProperties.canBeEdited`), media-viewer `onEdit`. Starting an edit does not touch the draft; the composer switches its source to `editText`/`editEntities`.
- The `EditBanner` above the field shows the original text (or the caption banner), a `count / limit` counter that turns danger over the limit, the inline error, and a Close button; system Back closes edit before it closes the chat.
- The send control becomes a checkmark; `canSend` while editing requires `editText != message.text` and (`media != null` or non-blank text). Save calls `conversation.editMessage(chatId, id, text, entities)`; on success `closeEdit()`, on failure `editError = …` and **the edit stays open with the text intact**.
- `TdConversationActions.edit` re-checks `canBeEdited && isOutgoing && editable(content)`, bounds by the text or caption limit, refuses an empty text edit, **skips an unchanged edit** (Telegram answers `MESSAGE_NOT_MODIFIED`), uses `EditMessageText` or `EditMessageCaption` with `showCaptionAboveMedia` preserved, and does not insert a still-scheduled message into visible history.
- Editing a translated message invalidates its cached translation (`LaunchedEffect(messages)`).

**Windows.**

- State: `page.editingId` only. `beginEdit(messageId, text)` flushes the draft, clears the reply, sets `editingId`, fills the composer, focuses it and puts the cursor at the end. The edited text is **not** saved as a draft (and the old draft was just flushed/kept).
- Banner: a muted `"Editing message"` line plus a `Cancel` button wired to `resetComposer()` — which flushes a draft, resets `editingId`, clears the composer **and cancels the reply**, and resets `loadedChatId = 0` (so the next `chatStateChanged` may reload the chat's draft over the composer).
- `submitComposer()` → `editChatMessage(editingId, text)` then **immediately** `composer.text = ""`, `editingId = 0`. Failure only surfaces as the shared `chatError` row above the composer; the edited text is gone and the edit mode is closed.
- Repository: `editMessage` trims, drops empty text, sends `editMessageText` with `inputMessageText(clear_draft = false)`, and on success patches `updateContent(messageId, content, edited = true)`; on error sets `"Could not edit the message: %1"`.
- `MessageBubble` shows Edit for any outgoing message that is not pending/failed — including stickers/voice/polls that have no text — and no capability query happens before the menu is shown.

**Gaps**

1. Failed edit destroys the user's text and exits edit mode (Android keeps both).
2. No original-text banner, no counter, no inline edit error, no "edited" limit awareness.
3. No capability check before offering Edit (`canBeEdited`, `isOutgoing`, editable content type); TDLib errors become a generic banner.
4. No "unchanged edit" guard → avoidable `MESSAGE_NOT_MODIFIED` errors.
5. No caption-vs-text distinction (`EditMessageCaption` path missing) and no `showCaptionAboveMedia` preservation.
6. `Cancel` also cancels the reply and can reload the draft over the composer — surprising side effects.
7. Editing is text-only; entities are not preserved or edited.
8. No edit of a scheduled message.

### 3.4 Send and send failure

**Android.**

- `sendCurrentDraft(options)` is the single send path: refuses when `secretChatClosed`, `editId != null` or `inlineSending`; handles pending stickers, attachments, extra drafts, then plain text.
- Text is bounded first (`boundTelegramText(text, entities, limits.text)`), blank text is rejected (`err_message_empty`), and an over-limit draft shows `chat_truncated_notice` under the field.
- The reply permission is re-checked at send time; the request is `SendMessage(chatId, content, options, topicId = reply.topicId, replyTo = InputMessageReplyToMessage(...))`.
- `sending = true` puts a spinner on the send control (`SendControlFace`); the field is `locked` while busy; a "sending" state description is exposed to accessibility.
- The draft is cleared **only on success** (`updateDraft { MessageDraft() }`), the bot's force-reply keyboard is dismissed, and a scheduled send shows `chat_schedule_success`.
- On failure the draft stays, `error` shows `userMessage(..., err_chat_send_failed)`, the stealth fallback may offer "send now", and the Composer Launch overlay is cancelled. Failed messages render with `MessageSendState.Failed` and can be retried with `ResendMessages` (`retryMessages`, with `stealth.checkResend`).
- `clearDraft = false` is used deliberately for text injected from other apps/thread comments so the destination draft is untouched.

**Windows.**

- `submitComposer()` trims, refuses empty text, and for a new message does `clearDraft()` then `sendChatMessage(text)`.
- `AppController::sendChatMessage` clears `m_chatError`, captures `m_replyToMessageId`, calls `repository->sendText(text, replyTo)`, then clears the reply target and emits state.
- `TelegramRepository::sendText` trims again, drops empty text, and sends `sendMessage` with `inputMessageText(clear_draft = true)`, `reply_to`, `topic_id: null`, `options: null`, `reply_markup: null`, 120 s timeout. On the `message` response it adds the message, resolves reply previews and marks the chat read; on an `error` response it sets `"Could not send the message: %1"`.
- `updateMessageSendFailed` → `markFailed(oldId)` (bubble shows `Not sent`, bubble opacity 0.8) plus `"A message could not be sent: %1"`. `updateMessageSendSucceeded` → `replaceId`.
- There is no optimistic local echo, no retry action, no length limit, and no cancel.

**Gaps**

1. **Draft cleared before the send**: a failure loses the typed text (Android keeps it).
2. Reply target cleared before the result: a failure loses the reply too.
3. No text/caption limit enforcement or over-limit notice; TDLib errors surface as a banner.
4. No retry for a failed message (`ResendMessages` never used; `Not sent` is terminal).
5. No send-in-flight state on the send button; the composer stays interactive and the button can be pressed again (duplicate sends are prevented only by the empty-text check after the clear).
6. No optimistic pending bubble and no local-id → server-id mapping for text (only TDLib-driven updates).
7. `options: null` means silent/scheduling can never be attached (see 3.6), and `reply_markup: null` means bot keyboards are dropped.

### 3.5 Typing

Typing is covered here only as far as the composer owns it. The deeper audit of typing and
presence (notification-side typing, `InteractionPolicy`, online/last-seen) lives in
`docs/parity/07-notifications-typing.md`; the findings below agree with it.

**Android — outgoing.**

- `typedDraft(text)` runs on every keystroke: `conversation.startChatActivity(chatId, ChatActivity.Typing)` when non-blank, `endChatActivity(...)` when blank.
- `TdConversationEffects` keeps one activity loop per chat: sends `SendChatAction(chatId, topic_id = null, business_connection_id = "", ChatActionTyping)` immediately, re-sends every 4 s (`ACTIVITY_REFRESH_NS`), stops itself 5 s after the last keystroke (`TYPING_LIFETIME_NS`) and sends `ChatActionCancel`. Calling start again for the same activity only refreshes `touchedAt`, so keystrokes do not produce a request each.
- The loop also covers recordings (`ChatActionRecordingVoiceNote`, `…VideoNote`) and upload activities (`…UploadingPhoto/Video/VideoNote/VoiceNote/Document`), resumes uploads after typing ends, and is gated by `InteractionPolicy` (`refreshPolicy()` cancels and sends `ChatActionCancel` when typing is no longer allowed).

**Android — incoming.**

- `TdProfiles.updateTyping` handles `UpdateChatAction`: drops the account's own sender, keys by `MessageSenderUser`/`MessageSenderChat`, keeps insertion order, refreshes a 6 s expiry timer per typist, removes on any non-typing action, and publishes `List<String>` of resolved names (first name / chat title). `ChatScreen` shows `AnimatedTypingRow(typingNames)` with `typingLabel()` ("Alice is typing", "Alice and Bob are typing", "…and N more"), hidden while deleting/searching/selecting.
- Forum topics are deliberately not distinguished — one line per chat.

**Windows — outgoing.**

- `composer.onTextChanged` calls `BeyGramApp.notifyTyping()` on **every** text change (including paste and programmatic clears) whenever the trimmed text is non-empty → `AppController::notifyTyping` → `TelegramRepository::sendTyping` → `sendChatAction`/`chatActionTyping`.
- No debounce, no refresh loop, no lifetime, no `chatActionCancel` when the field is emptied or the chat is closed. One TDLib request per keystroke.

**Windows — incoming.**

- `processUpdate` handles `updateChatAction` only for `m_currentChatId`, maps `chatActionLabel()` (`typing…`, `sending a photo…`, `sending a file…`, otherwise empty) and emits `typingChanged`; `AppController` stores it in `m_typingText` and clears it on `openChat`/`closeChat`.
- `TypingIndicator` shows `BeyGramApp.typingText` plus three dots. No sender names, no per-user expiry, no "and N more".

**Gaps**

1. Outgoing typing is a request per keystroke — no throttle and no refresh loop; a fast typist generates dozens of `sendChatAction` calls.
2. No `ChatActionCancel` when the field becomes empty, when the chat is closed, or when typing is disallowed.
3. No typing lifetime, so the last action is whatever TDLib decides to expire (the indicator is only cleared by a later update).
4. Incoming typing shows a generic label with no names and no 6 s safety expiry — a missed cancel update leaves the line on screen until the next action or a chat switch.
5. No recording/upload activities; `sendUploadPhotoRequest` / `sendUploadDocumentRequest` are built and tested but never called.
6. No policy gate for typing (Android routes every chat action through `InteractionPolicy`).
7. No `topic_id`/`business_connection_id` on incoming filtering (outgoing request already sends null/empty correctly).

### 3.6 Scheduled and silent messages

**Android.**

- Long-press on the send control (or long-press on a sticker) opens `SendOptionsDialog`: time-zone line, **Send now**, **Send silently** (`disableNotification = true`), **Send later** (platform date picker → time picker, `isFuture()` requires `> now + 10 s`), and an explicit "scheduling unavailable in secret chats" note when `canSchedule` is false. Options are one-shot: nothing is remembered for the next send.
- `MessageSendOptions` is turned into `TdApi.MessageSendOptions{disableNotification, schedulingState = MessageSchedulingStateSendAtDate(at, 0)}` (`sendOptions()`), and `requireFutureSchedule` rejects a stale picker value.
- A scheduled send is **not** inserted into visible history (`if (message.schedulingState == null) history.put(...)`), shows `chat_schedule_success`, and skips Composer Launch.
- `ScheduledMessagesDialog` is the only queue UI, reads `GetChatScheduledMessages` (server is the source of truth) and refreshes every 5 s while open; per row: Retry (only when failed), Reschedule (date/time picker + `EditMessageSchedulingState`), Send now (`EditMessageSchedulingState(..., null)`), Edit (`EditMessageText`, stays out of history), Cancel (`DeleteMessages(revoke = false)`). Each action is guarded by `GetMessageProperties.canEditSchedulingState` (`err_schedule_edit_unavailable`).
- Hidden-presence ("stealth") mode layers a server-side scheduled send with a 15–120 s lead on top of every send path, with an explicit "send now anyway" consent (`sendNowConsent`); the local delayed queue (`data/delayed`) is a separate on-device convenience for plain text only (24 h TTL, 50 entries, `Queued/Dispatching/Failed/Uncertain`, strip above the composer, can restore the draft).

**Windows.** Nothing. There is no send-options menu, no long-press/secondary action on Send, no `scheduling_state` (only `ForwardMessages` writes `scheduling_state: null`), no `disable_notification` for normal sends, no `getChatScheduledMessages`, no `editMessageSchedulingState`, no `resendMessages`, and no scheduled-messages UI or entry point. The only silent path in the codebase is `ForwardMessages(..., silently)` whose default is `false` and which QML never sets.

**Gaps** — the whole feature area:

1. Silent send (`disable_notification`).
2. Send later (`MessageSchedulingStateSendAtDate`), including the 10 s lead rule and secret-chat exclusion.
3. Scheduled-messages list with edit / reschedule / send now / retry / cancel.
4. Excluding scheduled sends from visible history and from entrance animation.
5. Server-clock/stealth scheduling and the local delayed queue are Android-only (optional for Windows parity; note them as explicitly out of scope unless requested).

### 3.7 Keyboard and composer input

**Android.**

- The composer sits in `ChatScreenColumn(...).navigationBarsPadding().imePadding()`, so the field follows the IME; presence row and banners stay above the field.
- The field is `FormattingTextField` with `maxLines = 5`, `KeyboardCapitalization.Sentences`, `locked = busy || hidingComposerForLaunch`, `focusRequest` (bumped on reply start) and cursor adoption after emoji/backspace edits. Return inserts a newline; sending is the round send control.
- The send control is `focusProperties { canFocus = false }` so tapping it does not close the keyboard, has `onClickLabel`/`onLongClickLabel` semantics, and a `stateDescription` while sending.
- Focus loss closes the sticker panel; the field reports `boundsInWindow` for Composer Launch.
- Read-only chats get `ReadOnlyChatBar` instead of the composer; secret-chat-closed gets its own string.

**Windows.**

- Composer is a `TextArea` (`wrapMode: Wrap`, height 42–92 px with internal scrolling), `placeholderText: "Write a message"`, accent border on focus.
- `Keys.onPressed`: `Return` without `Shift` → `submitComposer()`; `Shift+Return` inserts a newline. This is the desktop convention and differs deliberately from Android (Enter = newline, button = send).
- `submitComposer()` calls `composer.forceActiveFocus()` after clearing, so focus is kept for the next message.
- The Send button is a `PrimaryButton` with `enabled: composer.text.trim().length > 0`, label `Save` while editing; there is no busy/disabled state during the request.
- No focus request when opening a chat or when starting a reply; no IME concerns (desktop), no emoji/sticker panel, no formatting toolbar, no read-only composer state, no character counter.

**Gaps**

1. No busy/locked state: the field and Send stay live during an in-flight send.
2. No focus when starting a reply or opening a chat with a draft.
3. No read-only/closed-chat composer replacement (sending into a channel without post rights is only rejected by TDLib).
4. No character counter / over-limit notice.
5. No `Shift+Enter`/`Enter` discoverability affordance (hint text) — minor.
6. Draft is not restored into a focused, cursor-at-end field.

---

## 4. Parity matrix

| Capability | Android | Windows | Status |
| --- | --- | --- | --- |
| Draft per chat, survives restart | Yes (prefs, per account) | Yes (JSON, global) | Partial |
| Draft account scoping | Per account | Global file | **Gap** |
| Draft cloud sync (`SetChatDraftMessage`) | Yes, 400 ms debounce | None | **Missing** |
| Draft conflict rules (synced/unsynced, edited-while-open) | Yes | None | **Missing** |
| Draft entities (rich text) | Yes | No | **Missing** |
| Draft per forum topic | Yes | No topics | **Missing** |
| Draft flushed on every chat switch | Yes (synchronous) | Partial (450 ms; lost via global search) | **Gap** |
| Reply target persisted with draft | Yes | Stored, never restored | **Gap** |
| Reply preview in composer (sender + 2 lines) | Yes | One line, no sender | Partial |
| Reply permission check (`canBeReplied`) | Yes (start + send) | None | **Missing** |
| Swipe-to-reply | Yes | No | **Missing** |
| Quoted block in bubble | Yes | Yes | Parity |
| Quote tap = jump to original | Yes (scroll/highlight/context window) | Tap = reply | **Divergent** |
| Cross-window reply hydration (`GetRepliedMessage`) | Yes | No | **Missing** |
| Unavailable-original state | Yes | No | **Missing** |
| Story / external reply rendering | Yes | Parsed, not rendered | Partial |
| Edit mode: open, banner with original, counter | Yes | Bare "Editing message" | Partial |
| Edit: capability + content-type gate | Yes | No | **Missing** |
| Edit: keep text and mode on failure | Yes | No (text lost, mode closed) | **Gap** |
| Edit: unchanged-edit guard | Yes | No | **Missing** |
| Edit: caption vs text, caption-above preserved | Yes | Text only | **Missing** |
| Edit: scheduled message | Yes | N/A (no scheduled) | **Missing** |
| Send: draft kept on failure | Yes | Cleared before send | **Gap** |
| Send: reply kept on failure | Yes | Cleared before send | **Gap** |
| Send: text/caption limits + notice | Yes | None | **Missing** |
| Send: busy state on send control | Yes | No | **Gap** |
| Send: retry failed message (`ResendMessages`) | Yes | No | **Missing** |
| Typing: throttled/refreshed outgoing action | Yes (4 s loop, 5 s life) | One request per keystroke | **Gap** |
| Typing: cancel on empty/close | Yes | No | **Missing** |
| Typing: named incoming typists + expiry | Yes (6 s, multi-user) | Generic label, no expiry | **Gap** |
| Recording/upload activities | Yes | Builders exist, unused | **Missing** |
| Typing policy gate | Yes | No | **Missing** |
| Silent send | Yes | No | **Missing** |
| Send later | Yes | No | **Missing** |
| Scheduled messages list (edit/reschedule/send now/cancel) | Yes | No | **Missing** |
| Stealth/server scheduling, local delayed queue | Yes | No | **Out of scope** (Android-only feature) |
| Keyboard: Enter vs Shift+Enter | Enter = newline, button sends | Enter sends | Intentional divergence |
| Composer focus on reply/open | Yes | No | **Gap** |
| Read-only composer state | Yes | No | **Gap** |

---

## 5. Priority

**P0 — correctness/privacy, fix before the next parity milestone**

1. Account-scope `DraftStore` (per-account file), or drafts leak between accounts.
2. Flush the draft on *every* chat change (including `GlobalSearchPage`), and stop clearing the reply target before the flush.
3. Restore the draft's reply target when loading a draft.
4. Keep the draft and the reply target when a send fails; keep the edited text and edit mode when an edit fails.

**P1 — core composer parity**

5. Send-options entry: silent send + send later (`disable_notification`, `MessageSchedulingStateSendAtDate`, 10 s lead, secret-chat exclusion) and the scheduled-messages list.
6. Throttled outgoing typing (single loop: send now, refresh every 4 s, 5 s life, cancel on empty/close/chat switch) and incoming typing with names + expiry.
7. Send/edit limits (text/caption), over-limit notice, unchanged-edit guard, `canBeEdited`/content-type gate, caption edit path.
8. Reply permission check at reply start and at send; reply preview with sender and truncation.
9. Busy/locked composer state and retry for failed messages.

**P2 — richer parity**

10. Draft cloud sync (`SetChatDraftMessage` + `UpdateChatDraftMessage` + `draft_message` on open) with synced/unsynced conflict rules.
11. Cross-window reply hydration (`GetRepliedMessage`) and unavailable-original state; quote-tap = jump (with a context view) instead of reply.
12. Swipe-to-reply gesture, composer focus on reply/open, read-only composer, character counter.
13. Rich-text entities in drafts and edits (prerequisite: a formatting UI).

**P3 — explicitly deferred**

14. Forum-topic drafts, stealth scheduling, on-device delayed queue, bot keyboards/`forceReply`, `pollOptionId` replies.

---

## 6. Port plan

No build or test was run for this plan; every item below is a proposal.

### Phase 1 — stop losing data (P0)

- **`src/telegram/DraftStore.{h,cpp}`**: add an account id to the constructor (file becomes
  `state/drafts-<safeAccountId>.json` via `AppPaths::safeComponent`) and keep `version` at 2 with a
  migration that reads the old `state/drafts.json` once for the first account. Add `entities`
  (JSON array) and `synced`/`updatedAt` fields now, even if entities are empty.
- **`src/ui/AppController.{h,cpp}`**: construct `m_drafts` from the account registry's active account
  and reload on account switch; add `Q_INVOKABLE void saveDraftAndReply(...)` or extend
  `saveDraft` so the reply target and the preview travel together; add
  `Q_INVOKABLE void flushDraftFor(qint64 chatId, const QString& text, qint64 replyTo)`;
  do **not** clear `m_replyToMessageId` in `openChat` until the previous chat's draft was saved.
- **`qml/pages/ChatsPage.qml`**: call `page.flushDraft()` from `onChatStateChanged` *before*
  `loadDraft()`; expose `flushDraft` for `Shell.qml`/`GlobalSearchPage` to call before
  `BeyGramApp.openChat`; in `loadDraft()` restore
  `BeyGramApp.beginReply(draftReplyToMessageId(id), storedPreview)` when the stored id is non-zero
  (needs a `draftReplyPreview` accessor or a stored preview string).
- **`qml/pages/ChatsPage.qml` `submitComposer()`**: stop calling `clearDraft()` before the send;
  clear the draft only from a `chatStateChanged`/`messageSent` confirmation, or add an
  `AppController::messageSent(qint64 chatId)` signal emitted from `sendText`'s success branch.
- **`src/telegram/TelegramRepository.cpp` `sendText`/`editMessage`**: add
  `void messageSendFinished(qint64 chatId, bool ok)` (or reuse `errorOccurred`) so QML can
  re-show the text/reply on failure; keep `m_replyToMessageId` until the result.
- **Tests**: extend `tests/tst_draft_store.cpp` with account isolation, reply-target round trip,
  entity round trip, and "clear on empty" per account; add a `tst_app_controller` case (or a
  draft-only helper) asserting that a failed send leaves `draftText`/`draftReplyToMessageId`
  unchanged. QML logic can be covered by a `qmltestrunner` scenario if the project already uses it
  (not verified here).

### Phase 2 — send options and typing (P1)

- **New `src/telegram/SendOptions.{h,cpp}`** (pure, like `TypingStatus`): `struct SendOptions
  { bool disableNotification; qint64 scheduledAtSeconds; }`, `scheduledState(at)`,
  `sendOptionsJson(options)`, `bool scheduledTimeIsFuture(now, at)` with
  `kMinScheduleLeadSeconds = 10`.
- **`src/telegram/TelegramRequests.cpp`**: replace `options: null` in
  `sendTextMessageRequest`/media with `sendOptionsJson(...)`; add
  `getChatScheduledMessagesRequest`, `deleteScheduledMessageRequest` (reuse
  `deleteMessagesRequest(revoke=false)`), `editMessageSchedulingStateRequest`,
  `resendMessagesRequest`, and `getRepliedMessageRequest`.
- **`src/telegram/TelegramRepository.{h,cpp}`**: `sendText(text, replyTo, SendOptions)`,
  `sendMedia(..., SendOptions)`, `scheduledMessages(chatId)` → a `ScheduledMessagesModel`
  (new `src/telegram/ScheduledMessagesModel.{h,cpp}` mirroring `PinnedMessages`), `reschedule`,
  `sendScheduledNow`, `cancelScheduled`, `retryFailed(messageId)`, and reply hydration
  (`getRepliedMessage` filling `ReplyPreviewRole` for out-of-window targets, plus an
  `unavailable` flag).
- **`src/telegram/TypingStatus.{h,cpp}`**: add `sendCancelRequest(chatId)` and keep the upload
  builders; **`TelegramRepository`**: replace the per-call `sendTyping()` with a `QTimer`-driven
  `TypingBeacon` (`src/telegram/TypingBeacon.{h,cpp}`: start/refresh/stop, 4 s refresh, 5 s life,
  cancel on stop) and a per-chat incoming typing registry with 6 s expiry and sender names
  (`ChatAction` already carries `sender_id`).
- **`src/ui/AppController.{h,cpp}`**: `Q_INVOKABLE void sendChatMessage(const QString&, bool silent,
  qint64 scheduledAt)`, `notifyTypingStopped()`, `typingText` composed from names, and
  `Q_PROPERTY` for the scheduled model.
- **`qml/pages/ChatsPage.qml`**: add a `SendOptionsDialog` (send now / silently / later with a
  `DatePicker`-less QML date+time picker), a `ScheduledMessagesDialog` (list + actions), a
  long-press/right-click on Send, an over-limit counter, and a busy state
  (`enabled: !sending && text.trim().length > 0`).
- **Tests**: new `tests/tst_send_options.cpp` (lead rule, JSON shape, secret-chat exclusion helper),
  `tests/tst_scheduled_model.cpp`, `tests/tst_typing_beacon.cpp` (refresh/life/cancel timing with
  `QSignalSpy` + a fake transport), extend `tests/tst_reply_info.cpp` for
  `getRepliedMessage` parsing and `tests/tst_chat_models.cpp` for hydrated/unavailable previews.

### Phase 3 — reply and edit UX (P1/P2)

- **`src/telegram/ReplyInfo.{h,cpp}`**: add `canBeRepliedRequest(chatId, messageId)`
  (`getMessageProperties`) and a `ReplyTarget` preview struct with sender/summary; keep the
  existing parser.
- **`src/telegram/TelegramRepository.cpp`**: `beginReply` path gains a permission check and a
  `replyRejected(chatId, messageId)` signal; `editMessage` gains the unchanged guard, the
  text/caption switch (`editMessageCaption`), `showCaptionAboveMedia` preservation and a
  capability pre-check; `sendText` inherits `topicId` from the reply target once topics exist.
- **`qml/pages/ChatsPage.qml`**: reply banner with sender + 2-line elided summary; `beginEdit`
  keeps a copy of the original text for the banner; `submitComposer()` restores the edit text on
  failure and keeps `editingId`; `resetComposer()` no longer cancels the reply when cancelling an
  edit.
- **`qml/components/MessageBubble.qml`**: quoted block click → `jumpRequested(messageId, chatId)`
  (scroll + highlight, with a context view for other chats) instead of `replyRequested`; Edit
  visibility driven by a `canEdit` model role; `replyUnavailable` styling.
- **Tests**: extend `tests/tst_reply_info.cpp` (permission request shape), `tst_chat_models.cpp`
  (reply jump/highlight helper if extracted), and add a pure `EditGuard` unit
  (unchanged/empty/limit/caption) if the logic is moved out of the repository.

### Phase 4 — cloud drafts (P2)

- **`src/telegram/DraftStore.{h,cpp}`**: add `synced`, `remoteUpdatedAt`, `setRemote(...)`,
  `markSynced(...)` and a `keepsUnsentLocal` equivalent; keep the file write synchronous on edit
  and add a 400 ms debounced cloud writer (a `QTimer` in `DraftStore` or a small
  `DraftSync` class).
- **`src/telegram/TelegramRequests.cpp`**: `setChatDraftMessageRequest(chatId, text, replyId,
  date)`, `getChatDraftMessageRequest(chatId)`.
- **`src/telegram/TelegramRepository.cpp`**: handle `updateChatDraftMessage` and
  `draft_message` from `getChat`/`openChat`; forward to `AppController` → `DraftStore`.
- **`src/ui/AppController.{h,cpp}`**: emit `draftChanged(chatId)` when the store changes so QML can
  refresh an open composer without polling.
- **Tests**: extend `tests/tst_draft_store.cpp` (synced/unsynced conflict matrix, remote clear,
  clock skew) and add `tests/tst_draft_sync.cpp` with a fake transport for debounce/cancel.

### Phase 5 — gestures and polish (P2/P3)

- Swipe-to-reply in the QML `ListView` delegate (`DragHandler`/`MouseArea` with a 56 px trigger,
  haptic-free equivalent: visual arming), focus request on reply/open, read-only composer bar,
  character counter, forum-topic drafts, delayed queue.

---

## 7. Constraints for whoever implements this

- The connected phone holds the user's real account: no instrumentation tests, no
  `connectedAndroidTest`, no reinstall, no data clear. Any Android verification is by reading and
  by the user's own UI actions.
- Windows verification for this area must stay in the existing pure-unit test targets
  (`tst_draft_store`, `tst_reply_info`, `tst_typing_status`, `tst_chat_models`) plus new ones of
  the same shape. Nothing here has been built or run.
- Drafts and reply targets are user content: keep them out of logs (the current `DraftStore`
  already logs only counts/errors — preserve that) and out of diagnostics.
