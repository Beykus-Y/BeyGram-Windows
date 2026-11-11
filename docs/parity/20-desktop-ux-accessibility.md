# 20 — Desktop UX & accessibility parity (Android -> Windows)

**Scope:** navigation model, keyboard shortcuts, focus order and focus visibility, screen-reader
names and semantics, text scaling, contrast, window state, tray behaviour, and the notification
surface as a desktop interaction.

**Method:** read-only static audit. Every statement below comes from reading the files listed in
§1. No CMake configure, no build, no `ctest`, no QML/`qmltestrunner` run, no Narrator/High Contrast
session, no emulator or device, no `adb`, and no git command was executed for this document. Nothing
here is a build or test result. Where Android documents its own verification, it is quoted as the
Android project's claim, not as evidence about Windows.

Legend for severity:

- **S1** — blocks a group of users (keyboard-only or screen-reader users cannot complete a core task).
- **S2** — core task is possible but hard, undiscoverable, or fails WCAG AA.
- **S3** — polish / consistency / future-proofing.

---

## 1. Files reviewed

### Android — navigation, semantics, accessibility support

| File | What it defines |
| --- | --- |
| `app/src/main/java/com/beykus/beygram/navigation/AppRoute.kt` | Full route enum (55 destinations), legacy-route remapping, fallback to `Start` |
| `.../navigation/MainTab.kt` | Four main tabs, `TabVisibility` (Chats cannot be hidden), `MainTabPreferences` |
| `.../BeyGramApp.kt` | Route state (`rememberSaveable`), per-route `BackHandler` chain, adaptive rail selection, reduced-motion provider |
| `.../ui/components/MainTabs.kt` | Bottom bar + rail shared items, `Role.Tab` + `selected`, 48 dp targets, `tabLabelSize` cap, measured dock, `MainTabsScaffold` |
| `.../ui/components/AdaptiveNavigationRail.kt` | 600 dp rail threshold, 88 dp rail, scrollable, `selectableGroup()` |
| `.../ui/components/MainTabTitle.kt` | Tab titles as `heading()` |
| `.../ui/components/SearchField.kt` | Search height policy driven by `fontScale` |
| `.../ui/accessibility/AccessibilitySupport.kt` | 48 dp touch target, spoken-label composition, polite live regions |
| `.../ui/accessibility/MotionPreferences.kt` | System reduce-motion (animator scale + touch exploration), lifecycle + ContentObserver refresh |
| `.../ui/motion/MotionSettings.kt`, `MotionCompose.kt` | Motion presets/categories, resolution that user/system restrictions always win |
| `.../ui/screens/chats/ChatsScreen.kt` | One merged focus stop per chat row, `Role.Button`, unread/muted/secret labels, badge that grows with `fontScale` |
| `.../ui/screens/chat/ChatScreen.kt` | Composer send target + `stateDescription`, `onClickLabel`/`onLongClickLabel`, message selection semantics, chat-info merge |
| `.../ui/screens/chat/MessageActions.kt` | Action sheet roles, radio choices |
| `.../ui/screens/settings/SettingsScreen.kt` | Section headings as `heading()` |
| `.../ui/screens/settings/NavigationSettingsScreen.kt` | Tab visibility switches, locked Chats row, live preview |
| `.../ui/theme/ThemeOverrides.kt`, `Theme.kt`, `DynamicTokens.kt` | `MinTextContrast = 4.5`, accent derivation, readable-on-bubble checks |
| `.../data/appearance/AppearanceConfig.kt` | Message text size 12–22 sp, line spacing, density, motion |
| `.../ui/screens/theme/AppearanceMessagesScreen.kt` | Text-size slider and hint |
| `app/src/main/res/values/strings.xml` | `a11y_*` labels (RU default), format arguments |
| `app/src/test/.../AccessibilitySupportTest.kt`, `TabLabelSizeTest.kt`, `SearchFieldHeightPolicyTest.kt` | Label composition, 48 dp growth, tab-label cap, search-height policy |
| `app/src/androidTest/.../*AccessibilityTest.kt` | Compose semantics tests (recording, forum topics, colour swatches, chat switches) |

