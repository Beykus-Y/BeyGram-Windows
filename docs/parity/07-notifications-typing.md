# Parity 07 — Notifications, per-chat settings, mentions, typing and presence

Read-only audit of the Android reference implementation versus the Windows client.

- Repository: `F:\BeyGram Windows`
- Method: static reading of Android Kotlin sources under `app/src/main/java/...` and Windows C++/QML under `src/`, `qml/`, `tests/`.
- **No build, CMake configure, test run, GUI launch or device interaction was performed.** Every statement below is a code-reading claim, not an observed behaviour. No files were modified except this document.

---

## 1. Android files reviewed

### 1.1 Notification delivery

| File | Role |
|---|---|
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdNotifications.kt` | Authoritative notification source: consumes TDLib's notification API, groups, snapshots, alert ids, previews, chat metadata |
| `app/src/main/java/com/beykus/beygram/notifications/NotificationRuntime.kt` | Process-level presenter: combines notification/presentation/auth/settings state, watermarks, dismissal retry, reply outbox, secondary accounts |
| `app/src/main/java/com/beykus/beygram/notifications/AndroidNotificationRenderer.kt` | Builds and posts `MessagingStyle` notifications, channels, stale-entry reconciliation, reply action, failure notes |
| `app/src/main/java/com/beykus/beygram/notifications/NotificationPreferences.kt` | Per-account preferences (`enabled`, `background`, `previews`), handled/dismissed/pending watermarks, intent and reply session tokens |
| `app/src/main/java/com/beykus/beygram/notifications/NotificationIntents.kt` | `PendingIntent` identity: per-account URI paths, tokens, chat/dismiss/reply/settings/proxy intents |
| `app/src/main/java/com/beykus/beygram/notifications/NotificationReceivers.kt` | Boot/package-replaced reconcile, dismiss receiver, reply receiver with `goAsync()` |
| `app/src/main/java/com/beykus/beygram/notifications/TelegramNotificationService.kt` | Foreground service, start/stop eligibility, pending-delivery wake lock, safety re-check |
| `app/src/main/java/com/beykus/beygram/notifications/ReplyOutbox.kt` | Durable reply queue (fsync, TTL, attempt counting) |
| `app/src/main/java/com/beykus/beygram/notifications/NotificationReplyToken.kt` | HMAC routing token for reply actions |
| `app/src/main/java/com/beykus/beygram/notifications/StealthReplyPolicy.kt` | Whether a notification reply may be attempted while presence is hidden |
| `app/src/main/java/com/beykus/beygram/security/NotificationPreviewPolicy.kt` | What a notification may reveal (secret chat, unresolved metadata, protected content) |
| `app/src/main/java/com/beykus/beygram/domain/model/TelegramNotification.kt` | `TelegramNotice`, `TelegramNoticeGroup`, `TelegramNotifications`, `NotificationPresentation`, `NotificationOpenRequest` |

### 1.2 Per-chat settings, mute, mentions

| File | Role |
|---|---|
| `app/src/main/java/com/beykus/beygram/data/tdlib/ChatMuteState.kt` | Effective mute incl. inherited scope default, scope map, `isKnown` |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdLibTelegramRepository.kt` | `setChatMuted`, `chatNotificationPreferences`, `setChatNotificationPreferences`, `UpdateChatNotificationSettings`, `UpdateScopeNotificationSettings`, `UpdateChatAction`, `setAppForeground`, opened-chat tracking, saved sounds |
| `app/src/main/java/com/beykus/beygram/domain/model/ChatNotificationPreferences.kt` | Per-chat model: sound/`useDefaultSound`, preview/`useDefaultShowPreview`, mentions/`useDefaultDisableMentionNotifications` |
| `app/src/main/java/com/beykus/beygram/domain/repository/ConversationRepository.kt` | Contract for per-chat notification preferences, app foreground, chat activity |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ChatSettingsSheet.kt` | Per-chat UI: sound picker (default/silent/saved sounds), preview switch, mentions switch, reset |
| `app/src/main/java/com/beykus/beygram/ui/screens/chats/ChatActionsSheet.kt` | Chat list action: mute/unmute |
| `app/src/main/java/com/beykus/beygram/ui/screens/notifications/NotificationSettingsScreen.kt` | Global toggles (messages, background), channel state, battery exemption, permission request |
| `app/src/main/java/com/beykus/beygram/data/settings/StealthSettingsStore.kt` | `hideTyping`, `hideOnline` and per-chat exceptions feeding `InteractionPolicy` |

### 1.3 Typing and presence

| File | Role |
|---|---|
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdConversationEffects.kt` | The only gateway for read receipts, content-open, **online presence** and **outgoing chat actions**: 4 s refresh, 5 s typing lifetime, cancel, uploads, policy gate |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdProfiles.kt` | Incoming `UpdateChatAction` roster: per-sender map, 6 s expiry, own-user filter, name resolution |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ChatPresence.kt` | Presence line, last-seen text and clock, 1/2/3/N typing labels, animated typing row |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ChatScreen.kt` | `typedDraft`: start/end typing on every text change |
| `app/src/main/java/com/beykus/beygram/data/presence/LastSeenSettingsStore.kt` | Relative vs exact last-seen display |

### 1.4 Android tests read (for expected semantics)

`app/src/test/java/com/beykus/beygram/notifications/`: `AccountNotificationIsolationTest.kt`, `AccountNotificationRoutingTest.kt`, `NotificationReplyTokenTest.kt`, `ReplyOutboxTest.kt`, `SecondaryBackgroundEligibilityTest.kt`, `StealthReplyPolicyTest.kt`; plus `data/tdlib/ChatMuteStateTest.kt`, `data/tdlib/ChatNotificationPreferencesTest.kt`.

Android's own reference document is `docs/notifications.md` (delivery, privacy, dismiss, reply, settings), which matches the code read here.

---

## 2. Android behaviour reference

### 2.1 Source of truth: the TDLib notification API, not message updates

`TdNotifications` never infers an alert from `UpdateNewMessage` or unread counts. It:

1. Sets `notification_group_size_max = 10` and `notification_group_count_max = 25` once authorized.
2. Treats `UpdateActiveNotifications` as a full snapshot (`snapshot = true`), replacing all groups, dropping call groups, and re-preserving a still-pending `alertId` for the same chat.
3. Merges `UpdateNotificationGroup` incrementally: removes `removedNotificationIds`, adds `addedNotifications`, dedupes by id, sorts by id, keeps the newest 10. A group disappears when `totalCount == 0` or the merged list is empty.
4. Computes the alert id from `notificationSoundId != 0` **and** `!isSilent` **and** `id > previous max`. An unposted alert survives later content updates while its notice exists.
5. Handles `UpdateNotification` (content edit, silent), `UpdateHavePendingNotifications` (drives the delivery wake lock), `UpdateChatTitle`, `UpdateChatPermissions`, `UpdateChatHasProtectedContent`.
6. Resolves metadata asynchronously with `GetChat`: title, `isGroup` (basic group / supergroup), `isSecret`, archived, `replyable` (not secret, not channel, `canSendBasicMessages`), `hasProtectedContent`. Until it arrives, `metadataReady = false` and the title is hidden.

Because TDLib decides *whether* something notifies, Android automatically honours: server-side mute, `use_default_mute_for`, scope defaults, mention-only settings, silent messages, scheduled/edited notices, cross-device reads, and per-chat sound ids. Windows does not get any of this for free (see §4).

### 2.2 Routing and identity

- Tray tag is stable per account and group: `beygram_message_<groupId>` for the legacy account, `beygram_message_<accountId>_<groupId>` otherwise (`accountNotificationTag`). Two native accounts can never collide.
- Chat intent: `PendingIntent.getActivity` to `MainActivity` with action `OPEN_NOTIFICATION_CHAT`, a unique data URI `beygram://<pkg>/chat/<chatId>/<groupId>/<maxId>/<topicId>` (or `.../account/<id>/chat/...`), extras `chatId/accountId/groupId/maxId/topicId` and a private `intentToken`. Flags: `UPDATE_CURRENT | IMMUTABLE`, `CLEAR_TOP | SINGLE_TOP`.
- The Activity re-validates the token **and** recomputes the expected URI path from the extras, so a tampered `accountId` extra cannot reroute an existing link (`accountForChatIntent`).
- Dismiss intent: broadcast to `NotificationDismissReceiver`, path `dismiss/<groupId>/<maxId>` (+account prefix), validated the same way.
- Reply intent: broadcast to `NotificationReplyReceiver`, path `reply/<groupId>` (+account prefix), one identity per group, `FLAG_ONE_SHOT`, mutable only because `RemoteInput` needs it. The token is an HMAC over session + chat/group/max/replyTo/topic (`replyRoutingToken`), so a kept action from an older session is refused.
- A notification tap routes to the exact chat, including a chat outside the main list, and preserves an unconsumed open request across Activity recreation.

