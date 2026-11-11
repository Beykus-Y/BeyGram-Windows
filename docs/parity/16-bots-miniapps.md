# Bots, bot commands, keyboards, deep links, Mini Apps — Android → Windows parity audit

Status: read-only static audit of the repository at `F:\BeyGram Windows`.
Date: 2026-11-11.
Nothing in this document was produced by building, running, or testing either client.
Every statement below comes from reading the sources named in §2 and §4.

Companion documents: [PORTING_AUDIT.md](../PORTING_AUDIT.md) (M22 bots/mini apps,
M20 deep links), [PORTING_ROADMAP.md](../PORTING_ROADMAP.md) (Phase 5, items 4),
[WINDOWS_TDLIB.md](../WINDOWS_TDLIB.md), [conversation-architecture.md](../conversation-architecture.md).

---

## 1. Scope and method

In scope:

- bot identification in a chat and the explicit `/start` action;
- bot command menus (`getUserFullInfo.botInfo.commands`) and the composer entry point;
- reply keyboards (`replyMarkupShowKeyboard`, `replyMarkupForceReply`): render, collapse,
  one-time dismissal, placeholder, Web App buttons;
- inline keyboards on messages (`replyMarkupInlineKeyboard`): callback, URL, copy-text,
  user, Web App and unsupported buttons, plus callback answers;
- inline mode (`@bot query`): parsing, resolution, pagination, sending;
- Mini Apps / Web App: consent, launch through TDLib, theme hand-off, bridge event
  allowlist, main/back buttons, data validation and `sendWebAppData`, close lifecycle,
  WebView hardening;
- deep links (`https://t.me`, `https://telegram.me`, `tg://`), pending-link persistence
  across sign-in, multi-account binding, and `t.me`/`tg://` entity links inside message
  text.

Out of scope: the diagnostics report bot
(`app/src/main/java/com/beykus/beygram/diagnostics/report/BotReplyParser.kt`,
`BotReplyCorrelator.kt`) and music inline-bot playback (`playback/MusicBotCache.kt`) —
both are bot conversations but belong to the diagnostics and music work packages.
Payments inside Mini Apps are described as Android currently implements them (explicitly
unsupported); no payment feature is proposed here.

Method: the Android client is the functional reference and is read first (§2–§3). The
Windows tree is then read to establish what actually exists (§4). Only then are gaps and
a plan written (§5–§8). Where Android and Windows disagree, Android behaviour is treated
as the specification unless the Windows deviation is deliberate and documented.

---

## 2. Android files reviewed

### 2.1 Core bot / Mini App sources

| File | Why it matters |
|---|---|
| `app/src/main/java/com/beykus/beygram/domain/model/BotFeatures.kt` (72) | `BotCommand`, `BotCommandMenu`, `ParsedInlineQuery`, `parseInlineQuery`, `InlineBotResult(s)`, `mergeInlineBotResults`, `nextInlineQueryOffset`, `MusicBotTrack`. |
| `app/src/main/java/com/beykus/beygram/domain/model/BotWebApp.kt` (129) | `BotWebAppTheme`, `BotWebAppSource`, `BotWebAppLaunch`, `validateBotWebAppData`, `normalizedWebAppUrl`, `isAllowedWebAppNavigation`, `webAppOriginRule`. |
| `app/src/main/java/com/beykus/beygram/domain/model/MessageMarkup.kt` (177) | `TextStyleKind` (incl. `Command`), `BotButtonKind`, `BotButton`, `BotKeyboard`, `BotCallbackAnswer`, entity validation/round-trip. |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdBotFeatures.kt` (436) | Command menu, inline queries, music bot, `sendInlineResult`, `openWebApp`, `sendWebAppData`, `closeWebApp`, inline-bot resolution cache. |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdBotKeyboards.kt` (42) | Per-chat reply-keyboard state (`StateFlow`), versioned refresh, `dismiss`, `callback`. |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdMessageMarkup.kt` (119) | `inlineKeyboard(markup)`, `botKeyboard(message)`, TDLib ↔ domain entity mapping. |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/BotButtons.kt` (125) | `InlineBotButtons`, `BotButtonRows`, `BotReplyKeyboard`. |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/BotCommandMenu.kt` (110) | Composer command button + searchable command sheet. |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/BotInlineResults.kt` (347) | Inline-results panel: debounce, pagination, thumbnails, send. |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/BotStartBar.kt` (78) | Explicit `/start` action for a fresh bot chat. |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/BotWebAppScreen.kt` (380) | Full-screen WebView, navigation policy, WebView settings, main/back button, reload/error. |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/TelegramWebAppBridge.kt` (340) | Origin-bound `TelegramWebviewProxy.postEvent` bridge, event allowlist, parsers, theme payload. |

### 2.2 Deep-link sources

| File | Why it matters |
|---|---|
| `app/src/main/java/com/beykus/beygram/deeplink/TelegramDeepLink.kt` (353) | Fail-closed parser for `https://t.me`, `https://telegram.me`, `tg://`; `TelegramDeepLinkRoute`; router. |
| `app/src/main/java/com/beykus/beygram/deeplink/PendingTelegramDeepLink.kt` (122) | Process-independent pending-link inbox, token/account scoping, `SharedPreferences` storage. |
| `app/src/main/java/com/beykus/beygram/deeplink/MessageEntityLink.kt` (131) | Entity → action mapping (public chat, user, message, invite, external URL, email, phone, proxy) with spoiler suppression. |

### 2.3 Integration points read (bot-relevant sections only)

