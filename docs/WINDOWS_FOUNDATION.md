# BeyGram Windows — native foundation

This document explains what Phase 0 builds, how to build and test it, and which
parts are deliberately incomplete. The Android client under `app/` remains the
functional reference and is not part of this build.

## What works

- A C++20 / Qt 6 desktop application (`beygram.exe`) with a Qt Quick shell.
- Module boundaries: `core`, `platform/windows`, `telegram`, `media`, `ui`, QML.
- Runtime loading of TDLib's JSON interface (`tdjson.dll`).
- A real TDLib authorization state machine (no login form in Phase 0).
- Honest empty/error states: the UI never invents an account or chats.
- Thread-safe logging with secret redaction, rotation and an in-app log page.
- DPAPI-based secret storage with unit tests.
- Warm/Graphite palettes and FollowSystem/Light/Dark/AMOLED brightness.
- Windows CI: configure, build, tests, deployment and a QML smoke test.

## What does not work yet

- No login screen, no chats, no messages, no media, no notifications.
- No music player, playlists, BG Beat or BG Mix audio.
- No proxy/VPN management, app lock, updater or installer.
- TDLib is not bundled in this repository and is not present on every machine.

The window and the Overview page say this explicitly. A missing dependency is
shown as a missing dependency.

## Requirements

| Tool | Version used |
|---|---|
| Windows | 10/11 x64 |
| MSVC | Visual Studio 2022 (19.44 toolset tested) |
| CMake | 3.24+ |
| Qt | 6.8.3 MSVC 2022 x64 (`Core`, `Gui`, `Quick`, `QuickControls2`, `Test`) |
| Ninja | optional; the Visual Studio generator is supported |

Qt is installable without an account with `aqtinstall`:

```powershell
python -m pip install aqtinstall
python -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 `
    -m qtshadertools --outputdir C:\Qt
```

## Configure, build and test

From a Visual Studio Developer PowerShell (or let the Visual Studio generator
locate MSVC):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64 `
  -DBEYGRAM_BUILD_TESTS=ON

cmake --build build --config Release --parallel

$env:PATH = "C:\Qt\6.8.3\msvc2022_64\bin;$env:PATH"
ctest --test-dir build -C Release --output-on-failure
```

Run the app:

```powershell
$env:PATH = "C:\Qt\6.8.3\msvc2022_64\bin;$env:PATH"
.\build\Release\beygram.exe
```

Debug build: use `--config Debug` and `build\Debug\beygram.exe`.

## TDLib

The client uses TDLib's **official JSON interface**, not generated bindings:

- `td_json_client_create`, `td_json_client_send`, `td_json_client_receive`,
  `td_json_client_execute`, `td_json_client_destroy`.
- `src/telegram/TdJsonTransport.cpp` loads the DLL at runtime and polls
  `receive` on a worker thread.
- Requests are JSON objects; updates are emitted to the GUI thread.
- `src/telegram/TelegramSession.cpp` owns the auth state machine. It sends
  `setTdlibParameters` only after TDLib asks for it.

Build TDLib for Windows x64 with MSVC following the official instructions
(<https://github.com/tdlib/td>), producing `tdjson.dll` and its dependencies
(OpenSSL, zlib, etc.). Then either:

```powershell
# Put the DLL next to the executable at configure time:
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64 `
  -DBEYGRAM_TDLIB_DIR=C:\td\build\bin
```

or set it at runtime:

```powershell
$env:BEYGRAM_TDLIB_DLL = "C:\td\build\bin\tdjson.dll"
```

When found by CMake, `tdjson.dll` is copied next to `beygram.exe`. Its dependent
DLLs must also be reachable (same folder or `PATH`).

### API credentials

Phase 0 reads credentials from the process environment:

```powershell
$env:BEYGRAM_TELEGRAM_API_ID = "123456"
$env:BEYGRAM_TELEGRAM_API_HASH = "your_api_hash"
```

