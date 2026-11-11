# 19 — Calls, VPN/Mihomo and proxy management parity

Status: read-only parity audit  
Date: 2026-11-11  
Android reference: `app/` (the Kotlin client remains the functional specification)  
Windows reference: `src/`, `qml/`, `tests/`  
Build/test status: **not run** — this audit made no builds, no test runs, no GUI launches and no git changes.

Scope:

- Android calls: history list, call state, row mapping, paging, tab entry and related privacy/notification handling.
- Android VPN/Mihomo: local proxy runtime, lifecycle, subscription/profile handling, automatic node selection, restore behaviour and diagnostics.
- Android proxy management: TDLib proxy add/enable/disable/remove, latency test, route restore and the three proxy screens.
- Windows current status for the same areas and a concrete implementation plan.

---

## 1. Executive summary

1. **Android has no VoIP calls.** The Calls tab is a read-only history built from TDLib `messageCall` messages. There is no call placement, ringing, accept/decline, media or call-state machine. Parity for Windows means the same history-only feature, not a calling client.
2. **Android has no Android `VpnService`, TUN interface or system VPN profile.** “VPN/Mihomo” in Android means an embedded Mihomo core started as an authenticated loopback SOCKS listener on `127.0.0.1:<variant port>`, kept alive by a foreground service. TDLib is pointed at that local SOCKS entry through its proxy API.
3. **Android proxy management is a full TDLib-proxy UI:** subscription route, direct route, saved MTProto/SOCKS5/HTTP proxies, add/enable/disable/remove, latency probes, previous-route restore after process restart, and a non-dismissible restore-failure dialog.
4. **Windows currently has bootstrap proxy configuration only.** It reads `BEYGRAM_PROXY` (or `proxy.txt`) and calls TDLib `addProxy` with `enable=true` during startup. There is no proxy list, no add/edit/remove UI, no latency test, no route restore, no subscription/Mihomo, no network monitor and no system VPN.
5. **Windows has no Calls page and no call-history model.** `messageCall` is rendered only as the literal preview text `Call` in the chat list and content label.
6. Highest-value gaps, in order:
   - **P1:** TDLib proxy CRUD + route restore + bootstrap dedupe (connectivity-critical and comparatively small).
   - **P2:** proxy UI, latency test, network-change awareness, diagnostics.
   - **P3:** Calls history (small, self-contained, no new platform dependency).
   - **P3 decision gate:** subscription/Mihomo. Bundling a Mihomo core on Windows is a separate product, licensing and security decision. Do not promise it before that decision.

---

## 2. Android files reviewed

### 2.1 Calls

| File | What was verified |
|---|---|
| `app/src/main/java/com/beykus/beygram/ui/screens/calls/CallsScreen.kt` | Read-only list, loading/empty/error states, pagination, row layout, direction icons, summaries, date formatting, no action button. |
| `app/src/main/java/com/beykus/beygram/ui/screens/calls/CallsScreenState.kt` | Per-account ViewModel state; first-page replacement with overlap guard; append dedupe by `chatId:messageId`; `nextOffset` handling. |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdCallHistory.kt` | `SearchCallMessages(offset, 50, false)`, filtering `MessageCall`, parallel chat lookup, missing-chat tolerance, discard-reason/duration mapping. |
| `app/src/main/java/com/beykus/beygram/domain/model/Contacts.kt` | `CallStatus`, `CallRecord`, `CallHistoryPage`. |
| `app/src/main/java/com/beykus/beygram/domain/repository/TelegramRepository.kt` | `loadCallHistory(offset)` contract and paging comment. |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdLibTelegramRepository.kt` | `loadCallHistory` delegates to `TdCallHistory` (line 1450). |
| `app/src/main/java/com/beykus/beygram/BeyGramApp.kt` | Calls route/tab wiring (lines 213, 2518–2519), per-account screen key. |
| `app/src/main/java/com/beykus/beygram/navigation/MainTab.kt`, `navigation/AppRoute.kt` | Calls is a main tab and a route. |
| `app/src/main/java/com/beykus/beygram/ui/components/MainTabs.kt` | Calls label/icon (`Icons.Rounded.Call`). |
| `app/src/main/java/com/beykus/beygram/ui/screens/settings/NavigationSettingsScreen.kt` | Calls tab can be hidden/shown with Contacts and Settings. |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdNotifications.kt` | Call notification groups are deliberately excluded from message notification groups (lines 59, 72). |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdPrivacy.kt` | `PrivacySetting.Calls` maps to `UserPrivacySettingAllowCalls`. |
| `app/src/main/res/values*/strings*.xml` | Calls labels, status summaries, empty/error text in all shipped locales. |
| `app/src/test/java/com/beykus/beygram/ui/screens/calls/CallsScreenStateTest.kt` | Expected first-page/append/overlap/gap rules. |
| `app/src/test/java/com/beykus/beygram/data/tdlib/CallEventDurationTest.kt` | Service-message call duration formatting edge cases (related to call events, not the Calls list formatter). |

### 2.2 VPN/Mihomo and proxy management

