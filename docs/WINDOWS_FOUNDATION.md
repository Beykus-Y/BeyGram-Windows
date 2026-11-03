# BeyGram Windows — native client (Phase 2)

This document describes the native Windows client. The Android client under
`app/` remains the functional reference and is not part of this build.

## What works

- C++20 / Qt 6 desktop application (`beygram.exe`) with a Qt Quick shell.
- Real TDLib JSON transport loaded from a trusted location.
- Full authorization flow driven by TDLib states:
  phone number, login code, two-step password, email address/code (when
  Telegram asks for it), registration, and QR login.
- Session restoration across restarts with an encrypted TDLib database.
- Explicit `close`, explicit `logOut`, and bounded emergency shutdown.
- Per-account storage directories, a durable account registry and a
  single-process lock for the active TDLib database.
- API credentials and the TDLib database key protected with Windows DPAPI.
- Request/response correlation through TDLib `@extra`.
- Main chat list, chat search, message history, text sending, editing,
  deletion and read receipts through TDLib updates.
- Real chat avatars and photo/video/document/sticker thumbnails, downloaded
  on demand and cached by TDLib; full media viewing is not implemented yet.
- Optional SOCKS5/HTTP/MTProto proxy via `BEYGRAM_PROXY` (needed where Telegram
  is not directly reachable).
- Thread-safe redacting logger with rotation and an in-app log page.
- Warm/Graphite palettes and FollowSystem/Light/Dark/AMOLED brightness.
- Qt Test suites and Windows CI.

## What does not work yet

- Rich text entities, full media viewing/playback, stickers, reactions, replies,
  albums, drafts, folders/archive UI, notifications, profiles and calls are not
  in this slice. Media messages show downloaded thumbnails but have no viewer;
  unsupported content still uses honest placeholders.
- No music, BG Beat/BG Mix, VPN or updater.
- Wrong-code and wrong-2FA behaviour is implemented in the session and mapped
  from real TDLib errors, but automating those cases needs a dedicated test
  Telegram account. They are not run against a real account in CI.
- No automatic migration of the Android TDLib database (TDLib databases are
  platform-bound). Windows authenticates separately.

## Requirements

| Tool | Version used |
|---|---|
| Windows | 10/11 x64 |
| MSVC | Visual Studio 2022 (19.44 tested) |
| CMake | 3.24+ |
| Qt | 6.8.3 MSVC 2022 x64 (`Core`, `Gui`, `Quick`, `QuickControls2`, `Test`) |
| TDLib | pinned commit `4c6377aa…` built with MSVC x64 |
| OpenSSL / zlib | 3.6.5 / 1.3.2 via vcpkg |

TDLib build instructions: [WINDOWS_TDLIB.md](WINDOWS_TDLIB.md).

## Configure, build and test

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64 `
  -DBEYGRAM_TDLIB_DIR=F:\tdlib\build\bin\Release `
  -DBEYGRAM_BUILD_TESTS=ON

cmake --build build --config Release --parallel

$env:PATH = "C:\Qt\6.8.3\msvc2022_64\bin;$env:PATH"
ctest --test-dir build -C Release --output-on-failure
```

Without `-DBEYGRAM_TDLIB_DIR`, the app still builds and reports the missing
runtime honestly. The integration test skips with an explicit message.

Run:

```powershell
.\build\Release\beygram.exe
```

## Proxy

If Telegram is not directly reachable, set `BEYGRAM_PROXY` before starting:

```powershell
# SOCKS5 (for example a local Clash/Mihomo mixed port)
$env:BEYGRAM_PROXY = "socks5://127.0.0.1:7897"

# HTTP CONNECT
$env:BEYGRAM_PROXY = "http://127.0.0.1:8080"

