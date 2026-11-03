# BeyGram Windows — Porting Audit

Status: Phase 0 (architecture audit and native foundation)
Reference branch: `origin/DEV` at `fc741ef0` (merge of #671)
Windows branch: `feature/windows-foundation`
Date: 2026-11-03

This audit is based on the actual Android sources, not on the README alone. Every
module named below was located in the repository and its implementation inspected
at least at the level of its public entry points, TDLib calls, persistence and
tests.

---

## 1. Repository baseline

| Fact | Value |
|---|---|
| Windows repository | `Beykus-Y/BeyGram-Windows` (import of the Android repository with history) |
| Default branch on GitHub | `backup/main-before-rollback-pr321-20261022` |
| Local starting point | `origin/DEV` = `fc741ef0` |
| `main` vs `DEV` | `main` is 179 commits behind `DEV` and has 4 commits not in `DEV` |
| Android sources in `DEV` | 619 Kotlin files, 123,626 lines |
| Android unit tests | 374 Kotlin test files (JVM/Robolectric), plus screenshot goldens |
| Native Android code | `app/src/main/cpp/bgbeat_jni.cpp`, `bgmix_jni.cpp`, CMake with BG Media |
| BG Media | git submodule `third_party/bg-media` (not initialised in the clone; empty directory) |
| TDLib on Android | generated Java bindings + `libtdjni.so` for `arm64-v8a` |
| Android min/target SDK | 29 / 36, NDK 30.0.15729638 |
| Android UI | Jetpack Compose, single `MainActivity`, five design styles |

The Windows port starts from `DEV`, because it is the actively developed
functional reference. `main` is not merged into `DEV` by this branch and neither
branch is modified.

The Android tree is preserved in `app/`, `docs/`, `gradle/` and the Android
workflows. The new client lives in `src/`, `qml/`, `tests/`, `cmake/` and its own
workflow.

---

## 2. Architecture as it actually exists on Android

### 2.1 Composition and navigation

- `MainActivity.kt` is the only activity. It applies the theme, app lock,
  edge-to-edge insets, deep links and share intents, then hosts `BeyGramApp`.
- `BeyGramApp.kt` (about 60 KB) is the composition root: it owns the repository
  selection, saved navigation route, tab visibility, appearance drafts, player
  state, notification runtime, account switching and most screen routing.
- Navigation is a hand-written `AppRoute` enum plus `rememberSaveable` state.
  There are 50+ routes (`AppRoute.kt`), four main tabs (Chats, Calls, Contacts,
  Settings) and a separate bottom sheet for account switching.
- There are 18 screen packages and 269 UI files (70,632 lines): chats,
  conversation, media, profile, music, stories, settings, proxy, diagnostics,
  calls, contacts and theme.

**Consequence for Windows:** the screen tree and state transitions are the real
product specification. `BeyGramApp.kt` cannot be translated mechanically; the
Windows equivalent should be an explicit navigation stack (`QML StackView` or a
C++ router) with screen-owned state, not a 2,000-line composition root.

### 2.2 Domain and data boundaries

- `domain/repository/TelegramRepository.kt` is the single UI-facing contract.
  It extends conversation, notification, storage, account-security and
  group-administration contracts.
- `domain/model/` contains 55 immutable model files (chats, messages, media,
  privacy, stories, secret chats, gifts, payments, deep links).
- `data/tdlib/TdLibTelegramRepository.kt` is the main implementation
  (3,436 lines). It owns the TDLib client, chat lists, folders, profiles,
  updates, proxies and many actions; it delegates to focused helpers.
- `TdConversationActions.kt` implements sending, replies, edit/delete and
  permission checks. `TdConversationEffects.kt` is the only gateway for read
  receipts, content-open receipts, online presence and typing actions.
- `MessageHistoryStore.kt` merges pages and live updates with revision
  protection so a late page cannot roll back a newer edit.
- Several stores keep bounded caches and deduplicate reads/writes
  (`KeyedMutex`, `CoalescedPublisher`, `MaintenanceLane`, `MediaFileStore`).

**Consequence:** the repository boundary is a good porting seam. On Windows it
should become a `TelegramRepository` C++ interface with TDLib JSON requests and
Qt models, keeping these stores and their invariants. Do not let QML call TDLib.

### 2.3 TDLib

- TDLib is used through Java bindings (`org.drinkless.tdlib.Client`) and one
  native `libtdjni.so`; there is no custom Telegram protocol code.
- `TdLibConfig` reads `api_id`, `api_hash`, a per-account database directory,
  files directory, language, device model and app version.
- `TdLibRuntime` checks the Java classes and `tdjni`; `TdLibBootstrap` turns that
  into `TdLibReadiness`. When TDLib is absent, `UnavailableTelegramRepository`
  keeps the app buildable and the start screen explains why.
- Multi-account support is account-scoped: `TdAccountRegistry`,
  `TdAccountNativeClientOwner`, `TdSecondaryNativeLogin`,
  `TdSecondarySessionCoordinator` and `TdAccountUpdateRouting` create separate
  native clients and databases, then bind complete repositories. The first
  account keeps its legacy `filesDir/tdlib` database and preference names.
  This is documented as an experimental area with explicit acceptance gaps
  (`docs/multi-account-unified-repositories-185.md`).

**Windows decision:** use the official TDLib **JSON interface** (`tdjson.dll`)
directly from C++20. Each account gets its own `TelegramSession`; no global
client. Phase 0 loads the DLL at runtime, reports absence, and implements the
authorization state machine without a UI.

### 2.4 Persistence and platform services

| Concern | Android implementation | Windows replacement |
|---|---|---|
| Key-value settings | DataStore + SharedPreferences | JSON/QSettings per module + typed stores |
| TDLib data | app-private database/files | per-account AppData directories |
| Secrets / app lock | Android Keystore, PIN hashing, biometrics | DPAPI, Windows Hello, credential store |
| Audio/video | Media3/ExoPlayer, MediaCodec, CameraX | Qt Multimedia / Media Foundation; BG Media native |
| Notifications | foreground service + notification channels | tray + Windows toast + background host |
| Network changes | ConnectivityManager | Windows network status APIs / Qt Network |
| VPN/proxy | Android `VpnService` + Mihomo AAR | external proxy/Mihomo process or Windows service |
| Share target | ACTION_SEND intents | Windows file drop / shell integration |
| Updates | APK download + installer intents | signed MSIX/EXE updater |

### 2.5 Tests

The Android suite has 374 unit-test files, concentrated in TDLib mapping,
media policy, appearance/theme, notifications, music and `.bgmusic`, video-note
diagnostics and setting stores. It is the best source of expected behaviour and
of edge cases. Pure tests can be translated to Qt Test almost one-to-one; tests
that need Android resources, Compose or TDLib are re-specified for Windows.

---

## 3. Feature verification

Legend: **Yes** = user-facing implementation found; **Partial** = implemented
with documented limitations; **History only** = no live feature.

| Feature | State | Evidence |
|---|---|---|
| Phone/code/password authorization | Yes | `LoginPhoneScreen`, `LoginCodeScreen`, `LoginPasswordScreen`, `TdAuthorizationState` |
| QR login | Yes | `AuthState.WaitingForQr`, `requestQrLogin`, QR route |
| Multi-account | Partial (experimental) | account registry/native owner/session coordinator; docs list untested stress cases |
| Chat list, archive, folders, pins, mute | Yes | `ChatsScreen`, `ArchiveScreen`, `FolderManagerScreen`, `TdChatFolders` |
| Message history, replies, edit, delete | Yes | `ChatScreen`, `MessageHistoryStore`, `TdConversationActions` |
| Media send/view/albums | Yes | `MediaComposer`, `MediaViews`, `MessageAlbum`, `TdMediaMapper` |
| Voice messages, video notes | Yes (CameraX-specific) | `AudioNoteRecorder`, `VideoNoteRecorder` (59 KB), `VideoNote*.kt` |
| Reactions, paid reactions, stickers, custom emoji | Yes | `TdMessageReactions`, `TdStickerCatalog`, `TdPaidReactions` |
| Gifts, Stars, Premium | Yes | `TdPayments`, `GiftsScreen`, `Premium` routes |
| Secret chats | Yes | `TdSecretChats`, `SecretChatKeyEmoji`, `SecretChatSettings` |
| Stories | Yes | `TdStories`, `StoryBar`, publisher/archive screens |
| Calls | History only | `TdCallHistory` reads `MessageCall`; no VoIP implementation |
| Bots, inline queries, mini apps | Yes (Android WebView) | `TdBotFeatures`, `BotWebAppScreen`, `TelegramWebAppBridge` |
| Translation / transcription | Yes | `MessageTranslation`, speech-recognition mapping/trial store |
| TDLib proxies, Mihomo SOCKS | Yes | `Proxy*Screen`, `VpnBackend`, `MihomoProxyService` |
| Notifications without FCM | Yes (foreground service) | `NotificationRuntime`, `TelegramNotificationService` |
| App lock / biometrics | Yes | `security/AppLock*`, `BiometricAuthenticator` |
| Themes: Default/M2/M3/Liquid Glass/Signature | Yes | `DesignStyle`, `docs/design-system-v2.md`, `docs/liquid-glass.md` |
| Music library, playlists, `.bgmusic` | Yes on `DEV` | `data/music`, `data/music/bgmusic`, `MyMusicScreen` |
| BG Beat / BG Mix | Partial (JNI, BG Media missing) | `bgbeat_jni.cpp`, `bgmix_jni.cpp`, `media/bgbeat`, `media/bgmix` |
| Equalizer | Yes | `EqualizerController`, `EqualizerStore`, `EqualizerScreen` |
| Diagnostics + bug report bot | Yes | `diagnostics/*`, `ReportProblemScreen`, redaction |
| Analytics toggle | Yes | `analytics/AnalyticsStore`, `AnalyticsApi` |
| APK updater | Yes (Android only) | `StableUpdatesScreen`, `DevUpdateRepository` |
| Share target / deep links | Yes | `share/*`, `deeplink/*` |

Notable absence: there are **no voice/video calls** in the Android client. The
Calls tab is call history from Telegram messages. The Windows roadmap must not
promise VoIP unless it becomes a new feature.

---

## 4. Porting categories

### A — direct algorithm port (C++20)

These are pure logic: no Android types, no Compose, no JNI. They should be
translated with the same test cases.

- Theme token maths, contrast/readability rules, AMOLED derivation, bubble
  geometry, density/spacing tokens (`ui/theme`, `data/appearance`,
  `docs/theme-engine.md`).
- `.bgmusic` v1 container: manifest limits, strict JSON rules, archive entry
  validation, cover magic bytes, import decision/planning, export mapping
  (`data/music/bgmusic/*`, `docs/formats/bgmusic-v1.md`). The full format is
  31 KB of specification and 12 source files — a high-value direct port.
- Message markup conversion, markdown/entity limits, text truncation.
- Auto-download policy, storage cleanup rules, media limits, traffic accounting
  (`data/media`, `data/tdlib/StorageCleanupRules`, `AutoDownload*`).
- Video-note diagnostics maths: frame timing, FPS decisions, AV sync, bitrate
  policy, root-cause classification (`data/media/VideoNote*`). The camera and
  codec parts are category B/D; the analysis itself is A.
- Notification grouping/merging, unread rules, reply routing tokens (HMAC),
  notification preview policy.
- Search filters, contact alphabet grouping, forum topic preview rules,
  deep-link validation, username/phone validation.
- Music queue ordering, shuffle, smart collections, replacement rules,
  playback window (`data/music`, `playback/PlaybackShuffleOrder`).
- Settings migration and sanitisation rules (appearance, stealth, translation,
  storage, chat overrides).
- Diagnostics redaction and report nonce/correlation rules (except Android
  transport).
- Analytics body/retry policy (pure parts).

### B — platform adaptation

Same behaviour, different OS API.

- TDLib runtime and session management: Java JNI → `tdjson.dll` C API.
- Per-account storage layout and database leases → Windows directories and
  file locks; DPAPI for secrets.
- Media playback: Media3/ExoPlayer → Qt Multimedia / Media Foundation; BG Mix
  native output → WASAPI/miniaudio adapter.
- Microphone/camera capture, video-note recorder, photo editor → Windows Media
  Foundation / DirectShow / Windows Graphics Capture; no CameraX equivalent.
- Notifications: channels/foreground service → tray icon, Windows toast,
  background process, notification reply activation.
- Network/proxy: Android `VpnService`, Mihomo AAR → Mihomo external core,
  Windows service or SOCKS listener; TDLib proxy requests are portable.
- App lock: Android Keystore/biometrics → DPAPI + Windows Hello.
- File pickers, share intents, drag-and-drop, file associations.
- APK updater → signed Windows installer/updater (new design).
- Locale handling and RTL → Qt translation + bidi (portable, but resources
  differ).
- Deep links (`tg://`) → Windows protocol registration.
- System theme and reduced transparency → Windows personalisation APIs.

### C — interface rewrite (Compose → QML)

All 269 UI files. The UI is 57% of the Kotlin corpus, so this is the largest
work package even though screen logic is smaller than the file count suggests.

Screens/groups:

1. Auth: start, phone, code, password, QR.
2. Main shell: tabs, navigation rail/tablet layout, account switcher, app lock.
3. Chats: list, archive, folders, search, global search, new chat, channel/group
   creation, chat actions.
4. Conversation: header, message list, bubbles, albums, composer, attachments,
   stickers/GIF, reactions, replies, pinned, scheduled, drafts, search, forum
   topics, bot keyboards, inline keyboards.
5. Media: photo/video/audio viewers, shared media, media editor, voice notes,
   video notes, camera.
6. Profiles/contacts: own profile, user/group/channel, members, shared media,
   gifts, premium, stars, sessions, privacy.
7. Music: My Music, playlists, now-playing sheet/bar, queue, search, equalizer,
   BG Beat sections, `.bgmusic` chat card/preview/export.
8. Stories: bar, viewer, publisher, archive, privacy editor.
9. Settings: appearance/themes (5 styles, Liquid Glass quality), notifications,
   storage, auto-download, video messages, language, sessions, security,
   stealth, translation, proxy, diagnostics, updates.
10. Calls history.
11. Dialogs, bottom sheets, context menus, toasts/snackbars.

Desktop-first deviations to design deliberately, not copy blindly:
three-pane chat layout, keyboard shortcuts, drag-and-drop, context menus,
multi-window, resizable panels, desktop typography and hit targets, hover
states, tray behaviour, high-DPI scaling.

### D — special cases (not literal ports)

- **Android biometrics** and `BiometricPrompt`: Windows Hello is the analogue,
  but the credential model and fallback differ; must be re-specified.
- **Android VpnService + bundled Mihomo AAR**: running a VPN is a system-level
  Windows service/driver decision. Phase 0 keeps only the TDLib proxy boundary;
  shipping a VPN needs a separate design and signing/security review.
- **Foreground service notification model**: Windows has no direct equivalent.
  Background delivery needs a tray application, startup task and possibly a
  Windows service; behaviour must not be claimed until implemented.
- **CameraX video-note pipeline** (Camera2/CameraX, MediaCodec, sensor survey):
  cannot be ported 1:1; rebuild on Media Foundation.
- **Material You dynamic colours**: Android wallpaper extraction has no Windows
  equivalent; keep Warm/Graphite and a future accent picker.
- **Android share target and direct-share shortcuts**: replace with shell
  integration and drag-and-drop, not an intent filter clone.
- **Mini apps / WebView**: the main interface must not be a WebView. Third-party
  mini apps may use an isolated component (Qt WebEngine) later; this is a
  product and security decision, not Phase 0.
- **Android APK updater**: replace with a signed desktop updater; do not reuse
  APK manifests.
- **ADB/Android diagnostics**: replace with Windows event/log collection.
- **Material 2/3 pixel-exact chrome**: keep the styles as design languages, but
  desktop spacing/hover/focus rules must diverge where Android patterns do not
  translate.
- **Android screenshot goldens**: QML visual tests need a different harness
  (Qt Quick Test + image comparison) rather than Roborazzi.

---

## 5. Module-by-module migration map

Complexity: S < 1 week, M 1–3 weeks, L 1–2 months, XL > 2 months for one
experienced C++/Qt engineer. Priority: P1 is the Phase 1–2 critical path.

| ID | Module | Key sources | Purpose | Strategy | Cat. | Cplx | Risks | Required tests | Prio |
|---|---|---|---|---|---|---|---|---|---|
| M01 | TDLib runtime, config, auth state | `TdLibConfig`, `TdLibRuntime`, `TdLibBootstrap`, `TdAuthorizationState`, auth screens | Start TDLib, map authorization states, login | JSON interface in C++; state machine + QML forms | B/C | M | TDLib Windows build; ABI; secrets; QR | state mapping, parameter building, no-secret logging, login integration | P1 |
| M02 | Accounts and sessions | `TdAccount*`, `TdSecondary*`, account switcher docs | Several independent TDLib clients/databases | One `TelegramSession` per account; registry; leases | A/B | L | DB locking; update routing; logout/delete data loss | registry, routing isolation, restart/restore, logout | P2 |
| M03 | Repository core and updates | `TdLibTelegramRepository`, `TdRequests`, `TdAccountUpdateRouting`, `CoalescedPublisher`, `KeyedMutex` | Domain API over TDLib, event routing | C++ repository interface + async request layer + Qt models | A/B | XL | 3.4k-line god class; ordering; lifetimes | request correlation, routing, backpressure, cancellation | P1 |
| M04 | Chats, archive, folders, search | `ChatsScreen`, `ArchiveScreen`, `FolderManagerScreen`, `GlobalSearchScreen`, `TdChatFolders`, search helpers | Chat list ordering, folders, search | QML list models over C++ repositories | A/B/C | L | ordering/pins; server folders; perf | order/pin/mute, folder rules, search paging | P1 |
| M05 | Conversation and message actions | `ChatScreen` (70 KB), `MessageHistoryStore`, `TdConversationActions`, `TdConversationEffects`, drafts, pinned, scheduled | History, send/edit/delete, replies, receipts | C++ stores + QML conversation | A/B/C | XL | correctness of pagination/edits; receipts; typing | history merge, revisions, permissions, read markers | P1 |
| M06 | Media files and storage | `TdMediaMapper`, `MediaFileStore`, `OutgoingMediaImporter`, `AutoDownload*`, `TdStorage`, cleanup | Download/upload, cache, limits | Qt network/file plumbing over TDLib file API | A/B | L | file lifecycle; disk limits; partial files | download state, limits, cleanup, quotas | P2 |
| M07 | Composer and attachments | `MediaComposer`, `FormattingTextField`, stickers, bot keyboards, albums | Compose outgoing messages | QML composer + C++ send pipeline | A/B/C | L | rich text entities; paste/drag-drop; IME | draft persistence, entity limits, album send | P2 |
| M08 | Voice/video notes | `AudioNoteRecorder`, `VideoNoteRecorder` (59 KB), `VideoNoteDiagnostics`, `VideoNoteFps*` | Record, process, diagnose circular video | Media Foundation capture; reuse analysis algorithms | A/B/D | XL | codec/camera differences; AV sync | policy/analysis unit tests, device capture matrix | P2 |
| M09 | Secret chats | `TdSecretChats`, `SecretChatKeyEmoji`, `SecretChatSettings`, secret notification rules | Create/close, TTL, key emoji, privacy | TDLib calls are portable; QML screens | A/B/C | M | E2E guarantees; screenshot/privacy; notification leaks | create/close validation, TTL, key emoji mapping | P3 |
| M10 | Reactions, stickers, gifts, Premium, Stars | `TdMessageReactions`, `TdStickerCatalog`, `TdPayments`, `GiftsScreen`, profile premium | Monetisation and expression | Domain mapping is portable; checkout opens external URL | A/B/C | L | payments must stay external; Premium limits | reaction filtering, limits, payment URL safety | P3 |
| M11 | Stories | `TdStories`, `Story*Screen`, `StoryMediaPreparer` | Post/view/archive stories | TDLib portable; media prep adaptation | A/B/C | L | upload/privacy; expiry; moderation | story state, privacy, archive paging | P3 |
| M12 | Profiles, contacts, groups, channels | `ProfileScreen`, `GroupProfile`, `TdProfiles`, `TdContacts`, `TdGroupAdministration` | Profiles, members, admin actions | C++ domain + QML | A/B/C | L | permissions; large member lists | permission checks, profile mapping | P2 |
| M13 | Notifications and background | `NotificationRuntime` (23 KB), service, renderer, reply outbox, preferences | Delivery, grouping, inline reply, background | Windows toast + tray + background host; reuse grouping/token rules | A/B/D | XL | no FCM; OS restrictions; reply delivery | grouping, token HMAC, outbox replay, background lifecycle | P2 |
| M14 | Music library and `.bgmusic` | `data/music/*`, `data/music/bgmusic/*` | Library, playlists, snapshots, import/export | Direct C++ port of format + stores; QML UI | A/B/C | L | format compatibility; ZIP/JSON security; large lists | format v1 vectors, fuzz, round-trip, import limits | P3 |
| M15 | Player, BG Mix, BG Beat, equalizer | `PlaybackEngine`, `BgMix*`, `BgBeat*`, `Equalizer*`, cpp JNI | Playback, transitions, beat analysis, EQ | Native BG Media in-process; Qt audio output; no JNI | A/B | XL | BG Media API stability; audio device; licensing | queue/transition tests, BG Media capability tests | P3 |
| M16 | Theme engine and design system | `ui/theme/*`, `ui/components/*`, `data/appearance/*`, motion stores, docs | 5 styles, tokens, Liquid Glass, motion | Port tokens/rules; QML component library; glass with Qt effects | A/C | XL | visual fidelity vs performance; glass on desktop | token/contrast tests, QML visual tests, performance | P2 |
| M17 | Settings, privacy, security | settings stores, `AppLock*`, `TdPrivacy`, `PrivacySettings` | Preferences, privacy rules, app lock | JSON stores + DPAPI; Windows Hello; QML | A/B/C | L | migration; lockout; privacy semantics | store migration, PIN hashing, DPAPI, privacy mapping | P2 |
| M18 | Proxy, Mihomo, VPN | `data/vpn/*`, proxy screens, `docs/vpn-mihomo.md` | Proxy routing and subscriptions | TDLib proxy calls portable; Mihomo as external core | A/B/D | XL | Windows VPN architecture; secrets; network changes | proxy CRUD, route selection, subscription parsing | P3 |
| M19 | Diagnostics, reports, analytics | `diagnostics/*`, analytics | Redacted logs, bug report, resource samples | Port redaction/report rules; Windows collectors | A/B | M | leaking secrets; report consent | redaction suite, report body, opt-in analytics | P1 (logging) |
| M20 | Deep links, share, updates | `deeplink/*`, `share/*`, update repos | Open tg links, share files, update app | Windows protocol + drop target + signed updater | B/D | M | security of URLs; updater trust | link parsing, path safety | P2 |
| M21 | Calls tab | `CallsScreen`, `TdCallHistory` | Call history only | Port list mapping; no VoIP | A/C | S | expectation mismatch | status mapping, paging | P3 |
| M22 | Bots, mini apps, translation, transcription | `TdBotFeatures`, `BotWebAppScreen`, `TelegramWebAppBridge`, translation/speech | Bot menus, inline results, mini apps, translate/transcribe | TDLib parts portable; mini apps need optional WebEngine; speech needs Windows API/service | A/B/D | L | WebView policy; untrusted content; speech privacy | URL validation, menu mapping, opt-in | P3 |
| M23 | Platform services | Manifest, FileProvider, share, intents, MediaSession | OS integration | Replace with Windows equivalents | B/D | L | notifications, file associations, single instance | integration smoke tests | P2 |
| M24 | Test and CI infrastructure | `app/src/test`, screenshot tests, workflows | Quality gate | Qt Test + QML tests + Windows CI; port pure tests | A/B | M | Android-only assumptions | CI green, coverage of migrated logic | P1 |

---

## 6. Directly portable versus rewritten

**Portable with little redesign (A):**
theme maths and contrast, `.bgmusic` format, message markup and limits, media
policy, video-note analysis, notification grouping/token/outbox rules, queue and
shuffle logic, search filters, validation, settings migration, redaction,
report correlation, storage cleanup. These are also the best first tests to
translate.

**Needs a platform backend but keeps behaviour (B):**
TDLib transport, media playback, capture, notifications, credentials/lock,
proxy routing, file handling, updater.

**Must be rebuilt in QML (C):**
every Compose screen and component, the navigation shell, gestures, animations
and the five design styles. Expect this to be the majority of the effort.

**Cannot be copied literally (D):**
CameraX pipeline, Material You wallpaper colours, Android VPN service,
foreground-service notification model, Android biometric prompt, APK updater,
intent-based share target, mini-app WebView policy, screenshot goldens.

---

## 7. Effort estimate

The Android client is not a tutorial-sized app: 123,626 lines of Kotlin, 269 UI
files, 3,436 lines in one TDLib repository, 374 unit-test files, native media
code and a five-style design system.

| Scope | Estimate (one experienced C++/Qt engineer) |
|---|---|
| Phase 0 foundation (this branch) | complete in days, not weeks |
| Auth + chat list + history + send/edit/delete | 2–3 months |
| Media, profiles, contacts, settings, themes | 2–4 months |
| Music, `.bgmusic`, BG Mix/Beat, equalizer | 2–3 months |
| Stories, gifts, Premium, secret chats, bots | 2–3 months |
| Notifications, proxy/VPN, security, updater, polish | 2–4 months |
| **Full functional parity (no VoIP, no calls by design)** | **≈ 12–18 engineer-months** |
| With desktop polish, accessibility and release hardening | 18–24 engineer-months |

These are planning ranges, not commitments. They assume the Android reference
stays buildable and testable during the port.

The single largest risk is not UI volume but TDLib lifecycle correctness under
multi-account load, plus Windows media/notification platform work. The second
largest is visual fidelity of Liquid Glass without the performance regressions
documented on Android.

---

## 8. Windows target architecture (decision record)

```
qml/                  presentation only: windows, panes, components, models
src/ui/               QML-facing controllers and list models (no TDLib JSON)
src/telegram/         TDLib JSON transport, sessions, auth, repositories
src/core/             logging, redaction, paths, result, colour rules
src/platform/windows/ DPAPI, Windows Hello, shell, notifications
src/media/            playback/capture boundaries and BG Media adapter
tests/                Qt Test suites for portable rules and integration seams
```

Rules:

1. QML never sends a TDLib request and never parses Telegram JSON.
2. Each account owns one `TelegramSession`; no singleton client.
3. Every blocking or network operation runs off the GUI thread.
4. Secrets are never logged and are stored with DPAPI, not plain JSON.
5. A missing TDLib / credential / service is shown as a real state; no fake
   chats or placeholder content that looks like working Telegram.
6. Android `app/` remains the reference until each module's migration is done;
   it is not deleted in Phase 0.