| File | What was verified |
|---|---|
| `app/src/main/java/com/beykus/beygram/data/vpn/VpnBackend.kt` | Process-wide controller; start/stop, restore, subscription import, route switching, probes, auto-select events, mutexes and generation guards. |
| `app/src/main/java/com/beykus/beygram/data/vpn/VpnSettingsStore.kt` | Shared-preferences settings: subscription URL, auto-select, 5–10 s handover delay; group name is fixed by the generated profile. |
| `app/src/main/java/com/beykus/beygram/data/vpn/VpnNetworkMonitor.kt` | `ConnectivityManager` default/all-network callbacks; validated Wi-Fi/cellular, active transport including VPN. |
| `app/src/main/java/com/beykus/beygram/data/vpn/MihomoProxyService.kt` | Foreground service; serialized start/stop queue; STARTING/RUNNING/STOPPING/FAILED states; notification text driven by upstream/Telegram metrics; no TUN. |
| `app/src/main/java/com/beykus/beygram/data/vpn/MihomoCoreBridge.kt` | In-process JNI bridge; profile import/validation; `getProxies`, `testDelay`, `changeProxy`, `resetConnections`, `startListener`/`stopListener`; selected-map persistence; probe result mapping. |
| `app/src/main/java/com/beykus/beygram/data/vpn/MihomoProfileNormalizer.kt` | YAML/Base64/URI-list parsing; disables LAN/listeners/external controller/TUN; pins loopback SOCKS port, authentication and empty `skip-auth-prefixes`; system DNS; injects `BeyGram Telegram` selector and `MATCH` rule; 5 MB limit. |
| `app/src/main/java/com/beykus/beygram/data/vpn/MihomoProxyClient.kt` | Loopback SOCKS client; per-variant port; process-wide authenticator; upstream HTTPS probe through the same listener; listener ownership check. |
| `app/src/main/java/com/beykus/beygram/data/vpn/LoopbackProxyCredentials.kt` | Random per-install SOCKS username/password in app-private prefs. |
| `app/src/main/java/com/beykus/beygram/data/vpn/LoopbackSocksProbe.kt` | SOCKS5 handshake proving the listener belongs to this installation (not merely an open port). |
| `app/src/main/java/com/beykus/beygram/data/vpn/SubscriptionClient.kt` | HTTPS-only URL policy, no URL user-info, same-origin redirects (max 3), 5 MB body limit, timeouts, HWID header, direct retry only on network failure. |
| `app/src/main/java/com/beykus/beygram/data/vpn/InstallationHwidProvider.kt` | Stable app-scoped random `bg-<uuid>` ID encrypted with Android Keystore; no hardware/account identifiers. |
| `app/src/main/java/com/beykus/beygram/data/vpn/TelegramAutoSelector.kt` | Sustained `Connecting`/`WaitingForNetwork` trigger, 8 s handover, network readiness, parallel probes, lowest-delay selection, retry loop, generation guard. |
| `app/src/main/java/com/beykus/beygram/data/vpn/AutomaticActivationGuard.kt` | Explicit Auto choice vs persisted preference; manual route invalidates queued auto work; first successful import activation. |
| `app/src/main/java/com/beykus/beygram/data/vpn/MihomoProbeCoordinator.kt` | One process-wide probe sweep at a time. |
| `app/src/main/java/com/beykus/beygram/data/vpn/ProxyRetry.kt` | Bounded retry helper and `awaitSettledProxyState` timeout. |
| `app/src/main/java/com/beykus/beygram/data/vpn/MihomoRuntimeDiagnostics.kt` | Lifecycle events and structured VPN outcomes; no URLs, endpoints, credentials or profile contents. |
| `app/src/main/java/com/beykus/beygram/data/vpn/VpnSubscriptionDiagnostics.kt` | Sanitized subscription lifecycle/probe diagnostics. |
| `app/src/main/java/com/beykus/beygram/data/vpn/MihomoCoreLogCapture.kt` | Core warning/error capture for the shareable connection log. |
| `app/src/main/java/com/beykus/beygram/domain/model/TelegramProxy.kt` | `TelegramProxyKind`, `TelegramProxyDraft`, `SavedTelegramProxy`. |
| `app/src/main/java/com/beykus/beygram/domain/model/VpnModels.kt` | Transport/network state, Mihomo connection/probe result, VPN settings and health-check URL. |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdLibTelegramRepository.kt` | Proxy section (lines 244–365): `GetProxies`, `AddProxy`, `EnableProxy`, `DisableProxy`, `RemoveProxy`, `currentRoute`, `isMihomoProxyEnabled`, TCP latency, local-entry ensure/replace, previous-proxy memory, comment-based identification. |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdSecondaryNativeTransport.kt` | Secondary-account route policy: direct disables proxy; shared Mihomo adds/enables a local proxy; blocked routes never fall back to direct. |
| `app/src/main/java/com/beykus/beygram/data/unavailable/UnavailableTelegramRepository.kt` | No-TDLib proxy stubs. |
| `app/src/main/java/com/beykus/beygram/domain/repository/TelegramRepository.kt` | Proxy contract (lines 332–353). |
| `app/src/main/java/com/beykus/beygram/ui/screens/proxy/ProxyListScreen.kt` | Subscription row, direct row, saved-proxy rows, latency labels, delete confirmation, add button, runtime failure display. |
| `app/src/main/java/com/beykus/beygram/ui/screens/proxy/ProxyConnectScreen.kt` | MTProto/SOCKS5/Web/Subscription tabs, default ports, validation, manual add+enable, subscription refresh, auto/manual selector. |
| `app/src/main/java/com/beykus/beygram/ui/screens/proxy/ProxySubscriptionPane.kt` | Link field, server list, best badge, delay display, test-all, select-server, diagnostics share. |
| `app/src/main/java/com/beykus/beygram/ui/screens/proxy/ProxyRestoreDialog.kt` | Non-dismissible retry/turn-off question after failed automatic restore. |
| `app/src/main/java/com/beykus/beygram/ui/screens/proxy/ProxyComponents.kt` | Kind labels, latency colour scale, server-count pluralisation, shared fields/rows/buttons. |
| `app/src/main/java/com/beykus/beygram/ui/screens/proxy/SecondaryProxySettingsScreen.kt` | Secondary-account route choice: follow primary, shared Mihomo, explicit direct; no independent editor. |
| `app/src/main/java/com/beykus/beygram/BeyGramApp.kt` | Proxy routes and restore dialog host (lines 2858–2884, 3065); process-wide `VpnBackend` creation. |
| `app/src/main/java/com/beykus/beygram/diagnostics/ConnectionChecks.kt` | Public address, network snapshot, route and latency checks for diagnostics. |
| `app/src/main/AndroidManifest.xml` | `INTERNET`, `ACCESS_NETWORK_STATE`, `MihomoProxyService` with `specialUse` foreground type; **no** `VpnService`/`BIND_VPN_SERVICE`. |
| `docs/vpn-mihomo.md` | Runtime model, credentials, profile handling, screens, probes, diagnostics, dependency. |
| `docs/multi-account-stage-e1-shared-mihomo.md` | Process-wide single listener invariant and fail-closed secondary-account routing matrix. |
| `app/src/test/java/com/beykus/beygram/data/vpn/MihomoProbeCoordinatorTest.kt`, `ProxyRetryTest.kt`, `MihomoCoreLogCaptureTest.kt`, `ui/screens/proxy/ProxySelectionTest.kt` | Probe serialization, retry/settle, log capture and selected-name fallback rules. |