### 2.3 Mute, mentions and per-chat settings

- Effective mute: `ChatMuteState` keeps `useDefaultMuteFor`/`muteFor` per chat plus `muteFor` per scope (`Private`, `Group`, `Channel`). A chat that inherits is muted when its scope is muted; `isKnown()` distinguishes "not yet known" from "not muted" so a cold list does not notify prematurely.
- Mute toggle writes only mute fields, preserving sound/preview/mentions (`setChatMuted`: clone existing settings, `useDefaultMuteFor = false`, `muteFor = Int.MAX_VALUE` or `0`, `SetChatNotificationSettings`).
- Mentions are **not** a client-side detector. TDLib groups and `disable_mention_notifications` decide it. The per-chat sheet exposes `disableMentionNotifications` with a `useDefault…` flag; while inherited, the sheet shows the scope's value and a save does not turn it into an override.
- Per-chat sheet (`ChatSettingsSheet`): sound = default / silent (`soundId = 0`, `useDefaultSound = false`) / one of `GetSavedNotificationSounds`; preview switch; mentions switch; reset to `ChatNotificationPreferences()`.
- Global screen (`NotificationSettingsScreen`): messages on/off, background connection on/off, per-channel blocked state, permission request, battery-exemption request. Sound is owned by the Android Messages channel, not by an app switch.

