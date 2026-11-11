# Parity 15 — Privacy, account security, secret chats, blocking and active sessions

Status: **read-only parity audit**. Every statement below comes from reading source
files. No GUI, CMake configure, build, APK install or test run was performed for this
document, so nothing here is a claim that Windows code works on a device or that a
build is green. Numbers quoted from Android (page sizes, timeouts, KDF parameters)
are the constants found in the sources listed in §2.

Android reference tree: `app/src/main/java/com/beykus/beygram/` (branch content as
checked out in this workspace).
Windows reference tree: `src/`, `qml/`, `tests/`.

---

## 1. Scope

Five product areas that Telegram groups under "Privacy and Security", plus the local
passcode:

| Area | Android entry point | Windows today |
|---|---|---|
| Privacy rules (who sees what / who may contact) | Settings → Privacy and security | **absent** |
| Block list + blocking/unblocking + report spam | Privacy screen → Blocked users, chat info menu | **absent** |
| Secret chats (E2E), key check, self-destruct TTL, close | Profile → Start secret chat, chat settings | **partial (read-only, no E2E surface)** |
| Active sessions (authorized devices) | Settings → Devices | **absent** |
| Passcode / app lock, biometrics | Settings → App lock, `AppLockActivity` | **absent** |
| Account security (2FA, recovery email, phone change, delete account) | Settings → Account security | **absent** |

Secret-chat *content* restrictions (no forwarding, no saving, no translation, no
auto-download, screenshot blocking) are in scope because they are part of the same
security contract, not a separate feature.

---

## 2. Android files reviewed

### 2.1 Domain models

| File | What it defines |
|---|---|
| `domain/model/Privacy.kt` | `PrivacySetting` (9 rules), `PrivacyRuleChoice`, `PrivacyRules` (+`isEditable`, `normalized()`, `withChoice()`), `PrivacySettingState`, `BlockedSender`, `BlockedSenderKind`, `BlockedSendersPage` |
| `domain/model/SecretChat.kt` | `SecretChatState`, `SecretChatDetails`, `SecretChatKey` (hex/emoji/identicon), `SecretChatPosting`, `SecretChatTtl`, `TtlParts`/`ttlParts`/`formatTtlDuration`, `SecretChatEligibility`, `MessageSelfDestruct`, `ViewOncePolicy` |
| `domain/model/SecretChatKeyEmoji.kt` | The official 5-emoji lookup table |
| `domain/model/AccountSecurity.kt` | `CloudPasswordState`, `NewPasswordValidation`, `validateNewPassword`, recovery-email validation, `matchesDeletionConfirmation`, `PhoneCodeDelivery`, `PhoneChangeCode`, phone/code input normalisation |
| `domain/model/DeviceSession.kt` | `DeviceSession(id, current, device, application, location, ip, lastActive)` |

### 2.2 TDLib layer

| File | What it does |
|---|---|
| `data/tdlib/TdPrivacy.kt` | `PrivacySetting → UserPrivacySetting` mapping, rules ↔ `UserPrivacySettingRules`, read-only fallback for unsupported rules, block-list paging (50), block/unblock, report-chat option selection |
| `data/tdlib/TdSecretChats.kt` | `createNewSecretChat`, `getSecretChat` details, TTL set, `closeSecretChat`, in-memory `StateFlow` directory, update application, eligibility validation |
| `data/tdlib/TdAccountSecurity.kt` | `GetPasswordState`, `SetPassword`, recovery-email set/confirm/resend/cancel, phone-change send/resend/check, `DeleteAccount`, error mapping |
| `data/tdlib/TdLibTelegramRepository.kt` | `activeSessions()` / `terminateOtherSession()` / `terminateAllOtherSessions()` (lines ~954–979), `setPrivateChatBlocked`, `loadBlockedSenders`, `setSenderBlocked`, `reportSpam`, `reportSpamForUser`, `createSecretChat`, `refreshSecretChat`, `setSecretChatTtl`, `closeSecretChat` |
| `data/tdlib/TdAccountSessionState.kt` | Isolated auth/connection projection per account (fail-closed on cross-account updates) |

### 2.3 Screens and UI