Android documentation read as specification: `docs/accessibility-audit.md` (issue #390),
`docs/i18n-qa.md` (issue #228), `docs/design-system-v2.md`, `docs/appearance-redesign.md`,
`docs/notifications.md`.

### Windows — shell, pages, components, platform

| File | Role today |
| --- | --- |
| `qml/Main.qml` | `ApplicationWindow`, default/minimum size, three global shortcuts, ambient wash, loader, two overlays |
| `qml/Shell.qml` | Fixed 268 px rail, six `NavButton`s, `Alt+1..6`, `StackLayout`, no menu bar |
| `qml/pages/ChatsPage.qml` | Fixed 360 px chat pane, folder tabs, chat filter, in-chat search, message list, composer, profile drawer |
| `qml/pages/OverviewPage.qml` | Account status, diagnostics, logout dialog |
| `qml/pages/SettingsPage.qml` | Theme chips, notification checkboxes, shortcut help text, about card |
| `qml/pages/GlobalSearchPage.qml` | Debounced global search, result rows, load-more |
| `qml/pages/ArchivePage.qml` | Archive browser (compiled into the module, not instantiated by any page) |
| `qml/pages/EnvironmentPage.qml`, `LogsPage.qml` | Diagnostics as primary navigation sections |
| `qml/AuthPage.qml` | Staged auth, focus per stage |
| `qml/components/AuthField.qml` | Label + `TextField` + hint |
| `qml/components/ChatRow.qml` | Chat list row: `Item` + `MouseArea`, no focus, no accessible name |
| `qml/components/MessageBubble.qml` | Message: rich text, media, right-click `Menu`, no accessible name or keyboard focus |
| `qml/components/ChatFolderTabs.qml` | Folder strip: `Accessible` on a decorative `Rectangle`, `MouseArea` click only |
| `qml/components/NavButton.qml`, `PrimaryButton.qml`, `SecondaryButton.qml`, `ChoiceChip.qml` | The only shared interactive primitives |
| `qml/components/MediaViewerOverlay.qml`, `MediaPlaybackOverlay.qml` | Full-window overlays with `focus: visible` and Escape |
| `qml/components/ForwardDialog.qml`, `ChatNotificationSettings.qml`, `ProfileEditDialog.qml` | Modal dialogs (only `ForwardDialog` is instantiated today) |
| `qml/components/SelectionToolbar.qml`, `ReactionPicker.qml`, `StickerPicker.qml`, `WallpaperPicker.qml`, `PollBubble.qml` | Carry `Accessible.*`, but are **not instantiated anywhere** |
| `src/ui/ThemeController.h` / `.cpp` | 16 colour tokens, 4 modes, 2 presets, text-on-surface/bubble repair |
| `src/ui/AppearanceStore.h` / `.cpp` | 7 persisted fields, including `reduceMotion` (never read by QML) |
| `src/ui/WindowStateStore.h` / `.cpp` | x/y/width/height/maximized JSON, size clamped, positions kept verbatim |
| `src/app/main.cpp` | Loads window state, applies geometry, saves on `aboutToQuit` |
| `src/ui/AppController.cpp` / `.h` | `QSystemTrayIcon` created lazily in `showNotification()`, no activation handler |
| `src/telegram/NotificationFormatter.h` | `NotificationPayload` already carries `chatId`/`messageId` |
| `src/core/ColorContrast.h` / `.cpp` | WCAG luminance/ratio, 4.5 text / 3.0 accent constants |
| `tests/tst_window_state.cpp`, `tst_colorcontrast.cpp`, `tst_theme.cpp`, `tests/CMakeLists.txt` | Existing coverage; no QML/UI test target |

Windows documentation read as claimed status: `docs/WINDOWS_FOUNDATION.md` (window state, tray
notifications), `docs/PORTING_AUDIT.md` (desktop-first deviations, M13/M16),
`docs/PORTING_ROADMAP.md` (Phase 2 item 8 shortcuts, Phase 3 item 8 accessibility pass, Phase 6 tray,
Phase 7 polish).

### Quantitative snapshot of the QML tree

| Measure | Count |
| --- | --- |
| QML files | 38 |
| `font.pixelSize` literals | 174 |
| `Accessible.*` declarations | 37, in 13 files |
| `MouseArea` declarations | 22, of which 19 use a pointing-hand cursor |
| `Behavior`/`ColorAnimation`/`NumberAnimation` | 25 |
| `ListView` | 7 |
| Dialogs / menus / popups | 9 |
| `qsTr()` calls | 9, all in the unused `SelectionToolbar.qml` |
| Primary navigation sections | 6 fixed |

---

## 2. Android reference model

### 2.1 Navigation and back

- `AppRoute` is a 55-value enum; a restored task maps removed routes to a safe destination
  (`ThemeEditor -> Appearance`, account-manager routes -> `Chats`) and falls back to `Start` for
  anything unknown (`AppRoute.kt:65-73`).
- Four main tabs (`Chats`, `Calls`, `Contacts`, `Settings`), with `Chats` non-hideable; visibility is
  persisted in `SharedPreferences("navigation")` (`MainTab.kt:6-41`).
- A wide window (>= 600 dp) replaces the bottom bar with an 88 dp leading rail, using the **same**
  items, selection state, 48 dp target and `Role.Tab` semantics (`AdaptiveNavigationRail.kt:40-109`,
  `MainTabs.kt:569-672`). The rail scrolls when the window is short.
- Back is explicit and layered: one `BackHandler` per secondary route, each returning to a named
  parent (`BeyGramApp.kt:1574-1596`). The restored route and chat are `rememberSaveable`
  (`BeyGramApp.kt:400`), so process death returns the user to the same screen.
- The scaffold reserves the **measured** navigation height, so a large font scale or a 3-button
  system inset cannot put the last row under the bar (`MainTabs.kt:581-583`, `645-648`).

### 2.2 Keyboard / hardware keyboard

Android's own audit treats hardware keyboard as a **manual acceptance item**, not as shipped code:
Enter sends, Shift+Enter makes a newline, Esc closes search/action bars
(`docs/accessibility-audit.md:113`). The only explicit key handling found in Android source is the
music seek slider (`MusicPlayerSheet.kt:686-699`). The Windows keyboard model must therefore be
designed for desktop, not copied from Android.

### 2.3 Focus order and semantics

The Android audit defines the intended structure (`docs/accessibility-audit.md:24-35`):

- one merged focus stop per chat row (title, preview, time, unread, muted, send state) instead of a
  stop per `Text`; avatars decorative inside the row;
- chat header avatar + title = one "chat info" stop;
- profile rows merge label + value; names and section headers are headings;
- list rows have a button role; delete choices are one radio target per row;
- `selectableGroup()` for tab sets so the selected tab is announced.

Implemented evidence: `ChatsScreen.kt:1269-1291` (merged description, `Role.Button`, selected state
for the open chat), `ChatScreen.kt:2097-2126` (send target with `stateDescription`, and a tap must
not steal text-field focus), `ChatScreen.kt:2717-2750` (`onClickLabel`/`onLongClickLabel`, selection
state), `MainTabs.kt:283-293` (`selectable(role = Role.Tab, selected = ...)`),
`MainTabTitle.kt:38-48` (`heading()`).

### 2.4 Screen-reader names and states

- Every checked screen's icon-only control has a label; visible status text is not duplicated in the
  spoken label (`docs/accessibility-audit.md:36-45`).
- Spoken labels are composed by pure helpers, deduplicating blanks and repeats
  (`AccessibilitySupport.kt:23-56`) and covered by JVM tests.
- Errors and dynamic status use polite live regions so TalkBack announces a new failure without
  moving focus (`AccessibilitySupport.kt:58-62`; applied across chat, list, profile, dialogs).
- Labels live in `strings.xml` as `a11y_*` resources with RU default and EN translation;
  `EnglishResourceCoverageTest` enforces presence and matching format arguments
  (`docs/i18n-qa.md:11-16`).

### 2.5 Text scaling

- Compose applies the system `fontScale` to every `sp` value. The app adds its own message text size,
  12–22 sp, default 16, clamped on read (`AppearanceConfig.kt:14-44`), exposed by a slider
  (`AppearanceMessagesScreen.kt:129-142`).
- Components that cannot simply grow are explicitly compensated: tab labels cap at 1.5x so four
  labels still fit while the icon and TalkBack name carry the tab (`MainTabs.kt:361-369`); search
  field minimum height grows with font scale up to 72 dp (`SearchField.kt:44-58`); unread badges and
  their radius grow with font scale (`ChatsScreen.kt:1313-1319`); the floating dock reserves its
  measured height.
- The appearance work explicitly verifies 1.3x and 2.0x font scale and "display size"
  (`docs/appearance-redesign.md:99`, `docs/accessibility-audit.md:108-109`).

### 2.6 Contrast

- `MinTextContrast = 4.5`, `MinAccentContrast = 3.0`; picked colours are accepted only when they
  read on both bubbles/surfaces, otherwise the theme value is kept (`ThemeOverrides.kt:8-93`).
- No new colour literals were introduced for the accessibility pass; labels reuse palette tokens
  (`docs/accessibility-audit.md:78-82`).
- Contrast is tested for token sets and Material semantic roles
  (`ThemePresetsTest`, `ThemeOverridesTest`, `MaterialSemanticRolesTest`, `BeyGramTokensTest`).

### 2.7 Reduce motion

- Android has no single reduce-motion flag, so the app treats either a zero animator duration scale
  **or** active touch exploration as a reduce request (`MotionPreferences.kt:27-28`), observes both
  live, and provides one `LocalSystemReducedMotion` for the whole tree (`:45-78`).
- Motion resolution applies user settings and the system flag first; a style may only make motion
  calmer, never longer (`MotionCompose.kt:23-41`). Message entrance skips its first offset frame when
  motion is off (`docs/accessibility-audit.md:69-75`).

### 2.8 Window state / configuration changes

Android restores the route, the selected chat, the selected account and the pending share target
across process death, and re-lays-out for width classes instead of restoring pixel geometry
(`BeyGramApp.kt:400`, `:676`). Window size is the platform's concern.

### 2.9 Notifications as an interaction surface

- One MessagingStyle notification per native group; tap routes to the correct chat, including chats
  outside the main list; inline reply sends through the shared client; swipe-away dismisses and calls
  `RemoveNotificationGroup` (`docs/notifications.md:16-30`).
- Previews are gated by both app settings and TDLib's `show_preview`; secret chats and unresolved
  metadata are hidden; lock-screen public version reveals only "new message"
  (`docs/notifications.md:25-26`).

---

## 3. Windows status

### 3.1 Navigation

`Shell.qml` is a fixed 268 px rail with six sections: Chats, Profile, Settings, Network, Logs,
Search (`Shell.qml:135-170`). There is no bottom bar, no adaptive rail, no tab hiding, no Contacts or
Calls section, and no menu bar. "Profile" opens `OverviewPage`, which is an account/diagnostics page,
not the Android user profile; `Network` and `Logs` are diagnostics promoted to primary navigation
where Android keeps diagnostics under Settings.

Section switching is only `Alt+1..6` (`Shell.qml:12-17`) or a mouse click. `StackLayout` keeps every
page alive and hides the inactive ones (`Shell.qml:201-228`), which is correct for state retention
and keeps hidden pages out of the tab chain.

`ArchivePage.qml` is compiled into the module (`CMakeLists.txt:117`) but no page instantiates it; the
archive entry point is missing entirely. The same is true for `SelectionToolbar`, `StickerPicker`,
`ReactionPicker`, `WallpaperPicker`, `PollBubble`, `ProfileEditDialog` and `ChatNotificationSettings`.

### 3.2 Keyboard shortcuts

| Shortcut | Where | Effect |
| --- | --- | --- |
| `StandardKey.Quit` (Ctrl+Q) | `Main.qml:19-22` | `Qt.quit()` immediately, no confirmation |
| Ctrl+R | `Main.qml:23-26` | `BeyGramApp.refresh()` |
| Ctrl+D | `Main.qml:27-30` | Toggle dark mode |
| Alt+1 … Alt+6 | `Shell.qml:12-17` | Select section |
| Enter | `ChatsPage.qml:712-718` | Send / save edit |
| Shift+Enter | implicit in `TextArea` | Newline |
| Esc | `MediaViewerOverlay.qml:15`, `MediaPlaybackOverlay.qml:15`, `ForwardDialog.qml:114-117`, `StickerPicker.qml:97` | Close that surface |

There is no shortcut for search, next/previous chat, reply, edit, delete, copy, forward, jump to
latest, folder switching, or pane cycling. There is no command palette and no menu bar, so shortcuts
are not discoverable through the UI. `SettingsPage.qml:141` documents "Alt+1..5 sections" while the
shell binds six.

`Ctrl+Q` exits without the confirmation Android gives for destructive/back actions; closing the
window also exits, so no background session survives.

### 3.3 Focus order and focus visibility

- The rail buttons are `Button`s and therefore tabbable; page content follows in creation order.
- `ChatRow` is a bare `Item` with a `MouseArea` (`ChatRow.qml:6-21`, `130-136`): not focusable, not
  reachable by Tab, no `Accessible` role.
- `MessageBubble` is a bare `Item`; the only non-pointer interaction is a right-click `Menu`
  (`MessageBubble.qml:326-330`, `333-378`). There is no keyboard path to Reply/Forward/Copy/Edit/
  Delete, and no context-menu key support.
- Folder tabs put `Accessible` on a decorative `Rectangle` and handle clicks with a `MouseArea`
  (`ChatFolderTabs.qml:57-59`, `112-117`), so they are neither focusable nor activatable by keyboard.
- Global search result rows and "Load more" are `MouseArea`-only
  (`GlobalSearchPage.qml:218-224`, `262-271`); the search field itself is focusable and Enter submits.
- The chat-filter clear "×" is a 16 px `Text` with a `MouseArea` (`ChatsPage.qml:191-207`) — no focus,
  and a target well below WCAG 2.5.8's 24x24 CSS px.
- No shared primitive draws a focus indicator: `NavButton` (`:40-51`), `PrimaryButton` (`:27-38`) and
  `SecondaryButton` (`:26-34`) only react to `pressed`/`hovered`. `ChoiceChip` draws only
  selected/hover (`:28-36`). Text fields do draw an accent border on `activeFocus`
  (`AuthField.qml:57-65`, `ChatsPage.qml:184-189`, `GlobalSearchPage.qml:121-127`).
- Nothing sets an initial focus item on launch or after a section switch, so a keyboard user starts
  from an unspecified position.
- `MediaViewerOverlay`/`MediaPlaybackOverlay` set `focus: visible` (`:13`) but are plain `Item`s, not
  `FocusScope`s; whether Tab can escape behind them needs runtime verification.
- The logout dialog sets neither `focus` nor an initial item (`OverviewPage.qml:184-226`), unlike the
  other dialogs which set `focus: true`.

### 3.4 Screen-reader names and semantics

37 `Accessible.*` declarations exist, concentrated in components that are **not reachable**:
`SelectionToolbar`, `ReactionPicker`, `StickerPicker`, `WallpaperPicker`, `PollBubble`,
`ChatNotificationSettings`, `ProfileEditDialog`. Reachable UI coverage is thin:

| Reachable control | Accessible today |
| --- | --- |
| `NavButton` | role + name from `text` (`:17-18`); selected state not exposed |
| `PrimaryButton`/`SecondaryButton` | role + name from `text` (`:14-15` / `:13-14`) |
| `ChoiceChip` | role `RadioButton`, name, `checked` (`:15-17`) |
| `ChatFolderTabs` | role/name/checked on the decorative `Rectangle`, not on an actionable item |
| `ArchivePage` row | `ListItem` + title only (`:119-120`), no preview/time/action |
| `ChatRow` | none |
| `MessageBubble` | none |
| `AuthField` | none: the visible label is a sibling `Text`, the `TextField` only exposes its placeholder |
| Search result rows, "Load more", "×" clear | none |
| Media viewer/playback buttons | only the button text; the media itself has no name/description |
| `Pill`, `StatusRow`, `Card`, headings | none; every title is a plain `Text`, so there is no heading navigation |
| Errors (`chatError`, search errors, startup errors) | plain `Text`, no announcement/live region |

No error is announced when it appears. No `Accessible.description` exists anywhere. No custom
`Accessible.onPressAction` is defined for the `MouseArea`-based rows, so even where a role exists it
cannot be activated from a screen reader's action list.

The UI is hard-coded English: 9 `qsTr()` calls, all in an unused component; `main.cpp` installs no
`QTranslator`. Android ships RU default + EN with a coverage test.

### 3.5 Text scaling and text size

All typography is literal `font.pixelSize` (174 occurrences). There is no app text-scale setting:
`AppearanceStore` has no text-size field, and Settings exposes only preset and brightness
(`SettingsPage.qml:36-100`). Windows display scaling changes the physical size of everything through
DPI, but the Windows "Make text bigger" text-size preference is not applied to explicit pixel sizes,
and a user who needs larger text has no in-app control. Fixed heights (nav 40, buttons 36, chips 32,
composer minimum 42, headers 58/64) do not grow with content either.

The Android message-size setting (12–22 sp) has no Windows counterpart, and `AppearanceStore`
fields `designStyle`, `bubbleStyle`, `wallpaperPreset`, `wallpaperIntensity`, `reduceMotion` are
persisted but never read by QML (grep: no references).

### 3.6 Contrast

The theme repairs body text only: `text` must read on `surface` and `bubbleOut`
(`ThemeController.cpp:145-149`). Measured WCAG 2.1 ratios from the literal palette values in
`ThemeController.cpp` (calculated from the hex constants, not from a running app):

| Pair | Warm light | Warm dark | Graphite light | Graphite dark | Threshold |
| --- | --- | --- | --- | --- | --- |
| `text` on `bg` | 16.17 | 16.00 | 15.59 | 15.57 | 4.5 |
| `text` on `surface` | 17.14 | 14.72 | 16.86 | 13.97 | 4.5 |
| `muted` on `bg` | 5.70 | 6.76 | 5.77 | 6.83 | 4.5 |
| `muted` on `surface` | 6.04 | 6.22 | 6.23 | 6.13 | 4.5 |
| `accentInk` on `accentSoft` | 7.26 | 8.56 | 6.87 | 8.43 | 4.5 |
| `onAccent` on `accent` | 5.58 | 7.28 | 5.67 | 7.81 | 4.5 |
| `danger` on `bg` | 6.20 | 6.57 | 6.08 | 6.38 | 4.5 |
| `warnInk` on `bg` | 6.99 | 12.94 | 6.85 | 12.56 | 4.5 |
| `line` on `surface` | 1.23 | 1.20 | 1.30 | 1.21 | 3.0 (non-text) |

So text tokens are healthy. The weak points are:

- `line` is the **only** boundary of `SecondaryButton`, `ChoiceChip` (unselected) and `Card`; at
  ~1.2:1 it cannot identify the component if the border is doing that job (WCAG 1.4.11). The label
  still identifies the action, so this is S2 rather than S1.
- Focus cannot be perceived at all on custom buttons (3.3), which is the larger WCAG 2.4.7 failure.
- `ChatRow`/`ArchivePage` avatar tints are six hard-coded literals (`ChatRow.qml:33-36`,
  `ArchivePage.qml:28-31`) with white initials. White on `#C2593F` = 4.38, on `#2F8A7B` = 4.16, on
  `#A9731D` = 4.07 — three of six miss 4.5:1 for small text.
- Windows High Contrast mode is not detected or honoured anywhere; the palette is always the
  app-chosen one.
- The contrast tests cover the helper (`tst_colorcontrast.cpp`) and five theme cases
  (`tst_theme.cpp`), not the full token matrix or the boundary tokens.

### 3.7 Window state

`WindowStateStore` persists `{x, y, width, height, maximized}` and clamps size to [640, 10000]
(`WindowStateStore.cpp:19-33`), keeping positions verbatim so negative multi-monitor coordinates
survive (`:80-87`). `main.cpp` restores it and saves on `aboutToQuit` (`:102-115`). Gaps:

1. **No screen validation.** If the monitor that held x/y is gone, the window opens off-screen with
   no way to recover except editing the JSON. Windows apps normally intersect with
   `QScreen::availableGeometry` and pull the window back.
2. **Maximized size leaks into normal size.** While maximized, `window->width()/height()` are the
   maximized dimensions, so the next "restore down" uses a full-screen-sized normal window.
   `normalGeometry()` is not stored.
3. **Minimized close.** `visibility() == Maximized` is false when minimized, so closing while
   minimized silently forgets the maximized state.
4. **Minimum size mismatch.** The store clamps to 640 but QML enforces 980x680 (`Main.qml:12-13`),
   so stored values below the QML minimum are accepted and then overridden.
5. **No session restore.** The app always opens on section 0 with no chat open; Android restores the
   route and chat. Section and current chat are not persisted anywhere.
6. **No DPI/awareness declaration in the repository.** `qt_add_executable(beygram WIN32 ...)` has no
   manifest (`CMakeLists.txt:74`); Qt may apply per-monitor awareness itself, but mixed-DPI
   behaviour is unverified and must not be claimed.
7. No multi-window / detachable chat, no remembered window per account, no fullscreen state.

### 3.8 Tray and notifications

`showNotification()` creates a `QSystemTrayIcon` on first use, sets a tooltip, shows it, and calls
`showMessage()` (`AppController.cpp:643-659`). It never connects `activated`, never adds a context
menu, and never removes the icon on quit (it dies with the controller). Consequences:

- Clicking the tray icon does nothing; there is no "Open BeyGram" / "Quit" menu.
- Clicking a notification does nothing: `NotificationPayload` already carries `chatId` and
  `messageId` (`NotificationFormatter.h:10-16`), but `notificationRequested(title, body)` drops them
  (`TelegramRepository.h:89`, `AppController.cpp:643`), so the click target cannot be recovered.
- Closing the window quits, so notifications stop; there is no close-to-tray or start-with-Windows
  option.
- `NotificationSettings.sound` and `notifyMutedChats` are persisted (`NotificationSettingsStore.h:14-19`)
  but not read by `showNotification()`, which uses only `enabled` and `showPreview`.
- No grouping, avatars, quiet hours, per-chat overrides, inline reply or mark-as-read; Android has
  all of these as shipped behaviour (`docs/notifications.md:16-30`).
- Suppression logic (current chat, outgoing, service, muted) exists and is unit-tested
  (`tst_notification_formatter.cpp`), which is the one part at parity.

### 3.9 Screen-reader plumbing on Windows

Qt Quick exposes items through the Windows UIA bridge only when an assistive client asks for
accessibility; whether Narrator sees the current tree, and whether the custom `MouseArea` rows are
skipped, is a **runtime question**. What the source already guarantees is that the reachable rows
have no accessible object to expose.

---

## 4. Gap register

| ID | Area | Android reference | Windows today | Severity | Priority |
| --- | --- | --- | --- | --- | --- |
| G1 | Keyboard reachability | Every row is one focus stop with a button role (`ChatsScreen.kt:1281-1291`) | Chat rows, messages, folder tabs, search results, archive rows are `MouseArea`-only | S1 | P1 |
| G2 | Focus visibility | Compose ripple + focus indication; 48 dp targets | No focus ring on `NavButton`/`PrimaryButton`/`SecondaryButton`/`ChoiceChip` | S1 | P1 |
| G3 | Shortcut coverage | Desktop-intent list in the audit (Enter/Shift+Enter/Esc) plus navigation | Only Quit/Refresh/Theme/6 sections/Enter/Esc-on-3-surfaces; no search, chat switching, reply, edit, delete | S2 | P1 |
| G4 | Back/Esc consistency | Layered `BackHandler` per route (`BeyGramApp.kt:1574-1596`) | In-chat search and chat filter cannot be closed with Esc; no Alt+Left; Ctrl+Q exits unconfirmed | S2 | P1 |
| G5 | Screen-reader names | Merged labels, icon-only labels, headings, live regions | None on chat rows, messages, auth fields, result rows, clear buttons, media | S1 | P1 |
| G6 | State exposure | `Role.Tab` + `selected`; `stateDescription` on send | Nav/folder selection not exposed; send button has no state; folder tab role is on a non-actionable rectangle | S2 | P1 |
| G7 | Structure | Headings for screen/section titles | Every title is plain text; no heading navigation | S3 | P2 |
| G8 | Dynamic announcements | Polite live regions for errors/status | Errors appear silently; no announcement mechanism | S2 | P2 |
| G9 | Text scaling | System font scale + 12–22 sp message size + component compensation | 174 fixed pixel sizes, no text-size setting, Windows text-size preference ignored | S1 | P1 |
| G10 | High contrast | Palette contrast gates + theme overrides | Windows High Contrast not detected or honoured | S2 | P2 |
| G11 | Contrast of boundaries/avatars | 4.5/3.0 gates on picked colours | `line` boundary 1.2–1.3:1; 3 of 6 avatar tints below 4.5:1 | S2 | P2 |
| G12 | Reduce motion | Live system flag + user presets, instant specs | `reduceMotion` stored but unused; 25 animations ungated; no system watcher | S2 | P2 |
| G13 | Window state | Route + chat restored; layout adapts to width | Off-screen restore possible; maximized normal size lost; no section/chat restore | S2 | P2 |
| G14 | Tray / notifications | Tap-to-chat, inline reply, grouping, quiet hours, per-chat | Inert tray icon, no menu, notification click lost, close quits, 2 settings inert | S2 | P2 |
| G15 | Localization | RU default + EN, coverage test, TalkBack labels localized | Hard-coded English; no translator; `a11y` names would also be English-only | S3 | P3 |
| G16 | Initial focus | Compose sets focus per screen/dialog | No initial focus on launch/section switch; logout dialog sets none | S2 | P2 |
| G17 | Overlay focus containment | Dialogs trap focus and return it | Overlays are `Item`s, not `FocusScope`s; containment unverified | S2 | P2 |
| G18 | Pointer target size | 48 dp policy, tested | Clear "×" ~16 px; dialog close buttons 26–30 px (below 24 CSS px for the "×") | S2 | P2 |
| G19 | Accessibility regression tests | JVM label tests + Compose semantics tests | No QML/UI test target; contrast tests cover only part of the matrix | S2 | P1 |
| G20 | Dead accessible components | — | 7 components with `Accessible.*` are unreachable, masking real coverage | S3 | P3 |

---

## 5. Implementation plan

### Phase A — Keyboard and focus foundation (P1)

Goal: every core task is completable with Tab/Shift+Tab/arrows/Enter/Esc and a visible focus ring.

Files:

- `qml/Main.qml` — register the shortcut table in one place; keep `StandardKey.Quit` but route it
  through a confirm/quit-to-tray decision once Phase F exists.
- `qml/Shell.qml` — `Ctrl+1..6` aliases for sections, `Ctrl+Tab`/`Ctrl+Shift+Tab` cycling,
  `Alt+Left` = back to the chat list, initial focus on the chat filter or chat list.
- `qml/components/FocusRing.qml` (new) — 2 px accent outline, 2 px offset, `visible: control.activeFocus`,
  drawn outside the background so it is never clipped; used by the four button primitives.
- `qml/components/NavButton.qml`, `PrimaryButton.qml`, `SecondaryButton.qml`, `ChoiceChip.qml` —
  add the ring and `Accessible.focusable`.
- `qml/components/ChatRow.qml` — replace `Item`+`MouseArea` with a focusable `ItemDelegate` (or
  `Button` with a custom background), `activeFocusOnTab: true`, `Keys.onReturnPressed`,
  `Keys.onSpacePressed`, arrow-key delegation to the `ListView`.
- `qml/components/MessageBubble.qml` — focusable delegate with Enter = open/play, Menu key /
  Shift+F10 = context menu, Delete = delete (with confirmation), Ctrl+C = copy.
- `qml/components/ChatFolderTabs.qml` — real `TabButton`/`Button` delegates with `Role.PageTab`,
  left/right arrow navigation inside the strip.
- `qml/pages/ChatsPage.qml` — `Ctrl+F` focuses the chat filter, `Esc` clears/closes it; `Ctrl+Shift+F`
  opens in-chat search, `Esc` closes it; `Alt+Up/Down` previous/next chat; `Ctrl+E` edit last
  outgoing, `Ctrl+Shift+R` reply, `Ctrl+J` jump to latest; enlarge the clear target to >= 24 px.
- `qml/pages/GlobalSearchPage.qml` — focusable result rows, Up/Down, Enter activates, "Load more"
  becomes a `Button`.
- `qml/pages/ArchivePage.qml` — same row treatment (or remove the dead page until the archive entry
  point lands).
- `qml/pages/SettingsPage.qml` — fix the "Alt+1..5" text to match the real bindings; add a
  "Keyboard shortcuts" card that lists the table.
- New `src/ui/ShortcutTable.h` / `.cpp` (optional but recommended): one declarative list of
  `{id, sequence, description}`, exposed to QML for the help card and checked for duplicate
  sequences in a unit test. This avoids QML strings drifting from documentation again.

Tests:

- New `tests/tst_shortcut_table.cpp` — no duplicate sequences, every section has a binding, the
  help text is generated from the table.
- New QML test target `tests/qml/tst_keyboard.qml` (see §6) — Tab order over the rail and chat pane,
  `Ctrl+F` focus, `Esc` closes both searches, `Alt+Up/Down` changes the current chat, Enter in a row
  opens the chat.
- Manual: keyboard-only walkthrough of chats -> search -> send -> edit -> delete -> settings.

### Phase B — Screen-reader names, roles and states (P1)

Goal: Narrator reads one meaningful stop per row and every control has a name, role and state.

Note: the exact `Accessible` attached-property names (`ignored`, `selected`, the heading role) depend
on the Qt version in use; confirm them against the Qt 6.8 `Accessible` type before coding, and keep
the fallback described per item.

Files:

- New `src/ui/AccessibleText.h` / `.cpp` — pure label composition ported from
  `AccessibilitySupport.kt`: `composeDescription(parts)`, `composeStatus(base, status)`,
  `chatRowDescription(title, preview, time, unread, muted, status, secret)`,
  `messageDescription(sender, text, time, edited, pending, failed, mediaKind)`. Unit-testable without
  a GUI.
- `src/ui/AccessibleAnnouncer.h` / `.cpp` (new, Qt >= 6.8) — `Q_INVOKABLE void announce(QString)`
  posting `QAccessibleAnnouncementEvent` (guard with `QT_VERSION >= QT_VERSION_CHECK(6,8,0)`; on
  older Qt, fall back to `QAccessibleEvent(ObjectShow)` or no-op). Registered as a QML singleton.
- `qml/components/ChatRow.qml` — `Accessible.role: Accessible.Button` (matching Android's
  `Role.Button` for a chat row), `Accessible.name` from the composed label, `Accessible.selected`,
  decorative avatar `Accessible.ignored: true`.
- `qml/components/MessageBubble.qml` — `Accessible.role: Accessible.ListItem` (or `Button` for
  actionable media), composed name, `Accessible.description` for actions, decorative media ignored,
  `Accessible.onPressAction`.
- `qml/components/ChatFolderTabs.qml` — role `PageTab`, name, `checked`, on the focusable item.
- `qml/components/NavButton.qml` — expose selected state (`Accessible.checked`/`selected`, whichever
  the Qt version supports; verify in the QML test).
- `qml/components/AuthField.qml` — `Accessible.name: control.label` on the `TextField`,
  `Accessible.description: control.hint`; same for the chat filter, in-chat search, global search and
  forward-dialog search fields.
- `qml/pages/ChatsPage.qml` — name for Attach, Send/Save (with state), Close error, Profile, Search,
  pinned banner, typing indicator (`Accessible.name` + `Accessible.liveRegion` equivalent through
  `BeyGramA11y.announce`), and the `×` clear.
- `qml/pages/OverviewPage.qml`, `SettingsPage.qml`, `EnvironmentPage.qml`, `LogsPage.qml` — expose
  screen and section titles with the platform heading role where the Qt version provides it
  (verify the exact `Accessible.role` value in the QML test); otherwise prefix the name so Narrator
  still announces structure.
- `qml/components/MediaViewerOverlay.qml`, `MediaPlaybackOverlay.qml` — names for the media, title
  as heading, Save/Close named, `FocusScope` root.
- `qml/pages/ArchivePage.qml` — full composed row label.
- Wire `BeyGramA11y.announce(...)` for `chatError`, `searchError`, startup errors, send failures and
  incoming typing in the open chat (polite, no focus move).

Tests:

- New `tests/tst_accessible_text.cpp` — port `AccessibilitySupportTest`: blanks and duplicates
  dropped, status not duplicated when visible, chat row includes unread/muted/secret, message state
  words, format arguments preserved.
- QML test `tests/qml/tst_accessible_names.qml` — instantiate each reachable component with sample
  data and assert `Accessible.name` is non-empty and the role is the expected one; assert the
  decorative avatar/initials expose no name.
- Manual Narrator pass (not run here): chat list -> chat -> message actions -> media -> profile ->
  settings, confirm one stop per row and that errors are announced.

### Phase C — Text scaling (P1)

Goal: a user can enlarge all text without clipping, and the choice persists.

Files:

- `src/ui/AppearanceStore.h` / `.cpp` — add `textScale` (default 1.0, clamp 0.85–2.0, step 0.05) and
  `messageTextScale` or a single app scale plus a message size in the same units; bump the file to
  version 2 and keep v1 files loading with defaults (same pattern as the existing damaged-value
  handling).
- New `src/core/UiMetrics.h` / `.cpp` — `scaledPx(base, scale)` with rounding and clamp, plus
  `scaledHeight(base, scale, minimum)` so heights grow with text instead of being multiplied blindly.
- `src/ui/ThemeController.h` / `.cpp` — expose `Q_PROPERTY(double textScale)` and
  `Q_INVOKABLE int px(int base)` (or a `BeyGramTheme.scaled(base)` function) plus
  `Q_INVOKABLE int minHeight(int base)`.
- All 38 QML files — replace `font.pixelSize: N` with `font.pixelSize: BeyGramTheme.px(N)`; replace
  fixed control heights with `Math.max(base, contentHeight + padding)` or `BeyGramTheme.minHeight(N)`.
  Prioritise `Shell.qml`, `ChatsPage.qml`, `ChatRow.qml`, `MessageBubble.qml`, `SettingsPage.qml`,
  `GlobalSearchPage.qml`, `AuthPage.qml`, the four button primitives and the dialog components.
- `qml/pages/SettingsPage.qml` — "Text size" slider 85–200 %, live sample text, Reset; optional
  separate "Message text size" matching Android's 12–22 sp range.
- `src/app/main.cpp` — push the stored scale into `ThemeController` at start and persist changes.

Tests:

- Extend `tests/tst_appearance_store.cpp` — v1 file loads with scale 1.0; v2 round trip; out-of-range
  and non-numeric values clamp/default; no write when unchanged.
- New `tests/tst_ui_metrics.cpp` — rounding, clamping, minimum height growth, scale 1.0 returns the
  original pixel values exactly (so the current look is preserved).
- Manual: 100/125/150/200 % over chat list, chat, composer, settings, search, dialogs, viewer;
  check no clipped labels or overlapping rows at the 980x680 minimum window.

### Phase D — Contrast, focus visibility and High Contrast (P1/P2)

Files:

- `qml/components/FocusRing.qml` (from Phase A) — ring colour must reach 3:1 against `bg`, `surface`
  and `surface2`; use `accentInk` in light and `accent` in dark, or `text` when the accent fails.
- `qml/components/SecondaryButton.qml`, `ChoiceChip.qml`, `Card.qml` — use a boundary token that
  reaches 3:1 (for example `Qt.alpha(BeyGramTheme.text, 0.38)`), or make the fill the affordance and
  keep the hairline purely decorative.
- `qml/components/ChatRow.qml`, `ArchivePage.qml` — replace the six literals with palette-derived
  tints and choose the initials ink with a contrast function instead of hard-coded white.
- `src/ui/ThemeController.h` / `.cpp` — add a high-contrast mode: `Q_PROPERTY(bool highContrast)`,
  resolved palette from system colours or a dedicated token set; `text`, `muted`, `line`, `accent`,
  `onAccent` all re-derived to pass 4.5/3.0.
- New `src/platform/windows/HighContrastWatcher.h` / `.cpp` — `SystemParametersInfo(SPI_GETHIGHCONTRAST)`
  plus `WM_SETTINGCHANGE`/`QEvent::ThemeChange` refresh; emits a signal the controller consumes.
  Register in `src/platform/windows/CMakeLists.txt`.
- New `src/core/ContrastAudit.h` / `.cpp` (or extend `ColorContrast`) — a table of required token
  pairs and their minimum ratios, used by the test and by a debug-only QML page.

Tests:

- Extend `tests/tst_colorcontrast.cpp` — the full matrix from §3.6 for all four palettes, boundary
  tokens at 3.0, avatar tints and chosen ink, High Contrast palette.
- Extend `tests/tst_theme.cpp` — high-contrast toggle changes tokens and still passes the matrix;
  switching back restores the previous palette.
- Manual: Windows High Contrast Black/White, 200 % text scale, and keyboard-only focus walk.

### Phase E — Window state and session restore (P2)

Files:

- `src/ui/WindowStateStore.h` / `.cpp` — store `normalX/Y/Width/Height`, `maximized`, `section`,
  `currentChatId`, `version: 2`; keep v1 loading. Add a pure
  `WindowState ensureVisible(WindowState, QRect available)` that pulls the window fully into the
  nearest screen and shrinks it if the screen is smaller.
- `src/app/main.cpp` — use `window->normalGeometry()` when saving, validate against
  `QGuiApplication::screens()`, restore section/chat after `BeyGramApp.ready`, and align the stored
  minimum with the QML minimum (980x680) or lower `Main.qml`'s minimum to the stored 640.
- `qml/Shell.qml` — `currentSection` from a controller property, written back on change.
- `qml/Main.qml` — keep minimum size in one place (a `BeyGramTheme`/`BeyGramApp` constant) so the
  store and QML cannot diverge.
- Optional `src/platform/windows/WindowPlacement` helper if `QScreen` intersection proves
  insufficient for mixed-DPI monitors.

Tests:

- Extend `tests/tst_window_state.cpp` — off-screen left/right/top/bottom, removed monitor, screen
  smaller than the saved size, maximized with a distinct normal geometry, minimized-close keeps
  maximized, v1 -> v2 migration, damaged fields.
- Manual: two monitors, unplug the secondary, restart; maximize, close, restart, restore down;
  close while minimized.

### Phase F — Tray and notification interaction (P2)

Files:

- `src/telegram/TelegramRepository.h` / `.cpp` — change `notificationRequested` to carry
  `NotificationPayload` (or `chatId`, `messageId`, title, body).
- `src/ui/AppController.h` / `.cpp` — `showNotification(const NotificationPayload&)`, remember the
  last target per tray message, connect `QSystemTrayIcon::messageClicked` to
  `openChat(chatId)` + `raise()`/`requestActivate()`, connect `activated` to show/raise, add a
  `QMenu` with Open / Toggle notifications / Quit, set a tooltip with the unread count, and respect
  `NotificationSettings.sound` and `notifyMutedChats` (or remove those fields from the store).
- `qml/pages/SettingsPage.qml` — "Close to tray" and "Start with Windows" switches once the tray
  exists; explain that closing keeps notifications alive only when the switch is on.
- `qml/Main.qml` / `src/app/main.cpp` — route the window close event through the tray policy
  instead of unconditional quit.
- Optional: `src/platform/windows/TrayController.h` / `.cpp` to keep Windows-specific icon/menu
  code out of `AppController`.

Tests:

- New `tests/tst_notification_target.cpp` — payload -> click target mapping, current-chat/muted/
  outgoing suppression unchanged, no target when the notification is suppressed.
- Extend `tests/tst_notification_settings.cpp` — `sound`/`notifyMutedChats` are either honoured or
  removed from the schema; no field may be stored and ignored.
- Manual: Windows 11 tray overflow, notification click opens the right chat, toast survives a
  minimized window, Quit from the tray performs the normal TDLib close handshake.

### Phase G — Localization readiness (P3)

Files:

- All QML pages/components — wrap user-facing strings in `qsTr()`; `Accessible.name` must use the
  same translated string as the visible label.
- `src/app/main.cpp` — install `QTranslator` from `:/i18n` (or the deployment folder), follow the
  system locale, add a language override in Settings.
- New `translations/beygram_ru.ts`, `beygram_en.ts`; add `qt_add_translations` to `CMakeLists.txt`.
- New `scripts/test_qml_literals.py` — fails when a `text: "..."`/`placeholderText: "..."`/
  `Accessible.name: "..."` literal is not wrapped in `qsTr`, mirroring Android's
  `EnglishResourceCoverageTest` intent. Register it as a CTest (Python is already used by
  `scripts/test_release_notes.py`).

Tests:

- `scripts/test_qml_literals.py` (new CTest) — no unwrapped literals; every `qsTr` key exists in both
  catalogs; format placeholders match.
- Manual: RU/EN, Narrator in both, RTL sanity (Arabic/Hebrew) with `LayoutMirroring`.

### Phase H — Reduce motion (P2)

Files:

- `src/platform/windows/AnimationPreference.h` / `.cpp` (new) — `SPI_GETCLIENTAREAANIMATION` plus
  `WM_SETTINGCHANGE` refresh, mirroring Android's live observer.
- `src/ui/ThemeController.h` / `.cpp` — `Q_PROPERTY(bool motionEnabled)` = user `reduceMotion` AND
  system animations; expose `Q_PROPERTY(bool systemAnimations)` for the settings copy.
- `qml/Main.qml`, `qml/Shell.qml`, `qml/components/*.qml` — gate the 25 `Behavior`/`ColorAnimation`
  instances with `enabled: BeyGramTheme.motionEnabled` (or `duration: BeyGramTheme.motionEnabled ? N : 0`).
- `qml/pages/SettingsPage.qml` — "Reduce motion" switch bound to `AppearanceStore.reduceMotion`
  (already persisted, currently inert) with a note when Windows animations are off.

Tests:

- New `tests/tst_motion_policy.cpp` — `motionEnabled = user && system`; a style cannot re-enable
  motion; toggling either input changes the result; default is on.
- Manual: Windows "Show animations in Windows" off, app restart and live change; verify no colour
  transitions, no message entrance, and no functional playback/animation removed.

---

## 6. Verification plan

No verification was performed for this document. When the phases land, the following are the
smallest honest checks:

### Automated (headless, no account)

| Test | Covers |
| --- | --- |
| `tests/tst_accessible_text.cpp` (new) | Label composition, deduplication, status/state words |
| `tests/tst_shortcut_table.cpp` (new) | Shortcut uniqueness and completeness |
| `tests/tst_ui_metrics.cpp` (new) | Scale arithmetic and clamps |
| `tests/tst_colorcontrast.cpp` (extended) | Full token matrix, focus ring, High Contrast palette |
| `tests/tst_window_state.cpp` (extended) | Off-screen, normal geometry, minimized, v1 -> v2 |
| `tests/tst_notification_target.cpp` (new) | Click target and suppression |
| `tests/tst_motion_policy.cpp` (new) | User/system reduce-motion resolution |
| `scripts/test_qml_literals.py` (new) | Unwrapped UI strings |
| `tests/qml/*.qml` (new target) | Tab order, Esc/Ctrl+F, accessible names on delegates |

A QML test target does not exist today. Add it with `find_package(Qt6 ... QuickTest)` and
`qt_add_qml_module`/`qmltestrunner` in `tests/CMakeLists.txt`, using `QT_QPA_PLATFORM=offscreen` like
the existing C++ tests. Do not claim QML accessibility behaviour from C++ tests alone.

### Manual (required, cannot be inferred from source)

1. Narrator (Ctrl+Win+Enter): chat list -> chat -> message actions -> media viewer -> settings;
   confirm one stop per row, names/states, error announcement, dialog focus return.
2. Keyboard only: Tab/Shift+Tab order, visible focus at every stop, Ctrl+F, Esc in both searches,
   Alt+Up/Down chats, context-menu key on a message, Enter/Space activation.
3. Text scale 100/125/150/200 % at the 980x680 minimum window: no clipped labels, no row overlap, no
   horizontal scrollbar in Settings/Overview.
4. Windows High Contrast Black/White and "Show animations in Windows" off, live where possible.
5. Mixed-DPI dual monitor: move the window, maximize, restart, unplug the secondary monitor.
6. Tray: icon menu, notification click opens the right chat, close-to-tray, Quit handshake.
7. RU/EN with Narrator, RTL layout.

---

## 7. Priority and sequencing

| Priority | Phase | Why |
| --- | --- | --- |
| **P1** | A — keyboard and focus foundation | Without it, keyboard-only users cannot open a chat or a message action at all; it also makes B and D testable |
| **P1** | B — screen-reader names, roles, states | The largest S1 gap: reachable UI has almost no accessible objects |
| **P1** | C — text scaling | A single global accessibility need with no workaround in the app today |
| **P1** | D — contrast and focus visibility (focus ring part) | The focus ring is part of A's acceptance; contrast fixes are cheap and testable |
| **P2** | E — window state and session restore | Prevents an unrecoverable off-screen window and matches Android's route restore |
| **P2** | F — tray and notification interaction | Turns an inert icon into the desktop notification surface Android already has |
| **P2** | G8/G16/G17/G18 — announcements, initial focus, overlay containment, target sizes | Completes the accessibility pass once A/B exist |
| **P2** | H — reduce motion | Already persisted; needs a system watcher and gating |
| **P3** | G — localization readiness | Important for the RU-first product, but no user is blocked today |
| **P3** | G7/G20 — headings and dead-component cleanup | Structure and maintainability |

Suggested order: **A -> B -> D(focus ring/contrast) -> C -> E -> F -> H -> G**, with G8/G16/G17/G18
folded into B and A respectively rather than treated as separate releases.

### Explicitly not claimed

- No Android or Windows build, no CMake configure, no `ctest`, no `qmltestrunner`, no application
  run, no screenshot comparison, no Narrator or High Contrast session, no emulator, no device, no
  `adb install`, and no git command was performed for this audit.
- No TDLib database, application data or account session was touched.
- Contrast ratios in §3.6 were computed from the hex constants in `ThemeController.cpp` with the
  WCAG 2.1 formula; they are arithmetic, not measurements from a rendered window.
- Whether Narrator/UIA exposes a given QML item, whether Tab can escape the media overlays, and
  whether Qt applies per-monitor DPI awareness without a manifest are runtime questions that remain
  unverified.
- The Android `accessibility-audit.md` device checks (TalkBack traversal, gestures, 2.0x font scale,
  RTL) are the Android project's stated remaining work, not evidence about either client.
