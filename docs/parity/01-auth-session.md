# Parity 01 — Auth, session, account registry

Read-only audit of the Android BeyGram authorization/session/account layer against the
Windows client. Based only on source reading: **no build, no test run, no device run**.
Where behaviour is inferred from code rather than observed, it is stated as such.

Reference (Android, in this repo): `app/src/main/java/com/beykus/beygram/**`.
Target (Windows): `src/telegram/**`, `src/platform/windows/**`, `src/ui/AppController.*`, `qml/**`.

---

## 1. Files reviewed

### Android

| Area | File | Depth |
|---|---|---|
| Auth state model | `domain/model/AuthState.kt` | full |
| QR model/validation | `domain/model/QrLogin.kt` | full |
| Auth contract | `domain/repository/TelegramRepository.kt` (auth section, L304–356) | full |
| State mapping | `data/tdlib/TdAuthorizationState.kt` | full |
| Repo auth machine | `data/tdlib/TdLibTelegramRepository.kt` (L372–409, 1941–2110, 2790–2999, 3422–3509) | full (auth regions) |
| Per-account snapshot | `data/tdlib/TdAccountSessionState.kt` | full |
| Account registry | `data/tdlib/TdAccountRegistry.kt` | full |
| Storage layout / DB lease | `data/tdlib/TdAccountStorageLayout.kt`, `TdAccountClientLifecycle.kt` | full |
| TDLib params | `data/tdlib/TdLibConfig.kt` | full |
| Account security | `data/tdlib/TdAccountSecurity.kt` | partial |
| Multi-account coordinator | `data/tdlib/TdSecondarySessionCoordinator.kt` | partial |
| Restore policy | `data/tdlib/TdSecondaryRestoreStore.kt` | partial |
| Secondary native login | `data/tdlib/TdSecondaryNativeLogin.kt` | grep-level |
| Auth screens | `ui/screens/auth/LoginPhoneScreen.kt`, `LoginCodeScreen.kt`, `LoginPasswordScreen.kt` | full |
| Auth routing | `BeyGramApp.kt` (L1540–1568, 2270–2318) | full |
| Account switcher UI model | `ui/screens/settings/AccountSwitcherModel.kt` | full |
| Device sessions policy | `ui/screens/settings/SessionsTimeoutPolicy.kt` | partial |
| App lock policy | `security/AppLockPolicy.kt` | partial |
| Design notes | `docs/multi-account-stage-c2-auth-management.md`, `...-d1-automatic-restore.md` | full |

### Windows

`src/telegram/TelegramSession.{h,cpp}`, `TelegramConfig.{h,cpp}`, `AuthState.{h,cpp}`,
`AuthRequests.{h,cpp}`, `AccountRegistry.{h,cpp}`, `CredentialStore.{h,cpp}`,
`SessionLock.{h,cpp}`, `TdJsonTransport.h`, `src/platform/windows/DatabaseKeyStore.{h,cpp}`,
`DpapiSecretStore.*` (referenced), `src/ui/AppController.{h,cpp}` (auth regions),
`src/core/AppPaths.cpp`, `src/app/main.cpp` (QR provider wiring), `qml/AuthPage.qml`,
`qml/Main.qml`, `qml/Shell.qml` (partial), `qml/pages/OverviewPage.qml` (partial),
`qml/pages/EnvironmentPage.qml` (grep-level), `tests/tst_telegram_session.cpp`,
`tst_auth_requests.cpp`, `tst_account_registry.cpp`, `tst_credential_store.cpp`,
`tst_session_lock.cpp`, `tst_database_key.cpp`, `tests/CMakeLists.txt`.

---

## 2. Android behaviour (the reference)

### 2.1 State machine

`AuthState` is the only UI contract: `Idle → Loading → WaitingForPhone | WaitingForQr |
WaitingForCode(phone) | WaitingForPassword(hint) | Ready | ConnectionTimeout | Error(UiText)`.

`TdLibTelegramRepository.handleAuthorizationState` (L2792) maps TDLib states:

| TDLib | UI | Side effects |
|---|---|---|
| `WaitTdlibParameters` | (stays `Loading`) | sends `SetTdlibParameters`; DB key is `ByteArray(0)` (unencrypted DB) |
| `WaitPhoneNumber` | `WaitingForPhone` | clears QR tracker; re-requests QR if a QR restart was pending |
| `WaitOtherDeviceConfirmation` | `WaitingForQr(link)` | validates `tg://login?token=`, dedups refreshes, arms 75 s staleness watchdog; a late link after cancel is dropped |
| `WaitCode` | `WaitingForCode(submittedPhone)` | clears QR |
| `WaitPassword` | `WaitingForPassword(hint)` | clears QR |
| `Ready` | `Ready` | loads chats, folders, analytics, drafts, stories, cache maintenance |
| `LoggingOut` | `WaitingForPhone` (or `Loading` while the client is being recreated) | clears downloads, folder state, drafts, stories, analytics identity, group-creation draft, secret-chat dir, per-account chat exceptions, delayed-send queue |
| `Closed` | `WaitingForPhone` (or `Loading` while reopening) | stops network monitors, clears the same state, disposes a secondary repository |
| anything else | **no branch** | state stays `Loading` → 35 s timeout → `ConnectionTimeout` |

`TdAccountSessionState` is the newer per-account reducer: it tags each update with
`accountId + generation`, rejects cross-account/stale updates, treats `CLOSED` as terminal
(no revival by a late update), and collapses `CLOSED` connectivity to `UNKNOWN`.

### 2.2 Edge cases as implemented on Android

| Case | Behaviour |
|---|---|
| Wrong code | `Loading` → `Error(err_auth_code_invalid)`; screen stays, field keeps text, button re-enables |
| Expired code | distinct `err_auth_code_expired` |
| Resend | `resendAuthenticationCode(reason = null)` |
| Flood wait | generic `err_tdlib_flood_wait` (seconds are **not** surfaced) |
| Wrong 2FA password | `Error(err_auth_password_invalid)`; hint shown; no password-recovery request |
| 2FA without recovery email | UI only (no recovery flow anywhere) |
| QR invalid link | `Error(err_auth_qr_link)`; never a crash, never a raw link in logs |
| QR refresh | `QrLoginLinkTracker`: `Refreshed` / `Unchanged` / `Invalid`; 75 s overdue → `err_auth_qr_stale` |
| QR cancel | no TDLib cancel API: `Close()` the unauthorized client, ignore late links, recreate it, return to phone |
| Email address / email code / registration | **not handled at all** (no `WaitEmailAddress`, `WaitEmailCode`, `WaitRegistration` branch anywhere in `data/tdlib`) — such a login hangs, then times out |
| Phone invalid / unoccupied / banned | curated strings; unoccupied is not turned into registration |
| Logout | UI exists only for **secondary** accounts (account switcher). The primary/legacy account has no logout action in the current tree |
| Remote logout (other device) | `LoggingOut` clears per-account local state and returns to phone |
| Process restart | TDLib reopens its DB and emits `Ready`; no credentials re-entered |
| Proxy | full runtime management: list/add/enable/disable/remove saved proxies, latency probe, current route, Mihomo shared-SOCKS listener, per-account route policy for secondaries |
| Multi-account | registry of up to 10 opaque IDs, legacy pinned first; one running secondary + legacy (D1 allows two secondaries); per-account dirs, DB leases, restore store, route preflight, per-account logout/delete, switcher |
| DB lock | in-process canonical-path lease held until `AuthorizationStateClosed` |
| App lock | PIN 4–8 digits (hashed), biometrics, timeout (0/60/300 s), 5 free attempts then 30 s→300 s backoff, lock on background |

---

## 3. Current Windows status

### 3.1 What exists and works (by inspection)

- `AuthStage` covers `Unavailable, NotConfigured, Locked, Idle, Loading, WaitTdlibParameters,
  WaitPhoneNumber, WaitCode, WaitPassword, WaitRegistration, WaitEmailAddress, WaitEmailCode,
  WaitQrCode, Ready, Closing, Closed, Error` — **a superset of Android's**.
- `TelegramSession` sends params, applies a proxy via `addProxy` before params, maps
  `code_info`, `password_hint`/`has_recovery_email_address`, `terms_of_service`, email
  `code_info`, and QR `link` (+`qrRevision`).
- Auth requests (`AuthRequests.cpp`) exist for phone, code, password, email address, email
  code, registration, resend, QR — with phone normalisation (7–15 digits, `00` prefix) and
  curated error text incl. `FLOOD_WAIT_<n>` seconds.
- `requestLogout()` → `logOut` (120 s), failure keeps the account authorized and returns to
  `Ready`; `Closed` after a requested logout rebuilds the session (`AppController` L299–306).
- `restartSession()` stops the transport, releases the lock, restarts; `shutdown()` performs a
  real `close` handshake with a 15 s timeout, `abort()` skips it.