| File | Sections read |
|---|---|
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ChatScreen.kt` (2935) | bot state (L333–407), force-reply wiring (L768, L1456), bot bars/panels (L1989–2059, L2133), consent + sheet + screen hosting (L2356–2383). |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdLibTelegramRepository.kt` (3695) | bot delegation (L1450–1492), keyboard refresh on `openChat` (L2395–2403), `updateChatReplyMarkup` (L2545) and `UpdateMessageReplyMarkup` branches, link resolution (L1052, L1206–1257). |
| `app/src/main/java/com/beykus/beygram/domain/repository/ConversationRepository.kt` (341) | Bot API surface (L12–57). |
| `app/src/main/java/com/beykus/beygram/MainActivity.kt` (486) | Intent intake, dedupe, pending inbox, app-lock gating (L89–130, L379–407). |
| `app/src/main/java/com/beykus/beygram/BeyGramApp.kt` (3102) | Deep-link routing/execution (L1074–1200), invite/proxy prompts. |
| `app/src/main/AndroidManifest.xml` (123) | `VIEW` intent filters for `https://t.me`, `https://telegram.me`, `tg` (L87–105). |
| `app/src/main/res/values-en/strings_chat.xml`, `strings_errors.xml` | User-visible bot/Mini-App copy and error strings. |

### 2.4 Tests read as the executable specification

| Test | Covers |
|---|---|
| `app/src/test/java/.../domain/model/BotFeaturesTest.kt` (77) | Inline-mention parsing boundaries, page merge/dedupe, cursor stop rule, URL policy, data validation. |
| `app/src/test/java/.../ui/screens/chat/TelegramWebAppBridgeTest.kt` (161) | Event allowlist, main/back button parsing, colours, send-data shapes, origin rule, theme JSON. |
| `app/src/test/java/.../deeplink/TelegramDeepLinkParserTest.kt` (236) | Hosts, `tg://` forms, bot `start`, invites, phones, proxy, rejection cases. |
| `app/src/test/java/.../deeplink/PendingDeepLinkInboxTest.kt` (135) | Cold start, token scoping, account binding, malformed input. |
| `app/src/test/java/.../data/tdlib/MessageMarkupTest.kt` (132) | Binary callback data, keyboard rows, entity round-trip. |

---

## 3. Android behaviour (the specification)

### 3.1 Bot identification and `/start`

- A chat is a bot chat when the profile request returns `TdApi.UserTypeBot`
  (`data/tdlib/TdProfiles.kt`, `ProfileKind.Bot`), not from the chat type string.
- `BotStartBar` (`BotStartBar.kt:28–78`) renders **Start bot** only when
  `info.kind == ProfileKind.Bot`, the chat has no older history, and **no outgoing message
  exists** in the loaded history (`ChatScreen.kt:1994–2002`). The tap sends the literal
  `/start` with `clearDraft = false`, shows a spinner, and reports failure as a chat error.
  It is independent of inline mode, commands, and Mini Apps.
- Bots may post, so the composer is not disabled in a bot chat.

### 3.2 Command menu

- Source: `getChat` → private chat → `getUser` (`userTypeBot`) →
  `getUserFullInfo.botInfo.commands` (`TdBotFeatures.kt:49–60`). Commands are trimmed and
  de-slashed; an empty list yields `null`, so no button appears. Groups and channels
  deliberately get **no** menu: this TDLib build has no `getBotCommands`/
  `updateBotCommands`, and `botCommandScope`/`getCommands` only manage the account's
  *own* bot.
- UI: composer icon button (`BotCommandMenu.kt:29–41`) opens a bottom sheet with search,
  `/command` in monospace, description, 52 dp rows and an empty state.
- Selecting a command **only inserts** `/command ` into the draft and clears entities
  (`ChatScreen.kt:369–376`); nothing is sent without a second tap. The insert action
  carries an accessibility label ("Insert /%1$s").

### 3.3 Reply keyboard (custom keyboard)

- Parsed from the chat's current markup message: `chat.replyMarkupMessageId` →
  `getMessage` → `replyMarkup` (`TdBotKeyboards.kt:22–33`, `TdMessageMarkup.kt:108–119`).
- Kinds: `KeyboardButtonTypeText` → `Text`, `KeyboardButtonTypeWebApp` → `WebApp`,
  everything else `Unsupported`. `ReplyMarkupForceReply` yields an empty keyboard with
  `forceReply = true` and a placeholder.
- State is per chat and versioned to defeat stale async refreshes; refreshed on `openChat`
  and on `updateChatReplyMarkup` (`TdLibTelegramRepository.kt:2396`, `:2545`).
- UI (`BotButtons.kt:87–125`): collapses/expands with a **Hide/Show bot buttons** toggle,
  hides the IME when it appears, caps the grid at 240 dp with vertical scroll, uses 48 dp
  minimum buttons, and guards against double sends.
- Text buttons send `button.text` as a normal message; `oneTime` keyboards dismiss
  themselves with `deleteChatReplyMarkup` after a successful send. The placeholder
  replaces the composer placeholder (`ChatScreen.kt:2039`). Web App buttons resolve the
  bot from the private chat (`botUserId = 0`) and launch through `getWebAppUrl`.
- Force-reply sets the reply target to the markup message (`ChatScreen.kt:768`) and is
  cleared after the send (`:1456`); it also counts as "no stealth consent needed" in the
  send path (`:1416`).

### 3.4 Inline keyboards on messages and callback queries

- `inlineKeyboard(replyMarkup)` maps `ReplyMarkupInlineKeyboard` rows into
  `BotButton(text, kind, value, data)` with kinds `Callback` (binary `data` kept as
  bytes), `Url`, `CopyText`, `User` (user id), `WebApp` (url) and `Unsupported`
  (`TdMessageMarkup.kt:97–106`). Attached to every `ChatMessage` at parse time and
  updated by `UpdateMessageReplyMarkup`.