| File | What it shows |
|---|---|
| `ui/screens/settings/SettingsScreen.kt` | "Devices and privacy" group: Account security, App lock, Privacy, Secondary accounts, Devices (sessions), Last seen formatting |
| `ui/screens/settings/PrivacySettingsScreen.kt` | Visibility group (phone, photo, last seen, bio, forwarded), contact group (calls, messages, group invites, find by phone), audience dropdown, "Always/Never allow" exception editor with contact picker, read-only lock state, Stealth entry, Blocked-users entry |
| `ui/screens/settings/BlockedSendersScreen.kt` | Paged block list (50/page, prefetch 8), unblock, report spam, empty/error states, avatar rows |
| `ui/screens/settings/ActiveSessionsScreen.kt` | Current session vs others, refresh, terminate one, terminate all others, confirmations, 20 s load / 25 s action bounded waits |
| `ui/screens/settings/SessionsTimeoutPolicy.kt` | `SessionAwait` (Completed/TimedOut) so a timeout is never mistaken for an empty list or a success |
| `ui/screens/settings/AccountSecurityScreen.kt` | 2FA card + dialog, recovery email card + pending-code card + dialogs, phone change card + `PhoneNumberChangeDialog`, delete account card + typed-word dialog |
| `ui/screens/settings/AppLockSettingsScreen.kt` | Enable/disable, change PIN, timeout (Immediately/1 min/5 min), biometrics, hide from Recents, local-only notes |
| `ui/screens/chat/SecretChatSettings.kt` | TTL presets + custom dialog, key dialog (12×12 identicon, 5 emoji, hex, sensitive clipboard), close-chat confirmation, "device-specific" notice |
| `ui/screens/chat/SecretChatPrivacy.kt` | Reference-counted `FLAG_SECURE` per window, with a policy hook for the lock screen |
| `ui/screens/profile/ChatInfoMenu.kt` | Block/unblock from chat info with confirmation |
| `ui/screens/settings/StealthSettingsScreen.kt`, `StealthPresets.kt` | Local anti-typing/anti-read-receipts switches (BeyGram-specific, not Telegram server rules) |
| `ui/screens/settings/LastSeenSettingsScreen.kt` | Local last-seen *formatting* only; the server rule lives in Privacy |

### 2.4 Local security

| File | What it does |
|---|---|
| `security/AppLockStore.kt` | Salted PBKDF2 hash in private prefs, persisted failure counter and lockout deadline, enable/disable semantics, "enabled without hash is not a lock" |
| `security/AppLockPinHasher.kt` | PBKDF2-HMAC-SHA256, 210 000 iterations default (10 000–600 000 clamp), 16-byte salt, 256-bit key, constant-time compare |
| `security/AppLockPolicy.kt` | 4–8 digit PIN, 5 free attempts then 30 s doubling to 5 min, timeout maths, external-handoff and unlock-handover rules |
| `security/AppLockSession.kt` | Clock-free foreground/background state machine |
| `security/AppLockManager.kt` | Process-wide lock, screen-off receiver, external-activity exemption, multi-account-wide |
| `security/AppLockActivity.kt` | Full-screen secure lock window, `FLAG_SECURE`, `setRecentsScreenshotEnabled(false)`, back swallowed, stops secret playback |
| `security/AppLockScreen.kt` | PIN entry, biometric prompt, lockout countdown |
| `security/BiometricAuthenticator.kt` | `BiometricPrompt` wrapper; PIN always remains available |
| `security/NotificationPreviewPolicy.kt` | Secret chats hide title+text+sender; protected content hides text+sender |

### 2.5 Tests reviewed (Android)

`app/src/test/java/com/beykus/beygram/`: `PrivacyRulesTest`, `TdSecretChatsTest`,
`SecretChatTest`, `TdAccountSecurityTest`, `AccountSecurityTest`, `AppLockPolicyTest`,
`AppLockSessionTest`, `AppLockPinHasherTest`, `NotificationPreviewPolicyTest`,
`SessionsTimeoutPolicyTest`, `TdAccountSessionStateTest`. These are the behavioural
specifications to port; they were read, not executed.

---

## 3. Android behaviour specification

### 3.1 Privacy rules

- Nine server-side settings, in screen order:
  `PhoneNumber`, `ProfilePhoto`, `LastSeen`, `Bio`, `ForwardedMessages` (visibility) and
  `Calls`, `Messages`, `GroupInvites`, `FindByPhoneNumber` (contact).
- Each maps to a TDLib `UserPrivacySetting*`: `ShowPhoneNumber`, `ShowProfilePhoto`,
  `ShowStatus`, `ShowBio`, `ShowLinkInForwardedMessages`, `AllowCalls`,
  `AllowUnpaidMessages`, `AllowChatInvites`, `AllowFindingByPhoneNumber`.
- Model is three choices (`Everyone`/`Contacts`/`Nobody`) plus two user-id exception
  lists. Rules are ordered server-side (first match wins), so exceptions are always
  emitted **before** the base rule: `AllowUsers`, `RestrictUsers`, then
  `AllowAll`/`AllowContacts`/`RestrictAll`.
- A user present in both exception lists is treated as **never allowed**
  (`PrivacyRules.normalized()` subtracts `never` from `always`). `Everyone` drops the
  always list; `Nobody` drops the never list.
- **Fail-closed editing.** If Telegram returns any rule the compact editor cannot
  round-trip faithfully (`RestrictContacts`, premium-only, chat-member, bot rules), the
  whole setting becomes `isEditable = false`, is shown with a lock icon and
  "managed elsewhere", and `setUserPrivacySettingRules` refuses to be built for it
  (`require(rules.isEditable)`). A compacted approximation could grant access the
  account denied.
- Empty server rule list means "nobody" (TDLib: no match ⇒ not allowed). When only
  `RestrictUsers` is present the base falls back to `Everyone`; when only `AllowUsers`
  is present it falls back to `Nobody`.