### 2.4 Previews, sound, visibility

- `NotificationPreviewPolicy`: secret chat or unresolved metadata ⇒ hide title (shows `BeyGram`); protected content or secret or unresolved ⇒ hide text **and** sender; `previews` off or TDLib `show_preview = false` ⇒ hide text.
- `MessagingStyle` with the conversation title as `conversationTitle`, `setGroupConversation(isGroup)`, one message per notice with the real sender name (or `Telegram` when hidden).
- Public (lock-screen) version always reveals only "BeyGram / New message".
- Sound: silent when the group is not fresh (`alertId <= handled`) or the chat is archived and `archiveSound` is off; `setOnlyAlertOnce(!fresh)`; priority high; `setNumber(totalCount)`; `autoCancel`.
- Reply action is added only when the title is visible (`!hideContent`) and the chat is `replyable`. Smart replies are disabled.
- Subtext carries the account display name for secondary accounts and a "reply not sent" note per group.

### 2.5 Click, dismiss, reply

- **Click** → open the exact chat/topic in the right account (§2.2).
- **Dismiss (swipe)** → write `dismissed_<group>` and `pending_<group>` watermarks immediately, cancel the tray entry, then call `RemoveNotificationGroup(groupId, maxId)` with bounded retry: 4 attempts, 2 s × 2^n, then 30 s × 2^round backoff, max 3 rounds, reopened on `Ready` or reconnection. A newer dismissal never regresses the watermark.
- **Reply** → text is validated against the session token, written to a durable outbox, then sent with `sendMessage(chatId, text, replyToMessageId, forumTopicId)`. A rejected reply target falls back to a plain message, except in a forum without a stored topic (never silently land in General). Success dismisses the group; failure marks the notification and retries (5 attempts, 60 s, 24 h TTL). Stealth `hideOnline` allows only the direct attempt from the notification; a later automatic attempt is dropped with a visible failure.
- **Sign-out** clears handled/dismissed/pending watermarks and rotates the reply token, so group ids restarting for the next account cannot inherit old state.

### 2.6 Foreground / open-chat suppression

`NotificationRuntime` combines `notifications`, `notificationPresentation`, `authState`, `preferences.state` and an invalidation counter:

- Post only when `config.background || presentation.foreground`.
- Skip a group when unauthorized, disabled, no permission, no notices, or `presentation.foreground && group.chatId in presentation.openedChats`.
- A deliberate suppression **consumes** the watermark (`handled(group, maxId)`), so re-enabling later does not ring for old messages. A signed-out state consumes nothing.
- `presentation.foreground` is set by `setAppForeground`; `openedChats` by `loadChat`/`closeChat`. So a backgrounded app still notifies for the chat that is open when the app returns to foreground only if the app is in the foreground.

### 2.7 Typing — outgoing

- `ChatScreen.typedDraft` calls `startChatActivity(chatId, Typing)` on every non-blank change and `endChatActivity(…, Typing)` when the field becomes blank.
- `TdConversationEffects.startActivity` starts one loop per chat. The loop re-sends `SendChatAction(chatId, null, "", ChatActionTyping)` every **4 s** while the last keystroke is younger than **5 s**; each new keystroke only refreshes `touchedAt` (no extra request storm). On expiry, policy block, replacement or explicit end it sends `ChatActionCancel`.
- Other activities are supported and replace the current one: recording voice/video note, uploading photo/video/video note/voice/document. Uploads are tracked per message and end when TDLib reports sent/failed; typing/recording resume the newest upload afterwards.
- `refreshPolicy()` immediately cancels typing in chats that no longer allow it; `signedOut()` cancels everything. All sends happen under the same lock as the decision, so a cancel cannot overtake a newer activity.

### 2.8 Typing — incoming, and presence

- `TdProfiles.updateTyping` keeps a per-chat map keyed by `u<userId>` / `c<chatId>`, ignores the user's own actions (once `myId` is known), keeps insertion order, and expires each typist after **6 s**. Non-typing actions remove the sender.
- `ChatPresence.typingLabel`: 1 name, 2 names, 3 names, or "first, second and N more". Blank name ⇒ "someone". Animated row retains names through the exit animation.
- Presence: `TdConversationEffects.updatePresence` sets TDLib option `"online"` to `foreground && policy.allows(OnlinePresence)`, only on change, retrying on error. `hideOnline` therefore actually hides presence.
- Chat header shows online / last seen (relative or exact per `LastSeenSettingsStore`, recomputed every 30 s) or group online member count.

---

## 3. Windows current status

### 3.1 Notification path (one tray balloon per message)

- `src/telegram/NotificationFormatter.{h,cpp}` — pure function `notificationForMessage(message, chatTitle, isPrivateChat, currentChatId, chatMuted)`.
  - Suppresses: outgoing, current chat, muted, invalid ids, and any content type outside a 15-type whitelist (service messages and unknown types never notify).
  - Title = chat title, else `Telegram`/`Chat`. Body = text or caption, whitespace collapsed, `fallbackLabel` for media, truncated to 180 chars + `…` (surrogate-safe).
  - **Not present:** sender name, mention flag, silent flag, per-chat preview/sound, secret-chat or protected-content masking, grouping, account identity.