- Rendering (`BotButtons.kt:25–70`): rows of equal-width tonal buttons under the bubble
  text, one busy flag per message, alert dialog for `showAlert` callback answers.
  - `Url` → `safeMessageUrl` (http/https/tg/mailto/tel only) then the platform handler.
  - `User` → `tg://user?id=<id>`, routed back through the app's own link handling.
  - `Copy` → clipboard, "Text copied" toast.
  - `Callback` → `getCallbackQueryAnswer(chat_id, message_id, callbackQueryPayloadData)`;
    non-blank text becomes a toast or alert, and a returned URL goes through the same
    safe-URL gate.
  - `WebApp` → the **raw button URL is never opened**; it is handed to the repository with
    `BotWebAppSource.InlineKeyboard` and the message's sender id.
  - `Unsupported` → explicit "not available in BeyGram; open it in Telegram" toast.

### 3.5 Inline (`@bot query`) mode

- `parseInlineQuery` accepts `^@([A-Za-z][A-Za-z0-9_]{2,31})(?:\s+([\s\S]*))?$` at the
  **start of the draft only**, so an ordinary sentence mentioning `@name` stays text
  (`BotFeatures.kt:17–28`).
- Resolution: `searchPublicChat` → private chat → `getUser` → `userTypeBot.is_inline`.
  Both hits and misses are cached for 10 minutes (max 32 usernames); failures are never
  cached (`TdBotFeatures.kt:304–336`).
- Query: `getInlineQueryResults(botUserId, chatId, null, query, offset)`; results are a
  display projection keyed by `id`, each carrying its own `inlineQueryId`. Pages merge
  first-occurrence-wins, and pagination stops on a blank or repeated cursor
  (`BotFeatures.kt:54–62`).
- Panel (`BotInlineResults.kt`): 350 ms debounce, generation token so stale pages are
  dropped, 236 dp scroll area, 44 dp thumbnails (smallest ≥96 px photo), kind icons and
  fallback labels, per-result progress, retry/load-more. "Not an inline bot" is a null
  page and produces **no error**. The panel is hidden while editing, preparing
  attachments, recording, or sending.
- Sending: `sendInlineQueryResultMessage` with the composer's reply target, forum topic and
  send options (stealth scheduling applies), `hideViaBot = false`. The composer is locked
  while the send is in flight so the literal `@bot query` cannot be sent too; on success
  the draft is cleared only if it is still the same text.
- An inline-results **Web App button** (`InlineQueryResultsButtonTypeWebApp`) is accepted
  only as a validated https URL and opened with `BotWebAppSource.InlineQueryButton`.

### 3.6 Mini Apps: launch, theme, consent, close

| Source | TDLib call | Launch id | Notes |
|---|---|---|---|
| Inline-keyboard `WebApp` button | `openWebApp(chat_id, bot_user_id, url, topic_id, reply_to, parameters)` | non-zero, must be closed | URL from TDLib is re-validated |
| Reply-keyboard `WebApp` button | `getWebAppUrl(bot_user_id, url, parameters)` | 0 | bot resolved from the private chat |
| Inline-results Web App button | `getWebAppUrl(...)` | 0 | from the results page |

- Theme: the active palette is converted to ARGB → RGB and sent as TDLib
  `themeParameters` inside `webAppOpenParameters`, with `application_name = "BeyGram"`
  and `webAppOpenModeFullSize`. No account or message data is passed.
- Consent: before any launch the chat shows **"Open Mini App?"** with a warning that the
  bot will receive entered data (`ChatScreen.kt:2359–2373`). A second request is ignored
  while one is opening or active.
- Lifecycle: the TDLib launch is closed with `closeWebApp(launch_id)` from a
  `DisposableEffect` when the screen is disposed, cleared, or the chat is recreated
  (`ChatScreen.kt:403–407`); `closeWebApp(0)` is a no-op. The chat stays composed
  underneath, so the draft and scroll position survive.
- In-app events handled: `ready`, `expand`, `request_theme` (replies `theme_changed`),
  `close`, `setup_main_button`, `setup_back_button`, `send_data`/`data_send`,
  `open_invoice`. Anything else is dropped. Payments are explicitly **unsupported**
  ("Payments in Mini Apps are not supported yet") rather than faked.
- Main button: visible only with non-empty text (≤64 chars), active/progress states,
  optional `#rgb`/`#rrggbb` colours, and a busy guard while data is being sent. Back-button
  visibility makes Back call `back_button_pressed` instead of closing the screen.
- Data: `sendWebAppData(bot_user_id, button_text, data)` after local validation — non-empty,
  ≤4096 UTF-8 bytes, and complete JSON when the payload starts with `{`, `[` or `"`
  (`BotWebApp.kt:37–77`). A TDLib `TOO_LONG` refusal is re-mapped to the same "too large"
  message. The button text that opened the Web App is required and must be non-blank. A
  successful send closes the Mini App.

### 3.7 Deep links

Parser (`TelegramDeepLink.kt`) is pure, fail-closed, and never performs a lookup:

- Accepts only `https://t.me`, `https://telegram.me` (exact host, no userinfo, no port) and
  `tg://`; input must be ≤2048 chars, trimmed, with no whitespace or control characters.
- HTTPS: `/<username>`, `/<username>/<msg>`, `/<username>/<topic>/<msg>`, `/c/<id>/<msg>`
  (+topic), `/+<invite>`, `/+<phone>`, `/joinchat/<hash>`, `/proxy`, `/socks`. Reserved
  first segments (`login`, `addstickers`, `addemoji`, `addtheme`, `share`, `invoice`,
  `giftcode`, `boost`, `s`, `bg`, `contact`, `iv`, `setlanguage`, `addlist`, …) are
  rejected as usernames.
- `tg://`: `resolve?domain|phone&post&thread&start|startgroup`, `join?invite`, `proxy`,
  `socks`, `login`, `user?id`. Unknown actions and extra path segments are rejected.