- `FindByPhoneNumber` is special: choices limited to `Everyone`/`Contacts`, no
  exceptions, and `Nobody` is rejected before the request.
- All nine settings are loaded concurrently; exception user ids are resolved to display
  names in one batch. Saving is optimistic with rollback of the whole list on failure.
- Rules are server state shared by every signed-in device — never a local preference.

### 3.2 Block list and blocking

- List comes from TDLib's **main** block list only (`blockListMain`), paged 50 at a
  time with offset = number of rows actually fetched (so a skipped unsupported sender
  still advances the cursor). Stops when a page is empty or yields no supported rows.
- Rows can be users or chats (supergroups/channels); the UI labels the kind honestly
  ("Channel/Group", "Bot", "User") instead of pretending a channel is a contact.
- Unblock = `setMessageSenderBlockList(sender, null)`; block = `...(sender, blockListMain)`.
- Report spam is offered only for user rows; it resolves the private chat and runs
  Telegram's `reportChat` option flow (option required → pick canonical "spam" option;
  text required → retry; never substitutes an unrelated reason).
- Blocking is also reachable from chat info (`ChatInfoMenu`) with an explicit
  confirmation dialog; `setPrivateChatBlocked` refuses groups/channels and self.
- Blocking never deletes a contact and never deletes chat history.

### 3.3 Secret chats

- Creation: `createNewSecretChat(userId)` after validating the partner is a regular,
  non-deleted user that is not the current account and not a bot
  (`SecretChatEligibility`).
- Details are **memory only**, per session, never persisted and never restored from a
  backup: `secretChatId`, partner, `Pending/Ready/Closed`, `isOutbound`, TTL, layer,
  `keyHash`.
- Chat TTL is TDLib's `Chat.messageAutoDeleteTime`; a `UpdateChatMessageAutoDeleteTime`
  value overrides a stale `Chat` snapshot (`mergedSecretChatTtl`).
- Closed chats stay readable but cannot accept new messages (`SecretChatPosting`).
- Key verification screen shows the three official views, matching Telegram for Android
  byte for byte:
  - hex fingerprint of the first 32 bytes, grouped by 4 bytes;
  - five emoji from the SHA-256 half (offset 16, 4 bytes each, sign bit dropped, modulo
    table size) — empty for legacy 16-byte hashes;
  - identicon: 12×12 for a 36-byte hash, 8×8 for a 16-byte one, 2 bits per pixel, four
    fixed colours `0xFFFFFFFF / 0xFFD5E6F3 / 0xFF2D5775 / 0xFF2F99C9`.
  - The fingerprint is copied as a **sensitive** clip (`EXTRA_IS_SENSITIVE`) and the
    dialog is itself `FLAG_SECURE`.
- Secret screens set `FLAG_SECURE` while visible, reference-counted so overlapping
  viewers cannot clear it early. The app lock registers its own policy.
- Consequences enforced elsewhere and part of the contract:
  - notifications: secret chat hides title, text and sender;
  - auto-download: denied for secret chats;
  - translation and transcription: refused;
  - chat export: refused;
  - view-once media: cannot be saved, forwarded or exported;
  - music/playback session restore: secret media is excluded;
  - app lock: `PlaybackEngine.stopSecret()`.

### 3.4 Self-destruct / TTL

- Chat-wide TTL: presets off / 1 hour / 1 day / 7 days, plus a custom day/hour/minute
  picker. Valid range is 0 (off) or 1 s … 365 days; a custom value below 60 s is
  rejected. Applies to messages sent after the change; already-delivered messages keep
  their own timer.
- Per-message self-destruct is a separate mechanism (`MessageSelfDestruct.Immediately`
  for view-once, `Timer(seconds)`), never mixed into the chat TTL.
- View-once is restricted to a single photo/video in a private, non-secret,
  non-scheduled send.

### 3.5 Active sessions

- `getActiveSessions` → `DeviceSession` with `deviceModel` falling back to `platform`,
  `applicationName`, `location`, `ipAddress`, `lastActiveDate` (unix seconds).
- Current session is displayed separately and **cannot** be terminated by the
  other-devices action.
- Terminating one session first re-reads the list and refuses ids that are current or
  unknown.
- Terminate-all uses `terminateAllOtherSessions` and is behind a destructive
  confirmation.
- Loads are bounded (20 s) and actions are bounded (25 s); a timeout is reported as a
  timeout, never as an empty list or as success, and a timed-out destructive action is
  never silently retried — the list is refreshed instead.

### 3.6 Account security (2FA, recovery email, phone, deletion)

- `GetPasswordState` is projected into a model that deliberately holds no password:
  `hasPassword`, `passwordHint`, `hasRecoveryEmailAddress`, masked pending recovery
  address, masked login-email pattern.
- `SetPassword(current, new, hint)`; empty new password disables 2FA. Local validation
  only catches empty/too-short/mismatch before the round trip; Telegram decides policy.
