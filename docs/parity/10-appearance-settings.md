# 10 — Appearance & settings parity (Android → Windows)

**Scope:** themes, design styles, bubble styles, wallpapers, motion, message text size, list density,
navigation placement, and the settings surface that exposes them.

**Method:** read-only static audit. Every statement below comes from reading the sources listed in
§1. No CMake configure, no build, no `ctest`, no QML run, no emulator/device, no `adb`, no git
command was executed for this document. Nothing here is a test result or a build result; where the
Android project documents its own verification, that is quoted as the Android project's claim, not
as evidence about Windows.

---

## 1. Files reviewed

### Android — data model and persistence

| File | What it defines |
| --- | --- |
| `app/src/main/java/com/beykus/beygram/data/appearance/AppearanceConfig.kt` | The whole appearance value object, defaults, text-size and corner-radius clamps |
| `.../data/appearance/AppearanceSettingsStore.kt` | DataStore file `appearance`, all 16 keys, save/read, legacy dark-theme migration |
| `.../data/appearance/ThemeMode.kt` | `FollowSystem / Light / Dark / Amoled` + `isDark(systemDark)` |
| `.../data/appearance/ThemePreset.kt` | `Warm / Graphite / MaterialYou` |
| `.../data/appearance/DesignStyle.kt` | `Default / Material2 / Material3 / LiquidGlass / Signature` |
| `.../data/appearance/GlassQuality.kt` | `Auto / High / Balanced / Lite` |
| `.../data/appearance/BubbleStyle.kt` | `Rounded / Soft / Square` |
| `.../data/appearance/ListDensity.kt` | `Compact / Comfortable / Spacious` |
| `.../data/appearance/MessageLineSpacing.kt` | `Compact 1.2 / Normal 1.375 / Relaxed 1.6` |
| `.../data/appearance/NavigationPlacement.kt` | `Auto / Pinned / Floating` + `forced` |
| `.../data/appearance/ChatWallpaper.kt` | `None / Solid / Gradient / Preset / Photo`, encode/decode, choice lists, intensity and dim bounds |
| `.../data/appearance/WallpaperPresets.kt` | 16 built-in presets, 8 motifs, light/dark/ink triplets |
| `.../data/appearance/WallpaperFiles.kt` | Photo import (1600 px long side, JPEG 85, EXIF orientation), delete-all-except, load |
| `.../data/appearance/MessageAnimation.kt` | Presets, easing, direction, tuning, config, versioned encode/decode (v2 27 parts, v1 11 parts) |
| `.../data/appearance/DesignStartupGuard.kt` | Crash-loop guard that gives up a non-Default style after one unfinished start |

### Android — theme tokens, design system, screens

| File | What it defines |
| --- | --- |
| `ui/theme/Color.kt` | `BeyGramTokens` (incl. `bubbleText`, `bubbleTime`), light/dark Warm and Graphite sets, container ramp |
| `ui/theme/Theme.kt` | Token → Material colour scheme, shape scale, type scale, `LocalMessageTextSize`, `LocalMessageLineSpacing` |
| `ui/theme/ThemePresets.kt` | Preset × mode → tokens, AMOLED override |
| `ui/theme/ThemeOverrides.kt` | Accent derivation, picked-colour validation, `contrastIssues`, 4.5 / 3.0 thresholds |
| `ui/theme/DynamicTokens.kt` | `MaterialYou` colours from the system wallpaper |
| `ui/design/DesignSystem.kt` | Shapes, spacing, surfaces, motion profile, components, FAB, main-screen hierarchy, bubble geometry, glass, nav placement resolution |
| `ui/design/DesignPresets.kt` | The five concrete `DesignSystem` values and `resolveGlassQuality` |
| `ui/design/DesignResolver.kt` | Resolution from saved config + device capability |
| `ui/screens/theme/ThemeSettingsScreen.kt` | Page stack, shared draft, Apply bar, undo, discard, photo cleanup |
| `ui/screens/theme/AppearanceHubScreen.kt` | Overview cards and summaries |
| `ui/screens/theme/AppearanceStyleScreen.kt` | Style carousel, preview, navigation placement, glass quality |
| `ui/screens/theme/AppearanceColorsScreen.kt` | Palette, brightness, colour sample |
| `ui/screens/theme/AppearanceMessagesScreen.kt` | Bubble shape, radius, text size, line spacing, density, reset |
| `ui/screens/theme/AppearanceWallpapersScreen.kt` | Patterns / colours / photo tabs, dim, blur |
| `ui/screens/theme/AppearanceAnimationsScreen.kt`, `MotionSettingsSection.kt` | Message animations + global motion |
| `ui/screens/theme/AppearanceCustomColorsScreen.kt` | Accent, outgoing bubble, message text/time swatches |
| `ui/screens/theme/AppearanceLabels.kt` | Every option's label and description resource |
| `ui/screens/theme/AppearanceDraftLogic.kt`, `AppearanceDraftSaver.kt` | Draft resets, undo coalescing, 17-part draft encoding (v4) |
| `ui/theme/AppearanceViewModel.kt` | In-memory config + write-through save |
| `ui/screens/chat/MessageBubbleLayout.kt` | `bubbleMetricsFor(style, density, radius, styleGeometry)` and density spacing |
| `ui/screens/chat/ChatWallpaperLayer.kt`, `PatternWallpaper.kt` | Wallpaper drawing |
| `ui/motion/MotionSettings.kt`, `data/motion/MotionSettingsStore.kt` | Global motion presets, speed, intensity, disabled categories |
| `data/settings/ChatAnimationSettingsStore.kt` | `animated_emoji`, `message_effects` (default `true`) |
| `ui/screens/settings/SettingsScreen.kt`, `NavigationSettingsScreen.kt` | Entry point (`theme_title` → Appearance) and tab visibility |