- `src/telegram/TelegramRepository.cpp:1036-1049` — `notificationRequested(title, body)` is emitted directly from `updateNewMessage`. Mute comes from `ChatData::muted` (`TelegramTypes.cpp:385-388`), which is `notification_settings.mute_for > 0` only.
- `src/ui/AppController.cpp:643-659` — `showNotification` lazily creates a `QSystemTrayIcon`, checks `settings.enabled`, non-empty title/body, `QSystemTrayIcon::isSystemTrayAvailable()`, then `showMessage(title, showPreview ? body : "New message", Information, 6000)`.
  - No `activated` handler, no click routing, no actions, no inline reply, no grouping/dedup, no sound control, no avatars, no window-focus awareness.
- `src/ui/NotificationSettingsStore.{h,cpp}` — JSON `{"version":1,...}` at `state/notification-settings.json` (app-global, not per account; `AppController.cpp:99-101`). Fields: `enabled`, `showPreview`, `sound`, `notifyMutedChats`. Robust parsing (only real JSON booleans, per-field defaults), `QSaveFile`, `changed()` only on real change.
  - **`sound` and `notifyMutedChats` are persisted but never read by any consumer** (verified by repository-wide search).
- `qml/pages/SettingsPage.qml:107-150` — only two toggles (desktop notifications, message preview) plus a static note.
- `qml/components/ChatNotificationSettings.qml` — a complete per-chat dialog (mute / sound / preview, `saveRequested(muted, sound, preview)`), listed in `CMakeLists.txt:91`, but **never instantiated anywhere** and with no C++ property behind it.
- Chat list: `ChatData::muted` is passed to `ChatRow.muted` (`qml/pages/ChatsPage.qml:229`), but `ChatRow.qml` never renders it (no muted icon, no dimmed badge). There is no mute action in the chat list (no context menu at all).

### 3.2 Mentions

No mention support. `updateChatUnreadMentionCount` is only listed as coalescable in `src/telegram/TelegramUpdateScheduler.cpp:85`; no model field, no handler, no badge, no per-chat mention switch, no detection in the formatter.

### 3.3 Typing

- Send: `qml/pages/ChatsPage.qml:725-729` calls `BeyGramApp.notifyTyping()` on **every** text change; `AppController::notifyTyping` (`AppController.cpp:933-937`) calls `TelegramRepository::sendTyping` (`:784-790`), which sends one `sendChatAction` per keystroke for the current chat. No throttle, no refresh loop, no expiry, no `ChatActionCancel` on clear/send/chat close, no policy gate.
- `src/telegram/TypingStatus.{h,cpp}` — request builders exist for typing, uploading photo, uploading document only. `chatActionLabel` maps only `chatActionTyping` / `chatActionUploadingPhoto` / `chatActionUploadingDocument`; everything else (video, video note, voice, recording, cancel) becomes an empty string.
- Receive: `TelegramRepository.cpp:915-924` handles `updateChatAction` only when `chatId == m_currentChatId`; emits `typingChanged(chatId, label)`. `AppController.cpp:633-639` stores a single `m_typingText` string. No sender names, no multi-typist, no 6 s expiry timer (relies on TDLib delivering a cancel), no suppression of the user's own actions (TDLib does not send them, so this is safe but unguarded), no activity kinds beyond the three labels.
- `qml/pages/ChatsPage.qml:363-367` shows a `TypingIndicator` from `typingText`.

### 3.4 Presence

- No `SetOption("online", …)` and no `GetOption`/`SetOption` calls anywhere in `src/telegram` (verified). The Windows client therefore never declares itself offline and never honours `hideOnline`.
- `TelegramUpdateScheduler::isIgnored` **drops** `updateUserStatus` and `updateChatOnlineMemberCount` before the repository sees them (`TelegramUpdateScheduler.cpp:67-75`).
- The chat header subtitle is a static type label (`TelegramRepository.cpp:47-54, 291`), e.g. "Private chat" / "Group". No online, last seen, or member count.
- `ChatData` has no presence/last-seen fields (`TelegramTypes.h:10-28`).

### 3.5 Windows tests that exist today

`tests/tst_notification_formatter.cpp` (10 cases: outgoing/current/muted/service/unsupported suppression, private/group titles, caption/label, whitespace + truncation, invalid ids), `tests/tst_notification_settings.cpp` (defaults, round-trip, no-op setter, corrupt JSON, per-field type rejection, `load()` change semantics), `tests/tst_typing_status.cpp` (3 request builders, large ids, no `message_thread_id`, 3 labels, cancel/unknown clearing, serialization).

These are solid unit tests for what exists; they do not cover grouping, routing, dismissal, per-chat settings, mentions, presence or typing throttling.

---

## 4. Gap analysis

Severity: **S1** = wrong behaviour a user will hit daily; **S2** = parity feature missing; **S3** = polish.