- Query parsing is strict: duplicate keys rejected, `%`-decoding validated and UTF-8 aware,
  malformed escapes rejected.
- `Bot(username, startParameter)` keeps the `/start` payload but `TelegramDeepLinkRouter`
  maps it to **Chat only** — the payload is never sent automatically
  (`TelegramDeepLink.kt:337–349`; asserted by
  `TelegramDeepLinkParserTest.botStartPayloadIsKeptButTheRouterOnlyOpensTheChat`).
- `tg://login` is recognised **only to be ignored**; the token is never stored, logged, or
  turned into a chat lookup.

Delivery (`MainActivity.kt`, `PendingTelegramDeepLink.kt`, `BeyGramApp.kt`):

- `MainActivity` accepts `ACTION_VIEW` data, skips re-delivery from Recents
  (`FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY`) and repeated identical data strings, then offers
  the link to `PendingDeepLinkInbox`.
- Only validated **actionable** links are stored, so a malformed intent cannot erase a
  valid pending link. Storage is app-private `SharedPreferences` with `raw`, `token`,
  `accountId`.
- Handling is token-scoped: a late result cannot clear a newer link, an already-bound link
  cannot move to another account, and an unbound link defaults to the legacy account
  (`PendingTelegramDeepLink.kt:44–70`).
- Routing waits for the account registry, `AuthState.Ready` and repository selection,
  binds the link to the selected account first, and re-checks the account after each
  lookup. While app lock is engaged the pending link is withheld (`MainActivity.kt:269`).
- Route execution (`BeyGramApp.kt:1101–1199`): Chat → `resolvePublicChat` (bots included;
  short legacy usernames such as `@gif` allowed), Phone → `searchUserByPhoneNumber` +
  `createPrivateChat`, UserId → `createPrivateChat`, Message → `getMessageLinkInfo`
  (topic-aware), Invite → `checkChatInviteLink` + confirmation prompt, Proxy →
  confirmation prompt. Failures surface as user messages, never silently.

### 3.8 Entity links inside message text

`MessageEntityLink.kt` turns entities into actions without letting text invent a lookup:
mentions → public chat (normalised username), `MentionName` → user id, URL →
`telegramLink()` first (so `t.me`/`tg://` are routed in-app instead of to a browser) and
then `http`/`https`/`mailto`/`tel`. Spoiler-covered links are excluded from both tapping
and the copy list.

### 3.9 Security model to reproduce

1. **A bot never supplies a URL that is opened directly.** Inline-keyboard Web App buttons
   go through `openWebApp`; reply-keyboard and inline-result buttons through `getWebAppUrl`.
   The TDLib-returned URL is re-validated as https before any WebView sees it.
2. **Mini App URL policy**: https only, no `user:pass@`, host required, port 1–65535;
   `javascript:`, `file:`, `content:`, `intent:`, `tg:` rejected (`BotWebApp.kt:79–94`).
3. **Navigation**: main-frame navigation must stay https and, when TDLib sets
   `requireSameOrigin`, stay on the launch origin. Iframes obey the same rule; blocked
   navigation is reported.
4. **WebView hardening**: JavaScript and DOM storage on; file access, content access,
   database, geolocation, multiple windows and JS-opened windows off; mixed content never;
   Safe Browsing on; downloads blocked; SSL errors cancelled; client certificates and HTTP
   auth cancelled; permission requests denied; renderer crash closes the screen.
5. **Bridge**: no `addJavascriptInterface`. A fixed document-start shim exposes
   `TelegramWebviewProxy.postEvent`; messages arrive through an origin-restricted
   `WebMessageListener` for the **exact launch origin** and **main frame only**, with a
   document-generation guard so a previous document's callbacks cannot fire after a
   navigation. Only an allowlisted event set is parsed; event type ≤128 chars, data
   ≤64 KiB, envelope ≤96 KiB; oversized/malformed payloads are dropped.
6. **No bot string is ever executed as JavaScript.** The only injected script is the
   constant shim; native→JS payloads are the theme JSON, `main_button_pressed` and
   `back_button_pressed`.
7. **Data to the bot** is validated locally (empty/oversize/invalid JSON) before
   `sendWebAppData`, and TDLib errors are mapped without echoing payload content.
8. **Deep links** are fail-closed, length-capped, whitespace/control-rejected, with a
   reserved-path list, strict query parsing, and `tg://login` never persisted. The pending
   inbox stores only validated actionable links and binds them to one account.
9. **Mini App launch URLs carry Telegram's signed `initData`** (the WebApp query/fragment
   returned by TDLib). Android never logs or persists them; Windows must hold the same
   rule, including a redaction rule (see §5, row 30).

---

## 4. Windows current status

Windows has **no bot, inline-mode, Mini App, or deep-link feature**. What exists is only
adjacent plumbing:

| Area | Windows today | Evidence |
|---|---|---|
| Outgoing markup | Every send hard-codes `reply_markup: null` | `src/telegram/TelegramRequests.cpp:42`, `:53`; `src/telegram/MediaSend.cpp:136`; `src/telegram/PollInfo.cpp:113` |
| Message model | No reply markup, no inline keyboard, no bot sender flag | `src/telegram/TelegramTypes.h:30–59`; `src/telegram/MessageListModel.h:15–42` |
| Chat model | Stores only the TDLib chat type string (`chatTypePrivate`, …); no bot flag | `src/telegram/TelegramTypes.cpp:370–383`; `ChatData::type` in `TelegramTypes.h:13` |
| Bot identity | `UserProfile.isBot` is parsed but never surfaced to QML (no `profileIsBot`) | `src/telegram/UserProfile.cpp:163–166`; `src/telegram/UserProfile.h:25`; `src/ui/AppController.h:113–122` |
| Bot command text | `textEntityTypeBotCommand` renders as a cosmetic `<span class="bot-command">` only | `src/telegram/TextFormatting.cpp:178–179` |
| Link taps | `openLink` allows http/https/tg/mailto and hands the URL to `QDesktopServices` — `tg://` leaves the app | `src/ui/AppController.cpp:1184–1194`; `qml/components/MessageBubble.qml:270` |
| Username resolution | No `searchPublicChat`, `createPrivateChat` or `getMessageLinkInfo` | absent from `src/telegram/*` |
| Deep links | No parser, no pending inbox, no URL registration, no single-instance forwarding, no `QCommandLineParser` | `src/app/main.cpp:28–121`; no protocol entry in `scripts/`, `dist/`, `.github/` |
| WebView | No Qt WebEngine dependency at all | `CMakeLists.txt:28` (Core, Gui, Quick, QuickControls2, Multimedia, Test, Widgets) |
| Composer | Plain `TextArea`; no command button, inline panel, bot keyboard or placeholder override | `qml/pages/ChatsPage.qml:634–737` |
| Message bubble | Text, link preview and reactions only; no keyboard row area | `qml/components/MessageBubble.qml:108–290` |
| Tests | No bot/deep-link/WebApp tests | `tests/CMakeLists.txt` (53 registered tests, none on this surface) |

Consequence: on Windows a bot chat behaves like any private chat — no command menu, no
custom keyboard, no inline keyboard buttons, no Mini Apps, and `t.me` links open in the
browser instead of the app.

---

## 5. Gap matrix

Legend: **P1** = bot chat is unusable or the gap is security-relevant; **P2** = missing
feature already scheduled by the roadmap; **P3** = optional component or polish.