Tests read (as specification of expected behaviour): `app/src/test/.../data/appearance/`
`ThemeModeTest`, `ThemePresetTest`, `DesignStyleTest`, `BubbleStyleTest`, `BubbleAppearanceTest`,
`ListDensityTest`, `MessageTextSizePolicyTest`, `MessageLineSpacing`-related tests,
`ChatWallpaperTest`, `ChatWallpaperBlurTest`, `MessageAnimationConfigTest`,
`AppearanceMigrationTest`, `DesignStartupGuardTest`.

Android documentation read: `docs/theme-engine.md`, `docs/appearance-redesign.md`,
`docs/design-system-v2.md`, `docs/beygram-signature.md`, `docs/liquid-glass.md`,
`docs/accessibility-audit.md`.

Reference item in the porting backlog: `docs/PORTING_AUDIT.md` — `M16 | Theme engine and design
system` (priority **P2**), plus the appearance line in `docs/WINDOWS_FOUNDATION.md`.

### Windows — implementation

| File | Role today |
| --- | --- |
| `src/ui/ThemeController.h` / `.cpp` | 16 colour tokens, 4 modes, 2 presets, AMOLED override, text-contrast repair, QML singleton `BeyGram.Theme 1.0 BeyGramTheme` |
| `src/ui/AppearanceStore.h` / `.cpp` | JSON store `appearance.json` with **7** fields, validation, clamping, `QSaveFile`, `changed()` |
| `src/app/main.cpp` | Loads the store, pushes preset/mode into the theme, mirrors theme changes back into the store, registers QML singletons |
| `qml/pages/SettingsPage.qml` | Two chip rows (preset, brightness) plus notifications/desktop/about cards |
| `qml/Shell.qml` | Fixed 268 px left sidebar; no bottom navigation, no placement axis |
| `qml/components/MessageBubble.qml` | Hard-coded radius 20 / tail 6, text `pixelSize: 15`, `text`/`muted` colours |
| `qml/components/WallpaperPicker.qml` | 15 preset swatches (`blue-notes` is missing) + intensity slider — **not instantiated anywhere** |
| `qml/components/ChoiceChip.qml`, `Card.qml`, `NavButton.qml` | The only themable primitives |
| `src/core/ColorContrast.h` / `.cpp` | WCAG ratio, 4.5 text / 3.0 accent constants, `readableOn` |
| `tests/tst_appearance_store.cpp` | 7 cases over the 7 stored fields |
| `tests/tst_theme.cpp` | 5 cases: Warm light tokens, toggle, AMOLED, Graphite accent, text readable on bubbles |
| `docs/WINDOWS_FOUNDATION.md`, `docs/PORTING_AUDIT.md` (M16, P2), `docs/PORTING_ROADMAP.md` | Current claimed Windows status |

---

## 2. Android model: every option, default, range, storage

`AppearanceConfig` is one immutable value. Everything below is persisted in the DataStore file
`appearance` (`preferencesDataStore(name = "appearance")`), one key per field, written by
`AppearanceSettingsStore.save()` on Apply.