| # | Area | Android | Windows | Severity |
|---|---|---|---|---|
| N1 | Notification source | TDLib notification API (groups, silent, mention-only, server mute) | `updateNewMessage` inference | **S1** |
| N2 | Mute correctness | `use_default_mute_for` + scope defaults (`ChatMuteState`) | `mute_for > 0` only ⇒ chats muted via their scope still notify | **S1** |
| N3 | Silent messages | `is_silent` / `notificationSoundId == 0` ⇒ no alert | always alerts | **S1** |
| N4 | Grouping / dedup | one `MessagingStyle` per TDLib group, watermarks, stale reconciliation | one balloon per message; no dedup, no replacement | **S1** |
| N5 | Click behaviour | exact chat/topic/account routing, token-validated | none (balloon click only focuses the app; no chat) | **S1** |
| N6 | Dismissal | watermark + `RemoveNotificationGroup` with retry; propagates to other devices | none; closing a balloon changes nothing server-side | **S2** |
| N7 | Mentions | TDLib groups + per-chat `disable_mention_notifications`; mention badge via `unread_mention_count` | none (no field, no UI, no switch) | **S2** |
| N8 | Per-chat settings | sound (default/silent/saved), preview, mentions, mute — read and written through TDLib with `use_default_*` semantics | `ChatNotificationSettings.qml` exists but unwired; no TDLib read/write; mute never displayed or toggleable | **S2** |
| N9 | Previews / privacy | secret chat hides title; protected content and unresolved metadata hide text+sender; per-message `show_preview` | global preview switch only; secret/protected content can leak in the body | **S1** (privacy) |
| N10 | Sound | channel-managed; silent for non-fresh and archived; per-chat sound id | `sound` setting persisted but unused; no silent-archive handling; no per-chat sound | **S2** |
| N11 | Suppression scope | only when app is foreground **and** that chat is open | suppresses whenever the chat is merely selected, even when minimised/unfocused | **S2** |
| N12 | Account identity | per-account tags, tokens, watermarks, outboxes | settings file and tray are app-global; `AccountRegistry` is single-account today | **S3** (design now, implement with multi-account) |
| N13 | Notification settings scope | per-account SharedPreferences | one global `state/notification-settings.json` | **S3** |
| T1 | Outgoing typing | 4 s refresh loop, 5 s lifetime, `ChatActionCancel`, policy gate | one request per keystroke, never cancelled, no expiry | **S1** (server load, stuck "typing…") |
| T2 | Typing activity kinds | typing, recording voice/video note, uploading photo/video/video note/voice/document | typing, uploading photo, uploading document | **S2** |
| T3 | Incoming typing | per-sender roster, 6 s expiry, 1/2/3/N labels, own-user filter | single label, current chat only, no expiry, no names | **S2** |
| T4 | Presence | `SetOption("online", foreground && allowed)`; honours `hideOnline` | never set; always online while running | **S1** (privacy) |
| T5 | Last seen / online display | header presence line, relative/exact style, 30 s clock, group member count | static type label; `updateUserStatus` dropped by the scheduler | **S2** |
| T6 | Stealth interaction | `hideTyping` / `hideOnline` gate every effect through `InteractionPolicy` | no policy, no stealth integration | **S2** |

---

## 5. Implementation plan

Guiding rule: **stop inferring notifications from messages and adopt TDLib's notification API**, exactly as Android does. That single change removes N1–N3, N6, most of N7 and the correctness half of N9, and it is also what makes grouping (N4) and dismissal (N6) possible. Typing throttling (T1) is independent and can land first because it is small and user-visible.

### Stage 1 — Outgoing typing correctness (P0, smallest blast radius)

Files:

- `src/telegram/TypingStatus.{h,cpp}` — add `sendCancelRequest`, `sendRecordingVoiceRequest`, `sendRecordingVideoNoteRequest`, `sendUploadVideoRequest`, `sendUploadVideoNoteRequest`, `sendUploadVoiceRequest`; extend `chatActionLabel` for all `ChatAction` types.
- New `src/telegram/ChatActivityLoop.{h,cpp}` — per-chat state machine: `start(chatId, Activity)`, `touch()`, `end(chatId, Activity)`, `setAllowed(chatId, bool)`, `signedOut()`, `shutdown()`.
  - Refresh every 4 s, typing lifetime 5 s, `ChatActionCancel` on end/expiry/policy block, one loop per chat, newest activity wins, upload activities resume after typing.
  - Injected clock and a `sendAction` callback so it is unit-testable without TDLib.
- `src/telegram/TelegramRepository.{h,cpp}` — replace `sendTyping()` with `startTyping()` / `stopTyping()` / `sendUploadAction(chatId, kind)`; route all actions through `ChatActivityLoop`; cancel on `closeChat`, send success, and sign-out; pass `topic_id` for forum chats once topics exist.
- `src/ui/AppController.{h,cpp}` — `Q_INVOKABLE void setTypingActive(bool)`; stop typing in `closeChat`/account reset.
- `qml/pages/ChatsPage.qml` — `onTextChanged: BeyGramApp.setTypingActive(text.trim().length > 0)`; also call `setTypingActive(false)` in `submitComposer()` and on page close.