- Recovery email: set → pending masked address + code card; confirm/resend/cancel.
- Phone change: `sendPhoneNumberCode` (change type) → `PhoneChangeCode` describing the
  delivery route, code length, resend availability and timeout; word/phrase routes
  accept letters; resend uses an explicit user-request reason; then `checkPhoneNumberCode`.
  Telegram documents this request for official apps, so a refusal gets its own message
  and is never retried through another method.
- Deletion requires typing an exact confirmation word (case/space-insensitive) **and**
  the current password, and only after the server accepts is local cleanup allowed.
- Passwords appear only as method arguments and request fields — never in a field,
  StateFlow, preference or diagnostic event. Server errors are mapped to specific
  messages; unknown details are never surfaced.

### 3.7 Passcode / app lock

- Optional app-wide PIN, 4–8 digits, shared by every account in the installation.
- Stored as salt + PBKDF2-HMAC-SHA256 (210 000 iterations, 16-byte salt, 256-bit key) in
  private prefs; the PIN is never stored. The file is excluded from backup allowlists.
- Brute-force throttle: 5 free attempts, then 30 s doubling capped at 5 min, persisted
  with wall-clock time so it survives process death. A broken crypto provider fails
  closed. A successful biometric resets the counter.
- Timeout: immediately / 1 minute / 5 minutes after backgrounding.
- Lifecycle subtleties that must be preserved:
  - a foreign activity BeyGram started itself (picker, camera, share sheet, permission
    prompt) is not a background period unless the screen went off or the trip exceeded
    10 minutes;
  - the hand-over from the lock activity to the main activity (≤2 s) must not re-lock;
  - a lock enabled while the process was already backgrounded must fail closed on the
    next return;
  - a successful unlock that arrives while the activity is paused is ignored.
- The lock window is `FLAG_SECURE`, hides itself from Recents screenshots where the OS
  supports it, swallows Back, and stops secret media playback.

### 3.8 Notification preview policy

`hideTitle = isSecret || !metadataReady`;
`hideText = !previews || !previewAllowed || hideTitle || hasProtectedContent`;
`hideSender = hideTitle || hasProtectedContent`.

---

## 4. Current Windows status

### 4.1 What exists

| Concern | Windows implementation | Notes |
|---|---|---|
| Session/auth | `src/telegram/TelegramSession.{h,cpp}`, `AuthState`, `AuthRequests` | Phone/code/password/email/QR, close, logout, restart |
| Account profile | `src/telegram/UserProfile.{h,cpp}`, `AppController::requestProfile` | Display name, username, bio, phone, photo, group/channel flags |
| Local secret storage | `src/platform/windows/DpapiSecretStore.{h,cpp}`, `DatabaseKeyStore` | DPAPI-wrapped API hash and per-account TDLib DB key; no plaintext fallback |
| Single-process guard | `src/telegram/SessionLock.{h,cpp}` (`QLockFile`) | Prevents two processes on one database — **not** a user passcode |
| Log redaction | `src/core/Redaction.{h,cpp}`, `Logging` | Redacts password/token/secret/phone/session keys |
| Notification privacy | `src/telegram/NotificationFormatter.{h,cpp}` + `NotificationSettingsStore` | Global "show preview" switch only; no per-chat secret/protected awareness |
| Chat type awareness | `TelegramRepository.cpp:33–54`, `UserProfile.cpp:180–191` | `chatTypeSecret` is recognised as a private chat and labelled "Secret chat" in the header subtitle |
| Tests | `tests/tst_user_profile.cpp`, `tst_notification_formatter.cpp`, `tst_notification_settings.cpp`, `tst_secret_store.cpp`, `tst_session_lock.cpp`, `tst_database_key.cpp` | None cover privacy rules, block lists, secret chats, sessions or app lock |

### 4.2 What does not exist

A repository-wide search for `getUserPrivacySettingRules`, `setUserPrivacySettingRules`,
`getBlockedMessageSenders`, `setMessageSenderBlockList`, `getActiveSessions`,
`createNewSecretChat`, `getSecretChat` returns **no matches** in `src/`, `qml/` or
`tests/`. Concretely:

- **Privacy rules:** no model, no request builder, no screen, no QML property.
  `SettingsPage.qml` contains only theme, notifications, desktop notes and about.
- **Block list:** no paging, no block/unblock action anywhere, no block entry in chat UI
  (Windows has no chat-info surface at all).
- **Report spam:** absent.
- **Secret chats:** `chatFromJson()` (`TelegramTypes.cpp:371–422`) reads only id, title,
  type name, unread count, mute, last message, photo and positions. It ignores
  `type.secret_chat_id`, `message_auto_delete_time` and `has_protected_content`. There is
  no `SecretChatDetails`, no key screen, no TTL control, no close action, no E2E badge,
  and no "closed secret chat cannot post" guard.
- **Secret-chat notification leak (concrete):** `processUpdate()` raises
  `notificationRequested` for `updateNewMessage` in any chat
  (`TelegramRepository.cpp:1053–1065`) and `notificationForMessage()` builds the title
  from the chat title and the body from the message text. A secret chat therefore
  currently produces a desktop toast with its title and content, and protected-content
  chats produce full text. Android hides both.