| Field | Options / range | Default | Storage key & encoding | Unknown-value behaviour |
| --- | --- | --- | --- | --- |
| `preset` | `Warm`, `Graphite`, `MaterialYou` (the last only shown when system colours are available) | `Warm` | `theme_preset` = enum name | `Warm` |
| `mode` | `FollowSystem`, `Light`, `Dark`, `Amoled` | `FollowSystem` | `theme_mode` = enum name | `FollowSystem` |
| `bubbleStyle` | `Rounded`, `Soft`, `Square` | `Rounded` | `bubble_style` = enum name | `Rounded` |
| `density` | `Compact`, `Comfortable`, `Spacious` | `Comfortable` | `list_density` = enum name | `Comfortable` |
| `accentColor` | any ARGB, `null` = preset accent | `null` | `accent_color` int; key removed when `null` | absent |
| `outgoingBubbleColor` | any ARGB, `null` = derived | `null` | `outgoing_bubble_color` int; removed when `null` | absent |
| `wallpaper` | `None`, `Solid`, `Gradient`, `Preset(id,intensity)`, `Photo(file,dim,blurred)` | `None` | `chat_wallpaper` = `none` / `solid:<hex>` / `gradient:<hex>:<hex>` / `preset:<id>:<intensity>` / `photo:<dim>:<file>` / `blurphoto:<dim>:<file>` | `None` |
| `messageTextSizeSp` | 12–22 sp, before system font scale | `16` | `message_text_size_sp` int | clamped, else 16 |
| `bubbleCornerRadiusDp` | 4–24 dp, `null` = shape's own radius | `null` | `bubble_corner_radius_dp` int; removed when `null` | clamped, `null` kept |
| `messageLineSpacing` | `Compact 1.2`, `Normal 1.375`, `Relaxed 1.6` | `Normal` | `message_line_spacing` = enum name | `Normal` |
| `messageTextColor` | ARGB, `null` = theme text | `null` | `message_text_color` int; removed when `null` | absent |
| `messageTimeColor` | ARGB, `null` = theme muted | `null` | `message_time_color` int; removed when `null` | absent |
| `messageAnimations` | see §2.4 | enabled, linked, `Fade`, speed 1, intensity 0.55, offset 12 dp, scale 0.94, `Smooth`, springiness 0.4, `Auto` | `message_animations` = versioned `\|`-joined string | defaults |
| `designStyle` | `Default`, `Material2`, `Material3`, `LiquidGlass`, `Signature` | `Default` | `design_style` = enum name | `Default` |
| `glassQuality` | `Auto`, `High`, `Balanced`, `Lite` | `Auto` | `glass_quality` = enum name | `Auto` |
| `navigationPlacement` | `Auto`, `Pinned`, `Floating` | `Auto` | `navigation_placement` = enum name | `Auto` |

Persistence properties:

- Corrupt file → `ReplaceFileCorruptionHandler { emptyPreferences() }`; an `IOException` on read
  emits empty preferences. So a damaged store degrades to defaults, never to a crash.
- Legacy migration: SharedPreferences `appearance` / `dark_theme` (boolean) is copied to
  `theme_mode` **only** while no `theme_mode` exists: `true → Dark`, `false → Light`.
- Setters are all-or-nothing: `save()` writes every key, removes optional keys when the value is
  `null`, and clamps text size and corner radius on write.
- Wallpaper photo files live in `filesDir/wallpapers`, named `wallpaper_<millis>.jpg`; names are
  validated to contain no path separator. `deleteAllExcept(keep)` prunes trial photos on Apply and
  at session end.
- Draft: `AppearanceDraftSaver` stores a positional 17-part list (v4). Drafts of 7–17 parts still
  restore; the new trailing fields (design style, glass quality, placement) default when absent.
  Draft survives Activity recreation; edits reach the app only on Apply; leaving dirty asks first.
- Undo: up to 50 checkpoints, one per discrete edit, exactly one per slider gesture
  (`AppearanceDraftUndoCoalescer`).
- `DesignStartupGuard`: writes an "unfinished start" marker before the first frame, clears it after
  the first frame; after `MAX_UNFINISHED_STARTS = 1` the saved **style** falls back to `Default`
  (and is saved) while colours, wallpaper, accounts and TDLib are untouched.

### 2.1 Themes and brightness

- Palettes: `Warm` (violet accent) and `Graphite` (blue accent), each with light and dark token
  sets; `MaterialYou` derives tokens from the system wallpaper on Android 12+.
- `Amoled` is the dark set with `bg = #000000`, `surface = #121214`, `surface2 = #1C1C20`,
  `line = #26262B` — applied whatever the system setting says.
- `FollowSystem` tracks the system dark flag and flips live; `Light`/`Dark`/`Amoled` override it.
- Brightness is an independent axis from palette and from design style.

### 2.2 Design styles and glass

Five styles, each a full token set (`DesignPresets.kt`):

| Style | Character | Navigation | Bubbles (radius / tail / reply) | Glass |
| --- | --- | --- | --- | --- |
| `Default` | the original BeyGram look; the fallback style | `PillTabs`, pinned | null (keeps legacy 20/6/12) | no |
| `Material2` | 4 dp corners, elevated surfaces, uppercase section labels, rectangular badges, dividers | `ClassicTabs`, pinned | 6 / 2 / 4 | no |
| `Material3` | tonal containers, no shadows, 56 dp search bar, filter chips, FAB with tonal fill | `NavigationBar`, pinned | 18 / 4 / 12 | no |
| `LiquidGlass` | translucent sampled-backdrop glass, light rims, specular highlight | `FloatingGlass`, **floating** | 20 / 8 / 14 | yes |
| `Signature` | ink-and-mark editorial hierarchy, row tiles inset 8 dp, accent reserved for actions | `SignatureMark`, pinned | 18 / 6 / 12 | no |

`GlassQuality` (`Auto`/`High`/`Balanced`/`Lite`) only affects `LiquidGlass`; other styles keep the
value for a later switch back. `Auto` resolves from Android version, window-blur availability,
reduced-transparency and power state; a damaged stored value resolves to `Lite`. Only `High`
samples and blurs the backdrop; `Balanced` keeps tint + rims; `Lite` is opaque and matte.

`NavigationPlacement` is a separate axis: `Auto` keeps the style's natural stance (so an update
never moves the bar), `Pinned` forces the full-width bottom slot, `Floating` forces the capsule.
`resolveNavigationFloating(placement, naturalFloating)` is the single decision point.