# MTProto proxy (secret in the URL query)
$env:BEYGRAM_PROXY = "mtproto://server:443?secret=0123456789abcdef0123456789abcdef"
```

Separate variables are also accepted: `BEYGRAM_PROXY_SERVER`,
`BEYGRAM_PROXY_PORT`, `BEYGRAM_PROXY_TYPE` (`socks5`, `http`, `mtproto`),
`BEYGRAM_PROXY_USERNAME`, `BEYGRAM_PROXY_PASSWORD`. The proxy summary appears
on the Environment page; the password is never shown or logged.

## First-run authentication

For a local test build that uses the same `secrets.properties` as the Android
project, configure with:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64 `
  -DBEYGRAM_TDLIB_DIR=F:\tdlib\build\bin\Release `
  -DBEYGRAM_BUILD_TESTS=ON `
  -DBEYGRAM_EMBED_API_CREDENTIALS=ON
```

The api_id/api_hash are read from the gitignored `secrets.properties` and
embedded as build defaults. Credential precedence is: environment variables,
then the DPAPI store, then these build defaults. Such a build is for local
testing only: the hash is extractable from the executable and must not be
distributed. The UI marks the source as `BUILD`.

Without this option, start the app and enter the api_id/api_hash in the
credentials card; they are stored with DPAPI.

1. Start the app. If `tdjson.dll` is missing, the Auth page says so and shows
   the trusted locations it checked.
2. Enter the Telegram `api_id` and `api_hash` from
   <https://my.telegram.org/apps>. They are validated (hash: 32 hex) and stored
   with DPAPI. Environment variables
   `BEYGRAM_TELEGRAM_API_ID` / `BEYGRAM_TELEGRAM_API_HASH` take precedence.
3. Choose phone login or QR login.
4. Follow the TDLib-driven stages: code, two-step password, email, or
   registration if Telegram requires them.
5. After `authorizationStateReady`, the account overview appears. The session
   is restored automatically on the next start.

The app never reports a successful login before TDLib reports
`authorizationStateReady`. Errors come from real TDLib responses.

## Security model

### Trusted TDLib loading

`tdjson.dll` is loaded only from:

1. `<application dir>\tdjson.dll`;
2. `<application dir>\tdlib\tdjson.dll`;
3. an absolute `BEYGRAM_TDLIB_DLL` path (explicit developer override).

`PATH` is not searched. Relative overrides are rejected. The Environment page
shows the loaded path and whether it came from the application directory.

### Credentials and database key

- API id/hash: DPAPI (`CryptProtectData`) under
  `%APPDATA%/Beykus/BeyGram Windows/secrets`.
- TDLib database key: 32 random bytes, DPAPI-wrapped per account in
  `tdlib/<account>/tdlib.database.key.dpapi` (the file name is a SHA-256 of the
  logical key).
- The database is opened with `database_encryption_key`. The app never falls
  back to an unencrypted database if the key cannot be read.
- Files in `%APPDATA%` are readable by the Windows user; DPAPI protects the
  secrets against other Windows users, not against malware running as the same
  user. This is the same trust boundary as any desktop Telegram client.

### Process lock

`tdlib/<account>/session.lock` is a `QLockFile`. A second BeyGram process gets a
clear "already open" error instead of corrupting the database. Stale locks from
a terminated process are recovered after the timeout.

### Logging

- `%APPDATA%/Beykus/BeyGram Windows/logs/beygram.log`, 5 MiB × 3 rotations.
- Redaction covers API keys, tokens, phone numbers, QR login links, bot tokens,
  proxy URLs and 32-hex secrets.
- Raw authorization JSON, phone codes, passwords and QR links are never logged.
- `BEYGRAM_VERBOSE_LOGS=1` enables debug lines; secrets remain redacted.

## Authorization lifecycle

| Action | TDLib request | Behaviour |
|---|---|---|
| Close window | `Close` | waits up to 5 s for `authorizationStateClosed`, then aborts the client if needed; database key and session stay |
| Log out (confirmed) | `LogOut` | waits for closed state; TDLib destroys its local data; automatic rebuild returns to the phone screen |
| Crash | — | DPAPI key and encrypted database remain; next start restores the session |
| Cancel login / stuck form | `Close` then restart | returns to the phone screen without deleting the database |

`AppController::shutdown()` is called before the QML engine is torn down.

## Request correlation

Every `TdJsonTransport::sendRequest` assigns a monotonically increasing
`@extra` id. Responses with that id are routed to the waiting handler, emitted as
`responseReceived` and are not mistaken for updates. Timeouts produce a
synthetic error and are cancellable. `cancelAllRequests(false)` discards
handlers during shutdown to avoid callbacks into a destructing session.

## Multi-account preparation

- `AccountRegistry` stores account ids and timestamps in
  `%APPDATA%/Beykus/BeyGram Windows/accounts/accounts.json` (no secrets).
- Every account has an isolated TDLib database/files directory and its own
  database key, lock and auth state.
- Phase 1 registers a single `primary` account. The UI has no account switcher
  yet, by design.

## Tests

`ctest` runs 20 suites:

- redaction and logging;
- WCAG contrast and theme tokens;
- path safety and trusted TDLib locator;
- TDLib parameter/auth-state/proxy mapping;
- DPAPI secret store, credential store and database key store;
- account registry and session lock;
- missing-runtime behaviour;
- auth request builders and QR image generation;
- chat/message parsing, chat filtering and message model invariants;
- send/edit/history/read/delete TDLib request builders;
- real TDLib integration (skipped when the DLL is absent): trusted load, local
  `td_execute`, `@extra` correlation, reach
  `authorizationStateWaitPhoneNumber` with a temporary database, reject a second
  session for the same account, close, reopen and verify restart.

Local Phase 2 run with TDLib 1.8.69: **20/20 passed**. Manual checks with a real
authorized session through a local SOCKS5 proxy: the chat list loaded 500+
chats with pins/unread/muted state, a private chat opened with real history,
messages rendered with sender/time/edited state, and the close handshake exited
with code 0. No API hash, phone number, code or message text appeared in the
application log. Sending a real message was intentionally not automated because
that mutates the owner's account; the request builder is unit-tested.

## CI

`.github/workflows/windows-foundation.yml`:

1. builds the app and unit tests on Windows Server 2022 with Qt 6.8.3;
2. deploys Qt and smoke-tests the QML shell;
3. a second job builds the pinned TDLib with vcpkg OpenSSL/zlib, caches it, then
   runs the full suite including the real TDLib integration test.

Android workflows are separate and unchanged.

## Verification matrix

| Scenario | Automated | Notes |
|---|---|---|
| trusted DLL discovery / rejection of relative override | yes | unit tests |
| TDLib exports and local `td_execute` | yes | integration test |
| `@extra` request/response correlation | yes | integration test sends `getMe` before authorization and checks the matched error response |
| real client reaches `authorizationStateWaitPhoneNumber` | yes | throwaway database, dummy API credentials |
| second process cannot open the same database | yes | integration test creates a second session against the same account |
| close handshake and database reopen | yes | integration test |
| API credential validation and DPAPI round trip | yes | unit tests |
| database key creation, reuse and damage detection | yes | unit tests |
| QR image generation | yes | unit test checks a real QR bitmap |
| chat list parsing, ordering, filter and unread roles | yes | unit tests |
| photo/video thumbnail selection and chat-photo parsing | yes | unit tests |
| real avatars and message thumbnails | manual | verified with the owner's authorized session through a local SOCKS5 proxy; TDLib downloads on demand |
| message parsing, model ordering, temporary-id replacement | yes | unit tests |
| send/edit/history/read/delete request shapes | yes | unit tests |
| real chat list and history | manual | verified with the owner's authorized session through a local SOCKS5 proxy; not automated |
| sending a real message | not automated | would mutate the owner's account; request builder is tested |
| phone → code → 2FA / wrong code / wrong password | UI and error mapping implemented | requires a dedicated Telegram test account; not run against a real account in CI |
| QR confirmation by another device | UI implemented | requires a second authorized device |
| registration and email verification | UI implemented | requires an unused phone number and email |

## Next phase

Phase 2 continues with rich text, replies, media, drafts, folders/archive and
notifications. See
[PORTING_ROADMAP.md](PORTING_ROADMAP.md) and
[PORTING_AUDIT.md](PORTING_AUDIT.md).
