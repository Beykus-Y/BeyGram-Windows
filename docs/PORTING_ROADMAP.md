# BeyGram Windows — Porting Roadmap

Companion to [PORTING_AUDIT.md](PORTING_AUDIT.md). The roadmap is ordered by
dependency and user value. Every phase ends with a buildable, tested Windows
artifact; no phase depends on a fake implementation from a later phase.

Legend: **P0/P1/P2** are execution priorities. Estimated durations assume one
engineer and exclude waiting for upstream TDLib/BG Media releases.

---

## Phase 0 — Architecture audit and native foundation (this branch)

**Status:** implemented, built and tested locally; TDLib is not bundled.

Deliverables:

- C++20 CMake project with separate targets: `beygram_core`,
  `beygram_platform_windows`, `beygram_telegram`, `beygram_media`,
  `beygram_ui`, `beygram` (desktop executable).
- Qt 6 Quick application with an honest three-region desktop shell:
  navigation, content and status/diagnostics.
- Runtime-loaded TDLib JSON transport with a real authorization state machine
  and no UI login form yet.
- Thread-safe redacting logger with rotation and an in-app log viewer.
- DPAPI secret store with tests.
- Theme engine port for Warm/Graphite, FollowSystem/Light/Dark/AMOLED.
- Windows CI workflow for configure, build and tests.
- This audit and roadmap.

Exit criteria (met locally):

- `cmake --build` produces `beygram.exe`.
- `ctest` passes all suites.
- Removing/omitting TDLib produces a visible "runtime missing" state, not a
  crash or fake connection.