- `SessionLock` = `QLockFile` per account dir, 30 s stale window; `Locked` stage is surfaced.
- Credentials: env → DPAPI (`CredentialStore`) → build-embedded; API hash validated as 32 hex.
- DB key: 64-hex random, DPAPI-wrapped, per account directory; damaged key fails closed
  instead of silently creating a new DB (`DatabaseKeyStore::loadOrCreate`).
- `AccountRegistry`: `accounts.json` (version 1), primary account auto-created, corrupt file is
  an error, `QSaveFile` atomic write, `AppPaths::safeComponent` path hardening.
- UI: one `AuthPage.qml` with phone/code/password/email/email-code/registration/QR panels,
  credential card, startup/`Locked`/`Unavailable` states, QR image provider (`beygramqr`),
  "Open in Telegram"; logout only from `OverviewPage`.

### 3.2 Windows is *ahead* of Android here (do not regress)

Email address/code login, account registration (terms + min age), per-code flood-wait seconds,
DPAPI-protected DB key + credentials, and an explicit primary-account logout action.

### 3.3 Known divergences and defects (by inspection)

| # | Area | Finding |
|---|---|---|
| W1 | QR | Link is not validated (`tg://login?token=` prefix, base64url, ≤512 chars). Any non-empty string is encoded into the QR image |
| W2 | QR | No refresh dedup and no staleness watchdog; if TDLib stops refreshing, the spinner/QR stays forever |
| W3 | QR | No "cancel" distinct from "use phone number instead": `restartSession()` skips the `close` handshake (no `Closed` wait), so an in-flight auth request is dropped rather than cancelled cleanly |
| W4 | QR | A `requestQrCodeAuthentication` rejection is shown as `"Could not request QR login: <raw TDLib text>"` — no curated mapping (Android maps it to a friendly string) |
| W5 | Timeouts | No auth-request timeout state. `startAuthTimeout`'s Android equivalent (35 s → `ConnectionTimeout` + reconnect) has no counterpart; the 30 s `m_busyTimer` only clears the spinner |
| W6 | Connection | `connectionTitle` is not shown on the auth page; `WaitingForNetwork` / `ConnectingToProxy` are invisible while signing in |
| W7 | Errors | `reportError` prefixes context (`"Could not submit the phone number: …"`), and unmapped TDLib messages reach the UI verbatim |
| W8 | Registry | No account limit, no `reserveSecondary`/`unregister`, no canonical/symlink path checks, no directory fsync; unknown IDs in `accounts.json` are silently dropped |
| W9 | Registry | `accountDirectory()` ignores its own `dataDirectory` and uses `AppPaths::tdlibAccountDirectory` (global), so a registry cannot be pointed at a test/temp root |
| W10 | Logout | Local app state is not scoped to the account: `DraftStore` (`state/drafts.json`) and notification settings survive logout; Android clears drafts/queues/exceptions |
| W11 | Remote logout | `Closed` without `m_logoutRequested` leaves the app on a "closed" screen needing a manual Retry; no "session ended on another device" message |
| W12 | Multi-account | `TelegramSession` is per-account by design, but `AppController` owns exactly one session for `registry.active()`; no switcher, no second client, no restore, no per-account logout/delete, no route policy |
| W13 | Proxy | Startup-only: `BEYGRAM_PROXY`/`proxy.txt` → `addProxy`. No UI, no saved proxy list, no enable/disable/remove, no latency, no Mihomo, no per-account route |
| W14 | Locks | Only the DB `QLockFile`. No app lock (PIN/biometrics/idle timeout), no `Locked`-adjacent UX beyond a retry |
| W15 | Tests | `tst_telegram_session` only covers "runtime missing", "invalid phone", "shutdown is safe". The state machine, QR handling, logout and restart paths are untested; `TdJsonTransport` is a concrete member, so there is no seam for a fake |
| W16 | Security note | Android's TDLib DB key is empty (unencrypted DB); Windows encrypts. Intentional divergence — keep Windows behaviour, do not "fix" it toward Android |

---

## 4. Parity matrix