They are used only to build `setTdlibParameters`; they are never logged and
never written to the repository. The DPAPI secret store is ready for the Phase 1
provisioning screen.

If either TDLib or the credentials are missing, the UI shows the exact reason
and does not start a session.

### Supported authorization states

The core maps TDLib `updateAuthorizationState` values to an internal
`AuthStage`: waiting for TDLib parameters, phone, code, password, registration,
QR, ready, closing, closed, error. Phase 0 has no forms for these states; it
reports them. Phase 1 adds the forms and graceful close/logout.

## Architecture

```
src/app/main.cpp          composition root, QML engine, shutdown order
src/ui/                   AppController, ThemeController (QML-facing only)
src/telegram/             TdJsonTransport, TelegramSession, config, auth state
src/core/                 logging, redaction, paths, Result, contrast rules
src/platform/windows/     DPAPI secret store
src/media/                BG Media capability boundary
qml/                      Main window, components, pages
tests/                    Qt Test suites
```

Dependency direction is one-way:

```
qml → ui → telegram → core
        ↓
   platform/windows
        ↓
     media (optional BG Media)
```

- QML never sends TDLib requests and never parses Telegram JSON.
- `TelegramSession` is per account; no global TDLib client.
- `TdJsonTransport` keeps TDLib's blocking receive loop off the GUI thread.
- Failed loads and unavailable services are values, not exceptions.
- Logs are redacted before they reach the file or the in-app viewer.

## Logging and diagnostics

- Location: `%APPDATA%/Beykus/BeyGram Windows/logs/beygram.log`.
- Rotation: 5 MiB per file, 3 rotated files.
- `BEYGRAM_VERBOSE_LOGS=1` includes debug lines.
- Redaction covers tokens, API secrets, phone numbers, QR login links, bot
  tokens, proxy/URL values and 32-hex secrets.
- The Logs page and the "Copy diagnostics" action expose only redacted data.

## Secret storage

`WindowsDpapiSecretStore` protects values with `CryptProtectData`
(per-user DPAPI plus a fixed application entropy value). File names are SHA-256
digests of logical keys, so keys do not appear on disk. Unit tests cover
round-trip, overwrite, removal, corrupt blobs and file naming.

This is foundation code; the credential provisioning UI arrives in Phase 1.

## BG Media / BG Beat / BG Mix

The Windows port keeps a native C++ boundary instead of JNI. Phase 0 reports
"BG Media is not linked" because:

- `third_party/bg-media` is an uninitialised git submodule in this checkout;
- linking and audio-device work belong to the music phase.

To experiment:

```powershell
git submodule update --init third_party/bg-media
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64 `
  -DBEYGRAM_WITH_BGMEDIA=ON
```

The CMake option only links the upstream core; it does not fake playback.

## CI

`.github/workflows/windows-foundation.yml` runs on Windows Server 2022 with
MSVC 2022 and Qt 6.8.3:

1. configure with the Visual Studio generator;
2. build Release;
3. run all Qt tests;
4. deploy Qt DLLs and QML dependencies;
5. start the app with the offscreen platform and assert it stays alive.

Android workflows are separate and unchanged.

## Verification performed for Phase 0

| Check | Result |
|---|---|
| CMake configure (Qt 6.8.3, MSVC 19.44) | passed |
| Debug and Release builds | passed |
| `ctest` (8 suites) | 8/8 passed |
| Application start with offscreen platform | stays alive, QML loads |
| TDLib absent | reported honestly in UI/log |
| DPAPI round trip | passed |
| Android tree modified | no |
| Android workflows modified | no |

## Next phase

Phase 1 adds the TDLib Windows build, secure credential provisioning, the full
login UI and session persistence. See
[PORTING_ROADMAP.md](PORTING_ROADMAP.md) and
[PORTING_AUDIT.md](PORTING_AUDIT.md).