- **Content restrictions:** no protected-content flag, so forwarding, saving
  (`saveOpenMediaAs`), copying and playback treat protected media like any other. No
  Windows equivalent of `FLAG_SECURE` (e.g. `SetWindowDisplayAffinity` with
  `WDA_EXCLUDEFROMCAPTURE`) is applied to any window.
- **Active sessions:** absent. `logout()` is the only session-management action.
- **Passcode / app lock:** absent. There is no idle timer, no Windows Hello integration,
  no lock screen, and no way to hide message content when the app is left open.
- **Account security:** absent. 2FA can only be entered during login
  (`submitPassword`); it cannot be set, changed or removed in the client, and there is
  no recovery email, phone change or account deletion.
- **Stealth mode:** absent (Android-only local feature; not a Telegram rule).

---

## 5. Gap matrix

Priority: **P0** = security/privacy leak or blocks a documented workflow;
**P1** = core parity for the area; **P2** = refinement/edge cases.

| # | Gap | Android reference | Priority |
|---|---|---|---|
| 1 | Secret-chat and protected-content notifications expose title/text | `NotificationPreviewPolicy` | P0 |
| 2 | No E2E indication, key verification, TTL or close for secret chats | `SecretChatSettings`, `SecretChatKey` | P0 |
| 3 | No protected-content enforcement (forward/save/copy/playback) | `ViewOncePolicy`, media guards | P0 |
| 4 | No privacy rules read/write | `TdPrivacy`, `PrivacySettingsScreen` | P1 |
| 5 | No block list, block/unblock, report spam | `BlockedSendersScreen`, `ChatInfoMenu` | P1 |
| 6 | No active sessions list or termination | `ActiveSessionsScreen` | P1 |
| 7 | No passcode / app lock / Windows Hello | `security/*` | P1 |
| 8 | No 2FA / recovery email / phone change / delete account | `AccountSecurityScreen`, `TdAccountSecurity` | P1 |
| 9 | No secret-chat creation flow | `createSecretChat` + profile action | P1 |
| 10 | No closed-secret-chat posting guard | `SecretChatPosting` | P2 |
| 11 | No "managed elsewhere" read-only rule state | `PrivacyRules.isEditable` | P2 |
| 12 | No session action timeouts / timeout≠empty distinction | `SessionsTimeoutPolicy` | P2 |
| 13 | No screenshot protection on secret/lock windows | `SecretChatPrivacy`, `AppLockActivity` | P2 |
| 14 | No Stealth mode (BeyGram-only local feature) | `StealthSettingsScreen` | P2 |
| 15 | No per-account scoping guarantee for privacy/secret state | `TdAccountSessionState` | P2 (already per-session by construction) |

---

## 6. Implementation plan

Design rules that follow the existing Windows architecture:

- QML never sends TDLib requests; `AppController` (or a dedicated controller) is the
  only QML surface.
- Pure request builders and parsers live in `src/telegram/`, take/return plain structs
  and `QJsonObject`, contain no logging and no `@extra` (the transport adds
  correlation) — same contract as `ProfileEdit.h`, `TelegramRequests.h`,
  `PinnedMessages.h`.
- No password, PIN or key hash is ever logged, persisted, or exposed as a QML property
  (`Redaction` already strips `password`/`secret`, but the rule is "never produce it").
- All server state is per `TelegramSession`/`TelegramRepository`; nothing is stored in a
  shared singleton, so future multi-account keeps working.
- Each new source file is added to `src/telegram/CMakeLists.txt` or
  `src/ui/CMakeLists.txt`, and each new test to `tests/CMakeLists.txt`
  (`beygram_add_test(tst_…)`).

### Stage 0 — P0: stop the notification leak (smallest, highest value)

Files:
- `src/telegram/NotificationFormatter.{h,cpp}` — extend
  `notificationForMessage(..., bool isSecretChat, bool hasProtectedContent)`.
- `src/telegram/TelegramTypes.{h,cpp}` — add `bool isSecretChat` and
  `bool hasProtectedContent` to `ChatData`; parse `type.@type == chatTypeSecret`,
  `has_protected_content`, and `message_auto_delete_time` in `chatFromJson()`;
  re-parse on `updateChatHasProtectedContent`.
- `src/telegram/TelegramRepository.cpp` — pass the flags at the
  `updateNewMessage` call site; handle `updateChatHasProtectedContent` and
  `updateChatMessageAutoDeleteTime`.
- `src/telegram/ChatListModel.{h,cpp}` — add `IsSecretRole`, `HasProtectedContentRole`
  (and later `SecretChatIdRole`) so QML can render a badge.

APIs (TDLib JSON): `updateChatHasProtectedContent`, `updateChatMessageAutoDeleteTime`,
`chat.has_protected_content`, `chat.message_auto_delete_time`,
`chat.type.{@type=chatTypeSecret, secret_chat_id}`.