| # | Capability | Android | Windows | Gap | Priority |
|---|---|---|---|---|---|
| 1 | Bot chat identity | `ProfileKind.Bot` from `userTypeBot` | `UserProfile.isBot` parsed, never surfaced | total | P1 |
| 2 | `/start` for a fresh bot chat | `BotStartBar` | none | total | P1 |
| 3 | Command menu fetch | `getUserFullInfo.botInfo.commands` | none | total | P2 |
| 4 | Command sheet (search, insert-only) | `BotCommandSheet` | none | total | P2 |
| 5 | Reply keyboard render / collapse | `BotReplyKeyboard` | none | total | P1 |
| 6 | Reply keyboard text send + one-time dismiss | `sendMessage` + `deleteChatReplyMarkup` | none | total | P1 |
| 7 | Force-reply placeholder + auto reply target | wired | none | total | P2 |
| 8 | Reply-keyboard Web App button | `getWebAppUrl` | none | total | P3 |
| 9 | Inline keyboard on a message | `inlineKeyboard` on every `ChatMessage` | markup dropped by `MessageData` | total | P1 |
| 10 | Callback button | `getCallbackQueryAnswer` + toast/alert/url | none | total | P1 |
| 11 | URL / copy / user buttons | yes | none | total | P1 |
| 12 | Unsupported button fallback | explicit toast | none | total | P2 |
| 13 | Live markup refresh | `updateMessageReplyMarkup` | none | total | P1 |
| 14 | Inline (`@bot`) parse + panel | yes | none | total | P2 |
| 15 | Inline pagination / dedupe / cursor stop | yes | none | total | P2 |
| 16 | Send inline result | `sendInlineQueryResultMessage` | none | total | P2 |
| 17 | Inline-results Web App button | yes | none | total | P3 |
| 18 | Mini App launch (both TDLib paths) | yes | none | total | P3 |
| 19 | Mini App consent dialog | yes | none | total | P3 |
| 20 | Mini App theme hand-off | yes | none | total | P3 |
| 21 | Mini App bridge (allowlist, origin-bound) | yes | none | total | P3 |
| 22 | Mini App data validation + `sendWebAppData` | yes | none | total | P3 |
| 23 | `closeWebApp` lifecycle | yes | none | total | P3 |
| 24 | WebView hardening / navigation policy | yes | no WebEngine at all | total | P3 |
| 25 | Deep-link parser + router | yes | none | total | P2 |
| 26 | Pending link across sign-in, account binding | yes | none | total | P2 |
| 27 | Protocol registration + single instance | Android intent filters | none | total | P2 |
| 28 | `t.me` entity routing in bubbles | in-app | opens external browser | wrong target | P2 |
| 29 | Username / phone / user / message / invite / proxy resolution | yes | none | total | P2 |
| 30 | Web App URL redaction rule | N/A (never logged) | no `tgWebAppData` rule in `Redaction.cpp` | future leak | P3 (ship with #18) |

Notes on the P1 rows:

- **#1/#2** matter because a fresh bot chat on Windows offers nothing but an empty
  composer; the user cannot even start the bot.
- **#5/#6** matter because many bots answer `/start` with a reply keyboard only; without
  it the conversation dead-ends.
- **#9–#13** matter because inline keyboards are the primary UI of most service bots
  (confirmations, navigation, payments-gated actions). A dropped markup makes those
  messages inert.
- **#30** is a forward-looking rule: the feature does not exist yet, so nothing leaks
  today, but the redaction rule must land in the same change as the Mini App host.

---

## 6. Implementation plan

Sequencing follows the existing Windows layering: pure, unit-testable modules in
`src/telegram/`, then repository wiring, then QML, then deep-link platform integration,
then the optional WebEngine host. New test targets follow `tests/CMakeLists.txt`'s
`beygram_add_test` pattern.

### Step 1 — Pure parsers and policy (P1)

| New file | Content | Mirrors |
|---|---|---|
| `src/telegram/BotMarkup.h/.cpp` | `enum class BotButtonKind { Text, Callback, Url, Copy, User, WebApp, Unsupported }`; `struct BotButton { QString text; BotButtonKind kind; QString value; QByteArray data; }`; `struct BotKeyboard { qint64 messageId; QVector<QVector<BotButton>> rows; bool oneTime; bool persistent; QString placeholder; bool forceReply; }`; `inlineKeyboardFromReplyMarkup(QJsonValue)`, `botKeyboardFromMessage(QJsonObject)`, `parseInlineQuery(QString)`, `struct InlineBotResult`, `inlineResultsFromJson(QJsonObject)`, `mergeInlineResults`, `nextInlineQueryOffset` | `TdMessageMarkup.kt:97–119`, `BotFeatures.kt:17–62` |
| `src/telegram/WebAppUrl.h/.cpp` | `normalizedWebAppUrl(QString) -> std::optional<QUrl>`, `webAppOrigin`, `webAppOriginRule`, `isAllowedWebAppNavigation(launch, target, requireSameOrigin)`; explicit rejection of userinfo, non-https, port 0 and control characters | `BotWebApp.kt:79–129` |
| `src/telegram/BotWebAppData.h/.cpp` | `enum class BotWebAppDataError { None, Empty, TooLarge, InvalidJson }`; UTF-8 size via `QString::toUtf8().size()`; JSON completeness via `QJsonDocument::fromJson` plus a no-trailing-bytes check | `BotWebApp.kt:37–77` |
| `src/telegram/TelegramDeepLink.h/.cpp` | `struct DeepLink` variant (`Username`, `Phone`, `UserId`, `Bot`, `Invite`, `Message`, `Proxy`, `Login`, `Invalid`), `parseDeepLink(QString)`, `routeDeepLink(...)`; manual path/query parsing as Android does; same reserved-path list, 2048-char cap, duplicate-key and percent-escape rejection | `TelegramDeepLink.kt` |
| `src/telegram/PendingDeepLink.h/.cpp` | `struct PendingDeepLink { QString raw; quint64 token; QString accountId; }`; `class PendingDeepLinkInbox` with `offer/restore/bind/take/clear`; abstract storage interface; JSON-file storage under `AppPaths::configDirectory()` written with `QSaveFile` | `PendingTelegramDeepLink.kt` |

Design notes:

- `data` must stay `QByteArray` — callback payloads are binary and must never round-trip
  through `QString` (see `MessageMarkupTest` "binary callback data and keyboard rows are
  preserved without decoding as text").
- `TelegramDeepLink` must not use `QUrl::fromUserInput` (it guesses schemes and hosts) and
  must reject a `tg://` action with extra path segments, matching Android.
- `WebAppUrl` compares origins as `(scheme, host, port)` with a default port of 443,
  exactly like `webAppOrigin`.

### Step 2 — Repository and request layer (P1)

| File | Change |
|---|---|
| `src/telegram/TelegramRequests.h/.cpp` | Add `searchPublicChatRequest`, `createPrivateChatRequest`, `searchUserByPhoneNumberRequest`, `getCallbackQueryAnswerRequest(chatId, messageId, QByteArray payload)`, `getInlineQueryResultsRequest(botUserId, chatId, query, offset)`, `sendInlineQueryResultMessageRequest(...)`, `openWebAppRequest(...)`, `getWebAppUrlRequest(...)`, `sendWebAppDataRequest(botUserId, buttonText, data)`, `closeWebAppRequest(launchId)`, `deleteChatReplyMarkupRequest(chatId, messageId)`, `getMessageRequest(chatId, messageId)`, `getMessageLinkInfoRequest(url)`, `checkChatInviteLinkRequest(url)` |
| `src/telegram/TelegramTypes.h/.cpp` | `MessageData` gains `QVector<QVector<BotButton>> inlineKeyboard`; `messageFromJson` parses `reply_markup`; add reply-markup-message awareness for chats (or a small `BotChatState`) |
| `src/telegram/MessageListModel.h/.cpp` | New `InlineKeyboardRole` returning a `QVariantList` of rows of `QVariantMap` (`text`, `kind`, `value`, `data` as `QByteArray`), so QML renders without JSON parsing |
| `src/telegram/TelegramRepository.h/.cpp` | Per-chat bot keyboard state + version, `botKeyboard(chatId)`, `botKeyboardChanged(qint64, BotKeyboard)`; refresh on `openChat` and `updateChatReplyMarkup`; apply `updateMessageReplyMarkup`; add `botCommandMenu`, `botCallback`, `inlineQuery`, `sendInlineResult`, `openBotWebApp`, `sendBotWebAppData`, `closeBotWebApp`, `dismissBotKeyboard`, `resolvePublicChat`, `openPrivateChat`, `resolvePhone`, `resolveMessageLink`, `checkInviteLink` |
| `src/telegram/UserProfile.h/.cpp` | Add `bool isInlineBot` (from `userTypeBot.is_inline`) and `QVector<BotCommand> commands` from `userFullInfo.botInfo.commands` |
| `src/ui/AppController.h/.cpp` | Properties `currentChatIsBot`, `profileIsBot`, `profileBotCommands`, `botKeyboard`, `miniAppActive/Title/Domain`, `pendingDeepLink`; invokables `sendBotCommand`, `requestBotCallback`, `requestInlineResults`, `sendInlineResult`, `requestBotWebApp`, `sendBotWebAppData`, `closeBotWebApp`, `dismissBotKeyboard`, `openExternalUrl` |

Scheduling seam: Android routes inline-result sends through `TdStealthScheduler`. Windows
has no scheduler yet, so the send path must at least accept the same "options" argument
shape, so stealth scheduling can be added without touching bot code again.

### Step 3 — Bot chat QML surface (P1)

| New/changed file | Content |
|---|---|
| `qml/components/BotKeyboardBar.qml` (new) | Reply keyboard above the composer: collapse/expand, scroll-capped grid, busy guard, one-time dismiss callback, Web App buttons |
| `qml/components/InlineKeyboard.qml` (new) | Row/column button grid reused by `MessageBubble`; emits `(kind, value, data)` |
| `qml/components/MessageBubble.qml` (edit) | Add an `inlineKeyboard` property and the grid under the text block; keep the existing `onLinkActivated` path |
| `qml/components/BotCommandSheet.qml` (new) | Searchable command list; insert only, never send |
| `qml/components/BotStartBar.qml` (new) | Explicit `/start` action for a fresh bot chat |
| `qml/pages/ChatsPage.qml` (edit) | Host the bars around the composer (currently `:634–737`), bind the placeholder override, wire the message delegate (`:460`) to `InlineKeyboard`, add the command button next to Attach |

Parity points: the sheet never sends; buttons are disabled while a request is in flight;
unsupported kinds show an explicit message instead of doing nothing.

### Step 4 — Inline (`@bot`) panel and send (P2)

| New/changed file | Content |
|---|---|
| `qml/components/BotInlineResultsPanel.qml` (new) | Debounced `Timer` (350 ms), generation counter so stale pages are dropped, pagination with a stop rule, retry, per-result progress, kind icons/labels, 44 px thumbnails |
| `qml/pages/ChatsPage.qml` (edit) | Show the panel above the composer when the draft starts with `@bot`; lock the composer while a result is being sent; clear the draft only if it is still the text that produced the result |

Parity points: a username that is not an inline bot yields no panel and no error; the
literal `@bot query` is never sent alongside a result.

### Step 5 — Deep links and protocol registration (P2)

| File | Content |
|---|---|
| `src/platform/windows/UrlProtocolRegistration.h/.cpp` (new) | Register/unregister an app-specific `beygram:` scheme under `HKCU\Software\Classes`; optional, explicitly user-confirmed `tg:` association; never silently take over `https://t.me`, which would require changing the user's default browser association |
| `src/app/main.cpp` (edit) | `QCommandLineParser` positional URL; `QLocalServer`/`QLocalSocket` single-instance guard that forwards the URL to the running instance and exits |
| `src/ui/AppController.h/.cpp` (edit) | `handleExternalUrl(QString)`, `pendingDeepLink`, invite/proxy confirmation state, routing through the new repository lookups; bind the pending link to the active account before the first lookup and re-check after it |
| `qml/components/DeepLinkPrompt.qml` (new) | Invite preview and proxy confirmation dialogs |
| `qml/pages/SettingsPage.qml` (edit) | Opt-in toggle for link handling with a plain-language explanation of what is registered |

Order of checks must mirror Android: ignore `tg://login`, ignore malformed/over-long input,
store only actionable links, never auto-send a bot `start` payload, never replay a link on
a different account.

### Step 6 — Mini App host behind an option (P3)

| File | Content |
|---|---|
| `CMakeLists.txt` | `option(BEYGRAM_WITH_WEBENGINE OFF)`, `find_package(Qt6 OPTIONAL_COMPONENTS WebEngineQuick)`, hard error only when the option is ON and the component is missing |
| `src/ui/MiniAppController.h/.cpp` (new) | Launch state machine (`Idle → Consent → Opening → Active → Closing`), TDLib call selection by `BotWebAppSource`, `closeWebApp` on teardown, data validation before `sendWebAppData`, theme payload |
| `src/ui/MiniAppPage.h/.cpp` (new) | `QWebEnginePage` subclass: `acceptNavigationRequest` enforcing https plus optional same-origin, certificate errors rejected, `featurePermissionRequested` denied, `createWindow` denied, downloads ignored |
| `src/ui/MiniAppBridge.h/.cpp` (new) | Document-creation `QWebEngineScript` shim plus `QWebChannel` (or `runJavaScript` with a private object) providing `TelegramWebviewProxy.postEvent`; exact origin and main frame only; allowlist `web_app_ready|close|expand|request_theme|setup_main_button|setup_back_button|send_data|data_send|open_invoice`; size caps; document-generation guard |
| `qml/components/MiniAppWindow.qml` (new) | Separate `Window`: title, origin/domain line, reload, close, optional main-button bar, load-error state |
| `src/ui/CMakeLists.txt` | Add the three sources only when WebEngine is available |

Mandatory isolation before shipping: off-the-record `QWebEngineProfile` per launch, no
persistent cookies or cache on disk, DevTools disabled,
`LocalContentCanAccessRemoteUrls`/`LocalContentCanAccessFileUrls` off,
`JavascriptCanOpenWindows` off, strict mixed content, no clipboard/notification/geo/media
permission grants, no downloads, and no bot-supplied string evaluated as script.

Capability reporting: with WebEngine absent, a Web App button must say Mini Apps are
unavailable in this build — never a silent no-op and never an external browser hand-off
(the URL carries signed init data and is meant for a WebView).

---

## 7. Test plan

All new tests are pure C++ (QtTest) and need no session, no WebEngine and no device.
Register each with `beygram_add_test(<name>)` in `tests/CMakeLists.txt`.

| Test | Asserts |
|---|---|
| `tests/tst_bot_markup.cpp` (new) | Callback data preserved byte-for-byte; row order; every button kind; force-reply/one-time/persistent/placeholder; inline-mention boundaries (`@ab`, `@inline.bot`, mid-sentence, multiline query); page merge dedupe keeps per-page query ids; cursor stop rule |
| `tests/tst_webapp_url.cpp` (new) | Accept a clean https URL; reject `http`, `javascript:`, `file:`, `content:`, `intent:`, `tg:`, userinfo, port 0, blank host, whitespace; same-origin with and without the flag; origin-rule string for default and custom ports |
| `tests/tst_bot_webapp_data.cpp` (new) | Empty; 4096-byte boundary in ASCII and multi-byte UTF-8; valid/invalid JSON; plain text accepted; trailing garbage rejected |
| `tests/tst_deeplink.cpp` (new) | Port Android's parser cases: both hosts, `tg://` forms, bot `start` kept but routed to chat only, `tg://user`, invite canonicalisation, phone vs invite, proxy/socks, reserved paths, duplicate query keys, over-long input, `tg://login` → ignore |
| `tests/tst_pending_deeplink.cpp` (new) | Cold-start restore; token scoping; account binding immutability; malformed input does not clear a valid pending link |
| `tests/tst_miniapp_policy.cpp` (new) | Consent/launch/close state machine and event allowlist through a fake page (no WebEngine needed) |
| `tests/tst_telegram_requests.cpp` (extend) | Shape of every new request builder; binary callback payload encoding; `reply_markup` null where appropriate |
| `tests/tst_telegram_types.cpp` (extend) | `reply_markup` → `MessageData::inlineKeyboard` for inline keyboards and `null` |
| `tests/tst_user_profile.cpp` (extend) | `isInlineBot` and `botInfo.commands` parsing, empty command list |

Manual/QA gates (not run in this audit, and not a substitute for the unit tests): bot chat
with a reply keyboard, callback alert vs toast, inline results pagination, Mini App
open/data/close, deep link while signed out, deep link on a second account, and blocked
navigation inside a Mini App.

---

## 8. Priority and sequencing

| Order | Work | Priority | Rationale |
|---|---|---|---|
| 1 | Step 1 pure modules + tests | **P1** | No UI risk, unblocks everything, and the URL/deep-link policy is security-relevant; can land while bot UI is still absent |
| 2 | Step 2 request/repository wiring for keyboards, callbacks, commands | **P1** | Most bot chats are reply-keyboard or inline-keyboard driven; without this the bot surface stays unusable even though plain messaging works |
| 3 | Step 3 reply keyboard, inline keyboard, command sheet, `/start` | **P1** | Completes the "bot chat works" milestone with no new dependencies |
| 4 | Step 4 inline (`@bot`) panel + send | **P2** | Large but self-contained; depends on the same request layer |
| 5 | Step 5 deep links + protocol opt-in | **P2** | Needs platform registration decisions and a settings surface; also unlocks `t.me` entity links in bubbles |
| 6 | Step 6 Mini Apps behind `BEYGRAM_WITH_WEBENGINE` | **P3** | Optional component, largest security review, already deferred by `PORTING_ROADMAP.md` Phase 5 and `PORTING_AUDIT.md` M22 |

Cross-cutting rules for every step:

- Keep the existing `reply_markup: null` behaviour for ordinary sends until a keyboard is
  actually being dismissed, and keep the `sendMessage` shape stable for
  `tests/tst_telegram_requests.cpp`.
- Keep TDLib JSON names in `TelegramRequests` only, and keep parsing in pure functions so
  `beygram_telegram` stays testable without a session.
- Never log a launch URL, callback payload or `initData`; add the redaction rule in the
  same change that introduces the launch path.

### Explicit non-goals

- No Mini App payments or invoice handling; keep Android's explicit "unsupported" message.
- No bot API for creating or managing bots (`botCommandScope`/`setCommands`); this client
  only consumes another bot's commands, exactly as Android does.
- No group/channel command menus (not resolvable with this TDLib surface).
- No automatic `/start` from a deep-link payload.
- No silent external-browser fallback for a Mini App URL.
- No system-wide `https://t.me` handler without explicit user opt-in.

---

## 9. Risks and open questions

1. **TDLib surface.** Android deliberately avoids `getBotCommands`; Windows must confirm
   the pinned TDLib revision (`docs/WINDOWS_TDLIB.md`) still exposes
   `openWebApp`/`getWebAppUrl`/`sendWebAppData`/`closeWebApp` and
   `getCallbackQueryAnswer` before Step 2 is scheduled.
2. **Binary callback data.** The current Windows JSON transport passes payloads as
   `QJsonObject`; `callbackQueryPayloadData` needs base64, and the round-trip must be
   byte-exact (Android's test is the reference).
3. **WebEngine packaging.** Qt WebEngine adds a large dependency and its own Chromium
   update cadence; keeping it behind `BEYGRAM_WITH_WEBENGINE` avoids blocking the bot
   milestone, but the build matrix then has two capability levels to document.
4. **Protocol registration policy.** Claiming `tg:` system-wide competes with Telegram
   Desktop and affects the whole machine; the decision (app-specific scheme only vs
   opt-in `tg:`) needs a product call before Step 5.
5. **Multi-account binding.** Windows has `AccountRegistry`; a pending link must bind to
   one account and never replay elsewhere, otherwise a link could open a chat in the
   wrong account.
6. **Force-reply vs send options.** Android lets force-reply bypass the stealth consent
   prompt; when Windows gains scheduled sending, that interaction must be re-checked
   rather than assumed.
7. **Theme hand-off drift.** Mini App colours are RGB (not ARGB) and come from the active
   palette; the mapping must be shared with `ThemeController` so both clients look alike.
8. **Accessibility and keyboard-only use.** Bot buttons, the command sheet, the inline
   panel and the Mini App window all need focus order, Esc handling and labels; this is a
   desktop-specific addition with no Android source to copy.

---

## 10. Verification status

- No build, CMake configure, Gradle task, unit test or GUI run was performed for this
  audit, so nothing here asserts that the Windows tree compiles or that the Android suite
  passes.
- No device or TDLib integration behaviour was verified. TDLib request/response shapes are
  taken from the Android sources and TDLib's documented JSON names.
- No git command was used; only this document was written.
- The Android file list in §2 is the set actually read. `ChatScreen.kt`,
  `TdLibTelegramRepository.kt` and `BeyGramApp.kt` were read only in the bot/deep-link
  sections named there, not in full.