TDLib APIs: `sendChatAction { chat_id, topic_id, business_connection_id, action }` with `chatActionTyping`, `chatActionCancel`, `chatActionRecordingVoiceNote`, `chatActionRecordingVideoNote`, `chatActionUploadingPhoto/Video/VideoNote/VoiceNote/Document`.

Tests (`tests/tst_typing_status.cpp`, new `tests/tst_chat_activity_loop.cpp`):

- All request builders and label mappings, including `chatActionCancel` → empty label.
- Loop: 4 s refresh cadence with a fake clock; no second request per `touch()`; cancel exactly once after 5 s of silence; replacement cancels the previous activity; policy block sends cancel immediately; `signedOut()` cancels all chats; no send after `shutdown()`.

### Stage 2 — Notification model from TDLib (P0)

Files:

- New `src/telegram/NotificationGroups.{h,cpp}` — pure model ported from `TdNotifications`: `NotificationNotice { id, date, sender, text, previewAllowed, messageId, topicId }`, `NotificationGroup { id, chatId, title, isGroup, isSecret, totalCount, notices, alertId, metadataReady, archived, replyable, hasProtectedContent }`, `NotificationState { groups, initialized, snapshot, hasPending }`.
  - `applySnapshot(UpdateActiveNotifications)`, `applyGroup(UpdateNotificationGroup)`, `applyNotification(UpdateNotification)`, `applyChatMetadata`, `markHandled(groupId, maxId)`, `isFresh(group)`, `pendingAlert`.
  - Options: `SetOption notification_group_size_max = 10`, `notification_group_count_max = 25`.
- `src/telegram/TelegramRepository.{h,cpp}` — handle `updateActiveNotifications`, `updateNotificationGroup`, `updateNotification`, `updateHavePendingNotifications`, `updateChatTitle`, `updateChatPermissions`, `updateChatHasProtectedContent`; resolve `GetChat` metadata for unknown chat ids; emit `notificationsChanged()`; expose `NotificationState`.
  - Keep `updateNewMessage` for the message model only — **remove** the `notificationRequested` emit there.
- `src/telegram/NotificationFormatter.{h,cpp}` — demote to a *text* helper for one notice (`noticeText`, `senderName`, 180-char truncation, media labels), with secret/protected/unresolved masking applied by the caller.
- `src/ui/AppController.{h,cpp}` — consume `notificationsChanged`, apply settings/watermarks, hand groups to a presenter.
- New `src/ui/NotificationWatermarks.{h,cpp}` (or extend `NotificationSettingsStore`) — `handled(groupId)`, `setHandled`, `dismissed`, `pendingDismissal`, `acknowledge`, all per account, reset on sign-out.
- `src/telegram/TelegramUpdateScheduler.cpp` — add `updateActiveNotifications` as urgent, keep `updateChatNotificationSettings` coalesced, add `updateChatUnreadMentionCount` handling (Stage 4).

TDLib APIs: `UpdateActiveNotifications`, `UpdateNotificationGroup`, `UpdateNotification`, `UpdateHavePendingNotifications`, `GetChat`, `RemoveNotificationGroup`, `SetOption`.

Tests (new `tests/tst_notification_groups.cpp`):

- Snapshot replaces groups, drops call groups, preserves a still-pending alert id for the same chat.
- Incremental merge: removals, additions, dedupe by id, newest-10 cap, group removal on `totalCount == 0`.
- Alert id only for non-silent notices newer than the previous max; pending alert survives a content update.
- Watermark: fresh only when `alertId > handled`; deliberate suppression consumes the watermark; sign-out resets it.
- Metadata: unresolved chat hides the title; `replyable` false for secret chats, channels and no-send-permission; protected content forces text/sender hiding.

### Stage 3 — Presentation, grouping and click routing (P0)

Files:

- New `src/ui/NotificationPresenter.{h,cpp}` — abstract sink: `post(group)`, `update(group)`, `cancel(groupId)`, `cancelAll()`, `setSoundEnabled(bool)`. One `TrayNotificationPresenter` implementation now; a WinRT toast backend later behind the same interface.
- `src/ui/AppController.{h,cpp}` — `showNotification` becomes group-driven: skip unauthorized/disabled/no-permission/empty/`foreground && openedChat`; mark handled **only after** a successful post; cancel stale entries only after a snapshot; add `Q_INVOKABLE void openNotificationChat(qint64 chatId, int groupId, int maxId, qint64 topicId)` and an `activated` handler on the tray that opens the chat.
- `src/telegram/TelegramRepository.cpp` — `dismissNotificationGroup(groupId, maxId)` → `RemoveNotificationGroup` with bounded retry (4 attempts, 2 s exponential, 30 s rounds, max 3, reopened on Ready/connected); never call `ViewMessages` or `OpenMessageContent` from notification code.
- `qml/pages/ChatsPage.qml` / `AppController` — track `openedChatId` separately from "window is focused"; suppress only when both hold. Add `setAppActive(bool)` from the main window's `activeChanged` (currently no window-activation tracking exists anywhere).
- Privacy: apply `NotificationPreviewPolicy` equivalents — secret chat or unresolved metadata ⇒ hide title; protected content ⇒ hide text and sender; per-message `show_preview` respected.