### 2.3 Bubbles, density, text

`bubbleMetricsFor(style, density, cornerRadiusDp, designBubbleGeometry)` composes three layers:
design-style geometry → bubble style → explicit user radius → density spacing.

- `Rounded` = legacy metrics (20 / 6 / 12, side inset 14, text inset 12, top 8).
- `Soft` = 14 / 4 / 9 on Default; on a styled base it subtracts 6 / 2 / 3 (floored 4 / 2 / 4).
- `Square` = 8 / 2 / 4 on Default; on a styled base it caps the corner at 8 and tail/reply at 2/4.
- `cornerRadiusDp` replaces only the main corner radius when non-null.
- Density changes message spacing only: `Compact` itemSpacing 4 / listPadding 8, `Comfortable`
  8 / 16, `Spacious` 12 / 20.
- Text size 12–22 sp is applied *before* the system font scale, through `LocalMessageTextSize`;
  line spacing is a multiplier through `LocalMessageLineSpacing`. The chat list and message list
  both consume these.

### 2.4 Message animations and global motion

`MessageAnimationConfig`:

- `enabled` (default `true`), `linkedDirections` (default `true`), plus `shared`, `sent`, `received`
  direction records. Turning the link off restores the last independent values without copying.
- 12 presets: `None`, `Fade` (default), `Slide`, `Spring`, `Pop`, `Flow`, `Drift`, `Quick`, `Tilt`,
  `Sway`, `ComposerLaunch`, `Custom`.
- Tuning: speed 0.5–2×, intensity 0–1, offset 0–32 dp, scale 0.80–1, springiness 0–1, easing
  `Smooth`/`Linear`/`EaseIn`/`EaseOut`/`Expressive`, direction `Auto`/`FromStart`/`FromEnd`/
  `FromTop`/`FromBottom`.
- Each preset declares the controls that actually affect it (`controls()`), so the UI hides
  irrelevant sliders.
- `ComposerLaunch` is outgoing-only; incoming falls back to `Flow` with only the speed preserved.
- Storage: v2 = `version|enabled|linked|shared(8)|sent(8)|received(8)` = 27 parts, numbers formatted
  `%.3f`; v1 = `enabled|sent(5)|received(5)` = 11 parts is still accepted and linked is inferred
  from `sent == received`.

`MotionSettings` (separate store, SharedPreferences `motion-settings`, schema v1):

- Presets `Off`, `Subtle` (1.25×, 50 %), `Balanced` (1×, 100 %), `Expressive` (0.75×, 150 %),
  `Custom`. `Off` keeps speed/intensity so they return when re-enabled.
- Speed 0.5–2, intensity 0–150 %; any manual change switches the preset to `Custom`.
- Seven decorative categories can be disabled together: `Navigation`, `Tabs`, `Panels`, `Lists`,
  `Media`, `Chats`, `Micro`. Functional transitions are not part of that switch.
- Chat animations: `animated_emoji` and `message_effects`, both default `true`, stored in
  SharedPreferences `chat-animations`.

### 2.5 Wallpapers

- `None` = plain theme background (the historical look).
- Solid: 6 colours (`#F3E9DC`, `#DCE9E1`, `#D8E4F2`, `#E8DDF0`, `#2B2622`, `#1F2A33`).
- Gradient: 5 top/bottom pairs (peach, mint, blue, plum, deep teal).
- 16 vector presets with stable ids (`lavender-flow` … `emerald-tide`), 8 motifs (Waves, Dots,
  Orbits, Doodles, Leaves, Arches, Grid, Confetti), three light + three dark colours and an ink
  colour per preset. Pattern intensity default 24, max 45; AMOLED paints the ornament over black.
- Photo: dim 0–70 (default 20), optional blur (Android 12+; older systems darken 15 points more),
  long side 1600 px, JPEG 85, EXIF orientation corrected.

### 2.6 Contrast and readability rules

- Body text ≥ **4.5:1** (`MinTextContrast`); accent ≥ **3:1** against the background
  (`MinAccentContrast`).
- A picked accent derives `accentSoft` (lerp toward bg 0.14 light / 0.22 dark), `onAccent`
  (white or black by higher ratio), `accentInk` (moved toward text until it reads) and `bubbleOut`
  (lerp 0.20 light / 0.28 dark).
- A picked outgoing bubble is kept **only** if the text it would show reads on it; otherwise the
  preset bubble stays.
- A picked message text or time colour is kept only while it reads on **both** the incoming
  (`surface`) and outgoing (`bubbleOut`) bubble; otherwise the theme colour stays.
- In the editor, a bubble or message-colour problem **blocks Apply**; a close accent is only a
  warning.
- Wallpaper date pill is opaque (≈6:1); its edge uses `muted` at 60 % because a hairline vanishes
  on bright photos.
- Read ticks keep the accent; the time colour never changes them.

---

## 3. Windows status today

### 3.1 What works

`ThemeController` reproduces the Android token maths for two presets and four modes:

- 16 `Q_PROPERTY` colours (`bg`, `surface`, `surface2`, `line`, `text`, `muted`, `accent`,
  `accentSoft`, `accentInk`, `onAccent`, `bubbleOut`, `ok`, `okInk`, `warn`, `warnInk`, `danger`).
- `Mode { FollowSystem, Light, Dark, Amoled }` with `isDark`-equivalent logic; live re-apply on
  `QStyleHints::colorSchemeChanged`.
- AMOLED override identical to Android (`#000000`, `#121214`, `#1C1C20`, `#26262B`).
- Contrast repair: message text is moved to a readable value if it fails on `surface` or
  `bubbleOut` (uses `core/ColorContrast`).
- Warm/Graphite hex values match `Color.kt` (checked by reading both tables).
- `toggleDarkMode()` leaves `FollowSystem` and becomes explicit `Light`/`Dark`, matching the
  Android editor's rule that a manual choice wins over the system setting.

`AppearanceStore` is a correct small store for what it models:

- JSON `{"version":1, …}` at `<dataDirectory>/appearance.json`.
- 7 fields: `themePreset` (Warm/Graphite), `themeMode` (4), `designStyle` (5 names accepted),
  `bubbleStyle` (3), `wallpaperPreset` (16 ids + `none`), `wallpaperIntensity` 0–45, `reduceMotion`.
- Unknown/foreign values normalize to the field default; intensity clamps; `changed()` fires once per
  real change; `QSaveFile` keeps the old file on a failed write; `load()` never writes.
- `MaterialYou` is deliberately rejected and falls back to `Warm`.

`SettingsPage.qml` exposes palette and brightness as chips and persists them through
`ThemeController` → `themeChanged` → `AppearanceStore`. `tst_appearance_store.cpp` and
`tst_theme.cpp` cover exactly those behaviours (as source; not executed for this audit).

### 3.2 What is stored but inert

`AppearanceStore` accepts `designStyle`, `bubbleStyle`, `wallpaperPreset`/`Intensity` and
`reduceMotion`, and `main.cpp` loads them — but:

- the store is **not registered** as a QML singleton, so no QML file can read or set them;
- `ThemeController` never receives them; only `themePreset` and `themeMode` are mirrored;
- no C++ or QML consumer reads `designStyle`, `bubbleStyle`, `wallpaperPreset`,
  `wallpaperIntensity` or `reduceMotion` (grep over `src/` and `qml/` finds only the store itself);
- `qml/components/WallpaperPicker.qml` is dead code: no file instantiates `WallpaperPicker`;
- therefore these five values round-trip through JSON and change nothing on screen.

### 3.3 What does not exist

- No design-system layer: no shapes/spacing/surfaces/components/motion/FAB/main-screen/bubble
  geometry tokens, no `designFor()` equivalent, no glass rendering.
- No `MaterialYou` equivalent (documented as Android-only in `ThemeController.h`).
- No bubble style, corner-radius, text-size, line-spacing or density application: `MessageBubble.qml`
  hard-codes radius 20 / tail 6, `pixelSize: 15`, and `text`/`muted`; `ChatsPage.qml` hard-codes
  row heights and font sizes.
- No wallpaper rendering at all (plain `BeyGramTheme.bg`/`surface` canvas).
- No custom colour overrides: no accent picker, no outgoing-bubble picker, no message text/time
  colours, and no `bubbleText`/`bubbleTime` token roles.
- No message entrance animations, no animation presets/tuning, no motion settings, no
  reduced-motion handling (`reduceMotion` has no consumer).
- No navigation placement axis: the desktop shell is a fixed 268 px sidebar; there is no bottom bar
  and no floating/pinned choice.
- No appearance hub, no preview, no draft/Apply/Undo/discard flow, no reset actions, no
  `DesignStartupGuard` equivalent.
- No photo wallpaper import, no solid/gradient choices, no pattern motifs (the unused picker shows
  only dark gradient triplets, omits `blue-notes`, and offers a 0–45 slider).
- No string resources for any of the above; labels are inline English literals.

### 3.4 One correctness finding

`AppearanceStore.h` states that every value is stored by the Android name "so a profile reads the
same on both clients". That is not true of the file: Android uses DataStore Preferences (protobuf)
with snake_case keys (`theme_preset`, `chat_wallpaper`, …), Windows uses a JSON object with camelCase
keys (`themePreset`, `wallpaperPreset`, …) and a different on-disk location. The **enum names** are
shared; the **file format is not**. Cross-client profile import/export would need an explicit shared
schema; the comment should not be read as an existing capability.

---

## 4. Gap matrix

Legend: **—** absent, **S** stored only, **W** wired end-to-end. Parity verdict: ✅ equivalent,
⚠️ partial, ❌ missing.