Tests: `tests/tst_notification_formatter.cpp` (secret hides title/text/sender;
protected hides text/sender, keeps title; normal chat unchanged);
`tests/tst_telegram_types.cpp` (flags parsed; missing fields default false).

### Stage 1 — P1: privacy rules and block list

New files:
- `src/telegram/PrivacySettings.h/.cpp`
  - `enum class PrivacySetting` (9 values) + `toTdSettingName()` returning the JSON
    `@type` string.
  - `enum class PrivacyRuleChoice { Everyone, Contacts, Nobody }`.
  - `struct PrivacyRules { PrivacyRuleChoice choice; QVector<qint64> alwaysAllow; QVector<qint64> neverAllow; bool editable; }`
    with `normalized()` and `withChoice()` ported exactly (never-wins dedupe,
    Everyone/Nobody list pruning).
  - `privacyRulesRequest(PrivacySetting)`, `setPrivacyRulesRequest(PrivacySetting, PrivacyRules)`
    (returns an empty/invalid object or throws-free `Result` when `!editable`).
  - `PrivacyRules privacyRulesFromJson(const QJsonObject&)` implementing the fail-closed
    fallback for `restrictContacts`/premium/member/bot rules and the
    empty-list ⇒ Nobody rule.
  - `bool supportsExceptions(PrivacySetting)` / `bool supportsNobody(PrivacySetting)`
    for the FindByPhoneNumber special case.
- `src/telegram/BlockedSenders.h/.cpp`
  - `blockedSendersRequest(offset, limit=50)` with `blockListMain`.
  - `setSenderBlockListRequest(kind, id, blocked)`.
  - `struct BlockedSender { enum Kind { User, Chat }; qint64 id; QString title, username; int photoFileId; bool isBot; Kind kind; }`
    and `blockedSendersFromResponse()` for the `messageSenders` array.
  - `reportChatRequest(chatId, optionId, text, messageIds)` + `chooseSpamReportOption()`
    (canonical "spam" only).
- `src/ui/PrivacySettingsModel.h/.cpp` — `QAbstractListModel` with roles
  `SettingRole`, `ChoiceRole`, `EditableRole`, `AlwaysCountRole`, `NeverCountRole`,
  plus `Q_INVOKABLE` `setChoice(int row, int choice)`.
- `src/ui/BlockedSendersModel.h/.cpp` — paged `QAbstractListModel`
  (`loadNext()`, `unblock(row)`, `reportSpam(row)`, `hasMore`, `total`).
- `src/ui/SecurityController.h/.cpp` (or extend `AppController` with a
  `PrivacyController`) — owns the models, sends requests through
  `TelegramRepository`, resolves exception display names via `getUser`.

APIs: `getUserPrivacySettingRules`, `setUserPrivacySettingRules`,
`getBlockedMessageSenders` (returns `messageSenders` in the pinned 1.8.69 schema),
`setMessageSenderBlockList`, `reportChat` (+ `reportChatResultOptionRequired` /
`…TextRequired` handling), `getUser`, `getChat`, `createPrivateChat`.

QML: `qml/pages/PrivacySettingsPage.qml`, `qml/pages/BlockedSendersPage.qml`,
`qml/components/PrivacyRuleRow.qml`, `qml/components/ExceptionPickerDialog.qml`,
and a "Privacy and security" entry in `qml/pages/SettingsPage.qml`.

Tests: `tests/tst_privacy_settings.cpp` (rule round-trip, never-wins, Everyone/Nobody
pruning, read-only fallback, FindByPhoneNumber restrictions),
`tests/tst_blocked_senders.cpp` (paging offset uses fetched count, empty page stops,
chat vs user mapping, spam option selection).

### Stage 2 — P1: active sessions and account security

New files:
- `src/telegram/ActiveSessions.h/.cpp` — `getActiveSessionsRequest()`,
  `terminateSessionRequest(id)`, `terminateAllOtherSessionsRequest()`,
  `struct DeviceSession { qint64 id; bool current; QString device, application, location, ip; qint64 lastActive; }`,
  `sessionsFromJson()`.