---

## 3. Android behaviour in detail

### 3.1 Calls: history only, no VoIP

**Entry**

- Calls is a main tab (`MainTab.Calls`) and route (`AppRoute.Calls`), optional in the navigation settings together with Contacts and Settings.
- The tab uses a per-account `CallsScreenState` ViewModel keyed `calls:<accountId>`, so switching tabs keeps already loaded pages.

**Data source**

- `TdCallHistory.page(offset)` sends `TdApi.SearchCallMessages(offset, 50, false)`.
- It keeps only `MessageCall` content, then loads each distinct chat in parallel. A missing/inaccessible chat is tolerated; the call is still shown without a title/photo.
- `nextOffset` is passed through unchanged; an empty value means no more pages.

**Status mapping**

- `duration > 0` → `Answered`.
- `CallDiscardReasonDeclined` → `Declined`.
- `CallDiscardReasonMissed` **and incoming** → `Missed`.
- Everything else (outgoing missed, disconnect, hang-up) → `NoAnswer`.

**Paging and merge rules**

- First page replaces the list.
- If the fresh first page overlaps any already loaded call, fresh calls are prepended and older pages are kept; the old `nextOffset` remains valid.
- If there is no overlap, older pages are dropped to avoid a gap between fresh and old calls.
- Appending skips calls already present by `chatId:messageId`.

**Row UI**

- Avatar (chat photo or initials), title, direction icon (`CallMade`, `CallReceived`, `CallMissed`, `CallEnd`), summary and date/time.
- Missed calls use the danger colour for title/summary.
- Summary examples: `Outgoing · call · 1:23`, `Missed · video call`, `Declined · call`, `Incoming · no answer`.
- Today uses a time format; other days use a date format.
- Empty, loading, error+retry and “show more” states.
- **No call action button** — the source comment explicitly says calls are not supported yet.

**Related surfaces**

- Call notification groups are filtered out of the message-notification model.
- Privacy settings expose Telegram's `allow calls` setting.
- No `TdApi.Call`, `CallState`, VoIP or call accept/discard code exists in the Android client.

### 3.2 “VPN” is a local authenticated SOCKS proxy, not a VPN

- No `VpnService`, TUN interface, `BIND_VPN_SERVICE` permission or system VPN profile.
- Mihomo is embedded as `libmihomo-android` and controlled in-process through JNI; no external controller socket is exposed.
- The listener is `127.0.0.1:<BuildConfig.MIHOMO_PROXY_PORT>`; Dev and Prod use different ports.
- The listener requires a random per-install username/password stored in app-private prefs. `LoopbackSocksProbe` proves the listener is BeyGram's own, not another app that merely holds the port.
- `MihomoProxyService` is a foreground service that serializes start/stop and publishes `STOPPED/STARTING/RUNNING/STOPPING/FAILED`. The listener is process-wide, not screen-owned.
- `VpnBackend` is created once with the application scope; it keeps automatic selection and restore alive without a screen.
- BeyGram-owned Java HTTP/TCP clients use `MihomoProxyClient` while the listener runs; subscription fetch is direct until the proxy is active, with one direct retry on network failure.

**Restore after process restart**

- TDLib keeps its enabled proxy across restarts.
- If that proxy is BeyGram's local entry, `VpnBackend.restore()` starts the listener again.
- Up to 5 attempts, 2 s apart; TDLib proxy state is queried up to 3 times, 2 s apart.
- If every attempt fails, `restoreFailed` raises a non-dismissible dialog: retry, or turn the proxy off. Telegram is **not** switched to direct automatically.
- Turning the proxy off restores the previously enabled manual proxy, or direct mode if there was none.
- A manual route chosen during restore wins and ends the round.

### 3.3 Profile and subscription handling

- Subscription URL must use HTTPS, have a host and contain no user-info; the default HTTPS port is accepted.
- Fetch: 10 s connect, 20 s read, max 3 same-origin redirects, max 5 MB, `X-BeyGram-HWID` with a random installation pseudonym.
- Profile input may be YAML, Base64-encoded YAML, or a plain list of proxy URIs (`ss`, `trojan`, `vless`, `vmess`, `hysteria2/hy2`, `tuic`).
- Normalization:
  - removes external controller/listeners/secret/authentication/skip-auth-prefixes;
  - sets `allow-lan: false`, `bind-address: 127.0.0.1`, `socks-port` to the app port, authentication to the per-install pair, `skip-auth-prefixes: []`;
  - forces `tun.enable: false`;
  - pins `dns.proxy-server-nameserver: [system]`;
  - adds/extends a `BeyGram Telegram` `select` group with `include-all: true`;
  - sets `mode: rule` and `rules: [MATCH,BeyGram Telegram]`;
  - validates the candidate profile before replacing the stored one; rolls back on failure.
- The selected node is stored per group in `mihomo-selection` and replayed as `selected-map` on initialization.
- Stored profiles from older builds are re-normalized on core initialization.
- The subscription tab shows the server list, per-node delay, best badge, “check latency” sweep and manual/automatic mode.

### 3.4 Automatic node selection

- Off by default.
- Watches TDLib `Connecting` **or** `WaitingForNetwork`, waits the configured handover delay (5–10 s, default 8 s), then probes while the condition persists.
- Requires internet availability plus a validated Wi-Fi or cellular route, and `ProxyRuntimeStatus.RUNNING`.
- Probes in batches of 4 through `testDelay` against `https://web.telegram.org/`.
- Selects the lowest-delay reachable node; keeps the current node if it is already fastest.
- Repeats every 20 s while the condition holds.
- `AutomaticActivationGuard` prevents an import/refresh from taking over a manually chosen Direct/SOCKS/MTProto route; only an explicit Auto choice may activate a profile.

### 3.5 TDLib proxy management

`TdLibTelegramRepository` is the source of truth for saved proxies:

| Operation | TDLib call | Details |
|---|---|---|
| List | `GetProxies` | Filters out BeyGram's local Mihomo entry by comment; maps MTProto/SOCKS5/HTTP to `SavedTelegramProxy`. |
| Add | `AddProxy(proxy, enable, comment)` | Manual screen adds and enables immediately; comment `BeyGram`. |
| Enable | `EnableProxy(id)` | Selecting a saved row applies it at once. |
| Disable | `DisableProxy()` | “Без прокси” / direct row. |
| Remove | `RemoveProxy(id)` | Delete confirmation; warns if the deleted proxy is active. |
| Current route | `GetProxies` + first enabled | Direct / Subscription / manual Proxy. |
| Subscription enabled | `GetProxies` | Enabled entry with comment `BeyGram Mihomo` and loopback SOCKS type. |
| Latency | raw TCP connect | `Socket.connect(server, port, 5 s)`; measures endpoint reachability, not proxy protocol correctness. |

Local Mihomo entry handling:

- Identified by comment `BeyGram Mihomo`, server `127.0.0.1`, SOCKS5 type.
- Created with `enable=false`, then `EnableProxy` is called explicitly.
- If port or credentials differ, the old entry is removed and recreated.
- Enabling remembers the previously enabled proxy id (or 0 for direct) in `mihomo_tdlib_proxy` prefs.
- Disabling only undoes the local entry; a manual proxy chosen later is left untouched.

### 3.6 Proxy UI

**Proxy list (`ProxyListScreen`)**

- Subscription section: one row with server count, best reachable server and latency; selecting starts the listener; “servers” opens the subscription tab.
- Direct row: “Без прокси”.
- Saved manual proxies: kind label, `server:port`, TCP latency, delete button.
- Add button opens the manual MTProto tab.
- Load errors keep the last known list; deletion optimistically removes the row and then reloads.
- Runtime `FAILED` message is shown inline.

**Connect screen (`ProxyConnectScreen`)**

- Tabs: MTProto, SOCKS5, Web (HTTP), Subscription.
- Default ports 443/1080/8080; switching tabs replaces an untouched default port.
- MTProto requires a non-blank secret; SOCKS5/HTTP accept optional username/password.
- Manual “Connect” adds the proxy with `enable=true` and returns; failures stay on the same screen.
- Subscription tab: link field, refresh action, auto/manual segmented control, server list with delay and best badge, “check latency”, “select server”, share connection log.

**Restore dialog (`ProxyRestoreDialog`)**

- Appears only when automatic restore exhausted all attempts.
- Back press and outside click are disabled.
- “Retry” runs a fresh round and shows a hint while it runs; “Turn off” calls `stopProxy()`.

### 3.7 Diagnostics and security

- Proxy lifecycle events go to the shareable connection log; debug builds also logcat.
- Probe results record a sanitized node label, delay and elapsed time; TLS failures include a short sanitized exception chain.
- URLs, headers (including HWID), bodies, secrets and proxy endpoints are removed.
- Exported diagnostics include connection state, network route state, timestamps and recent events.
- HWID is an encrypted random installation pseudonym, not a hardware identifier.
- The local SOCKS listener is authenticated specifically because every app on the device can reach `127.0.0.1`.

---

## 4. Windows current status

### 4.1 Calls

| Area | Current Windows state | Evidence |
|---|---|---|
| Calls page/nav | None. Shell sections are Chats, Profile, Settings, Network, Logs, Search. | `qml/Shell.qml:135–170`, `qml/Shell.qml:201–228` |
| Call-history request | None. No `searchCallMessages`, no `messageCall` parser, no paging model. | `rg searchCallMessages` finds nothing in `src/` |
| Call preview | `messageCall` becomes the literal string `Call` in chat preview/content text. | `src/telegram/TelegramTypes.cpp:286–287`, `:357–358` |
| Call row data | `MessageData` has no call id, duration, discard reason, video flag or direction-specific status. | `src/telegram/TelegramTypes.h:30–65` |
| Call UI | No Calls QML page, no call row component, no per-account call state. | `qml/pages/`, `qml/components/` |
| Call privacy/notifications | Not implemented; Windows has no privacy-settings surface and its notifications are message-based. | `src/ui/AppController.cpp`, `qml/pages/SettingsPage.qml` |

### 4.2 Proxy/VPN

| Area | Current Windows state | Evidence |
|---|---|---|
| Bootstrap configuration | Reads `BEYGRAM_PROXY`, then `proxy.txt` next to the executable, then `proxy.txt` in the per-user config directory; first non-comment line wins. | `src/telegram/TelegramConfig.cpp:20–44` |
| URL parsing | `socks5://`, `http://`, `https://` (mapped to HTTP proxy), `mtproto://…?secret=…`; username/password from URL user-info; separate `BEYGRAM_PROXY_*` variables as fallback. | `src/telegram/TelegramConfig.cpp:80–106` |
| Validation/summary | Requires non-empty server, port 1–65535, type socks5/http/mtproto; summary is `type server:port` and never includes the password. | `src/telegram/TelegramConfig.cpp:66–78` |
| Application | `AppController::prepareConfig()` assigns the parsed proxy to `TelegramConfig.proxy`. | `src/ui/AppController.cpp:189` |
| TDLib wiring | `TelegramSession::ensureTdlibParameters()` sends `addProxy` with `enable=true` and comment `BeyGram`, then immediately sends `setTdlibParameters` without waiting for the proxy response. | `src/telegram/TelegramSession.cpp:481–519` |
| Proxy types | HTTP, MTProto secret, SOCKS5 username/password. | `src/telegram/TelegramSession.cpp:521–542` |
| Proxy list/CRUD | None: no `getProxies`, `enableProxy`, `disableProxy`, `removeProxy` or saved-proxy model. | `rg` finds no such requests in `src/` |
| Latency test | None. | `src/` |
| Route restore | None. TDLib may keep the enabled proxy across restarts, but Windows never checks or restarts anything; there is no local listener to restore. | `src/telegram/TelegramSession.cpp`, `src/ui/AppController.cpp` |
| Subscription/Mihomo | None. No profile parser, no loopback listener, no subscription fetch, no auto-select, no core process. | `src/`, `CMakeLists.txt` |
| Network changes | None. No `QNetworkInformation`, no network monitor. | `rg QNetwork` finds nothing |
| UI | Environment page shows a read-only `Proxy` status row and a hint to set `BEYGRAM_PROXY` before start. No add/edit/remove/test. | `qml/pages/EnvironmentPage.qml:135–141` |
| Diagnostics | Diagnostics text includes `Proxy: <summary>` or `direct`. No route state, proxy events or probe results. | `src/ui/AppController.cpp:1284–1287` |
| VPN | No Windows VPN/TUN/driver and no system proxy integration. | `src/`, `CMakeLists.txt` |
| Tests | Only bootstrap parsing: URL env, `proxy.txt` next to the executable, summary redaction. | `tests/tst_telegram_config.cpp:126–203` |