| Capability | Android | Windows | Verdict |
|---|---|---|---|
| Phone + code | yes | yes | parity |
| 2FA password (+hint, recovery-email flag) | yes | yes | parity |
| Password recovery | no | no | parity (both missing) |
| Email address + email code | **no** | yes | Windows ahead |
| Registration (terms, min age) | **no** | yes | Windows ahead |
| QR login | yes, validated + watchdog + cancel | partial (W1–W4) | gap |
| Wrong code / expired / flood wait | curated | curated, +seconds | parity / Windows ahead |
| Auth timeout + reconnect | 35 s → `ConnectionTimeout` | none | gap (W5) |
| Connection phase on auth screen | yes | diagnostics only | gap (W6) |
| Logout (own account) | secondary only | primary only | both partial |
| Logout local cleanup | per account | none | gap (W10) |
| Restart / session restore | yes | yes | parity |
| Account registry | 10, atomic + fsync, path checks | JSON, no limit/checks | gap (W8) |
| Multi-account runtime | 1–2 secondaries + legacy, route policy, restore | single session | gap (W12) |
| Proxy management | full + Mihomo | env/file only | gap (W13) |
| DB/process lock | in-process lease | `QLockFile` | parity (different mechanism) |
| App lock | PIN + biometrics + backoff | none | gap (W14) |
| Secrets at rest | DB key none, api hash in build | DPAPI key + creds | Windows ahead |

---

## 5. Port plan

Ordered by risk to the user's account and by how much later work depends on it.
Each item names the file to add/change and the test that proves it.

### P0 — correctness and secret handling (do first)

1. **Pure auth reducer** — extract `TelegramSession::handleAuthorizationState` into
   `src/telegram/AuthReducer.{h,cpp}`:
   `AuthSnapshot { AuthStage stage; QString detail; AuthCodeInfo code; AuthPasswordInfo password; AuthEmailInfo email; AuthTerms terms; QString qrLink; }`
   and `AuthSnapshot reduce(const AuthSnapshot&, const QJsonObject& authorizationState)`.
   `TelegramSession` keeps the side effects (sending params, emitting signals) and becomes thin.
   *Tests:* `tests/tst_auth_reducer.cpp` — every TDLib state, unknown state, `Closing` vs
   `LoggingOut`, no-secret invariant (`qrLink` never in `detail`).
2. **Injectable transport** — introduce `src/telegram/TdTransport.h` (abstract `sendRequest`,
   `send`, `start/stop`, `updateReceived`), make `TdJsonTransport` implement it, inject into
   `TelegramSession`. *Tests:* `tests/tst_telegram_session_auth.cpp` with a `FakeTdTransport`
   (wrong code → `authError`, logout failure → `Ready`, `Closed` after logout → rebuild intent).
3. **QR tracker** — port `QrLogin.kt` to `src/telegram/QrLoginLinkTracker.{h,cpp}`
   (`parse(QString) → optional token`, `accept(link, nowMs) → Refreshed|Unchanged|Invalid`,
   `isRefreshOverdue(nowMs, 75s)`, `reset()`), keep the raw link private to
   `qrLoginLink()`/the image provider.
   *Tests:* `tests/tst_qr_login_tracker.cpp` (prefix/whitespace/percent-encoding/oversize rejection,
   duplicate link = Unchanged, overdue after 75 s, no link in `toString`).
4. **QR + auth timeouts in the session** — add `AuthStage::ConnectionTimeout`,
   `TelegramSession::reconnect()`, `cancelQrLogin()`, `qrLinkValid`, and a 35 s single-shot
   timer started by every `submit*`/`requestQrCodeLogin` (cancelled by any state update).
   Wire `reconnect()` to the existing network-monitor/transport restart.
   *Tests:* extend `tst_telegram_session_auth.cpp` (timeout → `ConnectionTimeout` → reconnect →
   `Loading`; late QR link after cancel ignored).
5. **Registry hardening** — `AccountRegistry`: `static constexpr int MaxAccounts = 10`,
   `reserveSecondary()`, `unregisterSecondary()`, canonical-path + symlink rejection for
   `accounts.json`, `QSaveFile` + directory durability, reject (not drop) malformed IDs, and
   make `accountDirectory()` use the registry's own root.
   *Tests:* `tests/tst_account_registry_multi.cpp` (limit, legacy pinned first, corrupt file
   fails, symlinked registry refused, unknown ID rejected, save/load round-trip).
6. **Logout cleanup** — add `AppController::clearAccountLocalState()` called from the
   `LoggingOut` path: `DraftStore::clearAll()` (new API), notification settings untouched,
   chat models reset. *Tests:* `tests/tst_draft_store.cpp` (clearAll), plus an
   `AppController`-level seam test only if a controller test target is introduced.

### P1 — feature parity users will notice