- Android sources are untouched (the inherited Android workflows were removed
  later by the owner's request; Android remains the source reference).

Not in Phase 0: login UI, chats, media, notifications, music, VPN, packaging.

---

## Phase 1 — TDLib and authorization (P0)

**Status:** implemented in `feature/windows-authentication` and validated with a
real TDLib build. Automated tests cover transport loading, a throwaway
`authorizationStateWaitPhoneNumber` session, close and database reopen. Real
wrong-code / wrong-2FA / QR-confirmation runs need a dedicated Telegram test
account and are left as a manual gate.

**Goal:** a real Telegram session that survives restarts, without a chat UI.

Scope:

1. Build and ship `tdjson.dll` for Windows x64 (MSVC) and document exact steps.
2. Provision the API credentials safely:
   - development: environment variables (already supported);
   - product: DPAPI-protected credential file with an in-app setup dialog.
3. Complete the authorization state machine:
   - phone → code → password, registration, QR;
   - error mapping to human messages, retry and reset;
   - `Close`/`LogOut` handshake with a bounded graceful shutdown.
4. Session persistence:
   - per-account directories under `%APPDATA%/Beykus/BeyGram Windows/tdlib`;
   - database encryption decision recorded (TDLib key handling, DPAPI-wrapped);
   - never log auth payloads, QR links or codes.
5. Minimal login QML screens using the Phase 0 components.
6. Account registry (single account first, structure for N accounts).

Exit criteria:

- A real account can log in, restart the app, and remain authorized.
- Wrong code/password shows Telegram's error without crashing or leaking it.
- QR login works and the link is never persisted.
- `LogOut` is explicit and distinguishes logout from close.
- Tests: state mapping, parameter building, redaction, persisted file
  permissions, graceful close.
- Manual test: fresh Windows user profile, no pre-existing data.

Duration: 3–5 weeks.

---

## Phase 2 — Chat list, history and the conversation MVP (P0)

**Status:** user-facing slice implemented in `feature/windows-authentication`:
main chat list with pin/unread/mute/search, TDLib-update ordering, text history,
text send/edit/delete, read receipts, and proxy support. Rich text, replies,
media, drafts, folders/archive UI and notifications remain open in this phase.

Scope:

1. C++ repository interface and TDLib request layer with a per-session update
   router.
2. Chat list model: order/pins/mute/archive/unread, folders, avatars.
3. History paging with revision protection and live update merging.
4. Conversation UI: messages, albums, replies, edit/delete, forwards, copy,
   text entities, drafts, pinned messages, in-chat search.
5. Read receipts and typing/presence through a policy gateway (port
   `ConversationRepository` / `TdConversationEffects` boundaries).
6. Send text and files; media metadata/download foundation.
7. Three-pane desktop layout: folders/chats, conversation, optional info pane.
8. Keyboard shortcuts: next/previous chat, search, reply, edit, delete, send.

Exit criteria:

- Read/send/edit/delete in private chats, groups and channels (permissions
  enforced by TDLib and re-checked before actions).
- No message duplication or rollback after live edits; late pages do not
  resurrect deleted messages.
- Chat list stays consistent across restart.
- UI never blocks on a TDLib call.
- Tests: history merge, revision guard, permission rules, draft persistence,
  search paging, read marker mapping.
- Manual test on a real account with slow network and a second device.

Duration: 6–10 weeks.

---

## Phase 3 — Media, profiles, settings, themes (P1)

**Goal:** the everyday client becomes usable beyond text.

Scope:

1. Photo/video/audio viewing, shared media, downloads and cache limits.
2. Composer attachments: picker, drag-and-drop, paste, albums, captions,
   formatting.
3. Profiles: own/user/group/channel, photo update, shared media, members.
4. Contacts, blocking, privacy and active sessions.
5. Settings framework with typed stores and migration tests.
6. Theme engine completion: five design styles, editor, wallpapers, motion,
   scale, light/dark/AMOLED, contrast guards.
7. Notification preferences UI only (delivery is Phase 6).
8. Accessibility pass: focus order, names, contrast, scalable fonts.

Exit criteria:

- Media round trip (send → view → download → cache cleanup) verified.
- Theme switch changes the whole shell without restart; custom colours remain
  readable.
- Settings survive upgrade from earlier Windows builds.
- QML visual smoke tests and contrast tests in CI.

Duration: 8–12 weeks.

---

## Phase 4 — Music, `.bgmusic`, BG Beat and BG Mix (P1/P2)

**Goal:** parity with the Android music library and native BG audio.

Scope:

1. Direct C++ port of `.bgmusic` v1 reader/writer with the full security and
   limit rules from `docs/formats/bgmusic-v1.md`.
2. Music library, playlists, smart collections, history, search.
3. Qt Multimedia playback with queue, shuffle, repeat, speed, session restore.
4. BG Media integration:
   - initialise the pinned BG Media core directly in C++ (no JNI);
   - BG Mix transitions and status; BG Beat analysis/segments;
   - choose the Windows audio backend and document licensing.
5. Equalizer and playback UI (mini-player, now-playing pane, queue).
6. `.bgmusic` chat card, preview, import and export.

Exit criteria:

- Format compatibility with Android v1 test vectors and fuzz corpus.
- Import of a hostile archive is rejected without unbounded memory/disk use.
- Playback continues when the main window is minimised; media keys work.
- BG Media absence is reported as a capability, never as working audio.
- Tests: format round-trip, limits, import planner, shuffle/window, session
  restore, BG Media capability.

Duration: 8–12 weeks.

---

## Phase 5 — Stories, gifts, Premium, secret chats, bots (P2)

**Goal:** feature parity for the extended Telegram surface.

Scope:

1. Stories: viewer, publisher, archive, privacy, reactions, expiry.
2. Gifts, Stars, Premium: read-only info, explicit external checkout, gift
   sending (never retry, never collect payment data).
3. Secret chats: create, TTL, key emoji, privacy, close, notification masking.
4. Bot platforms: command menus, inline queries, bot keyboards; mini apps
   behind an explicit optional WebEngine component (not the main UI).
5. Translation and transcription with opt-in and provider selection.
6. Polls, dice, geo, contacts, scheduled messages, topics/forums.

Exit criteria:

- Payments remain external; no card data enters the app.
- Secret-chat content is excluded from previews/notifications by default.
- Story and gift state survives restart and account switch.
- Security review of URL handling and mini-app isolation.

Duration: 10–14 weeks.

---

## Phase 6 — Notifications, proxy/VPN, security, background (P1)

**Goal:** the client behaves like a desktop messaging app when the window is
closed.

Scope:

1. Tray application and Windows toast notifications with grouping, avatars,
   quiet hours and per-chat preferences.
2. Inline reply and mark-as-read from notifications, with the same durable
   outbox semantics as Android.
3. Background connection host with explicit user consent, startup behaviour and
   clear connection status.
4. Proxy management (SOCKS5/MTProto/HTTP) and Mihomo routing design.
   Windows VPN decisions are separate; do not ship a driver without a dedicated
   security review.
5. Network change detection and reconnect policy.
6. App lock with DPAPI and Windows Hello; auto-lock on idle/session lock.
7. Secure data layout audit: DPAPI for secrets, no plaintext tokens, safe logs.

Exit criteria:

- Message delivery while minimised, with a truthful connection indicator.
- Reply outbox replay is at-least-once and never crosses accounts.
- Proxy enables/disables without losing the session.
- Lock/unlock and Windows session lock never expose message content.
- Tests: token HMAC, outbox replay, notification grouping, route selection.

Duration: 8–12 weeks.

---

## Phase 7 — Desktop-first polish and parity sweep (P2)

Scope:

- Multi-window (chat in separate window), tabbed chats, split view.
- Command palette, global search, advanced shortcuts.
- Drag-and-drop between chats, file associations, jump lists.
- UI scaling, RTL, five languages (ru/en/de/es/fr), accessibility audit.
- Performance: virtualised lists, image cache, startup time, memory budget.
- Liquid Glass quality levels with a real performance measurement on Windows.
- Compare every Android screen against the audit table; close gaps or record a
  deliberate platform difference.

Exit criteria:

- A screen-by-screen parity checklist is signed off.
- No P1/P2 crash in a two-week dogfood period.
- Performance budgets documented and met on a mid-range Windows machine.

Duration: 6–8 weeks.

---

## Phase 8 — Packaging and release (P2)

Scope:

- Signed installer/MSIX or portable build, per-user installation.
- Auto-update channel with signature verification and rollback.
- Crash reporting with explicit consent and redaction.
- Licence and third-party notices (TDLib, Qt, BG Media, Mihomo if bundled).
- Release checklist, reproducible CI, versioning, staged rollouts.

Exit criteria:

- A clean Windows 10/11 x64 machine installs, updates and uninstalls safely.
- Update signature failure is refused with the old version intact.
- Release artifact is reproducible from a tagged commit.

---

## Test strategy across phases

| Layer | Tool | What |
|---|---|---|
| Pure algorithms | Qt Test | format, policy, mapping, contrast, redaction |
| Repository integration | Qt Test + fake transport | request/response correlation, update routing, cancellation |
| TDLib integration | dedicated test account | auth, send/edit/delete, reconnect; never the user's main session |
| QML | Qt Quick Test / visual snapshots | component states, navigation, theme, layout |
| Platform | manual + scripted | toasts, tray, DPAPI, Windows Hello, DPI, audio devices, updater |
| Performance | ETW/QML profiler | startup, scroll, memory, battery/CPU idle |

Android tests are the source of expected values. Port them with the code they
cover, not after the fact.

---

## Explicit non-goals for the near term

- A custom Telegram protocol implementation.
- Java/JVM/Kotlin/Electron in the Windows app.
- A WebView-based main interface.
- VoIP calls (the Android client only shows call history).
- Android database migration into Windows (TDLib databases are not portable
  between platforms; Windows will authenticate separately).
- A VPN driver in the initial release.
- Merging this branch into `main` or `DEV` without the owner's approval.