**Important wording correction:** `README.md` and `docs/WINDOWS_FOUNDATION.md` say “proxy support”. That currently means **bootstrap proxy configuration only**, not Android-equivalent proxy management or Mihomo subscriptions. Parity documentation should not read it as feature parity.

### 4.3 Windows-specific correctness issues in the current bootstrap path

1. **No dedupe.** Every start sends a new `addProxy` with comment `BeyGram`; repeated starts can accumulate duplicate TDLib proxy entries because `getProxies` is never consulted.
2. **Bootstrap overrides a future user choice.** If a proxy UI is added later, `enable=true` on every start would silently re-enable the environment/file proxy and override a saved selection.
3. **No “direct” switch.** A user can only remove `BEYGRAM_PROXY`/`proxy.txt` and restart; there is no in-app route change.
4. **No failure state for a bad proxy.** A failed `addProxy` is reported as a generic auth error; the app does not expose a proxy-specific error or fallback choice.
5. **Password handling is good** (summary excludes it), but there is no proxy-URL-specific redaction test; only generic password redaction and the summary assertion exist.

---

## 5. Gap matrix

Legend: **P1** = connectivity-critical, **P2** = important, **P3** = parity/polish, **P4** = optional/platform-specific. Complexity: S/M/L/XL.

| ID | Area | Android | Windows now | Gap | Priority | Cplx |
|---|---|---|---|---|---|---|
| C1 | Calls tab | Main tab/route, optional in nav settings | None | Add Calls page + nav entry | P3 | S |
| C2 | Call history request | `SearchCallMessages(offset, 50, false)` | None | Add request + page parser | P3 | S |
| C3 | Call row mapping | Status/direction/video/duration/date | Only literal `Call` preview | Add `CallRecord` mapping | P3 | S |
| C4 | Call paging/merge | Overlap-aware first page, dedupe append | None | Port state rules + tests | P3 | S |
| C5 | Call avatars | Chat photo + initials fallback | No call model | Add photo roles via `MediaFileStore` | P3 | S |
| C6 | Call privacy/notifications | `allow calls`; call groups excluded | No privacy UI; message notifications only | Port with privacy/notification phase | P4 | M |
| P1 | Proxy list/CRUD | `GetProxies`/`AddProxy`/`EnableProxy`/`DisableProxy`/`RemoveProxy` | Bootstrap add only | Add manager + UI | P1 | M |
| P2 | Proxy latency | TCP connect, 5 s | None | Add TCP probe + model role | P2 | S |
| P3 | Route restore | Previous proxy remembered; restore dialog | None | Add route store + restore flow | P1 | M |
| P4 | Bootstrap dedupe/precedence | Local entry identified by comment; manual route respected | Re-adds every start; always enables | Query existing proxies before adding; define precedence | P1 | S |
| P5 | Proxy UI | List + connect tabs + delete confirm + restore dialog | Environment status row only | Add pages/dialogs | P2 | M |
| P6 | Subscription/Mihomo | Embedded core, loopback SOCKS, profile normalizer, auto-select | None | Product/licensing decision; likely separate phase | P3 | XL |
| P7 | Network monitoring | `ConnectivityManager` validated routes | None | Add `QNetworkInformation` monitor | P2 | M |
| P8 | Diagnostics | Route/events/probes, redacted | Summary only | Extend diagnostics, add redaction tests | P2 | S |
| P9 | Multi-account proxy policy | Process-wide listener; follow/shared/blocked | Account registry exists, no proxy policy | Define per-account route policy | P3 | L |
| P10 | System VPN/TUN | Not implemented (local SOCKS only) | None | Keep as explicit non-goal | — | — |

---

## 6. Implementation plan

### 6.1 Phase A (P1) — TDLib proxy core, no UI

Goal: make manual MTProto/SOCKS5/HTTP proxies and direct mode fully manageable and restorable, and stop the bootstrap path from duplicating or overriding routes.

**New files**

- `src/telegram/ProxyTypes.h`
  - `enum class ProxyKind { Mtproto, Socks5, Http };`
  - `struct ProxyDraft { ProxyKind kind; QString server; int port; QString username; QString password; QString secret; bool isValid(QString* error) const; };`
  - `struct SavedProxy { int id; ProxyKind kind; QString server; int port; bool enabled; QString comment; };`
  - `enum class ProxyRouteKind { Direct, Manual, Subscription };`
  - `struct ProxyRoute { ProxyRouteKind kind; SavedProxy proxy; };`
- `src/telegram/ProxyRequests.h/.cpp` — pure request builders/parsers, no logging:
  - `QJsonObject getProxiesRequest();`
  - `QJsonObject addProxyRequest(const ProxyDraft&, bool enable, const QString& comment);`
  - `QJsonObject enableProxyRequest(int proxyId);`
  - `QJsonObject disableProxyRequest();`
  - `QJsonObject removeProxyRequest(int proxyId);`
  - `QJsonObject pingProxyRequest(int proxyId);` (optional; Android uses TCP)
  - `QJsonObject proxyTypeObject(const ProxyDraft&);`
  - `std::optional<SavedProxy> parseAddedProxy(const QJsonObject&);`
  - `ProxyRoute routeFromProxies(const QJsonArray& proxies, const QString& subscriptionComment);`
  - `bool isSubscriptionProxy(const QJsonObject& addedProxy, const QString& subscriptionComment);`