Tests:

- `tests/tst_notification_formatter.cpp` extended: silent notice, protected content, secret chat, unresolved metadata, mention flag passthrough.
- New `tests/tst_notification_presenter.cpp` with a fake sink: no post when disabled/unfocused-open-chat/unauthorized; watermark advances only after a successful post; failed post keeps the entry; stale cancel happens only after a snapshot.
- New `tests/tst_notification_dismissal.cpp`: retry schedule, watermark monotonicity (an older dismissal cannot erase a newer max id), reset on reconnect.

### Stage 4 — Mute, mentions, per-chat settings (P1)

Files:

- New `src/telegram/ChatNotificationSettings.{h,cpp}` — TDLib `ChatNotificationSettings` ⇄ JSON mapping with `useDefault*` flags, mirroring Android's `toChatNotificationPreferences` / `withChatNotificationPreferences`: writing preview/mentions must not turn an inherited value into an override, and mute/story fields must be preserved.
- New `src/telegram/ChatMuteState.{h,cpp}` — per-chat `useDefaultMuteFor`/`muteFor` plus per-scope mute, `isMuted(chatId)`, `isKnown(chatId)`; fed by `GetChat`, `UpdateChatNotificationSettings`, `UpdateScopeNotificationSettings` (`GetScopeNotificationSettings` on demand).
- `src/telegram/TelegramRepository.{h,cpp}` — `setChatMuted(chatId, bool)`, `requestChatNotificationSettings(chatId)`, `saveChatNotificationSettings(chatId, settings)`, `savedNotificationSounds()`, `notificationScopeSettings(scope)`; replace the `mute_for > 0` read with `ChatMuteState`.
- `src/telegram/TelegramTypes.h` — add `unreadMentionCount` to `ChatData`; handle `updateChatUnreadMentionCount`.
- `src/ui/AppController.{h,cpp}` — QML-facing properties/invokables for the above; `notifyMutedChats` becomes a real global override; `sound` becomes a real switch (silent when off).
- QML: instantiate `qml/components/ChatNotificationSettings.qml` from the chat list / chat header (add a context menu — none exists today); show a muted icon in `ChatRow.qml`; add a mention badge in the chat list; extend `SettingsPage.qml` with sound and "notify muted chats"; add a "Mentions" row.

TDLib APIs: `GetChat`, `SetChatNotificationSettings`, `GetScopeNotificationSettings`, `SetScopeNotificationSettings`, `GetSavedNotificationSounds`, `ToggleChatIsMuted` (optional; explicit settings are more faithful).

Tests (new `tests/tst_chat_notification_settings.cpp`, extend `tests/tst_notification_settings.cpp`):

- Round-trip with inherited preview/mentions: saving one field leaves the other inherited; explicit values survive.
- Mute: `useDefaultMuteFor = true` follows the scope; explicit `mute_for` wins; `isKnown` false before the scope is read.
- Settings migration: version 1 → 2 (per-account path, new keys) keeps existing values; corrupt file still falls back per field.
- `notifyMutedChats` and `sound` actually gate a post in the presenter test.

### Stage 5 — Presence and last seen (P1)

Files:

- New `src/telegram/PresenceController.{h,cpp}` — `authorized()`, `setForeground(bool)`, `setHideOnline(bool)`, `setOnline(bool)`; sends `SetOption("online", OptionValueBoolean)` only on change, retries once on error, and re-asserts after reconnect.
- `src/telegram/TelegramUpdateScheduler.cpp` — stop ignoring `updateUserStatus` and `updateChatOnlineMemberCount`; keep them out of the coalescing set (they are small and order-insensitive per user/chat, so a per-key coalesce is acceptable if batching becomes an issue).
- `src/telegram/TelegramTypes.{h,cpp}` — `ChatData.onlineMemberCount`; a `UserPresence` map (`userId → status`) fed by `updateUser`, `updateUserFullInfo`, `updateUserStatus`.
- `src/telegram/TelegramRepository.{h,cpp}` — emit `presenceChanged(chatId)`; `currentChatPresenceText()` or expose structured presence to QML.
- New `src/ui/LastSeenSettingsStore.{h,cpp}` — relative/exact, mirroring `LastSeenSettingsStore.kt`.
- `qml/pages/ChatsPage.qml` — replace the static `currentChatSubtitle` with a presence line; a 30 s timer for relative text.

Tests: `tests/tst_presence.cpp` — option sent only on change; offline on foreground loss; `hideOnline` forces false; last-seen formatting for all statuses; group member count formatting; `updateUserStatus` reaches the model instead of being dropped.

### Stage 6 — Incoming typing parity (P1)

Files:

- New `src/telegram/TypingRoster.{h,cpp}` — port of `TdProfiles.updateTyping`: per-chat map keyed by sender, 6 s expiry with an injected clock, own-user filter, insertion order, name resolution from `m_userNames`/chat titles.
- `src/telegram/TelegramRepository.{h,cpp}` — feed every `updateChatAction` (not only the current chat) into the roster; emit `typingChanged(chatId)`; expose `typingNames(chatId)`.
- `src/ui/AppController.{h,cpp}` — `typingText` becomes `QStringList`-backed; keep the old property for compatibility or update the QML binding.
- `qml/pages/ChatsPage.qml` / `qml/components/TypingIndicator.qml` — 1/2/3/N labels, "someone" for unknown names, no layout jump on clear.