- `src/ui/SessionListModel.h/.cpp` — current/others split, `terminate(id)`,
  `terminateAllOthers()`, `refreshing`/`busy`/`error` properties; refuses the current id
  client-side (mirror of Android's re-check).
- `src/telegram/AccountSecurity.h/.cpp` — `getPasswordStateRequest()`,
  `setPasswordRequest(current,new,hint)`, `setRecoveryEmailAddressRequest`,
  `checkRecoveryEmailAddressCodeRequest`, `resendRecoveryEmailAddressCodeRequest`,
  `cancelRecoveryEmailAddressVerificationRequest`, `sendPhoneNumberCodeRequest`,
  `resendPhoneNumberCodeRequest`, `checkPhoneNumberCodeRequest`,
  `deleteAccountRequest`, plus `struct CloudPasswordState` (no secrets),
  `struct PhoneChangeCode`, and `accountSecurityErrorText()` mapping
  (`PASSWORD_HASH_INVALID`, `PASSWORD_MISSING`, `PASSWORD_TOO_FRESH`, `EMAIL_*`,
  `PHONE_CODE_*`, `PHONE_NUMBER_*`, `FLOOD_WAIT`/429).

QML: `qml/pages/ActiveSessionsPage.qml`, `qml/pages/AccountSecurityPage.qml`,
dialogs `PasswordDialog.qml`, `RecoveryEmailDialog.qml`,
`PhoneNumberChangeDialog.qml`, `DeleteAccountDialog.qml`.

Tests: `tests/tst_active_sessions.cpp` (parse, current flag, refusal of current id),
`tests/tst_account_security.cpp` (password state projection, error mapping, typed-word
confirmation, phone-code delivery mapping, no secret in any produced string).

### Stage 3 — P1: secret chats

New files:
- `src/telegram/SecretChats.h/.cpp`
  - `struct SecretChatDetails { qint64 chatId; int secretChatId; qint64 userId; State state; bool outbound; int ttlSeconds; int layer; QByteArray keyHash; }`
    — **memory only**, cleared on logout/`resetRepository()`.
  - `SecretChatKey`: `fingerprint()`, `emoji()` (port `SecretChatKeyEmoji` table
    verbatim), `identiconSide()`, `identicon()`, colours — byte-for-byte parity with
    Android/official clients. Note: the JSON interface encodes `bytes` as **base64**,
    so decode `key_hash` before applying offsets.
  - `SecretChatTtl`: presets 0/3600/86400/604800, `isValid()` (0 or 1..31 536 000),
    `customSeconds(days,hours,minutes)` with the ≥60 s rule.
  - `SecretChatEligibility::canStart()`, `canPost()`.
  - Request builders: `createNewSecretChatRequest(userId)`, `getSecretChatRequest(id)`,
    `closeSecretChatRequest(id)`, `setChatMessageAutoDeleteTimeRequest(chatId, seconds)`.
  - `detailsFromSecretChat(json, chatId, ttl)`.
- `src/ui/SecretChatController.h/.cpp` — `Q_INVOKABLE startSecretChat(qint64 userId)`,
  `refresh(chatId)`, `setTtl(chatId, seconds)`, `closeChat(chatId)`, and
  `Q_PROPERTY` facts for the current chat (`isSecret`, `secretState`, `ttlSeconds`,
  `keyFingerprint`, `keyEmoji`, `keyIdenticon`). Never exposes `keyHash` raw.
- `src/ui/WindowCaptureGuard.h/.cpp` — Windows equivalent of `FLAG_SECURE`:
  `SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)` with a graceful no-op on
  failure (documented as best-effort, exactly as Android documents `FLAG_SECURE`).

Repository wiring: `createSecretChat`, `secretChatDetails`, `setSecretChatTtl`,
`closeSecretChat`; handle `updateSecretChat`, `updateChatMessageAutoDeleteTime`;
suppress forwarding/saving/copying when `has_protected_content` or the chat is secret.

QML: E2E badge in the `ChatsPage.qml` header (next to the subtitle, which already says
"Secret chat"), a `ChatInfoPanel.qml` (Windows currently has no chat-info surface — this
is a dependency), `SecretChatTtlDialog.qml`, `SecretChatKeyDialog.qml` with the
identicon drawn in a `Canvas`/`ShaderEffect` from the exposed pixel list, and a
"Start secret chat" action on the profile/overview surface.

Tests: `tests/tst_secret_chats.cpp` — fingerprint grouping, emoji indices for known
hashes (16-byte ⇒ no emoji, 36-byte ⇒ 5), identicon side/pixels, TTL validation and
custom-seconds bounds, eligibility, `canPost` for closed chats, base64 key decode;
`tests/tst_notification_formatter.cpp` extended for the secret case.

### Stage 4 — P1: passcode / app lock

New files:
- `src/platform/windows/AppLockStore.h/.cpp`
  - JSON file under `AppPaths::configDirectory()` (`app-lock.json`, `{"version":1,…}`),
    written through `QSaveFile` like `NotificationSettingsStore`.
  - PIN: random 16-byte salt, PBKDF2-HMAC-SHA256 via `QPasswordDigestor::deriveKeyPbkdf2`
    with 210 000 iterations and a 32-byte key, constant-time compare; iteration count
    clamped on read (10 000–600 000).
  - Optionally seal the verifier with `DpapiSecretStore` so another Windows user cannot
    copy the hash; losing the DPAPI key disables the lock instead of locking the owner
    out (same "enabled without hash is not a lock" rule as Android).
  - Failure counter + lockout deadline persisted with wall-clock time.
- `src/ui/AppLockController.h/.cpp` — clock-free policy object ported from
  `AppLockPolicy`/`AppLockSession` (4–8 digits, 5 free attempts, 30 s→5 min, timeouts
  0/60/300 s, fail-closed transitions), plus foreground/background tracking driven by
  `QGuiApplication::applicationStateChanged` and a `QTimer` for the timeout.
- `src/platform/windows/WindowsHello.h/.cpp` — `IUserConsentVerifier` wrapper behind an
  interface with a stub implementation, so tests never touch WinRT. PIN always remains
  available (Android's rule).
- `qml/pages/AppLockPage.qml` (settings: enable, change PIN, timeout, biometrics) and
  `qml/LockOverlay.qml` (PIN entry, Hello button, lockout countdown), plus capture
  guarding through `WindowCaptureGuard`.

Tests: `tests/tst_app_lock.cpp` (policy maths, lockout escalation/cap, timeout
transitions, external-handoff/hand-over rules, corrupt file ⇒ disabled),
`tests/tst_app_lock_store.cpp` (round-trip, wrong PIN counting, lockout persistence,
iteration clamp, no PIN in the file).

### Stage 5 — P2: refinements

- Closed-secret-chat posting guard in the composer (disable send, explain why).
- Read-only "managed elsewhere" rendering and lock icon for non-round-trippable rules.
- Session action timeouts modelled as a distinct `Result` state
  (`TimedOut` ≠ empty ≠ success), mirroring `SessionsTimeoutPolicy`.
- `WindowCaptureGuard` on the media viewer while showing secret/protected media.
- Stealth mode (Android-only local feature): only after server-side privacy is done,
  and it must be documented as client behaviour that cannot hide anything from
  Telegram's servers.

---

## 7. Test strategy (planned, not executed)

| Layer | What is testable without a network | Files |
|---|---|---|
| Pure builders/parsers | request shape, rule round-trip, read-only fallback, paging offsets, error mapping, TTL maths, key fingerprint/emoji/identicon, base64 decode | `tst_privacy_settings`, `tst_blocked_senders`, `tst_active_sessions`, `tst_account_security`, `tst_secret_chats` |
| Local stores | JSON round-trip, corrupt file fallback, DPAPI unavailable, iteration clamp, lockout persistence, no secrets on disk | `tst_app_lock`, `tst_app_lock_store` |
| Policy objects | timeout maths, brute-force escalation, hand-over/external-trip rules | `tst_app_lock` |
| Models | roles, current/others split, refusing to terminate the current session, optimistic save + rollback | extend `tst_chat_models` or new model tests |
| Formatter | secret/protected notification masking | `tst_notification_formatter` |
| Integration (manual, dedicated test account only) | real rule read/write, block/unblock, session termination, secret chat create/key/TTL/close | `tst_tdlib_integration`-style, opt-in, never the owner's account |

Per `AGENTS.md`, nothing here may be validated by installing over the user's Dev/Prod
Android package; Android behaviour is the specification, and Windows verification is a
separate desktop build.

---

## 8. Invariants to preserve while implementing

1. Privacy rules and the block list are **server state**; no local cache may be
   authoritative, and none may leak between accounts.
2. A rule BeyGram cannot represent exactly must stay **read-only** — never save a
   compacted approximation.
3. `never` beats `always` for the same user; exceptions precede the base rule.
4. Passwords, PINs, key hashes and QR links are never logged, never stored in plaintext,
   never exposed as QML properties, and never written to diagnostics.
5. Secret-chat metadata is memory-only and cleared on logout; it is never restored from
   backup.
6. The current session can never be terminated by the "other devices" action.
7. A timed-out destructive action is reported as unknown and followed by a refresh —
   never retried silently.
8. Secret chats hide title, text and sender in notifications, and protected content
   hides text and sender, regardless of the global preview switch.
9. Account deletion happens only after the typed confirmation **and** the server
   accepts; local cleanup follows success.
10. Lockout and failure counters survive process death, and a broken crypto provider
    fails closed.

---

## 9. Suggested sequencing

1. Stage 0 (notification leak + chat flags) — small, self-contained, removes a real
   privacy defect and produces the `ChatData` fields every later stage needs.
2. Stage 1 (privacy + block list) — highest user-visible parity, no new platform code.
3. Stage 2 (sessions + account security) — mostly request/parse work, reuses the
   existing dialog patterns.
4. Stage 3 (secret chats) — needs a chat-info surface and the capture guard; largest
   UI effort and the only stage with cryptography-adjacent display logic.
5. Stage 4 (app lock) — independent of the network, can be parallelised, but should
   land after Stage 0 so the lock is never the only protection for exposed content.
6. Stage 5 (refinements) — after the above are stable.

---

## 10. Open questions for the Windows owner

1. Should the passcode be per Windows installation (Android behaviour: app-wide, shared
   by all accounts) or per account? Android is explicit that it is app-wide.
2. Is a Windows Hello prompt acceptable as the primary unlock, or must the PIN always be
   required first (Android keeps PIN as the fallback)?
3. Secret-chat capture protection on Windows is best-effort
   (`WDA_EXCLUDEFROMCAPTURE`); should the UI state that limitation explicitly, as
   Android does for `FLAG_SECURE`?
4. Should the block list show a "Report spam" action for chats too, or users only
   (Android: users only)?
5. `docs/PORTING_ROADMAP.md` schedules secret chats in Phase 5 and blocking/privacy in
   Phase 4. If the ordering above is adopted, those phase notes should be updated in the
   same change that implements them.