7. **Proxy controller + page** — `src/telegram/ProxyController.{h,cpp}` wrapping
   `getProxies`/`addProxy`/`enableProxy`/`disableProxy`/`removeProxy`/`pingProxy`, plus a
   `ProxyListModel`; `qml/pages/ProxyPage.qml` reachable from `AuthPage` and `SettingsPage`
   (Android shows "proxy help" on the phone screen). Keep env/`proxy.txt` as the bootstrap
   default. *Tests:* `tests/tst_proxy_requests.cpp` (request shapes, secret never logged).
8. **Per-account session manager** — `src/telegram/AccountSessionManager.{h,cpp}` owning
   `QHash<QString, TelegramSession*>`, monotonic generation per account, `sessionFor(id)`,
   `activeAccountId()`, `setActive(id)`, `logout(id)`, `forget(id)` (only after `Closed`),
   `restoreAuthorized()`. `AppController` uses it instead of the single `m_session`.
   *Tests:* `tests/tst_account_session_manager.cpp` with fake sessions (duplicate ID refused,
   no reopen before `Closed`, legacy ID not re-registered, logout ≠ close).
9. **Restore store** — `src/telegram/AccountRestoreStore.{h,cpp}` (JSON in
   `state/accounts-restore.json`): `authorized`, `manualStop`, `route` per opaque ID; only
   `Ready` makes an account eligible; a manual stop wins over a restart.
   *Tests:* `tests/tst_account_restore_store.cpp` (reserved-but-unauthorized never restores,
   corrupt/unknown route fails closed, stop flag cleared only by a new `Ready`).
10. **Account switcher** — `src/telegram/AccountSwitcherModel.{h,cpp}` (port of
    `AccountSwitcherModel.kt`: status, `canSignIn/canSelect/canLogout/canDelete`,
    error reasons) + `qml/components/AccountSwitcherSheet.qml`, plus
    `AppController::switchAccount/reserveAccount/logoutAccount/forgetAccount`.
    *Tests:* `tests/tst_account_switcher_model.cpp` (status precedence, route-paused not
    selectable, primary row never deletable).
11. **Auth page connection status** — show `connectionTitle` on `AuthPage.qml` with a
    `Reconnect` action while `wait_phone_number`/`ConnectionTimeout`, mirroring Android's
    `LoginPhoneScreen`.

### P2 — secondary

12. **App lock** — `src/security/AppLockPolicy.{h,cpp}` (pure rules: 4–8 digits, 0/60/300 s
    timeout, 5 free attempts, 30 s→300 s backoff) + `AppLockStore` (salted hash via
    `DpapiSecretStore`) + `qml/AppLockPage.qml` gating `Main.qml`'s `Loader`.
    *Tests:* `tests/tst_app_lock_policy.cpp`.
13. **Password recovery** (both platforms lack it): `requestAuthenticationPasswordRecovery` /
    `recoverAuthenticationPassword` in `AuthRequests.cpp`, a `wait_password_recovery` panel.
14. **Device-session management** (`getActiveSessions`/`terminateSession`/`terminateAllOtherSessions`)
    — Android has `ActiveSessionsScreen` + `SessionsTimeoutPolicy`; Windows has nothing.
15. **Registry labels/identity** — persist display name/username per account so the switcher
    does not need a live `getMe` for every row.

---

## 6. Priority summary

| Priority | Items | Why |
|---|---|---|
| P0 | Auth reducer, fake transport, QR tracker, auth/QR timeouts, registry hardening, logout cleanup | A stuck or unvalidated QR/timeout path is the current auth failure mode; registry/logout defects become data-loss risks the moment a second account exists |
| P1 | Proxy controller, session manager, restore store, switcher, auth-page connection status | Needed for the Windows "multi-account from the start" design already declared in `TelegramConfig.h`/`AccountRegistry.h` |
| P2 | App lock, password recovery, device sessions, account labels | Useful, but independent of the login path |

Explicit non-goals for this document: per-account chat/media repositories, notification
routing, and deep links — those belong to the conversation and notification parity audits.

---

## 7. Not verified

- No CMake configure, no build, no test execution, no GUI launch, no TDLib connection.
- Android behaviour above is read from source; in particular the claim that email/registration
  states have **no** handler is based on an exhaustive grep for
  `AuthorizationStateWaitRegistration|WaitEmailAddress|WaitEmailCode` across
  `app/src/main/java/com/beykus/beygram` (no matches).
- Windows "ahead" items (email, registration, flood-wait seconds) are verified only as request
  construction + state mapping, not against a live Telegram server.
- The Android primary-account logout absence is based on the absence of a logout action outside
  `AccountSwitcherSheet.kt` (which only offers it for secondary rows) and of any `LogOut` call
  for the legacy client.