Tests: `tests/tst_typing_roster.cpp` — ordering, duplicate refresh does not reorder, expiry at exactly 6 s, removal on non-typing action, own-user filter before and after `myId` is known, label composition for 1/2/3/4+.

### Stage 7 — Inline reply, accounts, toast backend (P2)

- Reply: durable outbox (`state/notification-outbox.json`, fsync + atomic replace), session token per account, 5 attempts / 60 s / 24 h TTL, forum-topic fallback, stealth hold. Needs a real toast backend with an input box; `QSystemTrayIcon::showMessage` cannot carry actions.
- WinRT toast backend: `ToastNotificationManager` with an AppUserModelID shortcut, `AdaptiveGroup`/`AdaptiveSubgroup` for the Android grouping model, `inputTextBox` + reply button, `Activated`/`Dismissed` events mapped to `openNotificationChat` / `dismissNotificationGroup`.
- Per-account routing: extend `NotificationSettingsStore`, watermarks and tags with the `AccountRegistry` account id; `AccountRegistry` is single-account today, so design the key now and implement with multi-account.

---

## 6. Priority summary

| Priority | Work | Why |
|---|---|---|
| **P0** | Stage 1 (typing throttle/cancel), Stage 2 (notification API model), Stage 3 (grouped presentation, click, dismiss, privacy masking) | Fixes daily-visible defects (N1–N5, N9, T1, T4) and is a prerequisite for everything else |
| **P1** | Stage 4 (mute/mentions/per-chat settings), Stage 5 (presence/last seen), Stage 6 (incoming typing roster) | Completes parity for the features named in this audit |
| **P2** | Stage 7 (inline reply, per-account routing, WinRT toast backend) | Needs a toast surface with actions; already scoped as Phase 6 in `docs/PORTING_ROADMAP.md` |

Recommended order: 1 → 2 → 3 → 5 → 6 → 4 → 7. Presence (5) and the typing roster (6) are cheap and independent of the notification rewrite, so they can run in parallel with Stage 2. Stage 4 depends on Stage 2/3 for the "consume the watermark on deliberate suppression" semantics.

---

## 7. Test strategy (to be implemented, not run)

**Unit (Qt Test, `tests/`)** — pure logic with injected clocks and fake sinks: notification group merge/snapshot/alert/watermark, dismissal retry, per-chat settings mapping and migration, presenter suppression rules, activity loop timing, typing roster expiry, presence option transitions.

**Integration (manual, on a real account)** — no device or account automation should be added:
1. Two devices: read a chat on the phone; the Windows tray entry disappears (N6) and no alert fires for already-read messages.
2. Mute a chat on the phone; Windows stops notifying (N2). Mute a group's scope; all inheriting chats stop.
3. Send a silent message from the phone; Windows posts without sound (N3).
4. Tap a Windows notification; the exact chat opens, including an archived chat and (later) a forum topic.
5. Type in Windows; the phone shows "typing…" for no longer than 5 s after the last keystroke, and immediately stops after sending or clearing the field (T1).
6. Open a secret chat on the phone; Windows shows no title, text or sender (N9).
7. Minimise Windows with a chat open; a message in that chat still notifies (N11).
8. Turn `hideOnline`/`hideTyping` on in the Android stealth settings equivalent; the Windows client must not appear online or typing (T4, T6).

**Explicit non-claims:** nothing in this document was built, linked, launched or tested. Timing constants (4 s/5 s/6 s), retry counts and TDLib option values are read from the Android sources, not measured on Windows.

---

## 8. Risks and notes for the implementer

- **Do not keep two notification paths.** Leaving the `updateNewMessage` emit in place while adding the notification API will double-notify. Remove it in the same change that adds `NotificationGroups`.
- **Watermark ordering matters.** Android advances `handled` only after a successful post (or a deliberate suppression). Doing it before the post loses alerts when the tray is unavailable; doing it never produces repeat rings.
- **Mute is not a boolean.** `use_default_mute_for` plus scope settings is the only correct model; a plain `mute_for > 0` check (today's Windows code) notifies for scope-muted chats.
- **Typing is a protocol contract.** TDLib drops an action after ~5 s; per-keystroke `sendChatAction` is both a request storm and a stuck-indicator source when the field is cleared.
- **Presence is a privacy switch, not cosmetics.** `SetOption("online")` must be tied to window foreground and to the stealth policy, or the client advertises activity the user asked to hide.
- **The unwired `ChatNotificationSettings.qml` is a trap.** It looks like parity but has no C++ property, no TDLib call and no instantiation; treat it as a design mock to wire, not as existing functionality.
- **Per-account design now, implementation later.** `AccountRegistry` is single-account, but notification files, tags and watermarks should be keyed by account id from the start to avoid a second migration (Android already does this).