- `src/telegram/ProxyRouteStore.h/.cpp` — JSON-backed, versioned store under `AppPaths::configDirectory()/proxy-route.json`:
  - `previousProxyId` (0 = direct), `lastRouteKind`, `bootstrapManaged` flag.
  - `QSaveFile` writes; never stores passwords; corrupt/missing file falls back to defaults.
- `src/telegram/ProxyLatency.h/.cpp` — `QTcpSocket` connect probe with 5 s timeout (parity with Android's raw TCP test); returns measured ms or a typed failure.
- `src/telegram/ProxyManager.h/.cpp` — async facade over `TelegramSession::sendRequest`:
  - `refresh()`, `add(draft, enable)`, `enable(id)`, `disable()`, `remove(id)`, `currentRoute()`, `setSubscriptionRoute(bool)`, `restorePreviousRoute()`, `measureLatency(server, port)`.
  - One `QMutex`/request queue for proxy mutations; a generation counter rejects stale responses.
  - Identify the subscription entry by comment (`BeyGram Mihomo`), exactly as Android does; identify user entries by their comment.
  - When enabling the subscription route, remember the previously enabled proxy id (or 0) in `ProxyRouteStore`; when disabling it, restore only if the current route is still the subscription entry.

**Modified files**

- `src/telegram/TelegramSession.h/.cpp`
  - Add a `bootstrapProxy()` path that first sends `getProxies`; if an enabled proxy already exists, skip the bootstrap `addProxy`; otherwise add with `enable=true`.
  - If `getProxies` fails/times out, keep the current behaviour as a fallback but log a sanitized warning.
  - Add a `proxySetupFinished` signal or callback so `AppController` can refresh the route after auth.
  - Do **not** add a second proxy on every start.
- `src/telegram/CMakeLists.txt` — add new sources.
- `CMakeLists.txt` — add `Network` to `find_package(Qt6 … COMPONENTS …)` for `QTcpSocket`/`QNetworkInformation`.
- `src/telegram/TelegramConfig.h/.cpp` — keep env/file parsing for bootstrap; add a clear precedence rule:
  - TDLib stored/enabled proxy wins over bootstrap on later starts;
  - bootstrap applies only when no enabled proxy exists;
  - `proxy.txt`/env remains the “first run / recovery” path.
- `src/ui/AppController.cpp` — expose route state and call `ProxyManager` after session start; update diagnostics.

**Tests**

- `tests/tst_proxy_requests.cpp`
  - request JSON shapes for all five TDLib calls;
  - MTProto secret required; SOCKS5/HTTP credentials optional;
  - port/server validation;
  - `addedProxy` parsing (enabled/disabled, comments, unsupported types);
  - route resolution: direct, manual, subscription;
  - summary never contains a password.
- `tests/tst_proxy_route_store.cpp`
  - round-trip, missing file, corrupt JSON, version fallback;
  - previous proxy restored only when the current route is the subscription entry.
- `tests/tst_proxy_latency.cpp`
  - local `QTcpServer` accepts → measured;
  - closed port → failure within timeout;
  - cancellation/destruction does not crash.
- `tests/tst_proxy_manager.cpp` (pure policy seams)
  - stale generation ignored;
  - add/enable/disable/remove ordering;
  - bootstrap dedupe decision (existing enabled proxy vs none);
  - manual route invalidates pending subscription activation.
- Add all four to `tests/CMakeLists.txt`.

**Acceptance**

- A user can add, select, deselect, and remove a proxy without restarting the app.
- The previous manual/direct route is restored after switching the subscription route off.
- Repeated starts do not create duplicate TDLib proxy entries.
- A bad proxy produces a proxy-specific error, not a generic auth error.
- No password or proxy endpoint appears in logs/diagnostics.

**Risks / verification points**

- `getProxies` before `setTdlibParameters` must be verified against the actual TDLib build. If TDLib rejects it in `WaitTdlibParameters`, use a bounded fallback: add the bootstrap proxy with `enable=true`, then reconcile with `getProxies` after auth and remove duplicates by comment.
- TDLib proxy entries are local to the account database. A future per-account proxy policy must not share ids across sessions.

### 6.2 Phase B (P2) — Proxy UI, network awareness, diagnostics

**New files**

- `src/telegram/ProxyListModel.h/.cpp`
  - Roles: `proxyId`, `kind`, `kindLabel`, `server`, `port`, `enabled`, `latencyMs`, `latencyState` (`unknown/measuring/reachable/unreachable`), `isSubscription`.
  - `setProxies()`, `setLatency(id, …)`; no media roles are needed.
- `src/ui/ProxyController.h/.cpp`
  - `Q_PROPERTY` list model, `busy`, `error`, `routeTitle`, `routeKind`, `restoreFailed`, `restoring`, `subscriptionConfigured`.
  - `Q_INVOKABLE` `refresh()`, `addProxy(kind, server, port, username, password, secret)`, `selectProxy(id)`, `selectDirect()`, `removeProxy(id)`, `testProxy(id)`, `testAll()`, `retryRestore()`, `turnOffSubscription()`.
  - Owned by `AppController` and exposed to QML; QML never builds TDLib JSON.
- `src/platform/windows/NetworkMonitor.h/.cpp`
  - `QNetworkInformation` reachability/transport; emits a `changed` signal; no polling.
  - Pure decision helper `shouldProbeAutomaticRoute(...)` ported from `TelegramAutoSelector.kt`.

**QML**

- `qml/pages/ProxyPage.qml`
  - Subscription section (server count/best server/status), direct row, saved proxies with kind/server:port/latency, add button, delete confirmation, inline errors.
- `qml/components/ProxyEditorDialog.qml`
  - Kind selector, server/port/secret or username/password fields, validation, “Connect” action; no password echo after submit.
- `qml/components/ProxyRestoreDialog.qml`
  - Non-dismissible retry/turn-off dialog matching Android semantics.
- `qml/pages/EnvironmentPage.qml`
  - Replace the read-only hint with a “Manage proxy” action and show the current route; keep the bootstrap hint as secondary help.
- `qml/pages/SettingsPage.qml`
  - Add a “Proxy” link (Android places it under Settings → Proxy for Telegram).
- `CMakeLists.txt` (root)
  - Add all new QML files to `qt_add_qml_module`.

**Tests**

- `tests/tst_proxy_model.cpp` — role names, latency state transitions, enabled/direct selection.
- `tests/tst_network_policy.cpp` — auto-probe decision table (enabled, network ready, Connecting/WaitingForNetwork, listener running).
- Extend `tests/tst_redaction.cpp` with proxy URLs, `mtproto://…secret=`, usernames/passwords and `X-BeyGram-HWID`-style headers.
- Add to `tests/CMakeLists.txt`.

**Acceptance**

- The Environment page and Settings page both lead to a working proxy manager.
- Adding a proxy from the UI immediately enables it and updates the route title.
- Removing the active proxy warns and falls back to direct or the previous proxy.
- Network loss/recovery updates route state without a restart.
- Diagnostics show route kind and probe status, never endpoints or secrets.

### 6.3 Phase C (P3) — Calls history

**New files**

- `src/telegram/CallHistory.h/.cpp`
  - `enum class CallStatus { Answered, Missed, Declined, NoAnswer };`
  - `struct CallRecord { qint64 messageId, chatId; QString title; int photoFileId; bool photoReady; int photoRevision; QString initials; int accentIndex; bool isVideo; bool isOutgoing; CallStatus status; int durationSeconds; qint64 dateSeconds; };`
  - `struct CallHistoryPage { QVector<CallRecord> calls; QString nextOffset; };`
  - `QJsonObject searchCallMessagesRequest(const QString& offset, int limit = 50, bool onlyDeleted = false);`
  - `CallStatus callStatusFromDiscardReason(const QString& type, int duration, bool outgoing);`
  - `CallHistoryPage parseCallHistoryPage(const QJsonObject& foundMessages, const QHash<qint64, ChatData>& chats);`
  - Missing chat → blank title, no photo, stable accent from chat id.
- `src/telegram/CallHistoryModel.h/.cpp`
  - `QAbstractListModel` roles: messageId, chatId, title, initials, accentIndex, photoFileId, photoReady, photoRevision, isVideo, isOutgoing, status, durationSeconds, dateSeconds.
  - `setFirstPage(page)`, `appendPage(page)`, `clear()`, `hasMore()`.
  - Port `CallsScreenState` rules exactly:
    - fresh first page overlaps loaded calls → prepend fresh, keep older, keep old offset;
    - no overlap → replace and drop older pages;
    - append skips known `chatId:messageId`.
  - `markPhotoReady(fileId)` updates matching rows (mirror `ChatListModel`).

**Modified files**

- `src/telegram/TelegramRepository.h/.cpp` — add `loadCallHistory(offset)` and route it to `CallHistory`; or keep `CallHistory` as a separate helper owned by `AppController`. Prefer the helper + controller to avoid growing the chat repository further.
- `src/ui/AppController.h/.cpp`
  - `Q_PROPERTY(QAbstractItemModel* calls READ calls CONSTANT)`
  - `Q_PROPERTY(bool callsLoading …)`, `callsError`, `callsHasMore`
  - `Q_INVOKABLE loadCalls()`, `loadMoreCalls()`, `refreshCalls()`, `requestCallPhoto(fileId)`.
  - Wire `MediaFileStore::fileReady` to `CallHistoryModel::markPhotoReady`.
- `src/telegram/CMakeLists.txt`, `src/ui/CMakeLists.txt` — add sources.
- `qml/Shell.qml` — add a “Calls” nav entry and stack page. Android's tab is optional; Windows can keep it always visible until a navigation-settings surface exists.
- `qml/pages/CallsPage.qml` — title, loading bar, empty state, error+retry, list, “show more”, row with `Avatar` and direction glyph.
- `qml/components/CallRow.qml` (optional) — title, direction icon/text, summary, date/time, missed tint.
- `CMakeLists.txt` (root) — add QML files.
- `src/telegram/TelegramTypes.cpp` — optionally improve chat preview from literal `Call` to a more useful status once call parsing exists; keep the existing `Call` fallback until then.

**Tests**

- `tests/tst_call_history.cpp`
  - duration > 0 → Answered;
  - declined → Declined;
  - missed incoming → Missed; missed outgoing → NoAnswer;
  - disconnect/hang-up → NoAnswer;
  - page parsing with missing chat;
  - limit/offset request shape.
- `tests/tst_call_history_model.cpp`
  - direct port of `CallsScreenStateTest`:
    - first load;
    - refresh with overlap keeps older pages;
    - refresh without overlap drops older pages;
    - refresh covering all loaded calls takes the new offset;
    - append skips duplicates.
- Add to `tests/CMakeLists.txt`.

**Acceptance**

- Calls page lists history for the active account, paginates, dedupes and survives tab switches.
- Missed/incoming/outgoing/declined/no-answer rows match Android text and tint.
- Avatars load through `MediaFileStore`; missing chats still render.
- There is **no** call/answer button, matching Android.

**Explicit non-goal:** no VoIP, no microphone/camera call pipeline, no call-state machine, no ringing UI.

### 6.4 Phase D (P3, decision gate) — Subscription/Mihomo

This is not a mechanical port. Android's Mihomo core is an Android AAR with JNI and a foreground-service model. Windows needs a deliberate decision.

**Option A — no bundled core (recommended first)**

- Keep manual TDLib proxies and the existing external `BEYGRAM_PROXY`/`proxy.txt` path.
- Show subscription/Mihomo UI as unavailable with an explicit explanation, not a fake control.
- Users run their own Clash/Mihomo and point `BEYGRAM_PROXY` at its mixed/SOCKS port.
- Effort: S. Parity: partial, documented.

**Option B — bundled `mihomo.exe` as a child process**

- `src/vpn/MihomoProcess.h/.cpp`: `QProcess` lifecycle, loopback port allocation, readiness handshake, crash/restart policy, graceful shutdown.
- `src/vpn/MihomoProfileNormalizer.h/.cpp`: same normalization rules as Android; requires a YAML library decision (`yaml-cpp` or a small reviewed parser). Do not hand-roll YAML.
- `src/vpn/SubscriptionClient.h/.cpp`: `QNetworkAccessManager`, HTTPS-only, same-origin redirects, 5 MB cap, timeouts, `X-BeyGram-HWID`.
- `src/vpn/MihomoSettingsStore.h/.cpp`: subscription URL, auto-select, handover delay, selected node, group name.
- `src/vpn/MihomoController.h/.cpp`: start/stop, import/validate, `getProxies`/`testDelay`/`changeProxy` equivalent (process API or external controller bound to loopback), auto-select, restore.
- `src/platform/windows/InstallationId.*`: random per-install ID protected with DPAPI, not a hardware identifier.
- QML: subscription tab inside `ProxyPage`/`ProxyEditorDialog`, server list, latency sweep, auto/manual selector, diagnostics share.
- Tests: normalizer vectors, URL policy, redirect/origin rules, size limits, selected-node persistence, auto-select decision table; manual integration test with a real core and a real subscription.
- Risks: GPL/compliance, process supervision, port collisions, firewall prompts, YAML parser security, secrets in command lines (never pass credentials as CLI arguments), auto-update of the core.
- Effort: L/XL. Do not start without a licensing/security decision.

**Option C — Windows VPN/TUN driver/service**

- Reject for this parity effort. Android itself does not implement a VPN; adding a Windows driver is a new system-level product with signing, security review and support burden.

### 6.5 Phase E (P2/P3) — Diagnostics, multi-account policy, polish

- Extend `AppController::diagnosticsText()` with:
  - current route kind (direct/manual/subscription), proxy enabled/disabled;
  - listener/process state (if Option B);
  - last probe result (sanitized node label, delay, elapsed);
  - recent proxy events from a bounded ring buffer.
- Never include: proxy server/port in shareable text unless explicitly accepted as non-secret; never include username/password/secret, subscription URL, HWID, or profile contents.
- Add redaction tests for `socks5://user:pass@host:port`, `mtproto://…?secret=…`, `X-BeyGram-HWID`, and subscription URLs with tokens.
- Multi-account: Android uses one process-wide subscription listener and a fail-closed secondary policy (follow primary / shared Mihomo / explicit direct / blocked). Windows should define the same matrix before enabling proxy controls per account. Until then, keep proxy management scoped to the active account and state that clearly.

---

## 7. Priority summary

| Order | Work | Priority | Why |
|---|---|---|---|
| 1 | TDLib proxy CRUD + route restore + bootstrap dedupe | P1 | Users behind restrictive networks currently need env/file editing and cannot switch routes in-app; duplicate entries and silent override are correctness bugs. |
| 2 | Proxy UI + latency + diagnostics | P2 | Makes the P1 core usable and observable; matches Android screens without new platform dependencies. |
| 3 | Calls history page/model | P3 | Small, self-contained parity win; explicitly no VoIP. |
| 4 | Network monitor + auto-reconnect policy | P2/P3 | Needed for desktop network changes; can follow the proxy core. |
| 5 | Subscription/Mihomo | P3 decision gate | Large, licensing/security-sensitive, and not required for manual proxy parity. |
| 6 | Multi-account proxy policy | P3 | Only relevant once multiple active accounts and proxy controls coexist. |
| — | Windows VPN/TUN | Non-goal | Android has no VPN either; a driver is a separate product. |

**Recommended first slice:** Phase A (P1) plus the pure tests. It is small enough to land independently, removes the current bootstrap bugs, and creates the seam (`ProxyManager`, `ProxyRequests`, `ProxyRouteStore`) that both the UI and any future Mihomo work need.

---

## 8. Explicit non-goals / do-not-promise list

- Do not claim Windows has voice/video calls. Android has history only.
- Do not claim Windows has a VPN, TUN adapter, system proxy or always-on background proxy. Android's “VPN” is a local SOCKS listener.
- Do not claim Mihomo/subscription parity until Option B is implemented and tested.
- Do not claim proxy management parity while only `BEYGRAM_PROXY`/`proxy.txt` exists.
- Do not log or export proxy passwords, MTProto secrets, subscription URLs, HWID values or profile contents.
- Do not reuse Android TDLib databases or proxy ids on Windows; TDLib databases are platform-specific.

---

## 9. Open decisions

1. **Subscription/Mihomo:** bundle `mihomo.exe` (Option B) or keep external-proxy-only (Option A)? This decides whether the subscription UI is built at all.
2. **YAML parser:** if Option B, choose and review a parser before writing the normalizer; Android's SnakeYAML behaviour (duplicate-key rejection, alias limits, 5 MB cap) must be matched.
3. **Bootstrap precedence:** should `BEYGRAM_PROXY`/`proxy.txt` be a one-time bootstrap, a forced override, or a fallback? Recommended: fallback only when TDLib has no enabled proxy, with a visible “configured by environment” badge.
4. **Proxy entry visibility:** should the BeyGram subscription entry be hidden from the manual list (Android hides it) or shown read-only? Recommended: hide, as Android does.
5. **Latency semantics:** Android measures a raw TCP connect to `server:port`; TDLib `pingProxy` measures through the proxy. Keep Android's raw TCP probe for parity and label it as “endpoint reachability”, not “proxy works”.
6. **Per-account proxy policy:** one process-wide subscription route (Android legacy model) vs per-account manual proxies. Decide before adding proxy controls to account switching.
7. **Calls tab visibility:** always visible on Windows, or add a navigation-settings surface first? Recommended: always visible initially; add visibility settings with the broader navigation-settings port.

---

## 10. Verification checklist for the future implementation

These are acceptance gates, not results of this audit.

- [ ] Add/edit/remove/enable/disable a manual proxy without restarting; route title updates.
- [ ] “Direct” disables the proxy and survives restart.
- [ ] Restart with a subscription/manual proxy does not create duplicates and does not silently override the stored route.
- [ ] Failed restore shows a non-dismissible retry/turn-off dialog; turn-off restores the previous route.
- [ ] Latency probe shows measuring/reachable/unreachable and never blocks the UI thread.
- [ ] Proxy password/secret never appears in UI after submit, logs, diagnostics or crash text.
- [ ] Calls history loads, paginates, dedupes and preserves pages across tab switches.
- [ ] Call status/direction/duration match Android for answered, missed, declined and no-answer cases.
- [ ] Calls page has no call/answer action.
- [ ] `ctest` proxy/call suites pass locally (to be run by the implementing change, not by this audit).