| Area | Verdict | One-line reason |
| --- | --- | --- |
| Warm/Graphite palettes | ✅ | token values match `Color.kt` |
| Brightness incl. AMOLED | ✅ | four modes, live system follow, AMOLED ramp |
| Design styles (5) | ❌ | name persisted, never resolved or drawn |
| Glass quality / Liquid Glass | ❌ | no glass material or capability probe |
| Bubble style / radius | ❌ | fixed 20 / 6 in `MessageBubble.qml` |
| Text size / line spacing | ❌ | fixed `pixelSize: 15`, no line-height token |
| List density | ❌ | fixed spacing in chat and message lists |
| Custom colours (4 picks) | ❌ | no picker, no `bubbleText`/`bubbleTime`, no gate |
| Wallpapers (preset/solid/gradient/photo) | ❌ | picker exists but is unused; nothing is drawn |
| Message animations | ❌ | no preset/tuning model, no entrance animation |
| Global motion | ⚠️ | one `reduceMotion` bool stored, no consumer |
| Navigation placement | ❌ | desktop sidebar only; axis undefined |
| Appearance hub / draft / Apply / Undo | ❌ | one flat Settings page |
| Contrast rules | ⚠️ | text auto-repair only; no editor-level blocking |
| Crash guard | ❌ | no equivalent |

Detailed matrix:

| Android capability | Windows today | Gap |
| --- | --- | --- |
| Warm / Graphite palettes | **W** | none for these two |
| FollowSystem / Light / Dark / Amoled | **W** | none |
| MaterialYou (system wallpaper colours) | — (rejected → Warm) | platform equivalent needed, or keep documented exclusion |
| Design styles (5) | **S** | resolution + rendering + component library |
| Glass quality + Liquid Glass | **S** | glass material, quality resolution, capability probe |
| Navigation placement (Auto/Pinned/Floating) | **S** | desktop analogue undefined; sidebar is the only layout |
| Bubble style (Rounded/Soft/Square) | **S** | metrics port + `MessageBubble.qml` |
| Bubble corner radius 4–24 dp | — | slider + metrics override |
| Message text size 12–22 sp | — | token/`font.pixelSize` plumbing |
| Line spacing 1.2/1.375/1.6 | — | `lineHeight` plumbing |
| List density (3) | — | message spacing + chat row metrics |
| Custom accent | — | picker + derived roles |
| Custom outgoing bubble | — | picker + contrast gate |
| Custom message text/time colours | — | `bubbleText`/`bubbleTime` roles + gate |
| Wallpaper None/Solid/Gradient/Preset | **S** (preset only) | renderer + light/dark tables + picker wiring |
| Wallpaper photo import + dim + blur | — | import pipeline + file lifecycle |
| Pattern intensity 0–45 | **S** | renderer |
| Message animations (12 presets, linked/split, tuning) | — | data port + QML entrance animations |
| Global motion (presets, speed, intensity, 7 categories) | — (only `reduceMotion`) | store + consumers |
| Animated emoji / message effects | — | toggles + consumers |
| Appearance hub / preview / draft / Apply / Undo | — | new settings surface |
| Contrast blocking + warnings for picks | partial (text auto-repair only) | editor-level checks |
| Design crash guard | — | startup guard |
| Reset messages / custom colours / animations / all | — | draft resets + UI |

---

## 5. Port plan

The plan keeps the existing, working theme path untouched and adds the missing axes one at a time.
Every phase is independently shippable and testable; nothing below requires the Android app or a
device.

### Phase A — Make the store complete and reachable (P1)

Files:

- `src/ui/AppearanceStore.h/.cpp` — extend `AppearanceSettings` with `density`,
  `accentColor` (optional int), `outgoingBubbleColor`, `messageTextSizeSp`,
  `bubbleCornerRadiusDp`, `messageLineSpacing`, `messageTextColor`, `messageTimeColor`,
  `messageAnimations` (string, versioned), `glassQuality`, `navigationPlacement`, plus wallpaper
  kind (`none|preset|solid|gradient|photo`), dim/blur/file. Bump the file `version` to 2 and read
  v1 files forward-compatibly (missing fields take defaults, exactly like Android's enum readers).
  Add setters with the same normalize/clamp/no-op rules, and include every new field in
  `settingsEqual`.
- `src/ui/ThemeController.cpp` — accept the extra roles needed by later phases
  (`bubbleText`, `bubbleTime`, optional accent/bubble/text/time overrides), or split overrides into
  `src/ui/ThemeOverrides.{h,cpp}` and keep `ThemeController` as the palette provider.
- `src/app/main.cpp` — register `AppearanceStore` as a QML singleton (e.g.
  `qmlRegisterSingletonInstance("BeyGram.Appearance", 1, 0, "BeyGramAppearance", &appearance)`),
  and connect `AppearanceStore::changed` → `ThemeController` for preset/mode plus the new axes.
  Replace the current one-way `themeChanged` mirror with a single apply path so QML edits and theme
  edits cannot fight.
- `src/ui/CMakeLists.txt` — new sources.

APIs: `AppearanceSettings` (value type), `AppearanceStore::set*` / `settings()` / `changed()`,
QML properties for every field (read/write), `Q_INVOKABLE resetMessages()`, `resetAll()`.

Tests (`tests/tst_appearance_store.cpp`, extend):

- defaults for all new fields;
- round-trip of every field through a second instance;
- clamps: text 12–22, radius 4–24 (and `null` preserved), intensity 0–45, dim 0–70, animation
  speed/intensity/offset/scale/springiness;
- unknown/foreign enum names fall back per field (including `MaterialYou` → `Warm`);
- v1 JSON file upgrades with defaults for new fields;
- corrupt JSON and wrong types keep defaults;
- no `changed()` for no-op setters;
- optional colours removed from the file when cleared.

### Phase B — Message presentation (P1)

Files:

- `src/ui/AppearanceMetrics.{h,cpp}` — pure port of `bubbleMetricsFor`, `BubbleMetrics`,
  `MessageLineSpacing` multipliers, text-size clamp. This is the cheapest high-value port because it
  is arithmetic with no platform dependency.
- `qml/components/MessageBubble.qml` — replace the literal `radius: 20`, `bottom*Radius: 6`,
  `leftMargin/rightMargin: 12`, `topMargin: 8`, `font.pixelSize: 15` and `lineHeight` with values
  from the metrics singleton; message text uses `bubbleText`, time uses `bubbleTime`.
- `qml/pages/ChatsPage.qml`, `qml/components/ChatRow.qml` — density-driven row heights/paddings and
  avatar size from the design system (or a density table until Phase D lands).
- `qml/pages/SettingsPage.qml` — new "Messages" card: bubble-style chips, radius slider with an
  "as the shape" reset, text-size slider 12–22 with a live sample, line-spacing chips, density chips,
  "Reset messages".

Tests (`tests/tst_appearance_metrics.cpp`, new): port the Android vectors —
`Rounded/Comfortable` = legacy metrics, `Soft`/`Square` deltas with and without style geometry,
density spacing triples, explicit radius override, and text-size clamp boundaries.

### Phase C — Wallpapers (P1)

Files:

- `src/ui/WallpaperPresets.{h,cpp}` — 16 ids, motif enum, light/dark/ink triplets, solid/gradient
  choice lists, intensity/dim bounds (generate mechanically from `WallpaperPresets.kt` so ids stay
  byte-identical).
- `src/ui/WallpaperFiles.{h,cpp}` — import with `QImageReader`, long side 1600, JPEG 85, EXIF
  orientation, safe file names under `<data>/wallpapers`, delete-all-except, load.
- `qml/components/ChatWallpaperLayer.qml` + `PatternWallpaper.qml` — port of the Compose layer:
  solid/gradient fill, 8 motifs drawn with `Canvas`/`Shape`, dim overlay, optional `MultiEffect`
  blur gated by capability.
- `qml/components/WallpaperPicker.qml` — revive: Patterns / Colours / Photo tabs, light and dark
  swatches, intensity slider, dim slider, blur toggle, import via `FileDialog`.
- `qml/pages/ChatsPage.qml` — draw the wallpaper behind the conversation.

Tests (`tests/tst_wallpaper.cpp`, new): port `ChatWallpaperTest` and `ChatWallpaperBlurTest`
encode/decode vectors (including `blurphoto:`), unknown preset id, unsafe file name rejection,
intensity/dim clamps, and import resize/orientation on a generated fixture image.

### Phase D — Design styles, glass, navigation placement (P2)

Files:

- `src/ui/DesignSystem.{h,cpp}` — port of the token structs and `designFor()`;
  `src/ui/DesignPresets.cpp` — the five `DesignSystem` values and `resolveGlassQuality`.
- `src/ui/DesignController.{h,cpp}` or properties on `ThemeController` — expose `navigationVariant`,
  shape/spacing/surface/component/FAB/main-screen/bubble tokens to QML.
- QML: replace hard-coded radii, paddings, elevations, title sizes, chip styles and search/composer
  variants in `Shell.qml`, `ChatsPage.qml`, `SettingsPage.qml`, `Card.qml`, `NavButton.qml`,
  `ChoiceChip.qml`, `MessageBubble.qml` with design tokens. Keep `Default` pixel-identical to today.
- Glass: `qml/components/GlassSurface.qml` using `MultiEffect`/`ShaderEffectSource`; `High` samples
  and blurs, `Balanced` tint + rim only, `Lite` opaque. Add a capability probe in the Windows
  platform layer (composition/blur availability, "reduce transparency" accessibility setting,
  battery/remote-session state) for `Auto`.
- `src/ui/DesignStartupGuard.{h,cpp}` — `QSettings` marker before the first frame, clear on
  `QQuickWindow::frameSwapped`, fall back to `Default` after one unfinished start. Only meaningful
  once non-Default styles render, but cheap to add alongside.
- Navigation placement: decide and document a desktop mapping before coding. Recommendation:
  `Auto` = current sidebar, `Pinned` = full-height sidebar, `Floating` = sidebar drawn as a floating
  rail over the content with margins. Do **not** invent a bottom bar that does not exist on desktop;
  if the product wants one, that is a separate shell decision, not a parity port.

Tests (`tests/tst_design_system.cpp`, new): port `DesignStyleTest` and the design-system tests —
five distinct profiles, `Default` values equal to the legacy constants, glass resolution for each
quality, damaged quality → `Lite`, `resolveNavigationFloating` truth table, and `cornerCap`
behaviour. `tests/tst_design_startup_guard.cpp`: port `DesignStartupGuardTest` (unfinished start,
recovery pending across a failed write, `Default` never falls back).

### Phase E — Custom colours, animations, motion (P2)

Files:

- `src/ui/ThemeOverrides.{h,cpp}` — port `withOverrides`, `readableInk`, `readsOnBubbles`,
  `contrastIssues`; add `bubbleText`/`bubbleTime` to `ThemeController`.
- `src/ui/MessageAnimation.{h,cpp}` — port the data model, `controls()`, validation,
  `flowFallback()`, and the versioned encode/decode (v2 27 parts, v1 11 parts). Keep the exact
  separator, part order and `%.3f` formatting so the stored string is interchangeable.
- QML: entrance animation in `MessageBubble.qml`/`ChatsPage.qml` driven by the resolved direction;
  `ComposerLaunch` uses a positioner from the composer to the bubble; `Flow` fallback for incoming.
- `src/ui/MotionSettings.{h,cpp}` — presets, speed 0.5–2, intensity 0–150, seven categories,
  `Custom` on manual edit; persisted in its own JSON or `QSettings` group.
- Reduced motion: read the Windows "show animations" system setting through the platform layer
  (Qt does not expose it portably); when off, force the master switch's effect without erasing the
  user's saved speed/intensity, mirroring Android's `LocalSystemReducedMotion` warning.
- `qml/pages/SettingsPage.qml` — "Motion" card: master switch, presets, two sliders, category
  switches, animated-emoji/message-effects switches, reset.

Tests: `tests/tst_theme_overrides.cpp` (picked accent derivation, bubble kept only when readable,
text/time kept only when readable on both, blocking vs warning), `tests/tst_message_animation.cpp`
(port `MessageAnimationConfigTest`: v1 and v2 vectors, clamp of every tuning value, ComposerLaunch →
Flow fallback with speed preserved, linked/unlinked round-trip, damaged string → defaults),
`tests/tst_motion_settings.cpp` (preset→values, `Off` keeps values, manual → `Custom`, clamps,
disabled-category round-trip).

### Phase F — Appearance hub, preview, and optional extras (P2/P3)

- `qml/pages/appearance/` — Overview, Style, Colours, Messages, Animations, Wallpapers,
  Custom colours, Preview; one draft object in QML or C++ with Apply/Undo/discard; reset actions
  matching `resetMessages`/`resetCustomColors`/`resetAnimations`/`resetAllAppearance`.
- Draft persistence across window close/reopen is a product decision; Android uses
  `rememberSaveable` + app-level draft. If implemented, mirror the 17-part positional schema so the
  field order and defaults stay identical.
- Optional `MaterialYou` analogue: derive an accent from the Windows accent colour
  (`HKCU\Software\Microsoft\Windows\DWM\ColorizationColor` or `UISettings`) and synthesise the same
  `accentSoft`/`accentInk`/`onAccent`/`bubbleOut` roles. This is a new capability, not a port; keep
  it behind the existing `Warm` fallback.
- Localisation: move the inline English labels into Qt translation catalogs when the settings
  surface grows.

### Test registration

Every new `tests/tst_*.cpp` needs one `beygram_add_test(...)` line in `tests/CMakeLists.txt`
(pattern: lines 22–74) and every new `src/ui/*.cpp` a line in `src/ui/CMakeLists.txt`
(lines 1–21). QML has no automated test harness in this repository today; visual parity should be
checked with a documented manual checklist (light/dark/AMOLED × five styles × three densities) until
a `qmltestrunner` target exists. This document does not assert that such a checklist has been run.

---

## 6. Priority

| Priority | Work | Why |
| --- | --- | --- |
| **P1** | Phase A (complete store + QML exposure + single apply path) | Without it, every other appearance feature is unreachable; it also fixes the "stored but inert" trap and the misleading cross-client comment |
| **P1** | Phase B (bubble style, radius, text size, line spacing, density) | Highest user-visible parity per unit of effort; pure arithmetic, easy to test |
| **P1** | Phase C (wallpaper presets + picker, then photos) | Android users expect their wallpaper to survive; the picker already exists unused |
| **P2** | Phase D (five design styles, glass, placement, crash guard) | Largest effort (M16 in `PORTING_AUDIT.md` is already rated XL/P2); needs a component library and a performance budget for glass |
| **P2** | Phase E (custom colours, message animations, motion) | Depends on A; contrast rules must land before colour pickers are exposed |
| **P3** | Phase F (hub/preview polish, MaterialYou analogue, photo extras, localisation) | Quality-of-life; no functional blocker |

Suggested order of delivery: **A → B → C → E(colours) → D → E(animations/motion) → F**. Colours
come before design styles because the contrast gate protects users from unreadable combinations and
is cheap once the store is complete.

Out of scope / explicitly not claimed:

- No Android build, Windows configure/build, `ctest`, QML run, screenshot comparison, emulator or
  device run was performed for this audit.
- No `adb install`, no package changes, no TDLib data touched.
- Visual fidelity of glass, pattern wallpapers and motion on Windows is unverified until a build and
  a visual checklist exist.
