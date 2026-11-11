# 06 — Media viewer, playback, voice and video notes

Parity audit of the Android photo/video/audio viewer, its gestures and pager, save/share,
video notes and voice playback against the Windows client.

- **Mode:** read-only audit. No CMake configure, no build, no QML preview, no test run, no device run, no git operations.
- **Consequence:** every "current status" statement below is derived from source reading only. Nothing here
  asserts that the Windows client builds, runs, or renders correctly.
- **Android side:** `app/src/main/java/com/beykus/beygram/**` (Compose, Media3/ExoPlayer).
- **Windows side:** `src/ui/**`, `src/telegram/**`, `qml/**`, `tests/**` (Qt 6 Quick + Qt Multimedia).

| | |
|---|---|
| Audit date | 2026-11-11 |
| Android reference | `app/src/main/java/com/beykus/beygram/{ui/screens/chat, ui/screens/profile, ui/screens/playback, playback, domain/model}` |
| Windows target | `src/ui/MediaViewerController.*`, `src/ui/MediaPlaybackController.*`, `src/telegram/*`, `qml/components/*`, `tests/*` |
| Overall Android status | Complete: paged photo/video viewer with zoom/dismiss, per-page states, shared audio engine, inline voice and video notes, export gating and view-once/secret handling |
| Overall Windows status | Placeholder: one-image viewer with Save As, one ad-hoc `QMediaPlayer` overlay, `Play` buttons for voice/audio/video notes. No pager, zoom, share, download state or inline playback |
| Priority | **P0/P1** — this is part of `M06`/`M08` and the media slice of `M23` in `docs/PORTING_AUDIT.md`; the viewer itself is Phase 2 work and the inline voice/video-note surfaces depend on the shared player |
| Related audits | `docs/parity/14-albums-attachments.md` (album grouping, composer, upload), `docs/parity/18-music-bgmedia.md` (queue, BG Beat/BG Mix, native BG Media) |

Paths are relative to the repository root.

---

## 1. Files reviewed

### 1.1 Android (reference behaviour)

| File | Lines | What it defines |
| --- | ---: | --- |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/MediaViews.kt` | 1721 | `MediaViewer` dialog, `MediaViewerPage`, `ZoomableImage`, `LocalVideo`, `VideoControls`, `AudioInPlayer`, `ProgressiveFileDataSource`, `PhotoPreview`, `DocumentRow`, `MediaPreview` (bubble), `MediaViewerItem`, `MediaViewerActions`, `viewerSwipeKinds`, save/share/open-external helpers |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/MediaViewerPager.kt` | 23 | `mediaViewerSelectedIndex`, `mediaViewerPageKey`, `mediaViewerShouldScrollToSelection` |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/MediaViewerPolicy.kt` | 41 | `concealed`, `secureWhileShowing`, `rightsFor`, `retainViewOncePage`, `MessageRights` |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/MediaViewerTransform.kt` | 27 | `MediaViewerTransform`, `mediaViewerTransform` (thumbnail-origin grow) |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/VideoPlayerState.kt` | 40 | `VideoPlayerState`, `VideoAction`, `afterPlaybackState`, `afterFailure`, `seekPositionMs` |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/VideoNote.kt` | 290 | `VideoNoteBubble` (in-feed circle), `PendingVideoNoteBubble`, `ProgressRing`, `MediaKind.playsInline` |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/VoiceBubble.kt` | 223 | `VoiceBubble`, `resampleWaveform`, `voiceDownloadFailed`, `voicePendingAfterError` |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/SharedMediaWindow.kt` | 121 | Two-sided paged attachment window used by the viewer (prefetch 6, restart/loadNear) |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ViewOnceStealthGate.kt` | 68 | View-once consent gate and `swipeable` page filtering |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ChatScreen.kt` | 2935 | Viewer call site (~L1048–1073, L2180–2227): page list assembly, window paging, `viewerPosition` (N of M), rights, export gating, actions, `originBounds` |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/MessageContextDialog.kt` | 183 | Second viewer entry point (single retained page, no paging) |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/MessageThreadScreen.kt` | 724 | Viewer inside a thread with `retainViewOncePage` |
| `app/src/main/java/com/beykus/beygram/ui/screens/profile/ProfilePhotoGallery.kt` | 169 | Profile gallery viewer (`fullFileOnlyWhenActive`) |
| `app/src/main/java/com/beykus/beygram/ui/screens/profile/SharedMediaSection.kt` | 704 | Shared-media viewer entry point |
| `app/src/main/java/com/beykus/beygram/ui/screens/playback/NowPlayingBar.kt` | 488 | `NowPlayingControls` reused by viewer audio page (seek, play/pause, speed chips) |
| `app/src/main/java/com/beykus/beygram/playback/PlaybackEngine.kt` | 2009 | Shared audio player: queue, focus, becoming-noisy, pause/stop, speed, sleep timer, media session |
| `app/src/main/java/com/beykus/beygram/playback/PlaybackModels.kt` | 530 | `PlaybackState`/`PlaybackItem` shape used by bubbles and viewer |
| `app/src/main/java/com/beykus/beygram/domain/model/MessageMedia.kt` | 111 | `MediaKind`, `MessageMedia`, `MediaFileState`, `AutoDownloadRequest` |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/AttachmentMediaHost.kt` | 48 | Overlay host semantics (`overlayPointerBarrier`) |
| `app/src/test/java/com/beykus/beygram/ui/screens/chat/{MediaViewerPagerTest, MediaViewerPolicyTest, MediaViewerTransformTest, MediaViewerZoomTest, VideoPlayerStateTest, VoiceBubbleStateTest, SharedMediaWindowTest}.kt`, `app/src/test/java/com/beykus/beygram/playback/PlaybackStateTest.kt` | — | Android-side policy/unit coverage for the same rules |

### 1.2 Windows (current implementation)

| File | Lines | What it does today |
| --- | ---: | --- |
| `src/ui/MediaViewerController.h` / `.cpp` | 57 / 220 | Single-file image viewer state: `open(fileId,title)`, `close`, `saveAs(path)`; `open/fileId/title/source/loading/error` |
| `src/ui/MediaPlaybackController.h` / `.cpp` | 70 / 281 | Single `QMediaPlayer` + `QAudioOutput`: local-only `open`, play/pause/toggle/stop/seek, volume clamp, sanitised `error`, exposes `mediaPlayer`/`videoOutput` |
| `src/ui/MediaImageProvider.cpp` | 91 | `image://beygrammedia/<fileId>?v=<revision>`, optional `round=1` circular crop |
| `src/telegram/MediaFileStore.h` / `.cpp` | 50 / 159 | TDLib file download + cached path map; `path`, `isReady`, `revision`, `fileReady`, `revisionChanged` |
| `src/telegram/TelegramTypes.cpp` | 548 | `messageThumbnailFileId`, `messageFullFileId`, `messageContentLabel`, `[Photo]/[Video]/…` fallbacks |
| `src/telegram/MessageListModel.cpp` | 314 | Bubble roles incl. `mediaFileId`, `mediaFullFileId`, `mediaWidth/Height`, `mediaReady`, `mediaRevision` |
| `src/ui/AppController.cpp` (`openMediaPlayback`, `saveOpenMediaAs`) | 1464 | Resolves a full-file id to a local path, waits for `fileReady`, delegates Save As |
| `qml/components/MediaViewerOverlay.qml` | 106 | Full-screen `Image`, click-to-close, Escape, Save as dialog, spinner, error line |
| `qml/components/MediaPlaybackOverlay.qml` | 122 | Full-screen `VideoOutput`, title bar, slider + play/pause + time, error line |
| `qml/components/MessageBubble.qml` | 408 | Thumbnail preview + click routing, `Play` row for voice/audio/video note |
| `qml/pages/ChatsPage.qml` (L460–497) | 982 | Bubble wiring; `onMediaOpenRequested` → viewer, `onMediaPlayRequested` → playback |
| `tests/tst_media_viewer.cpp`, `tests/tst_media_playback.cpp`, `tests/tst_media_image.cpp` | 253 / 219 / 75 | Controller-level tests (see §3.7) |

Not reviewed in depth / out of scope: composer (`MediaComposer.kt`), recorders (`VideoNoteRecorder.kt`,
`AudioNoteRecorder.kt`), photo editor, stories, BG Mix/BG Beat/equalizer, profile shared-media grid.

---

## 2. Android reference behaviour

### 2.1 Entry points and page set

- The viewer is a `Dialog` (`usePlatformDefaultWidth = false`, `decorFitsSystemWindows = false`) with
  `securePolicy = SecureOn` while a secret or view-once page is settled (`MediaViews.kt:958`).
- Opening from a bubble sets `selectedMediaId`; the viewer is rendered only while
  `viewed.media != null` (`ChatScreen.kt:2206`).
- **Page list selection is layered** (`ChatScreen.kt:2198–2202`):
  1. `windowPages` — the paged `SharedMediaWindow` (photos and videos) around the anchor;
  2. `historyPages` — already-loaded history filtered to `viewerSwipeKinds` = `{Photo, Video}`;
  3. single-page fallback — the one opened attachment.
  The first list containing `selectedMediaId` wins.
- Paging: `SharedMediaWindow` keeps messages sorted by id, deduped, `PREFETCH = 6` from each edge,
  one in-flight load at a time, generation counter drops stale responses (`SharedMediaWindow.kt:21–121`).
  `ChatScreen` calls `restart(id)` + `loadOlder()` when the anchor is not yet in the window, then
  `loadNear(id)` (`ChatScreen.kt:1055–1065`).
- **Photos and videos page together**: the window is loaded with
  `SearchMessagesFilterPhotoAndVideo` (`data/tdlib/ChatSearchRequest.kt:39`), so its pages and
  `viewerSwipeKinds` agree. Audio, voice, documents and stickers therefore open through the single-page
  fallback; voice/video notes are `playsInline()` and normally never reach the viewer (`VideoNote.kt:71`).
- Profile galleries pass `fullFileOnlyWhenActive = true`, so neighbours prefetch only thumbnails.
- Subtitle carries the position counter: `time · "N of M"` from `conversation.mediaPosition`
  (`ChatScreen.kt:1072, 2216`); the counter is deliberately *not* cleared while a swipe is pending
  (`ChatScreen.kt:1066–1069`), whereas rights are re-fetched per selected id.
- Title is `"You"` for outgoing, else sender name (falling back to chat title) — not the chat title.

### 2.2 Viewer chrome

- Background `Color(0xFF070B10)`; the whole column is inset by `WindowInsets.safeDrawing`.
- Top row: always-visible `Close`; when `controlsVisible` also title (16 sp semibold, 1 line, ellipsis),
  optional subtitle (12 sp, `0xFFA39C95`), a read-only traffic badge while the selected file is
  downloading (never for secret/secret-chat/view-once/hidden-spoiler), a `Share` button and a
  `More` menu. Top bar tint is `0xB3070B10` while controls are visible, transparent otherwise.
- `More` contains `Open in other app` (when exportable) and `Show in chat` (only when opened from chat).
- Edge paging buttons appear only when `pages.size > 1`: circle `0x80070B10`, 8 dp inset, disabled tint
  alpha 0.3 at the ends, labels `media_previous` / `media_next` (`MediaViews.kt:1015–1026`).
- Footer is a fixed `heightIn(min = 72.dp)` row so the page does not jump when a finished download adds
  `Save`. Actions in order: `Forward`, `Save`, `Edit`, `Delete` (danger `0xFFF97066`), accent `0xFFCFC4FF`.
- `externalError` and `fileNotice` are printed as announced errors below the footer.
- Controls return on every settled page change: `LaunchedEffect(pagerState.settledPage) { controlsVisible = true }`.

### 2.3 Page states (`MediaViewerPage`)

Resolution order (`MediaViews.kt:1154–1235`):

1. `spoilerHidden` → opaque `SafeSpoilerOverlay(seed = fileId)`, tap reveals **only that page**.
2. `concealed` (`isSecret && !active`, or `viewOnce && !active`) → renders nothing at all, even with a file on disk.
3. `tgs` → animated sticker with preview fallback, download button, failure caption.
4. progressive video source → `LocalVideo` from the growing TDLib prefix.
5. image with no local path → `PhotoPreview` (thumbnail + progress/error/retry).
6. video with no local path but a thumbnail → poster + "downloaded/total" text + linear progress + retry.
7. audio/voice with a path → `AudioInPlayer` (`NowPlayingControls`).
8. any other missing file → spinner / size text / retry.
9. image → `ZoomableImage`.
10. video/animation → `LocalVideo`.
11. fallback → size text + `Open file`.

Rules worth copying:

- Only the **active** page plays video/audio, is marked viewed, and may reveal a secret attachment
  (`MediaViews.kt:1044`).
- Viewed receipts: image when `active && !spoilerHidden && localPath != null && image/*`
  (`:1073–1075`); video when playback actually starts (`onStarted`); audio when the shared player reports
  `playing && started` (`:1655–1657`). Opening the viewer alone marks nothing.
- Auto-download: neighbours go through `AutoDownloadRequest` policy; the active page uses the explicit
  `downloadFile` (`:1076–1095`). Hidden/concealed pages are never requested.
- Progressive MP4: a probe (`Mp4StreamingProbe`) decides whether the partial file is playable; a
  `ProgressiveFileDataSource` reads only `downloadedPrefixBytes`, polls every 50 ms, asks for more every
  1 s, throws after 10 s without transfer and 45 s stall (`:1340–1441`).

### 2.4 Gestures, keyboard and accessibility

`ZoomableImage` (`MediaViews.kt:1265–1321`):

| Input | Effect |
| --- | --- |
| Pinch | scale `1f..5f`, pan clamped to the image |
| Drag (scale == 1) | moves the page vertically with the finger |
| Drag release beyond `height / 4` | dismisses the viewer |
| Drag while `scale > 1` | pan only; a scale that leaves 1 cancels the dismiss offset |
| Single tap | toggles chrome (`controlsVisible`) |
| Double tap | zoom to 2.5× **around the tap point**, or back to 1× |
| `+` / `−` buttons (bottom centre, only when chrome visible and page active) | ×1.5 / ÷1.5 around centre |

- Zoom buttons live *above* the image so their taps never toggle chrome or pan the photo.
- Edge paging buttons give TalkBack users a non-swipe path; content descriptions cover close, share,
  more, previous, next, zoom in/out, save, forward, edit, delete, spoiler hint, per-kind media labels.
- No hardware-keyboard handling exists on Android (touch platform). Windows must therefore design its own
  key map (§5.7) rather than "port" one.

### 2.5 Pager semantics (the subtle part)

- `mediaViewerSelectedIndex`: keep the same media id, else index 0.
- `mediaViewerPageKey`: real ids as keys; a synthetic `"unavailable-page-<i>"` key absorbs a stale index
  from a shrinking list instead of crashing.
- `mediaViewerShouldScrollToSelection`: never scroll while `isScrollInProgress`.
- `LaunchedEffect(selectedId)` only corrects the page on a real selection change.
- Selection is published **only when a scroll session ends** (`wasScrolling`), so pages prepended by
  paging never masquerade as a user selection (`MediaViews.kt:896–905`).
- Secure flag follows the *settled page*, not the selected id.
- `retainViewOncePage` re-inserts a view-once page TDLib deleted mid-view, in list order, so the viewer
  does not close under the user (`MediaViewerPolicy.kt:32–40`).

### 2.6 Open/close motion

- `mediaViewerTransform`: with thumbnail bounds, scale follows `origin.width / pageWidth` (min 0.1) and the
  page translates from the thumbnail centre to the screen centre; without bounds it starts at 96 %
  (`1 - 0.04 * travel`) (`MediaViewerTransform.kt:15–27`).
- Alpha equals progress; close waits `viewerSpec.durationMillis` before `onClose`, so the dialog animates
  out first. `MotionToken.MediaExpand` collapses to instant under reduced motion / animations off.

### 2.7 Video playback

`LocalVideo` + `VideoPlayerState` (`MediaViews.kt:1443–1625`, `VideoPlayerState.kt`):

- `ExoPlayer` per page; `REPEAT_MODE_ONE` for animations; `PlayerView` with controller disabled,
  `RESIZE_MODE_FIT`.
- Audio focus: `USAGE_MEDIA` / `CONTENT_TYPE_MOVIE`, `handleAudioFocus = !loop`,
  `setHandleAudioBecomingNoisy(true)` — animations take no focus.
- Page activation: active page plays (if lifecycle ≥ STARTED) and **pauses the shared music player**
  (`PlaybackEngine.pause()`), unless it is a silent animation; an inactive page pauses.
- `ON_STOP` pauses; `ON_START` resumes only silent loops on the active page.
- State machine: `playing/buffering/ended/failed/positionMs/durationMs`; buffering and ended clear each
  other; failure removes spinner and playback.
- Primary action: `Pause` while playing, `Replay` after end, else `Play`; replay seeks to default position.
- Controls: black 50 % bar, play/pause/replay icon, slider enabled only when `durationMs > 0`, time
  `m:ss / m:ss` that follows the drag fraction while dragging, seek committed once on release.
- Position polled every 250 ms while playing.
- Buffering shows a centred white spinner. Error shows `media_video_unsupported`; progressive sources also
  offer `Retry`, which re-prepares and plays.
- Viewed is reported on the first real `onIsPlayingChanged(true)` after a user/activation start, not on
  every return from buffering.

### 2.8 Audio in the viewer and the shared engine (audio focus)

- Opening the viewer never starts audio: `AudioInPlayer` only renders `NowPlayingControls` bound to the
  shared `PlaybackEngine`; play/pause toggles the shared player (`MediaViews.kt:1627–1672`).
- Controls: artwork, title, seek slider (enabled when duration known from player or message), position /
  total, large play/pause, speed chips (`PLAYBACK_SPEED_OPTIONS`), disabled as a whole when the shared
  player holds a different item.
- Shared player (`PlaybackEngine.kt`): one ExoPlayer for music/voice/video-note audio, media session +
  foreground service notification, queue with shuffle/repeat, sleep timer, per-kind speed
  (music vs voice), `pause()` that keeps the file/position, `stop()` that clears the queue and notification.
- Audio focus for the shared player: `setAudioAttributes(USAGE_MEDIA, CONTENT_TYPE_MUSIC,
  handleAudioFocus = true)`; becoming-noisy handled by the player. BG Mix additionally requests
  `AUDIOFOCUS_GAIN` and registers `ACTION_AUDIO_BECOMING_NOISY`, pausing on `LOSS`/`LOSS_TRANSIENT`.
- Starting a video pauses music; starting voice/video-note replaces the shared item; the feed keeps playing
  when the bubble scrolls away (except secret notes, which stop on dispose).

### 2.9 Voice notes (`VoiceBubble.kt`)

- Fixed 244 dp column: 48 dp round button, 40-bar waveform (resampled from `media.waveform`, each bar
  max of its slice), elapsed time while playing (total duration when idle), speed menu, transcription
  section underneath.
- Whole row and button are `combinedClickable` (tap = play/pause, long-press = message actions); gaps
  between button and waveform also start playback rather than opening the viewer.
- Waveform tap seeks: same item → `seekTo(fraction)` (or message-duration fallback), and resumes if
  paused; different item → starts at that position.
- Tap while the file is still downloading records a process-scoped "pending row" and starts automatically
  when the path appears; a download error clears the pending state and shows `voice_download_failed`
  instead of an endless spinner.
- Accessibility: `a11y_voice_message` description plus `loading/playing/paused` state description.
- Transcription (`TranscriptionSection`) shows pending/result/error and rating controls inline.

### 2.10 Video notes (`VideoNote.kt`)

- Idle circle 220 dp on a dark gradient, thumbnail or blurred `minithumbnail` under the frames,
  `VolumeOff` badge while not playing, unseen dot, optional highlight border.
- Tap plays **inline** through the shared player; while playing the circle animates to full screen width,
  shows a progress ring and play/pause overlay for 500 ms, then hides it.
- Remaining time counts down; speed menu; time/status icons row below; transcription section width-locked to
  the circle so incoming notes stay right-aligned.
- Frames come from `PlaybackEngine.attachVideo(row, TextureView)`; audio keeps playing when the frames
  detach (scrolled off), but a **secret** note stops via `PlaybackEngine.stopSecret()` on dispose and the
  window is kept capture-protected while it plays (`SecureWindowWhileVisible`).
- Same pending-download / failure / retry semantics as voice.

### 2.11 Save, share, open-external and export gating

- `exportPath` is non-null only when `canExport && !isSecret && !viewOnce && !hiddenSpoiler` **and** the
  file is local (`MediaViews.kt:929`).
- Every action re-asks the server: `confirmExport(messageId)` → `messageCapabilities(...).canSave` before
  sharing/saving (`:931–933`); the visible button may reflect an older answer.
- `Share`: `ACTION_SEND` with `FileProvider` URI, `clipData`, chooser; failure → `media_share_failed`.
- `Open in other app`: `ACTION_VIEW` with mime; failure → `media_no_app_for_format`; success also marks
  the message viewed.
- `Save`: MediaStore insert with `IS_PENDING`, `RELATIVE_PATH = Pictures/BeyGram | Movies/BeyGram |
  Downloads/BeyGram`, extension/mime reconciled by `resolveExportFileMetadata` + signature sniffing;
  the record is deleted on any failure; toast is `media_saved_gallery` / `media_saved_downloads` /
  `media_save_failed`.
- View-once and secret pages hide Share/Save entirely, and the dialog is `FLAG_SECURE` while they are settled.

### 2.12 Download interplay

- Bubbles and viewer pages use the same `AutoDownloadRequest` policy store (per account, per network);
  explicit taps bypass the policy and always download.
- Download progress is a first-class `MediaFileState` (size, downloaded bytes, `isDownloading`,
  `downloadPath`, `downloadedPrefixBytes`, error, upload bytes/progress).
- Viewer shows a traffic badge while the selected file downloads; bubbles show linear progress and a
  download badge instead of a stretched thumbnail.

---

## 3. Windows current status

### 3.1 `MediaViewerController`

- One file at a time: `open(fileId, title)`, `close()`, `saveAs(targetPath)`.
- `source()` is `image://beygrammedia/<fileId>?v=<revision>`, empty until `MediaFileStore::isReady`.
- `loading()` = open && fileId > 0 && !isReady.
- `saveAs` copies via `QSaveFile` in 256 KiB chunks, refuses the cached source path, refuses while
  downloading, reports one of a fixed set of English error strings.
- No page list, no selected id vs settled page, no per-page state, no message id, no rights, no
  view-once/secret/spoiler, no position counter, no video/audio, no share, no external open,
  no forward/edit/delete/show-in-chat, no download progress, no cancel, no image-decode error.

### 3.2 `MediaViewerOverlay.qml`

- Single `Image` (`PreserveAspectFit`, 34 px margins, `cache: false`, `asynchronous: true`), no
  `sourceSize`, so full-resolution decode happens on the image thread with no cap.
- A full-surface `MouseArea` **behind** the image closes the viewer on any click, including a click that
  was meant for the photo. There is no click-to-hide-chrome.
- Escape closes; there are no arrow/zoom/save shortcuts.
- Chrome: 60 px top bar with title, `Save as`, `Close`; `BusyIndicator` while loading; red error line at
  the bottom.
- `FileDialog` filters only `*.jpg *.jpeg *.png *.webp`; the accepted path goes to
  `BeyGramApp.saveOpenMediaAs`.
- No pager, no zoom/pan, no drag-to-dismiss, no thumbnail hero animation, no footer actions, no share,
  no "N of M", no sender/time, no video.

### 3.3 `MediaPlaybackController`

- One `QMediaPlayer` + `QAudioOutput`, local files only; `http(s)`, `rtsp`, `data`, `qrc` rejected.
- API: `open(url,title)`, `play/pause/toggle/stop/seek`, `volume` (clamped 0..1), `position`, `duration`,
  `playing`, `error`, `mediaPlayer`, `videoOutput`.
- Error strings are stripped of the file path before reaching QML.
- No buffering state, no ended/replay, no speed, no mute, no audio-focus policy, no queue, no
  "one player for the whole app", no background/media-session behaviour.

### 3.4 `MediaPlaybackOverlay.qml`

- Full-screen `VideoOutput` (58 px top / 86 px bottom margins), title bar with `Close`, bottom bar with
  slider, `Play/Pause`, `m:ss / m:ss`, error text.
- Escape calls `BeyGramPlayback.stop()`. **`stop()` does not clear the source**, so `open` stays true and
  the overlay does not close; the same is true for the `Close` button, which also calls `stop()`.
- `Component.onCompleted: BeyGramPlayback.videoOutput = videoOut` passes a `VideoOutput` (a `QQuickItem`)
  to `QMediaPlayer::setVideoOutput(QObject*)`, which Qt 6 documents for `QVideoWidget`,
  `QGraphicsVideoItem` or `QVideoSink`. This likely needs `videoOut.videoSink` instead; it is a
  must-verify item, not a confirmed defect.
- No buffering spinner, no ended/replay state, no seek-preview time, no volume/mute, no speed, no
  fullscreen toggle, no keyboard transport, no audio-only presentation, no auto-close, no focus trap
  beyond `Keys.onEscapePressed`.

### 3.5 `MessageBubble.qml`

- Photo, video, animation, sticker and document-with-thumbnail: an `Image` from `mediaFileId`
  (`PreserveAspectCrop`) with a spinner while `!mediaReady`. A document without a TDLib thumbnail has
  `mediaFileId == 0`, so no media block appears and only the `[Document]` text remains.
- Click routing by `contentType`: `video`, `voice`, `audio`, `video_note`, `animation` →
  `mediaPlayRequested(fullFileId)`; everything else → `mediaOpenRequested(fullFileId)`.
- Voice notes have no thumbnail at all, so their entire bubble is the `Play` row: a `PrimaryButton` plus
  the literal caption `Voice message` (likewise `Video message` / `Audio`).
- Sticker goes through `StickerBubble`; right-click menu offers Reply/Forward/Copy/Edit/Delete but no
  media-specific items (save, open externally, show in chat).
- No waveform, no progress ring, no inline playback, no duration, no speed, no transcription, no
  download progress bar, no download retry, no spoiler cover, no view-once lock, no sender/time on media.

### 3.6 Model and store plumbing

- `MessageData` exposes only `contentType`, `mediaFileId`, `mediaFullFileId`, `mediaWidth/Height`,
  `mediaReady`, `mediaRevision`. No `mimeType`, `fileName`, `size`, `duration`, `waveform`,
  `minithumbnail`, `hasSpoiler`, `isSecret`, `isSecretChat`, `viewOnce`, `isViewed`, `title/performer`.
- `MediaFileStore` has only `path`/`isReady`/`revision`; no progress, no error, no cancel, no
  `downloadPath`, no contiguous-prefix byte count, no per-file state signal.
- No auto-download policy, no traffic accounting, no media position (`N of M`) request.
- `AppController::openMediaPlayback` waits for `fileReady` with a one-shot connection that is never
  cancelled or generation-guarded: opening a second media before the first arrives lets the older
  `fileReady` handler call `playback.open()` later and replace the newer media.
- `AlbumGrid.qml` exists but is referenced nowhere, so albums render as independent bubbles.

### 3.7 Existing Windows tests

- `tests/tst_media_viewer.cpp` — 11 cases: invalid id, wait-for-download, cached source, source appears,
  close resets, saveAs guards (not open / downloading / copy / refuse overwrite), null store, signal count.
- `tests/tst_media_playback.cpp` — 10 cases: defaults, reject empty/remote/missing/directory, accept local
  or report backend error, rejected open closes media, volume clamp, transport before open, signal.
- `tests/tst_media_image.cpp` — image provider.
- No test covers paging, zoom math, selection retention, view-once retention, export gating, video state
  machine, voice/video-note state, or download state.

---

## 4. Gap matrix

Legend: **P0** = blocks credible media UX; **P1** = required for "viewer parity"; **P2** = policy/security;
**P3** = polish.

| # | Area | Android | Windows | Gap | Priority |
| --- | --- | --- | --- | --- | --- |
| 1 | Photo/video pager | Horizontal pager over a two-sided window, stable keys, selection published on settle | None; one file at a time | Full re-implementation needed | P0 |
| 2 | Zoom/pan | Pinch 1–5, clamped pan, double-tap 2.5× at point, ± buttons | None | Missing gestures and math | P0 |
| 3 | Dismiss | Drag-down > ¼ height at scale 1 | Click anywhere closes (also closes on intended photo clicks) | Wrong interaction model | P0 |
| 4 | Keyboard | N/A (touch); a11y prev/next buttons | Esc only; Esc fails to close video overlay | No key map; broken close | P0 |
| 5 | Video in viewer | ExoPlayer page with buffering/ended/failed, slider, replay | Separate rectangular overlay; `setVideoOutput` sink type suspect | No in-viewer video, no state machine | P0 |
| 6 | Playback close | N/A | `stop()` keeps source ⇒ overlay never closes | Visible defect | P0 |
| 7 | Chrome/actions | Title/subtitle + traffic badge + share + more + footer (forward/save/edit/delete) | Title + Save as + Close | Most actions missing | P1 |
| 8 | Save | MediaStore into Pictures/Movies/Downloads, mime/extension repair, success toast | `QSaveFile` copy to a chosen path, no success feedback | Narrow, image-only filters | P1 |
| 9 | Share | `ACTION_SEND` chooser with FileProvider | None | Missing | P1 |
| 10 | Open external | `ACTION_VIEW` | None | Missing | P2 |
| 11 | Download state | bytes/total, progress, error, cancel, prefix | Ready-or-not only | Needs store rework | P1 |
| 12 | Voice notes | Waveform, seek, inline playback, speed, transcription | `Play` button + label | Whole component missing | P1 |
| 13 | Video notes | Inline circle, ring, remaining time, speed, secure | Rectangular overlay | Whole component missing | P1 |
| 14 | Audio focus | Focus + becoming-noisy; video pauses music | Qt defaults only; no ducking/pause rules | Needs explicit policy | P1 |
| 15 | Shared player | One engine, queue, notification, per-kind speed | One ad-hoc player per open | Needs engine | P1 |
| 16 | Secret/view-once/spoiler | Conceal, secure window, export denial, retained page | Not modelled at all | Data + UI + policy | P2 |
| 17 | Viewed receipts | Per-kind, only on real start | None | Missing | P2 |
| 18 | Auto-download policy | Per account/network, viewer-aware | None | Depends on M06 | P2 |
| 19 | Position counter | `N of M` in subtitle | None | Needs repository call | P2 |
| 20 | Hero open/close | Grows from thumbnail bounds, 96 % fallback, reduced motion | No animation | Needs origin plumbing | P3 |
| 21 | Albums | Grouped album bubble | Unused `AlbumGrid.qml` | Wiring only | P2 |
| 22 | Capture protection | `FLAG_SECURE` while secret shown | None | `SetWindowDisplayAffinity` | P3 |
| 23 | Large-image decode | `maxDimension = 4096`, thumbnail-first | Full decode, no `sourceSize` | Memory/jank risk | P1 |

---

## 5. Port plan

### 5.1 Data layer (prerequisite)

1. `src/telegram/MediaFileStore.h/.cpp`
   - Add `struct MediaFileState { qint64 size; qint64 downloadedBytes; bool downloading; bool uploading;
     qint64 uploadedBytes; QString downloadPath; qint64 downloadedPrefixBytes; QString error; }`.
   - Track it from TDLib `updateFile` `local` fields: `downloaded_size`, `download_offset`,
     `downloaded_prefix_size`, `is_downloading_active`, `is_downloading_completed`, `path`. Emit
     `fileStateChanged(int fileId)` and `fileFailed(int fileId, QString reason)`.
   - Add `cancelDownload(int fileId)`, `downloadProgress(int fileId)` and `hasState(int fileId)`.
   - Keep `fileReady`/`revisionChanged` semantics unchanged so existing tests and QML keep working.
2. `src/telegram/TelegramTypes.h/.cpp`
   - Extend `MessageData` with `mimeType`, `fileName`, `fileSize`, `durationSeconds`, `waveform`
     (`QVector<int>`), `minithumbnail` (`QByteArray`), `hasSpoiler`, `isSecret`, `isSecretChat`,
     `viewOnce`, `isViewed`, `audioTitle`, `audioPerformer`.
   - Add `messageMediaKind(content)` returning a `MediaKind` enum shared with QML, and
     `messageHasSpoiler/isViewOnce/isSecret` helpers.
3. New `src/telegram/MediaViewerPages.h/.cpp` (or `MediaWindow`): the `SharedMediaWindow` equivalent —
   `restart(anchor)`, `loadOlder/loadNewer/loadNear`, sorted/deduped, generation-guarded. Reuse the
   existing `MessageSearch` paging call (`getChatMessageByDate`/`searchChatMessages` media filter as
   already used by `MessageSearch.cpp`).
4. New `src/telegram/MediaExport.h/.cpp`: `resolveExportFileName(displayName, mime, kind, signature)`,
   `canExport(capabilities, viewOnce, secret, spoilerHidden)` — pure functions, unit-testable.
5. Position counter: `getChatMessagePosition(chatId, topicId, searchMessagesFilterPhotoAndVideo,
   messageId)` + `getChatMessageCount(chatId, topicId, filter, false)`, returning `total - position + 1`
   of `total` (the Android `mediaPosition` formula), cached per selected page.

### 5.2 Viewer controller

Replace the single-file controller with a page-list model, keeping the current class name as the facade
so `AppController` and tests stay stable:

- `src/ui/MediaViewerController.h/.cpp`
  - `QAbstractListModel* pages()` with roles: `messageId`, `kind`, `fileId`, `thumbnailFileId`, `mimeType`,
    `fileName`, `width`, `height`, `durationSeconds`, `viewOnce`, `isSecret`, `hasSpoiler`, `state`,
    `downloadedBytes`, `size`, `error`.
  - `Q_PROPERTY(int selectedIndex / qint64 selectedId / bool loading / QString error / bool canExport /
    QString title / QString subtitle / int positionNumber / int positionTotal)`.
  - `openPages(QVariantList pages, qint64 selectedId, QString title, QString subtitle, bool canExport)`,
    `selectPage(qint64 messageId)`, `close()`, `requestPageFile(int index)` (explicit download),
    `revealSpoiler(int index)`, `savePageAs(int index, QUrl target)`, `sharePage(int index)`.
  - `confirmExport` equivalent: emit `exportRequested(qint64 messageId)` and accept
    `exportDecision(qint64 messageId, bool allowed)` from `AppController` after a fresh
    `getMessageProperties(chatId, messageId)` → `can_be_saved` (the same request
    `TdConversationActions.capabilities` uses on Android).
  - Port `MediaViewerPolicy` verbatim as pure C++ helpers: `concealed(secret, active)`,
    `secureWhileShowing(secret, viewOnce)`, `rightsFor(messageId, loaded)`,
    `retainViewOncePage(pages, retained)`.
- New `src/ui/MediaViewerMath.h/.cpp`: `viewerTransform(originRect, pageSize, travel)`,
  `clampImageOffset(offset, scale, size)`, `offsetAroundPoint(...)`, `offsetAroundCenter(...)`,
  `selectedIndex(pageIds, selectedId)`, `shouldScrollToSelection(...)` — direct ports, pure functions.
- New `src/ui/VideoPlayerState.h/.cpp`: `VideoPlayerState { playing, buffering, ended, failed, positionMs,
  durationMs }` with `fraction()`, `primaryAction()`, `afterPlaybackState(mediaStatus, playbackState)`,
  `afterFailure()`, `seekPositionMs(fraction, duration)`.

### 5.3 Viewer QML

Rewrite `qml/components/MediaViewerOverlay.qml` around a real pager:

- `ListView { orientation: ListView.Horizontal; snapMode: ListView.SnapOneItem; highlightFollowsCurrentItem }`
  with `currentIndex` bound to the controller's `selectedIndex`, `onMovementEnded` publishing the settled
  index, and `positionViewAtIndex` used only for genuine selection changes (never during a drag).
- Per-page `Loader` by kind: `PhotoPage.qml`, `VideoPage.qml`, `AudioPage.qml`, `StickerPage.qml`,
  `FilePage.qml`, `SpoilerPage.qml` — mirroring the Android resolution order.
- `PhotoPage.qml`: `PinchHandler` (1..5), `DragHandler` (pan when zoomed, vertical dismiss when at 1),
  `TapHandler` (toggle chrome), double-tap zoom to 2.5 at the tap point, `WheelHandler` (Ctrl+wheel zoom),
  `+`/`−` buttons, and `Image { sourceSize: Qt.size(w*2, h*2) }` with a 4096 cap.
- Chrome: top bar (close, title, subtitle `time · N of M`, traffic/loading text, share, more menu),
  edge paging buttons when `pages.length > 1`, footer `Forward / Save / Edit / Delete` with a fixed
  minimum height, error/notice lines.
- Save: `FileDialog` with kind-aware filters (`Images`, `Video`, `Audio`, `All files`) and the suggested
  file name from `MediaExport`.
- Share: Windows share sheet (`DataTransferManager` interop) or, as a first slice, `QFileDialog` +
  "reveal in Explorer"; choose one and document it.
- Hero animation: bubble passes its mapped rect to `MediaViewerController::openPages(... originRect ...)`;
  the page `Item` applies `viewerTransform` in `QQuickItem::updatePaintNode`-free form via
  `scale`/`translate` bindings driven by a `NumberAnimation` (instant when reduced motion is on).

### 5.4 Playback engine

- New `src/media/PlaybackEngine.h/.cpp` (the `src/media` boundary already exists for playback/capture):
  - Owns exactly one `QMediaPlayer` + `QAudioOutput` for in-app audio (voice, video-note audio, music).
  - `Q_PROPERTY(qint64 position, duration; bool playing, buffering, ended, failed; float fraction;
    float voiceSpeed, musicSpeed; QString title, performer, artworkPath; MediaKind kind)`.
  - `toggle(item)`, `play(item, startPositionMs)`, `pause()`, `resume()`, `stop()`, `seekTo(fraction,
    fallbackDurationMs)`, `setVoiceSpeed/setMusicSpeed`, `attachVideo(QObject* sink)`, `detachVideo()`,
    `stopSecret()`.
  - Policies: starting a non-silent video pauses music; starting voice/video-note replaces the shared
    item; `stop()` clears the item and hides the overlay.
  - Audio policy on Windows (no Android-style focus API): pause on output-device removal via
    `QMediaDevices::audioOutputsChanged`, duck to 30 % when the app window is inactive **if** the user
    enabled "duck on focus loss", and pause when another BeyGram instance starts playback (named mutex +
    local socket broadcast). Document the chosen rule in this file; do not pretend Windows has
    `AUDIOFOCUS_LOSS`.
- `src/ui/MediaPlaybackController` either becomes a thin view over `PlaybackEngine` or is retired; if
  retired, keep `tst_media_playback.cpp` semantics under the new class name to avoid losing coverage.
- Fix `setVideoOutput`: bind `videoOut.videoSink` and call `QMediaPlayer::setVideoSink`, and add an
  explicit `close()` that clears the source so the overlay actually disappears.

### 5.5 Bubble components

- Split `qml/components/MessageBubble.qml` media block into:
  - `qml/components/MediaThumbnail.qml` (photo/video/document poster, progress bar, download badge,
    retry, spoiler cover, view-once lock, `cursorShape`, double-click open).
  - `qml/components/VoiceBubble.qml` (244 dp, 48 dp button, 40-bar `Canvas` waveform from
    `message.waveform`, tap-to-seek, duration, speed menu, pending-download auto-play, failure text).
  - `qml/components/VideoNoteBubble.qml` (220 dp circle, thumbnail/minithumbnail, ring progress,
    remaining time, `VolumeOff` badge, inline `VideoOutput` bound to `PlaybackEngine`, secure handling).
  - `qml/components/AudioCard.qml` (artwork, title/performer, progress, play/pause, open player).
- `ChatsPage.qml` delegate passes the new roles (`mimeType`, `durationSeconds`, `waveform`, `viewOnce`,
  `hasSpoiler`, `isSecret`, `fileState`).
- Wire `AlbumGrid.qml` to `mediaClicked(index)` → `openPages(all album items, index)`.

### 5.6 Platform integration

- Share: `qml` → `AppController::shareMedia(QUrl)` using the Windows share UI (`DataTransferManager`
  interop through a small `platform/windows` helper) with a temp copy under the app cache (never expose
  the TDLib cache path to other apps).
- Open external: `QDesktopServices::openUrl(QUrl::fromLocalFile(...))` on an exported temp copy.
- Save: `QFileDialog` path + `MediaExport` name/mime resolution; success/notice string surfaced in the
  overlay footer.
- Capture protection: `SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)` while a secret or
  view-once page is settled; clear on close and on page change.

### 5.7 Windows keyboard map (new design, not an Android port)

| Key | Action |
| --- | --- |
| `Esc` | close viewer / stop and close playback |
| `←` / `→` | previous / next page (snapped) |
| `PageUp` / `PageDown` | previous / next page |
| `Home` / `End` | first / last page |
| `Space`, `K` | play/pause (video page, audio page, playback overlay) |
| `+` / `=` / `-` | zoom in / out around centre |
| `0` | reset zoom to 1 |
| `Ctrl+S` | Save page as |
| `Ctrl+C` | copy image to clipboard (photos only) |
| `F11` | toggle fullscreen (window-level) |
| `Tab` / `Shift+Tab` | move focus across close / prev / next / actions |
| `Delete` | delete message (only when `canDelete`) |

Focus rules: the overlay must take active focus on open (`forceActiveFocus`), trap `Tab` inside itself,
and restore focus to the originating bubble on close.

### 5.8 Tests (additive, no build claim)

| New test | Covers |
| --- | --- |
| `tests/tst_media_viewer_math.cpp` | `selectedIndex`, `shouldScrollToSelection`, `viewerTransform`, `clampImageOffset`, `offsetAroundPoint`, `offsetAroundCenter` |
| `tests/tst_media_viewer_policy.cpp` | `concealed`, `secureWhileShowing`, `rightsFor`, `retainViewOncePage` (ascending and descending lists, retained already present, empty list) |
| `tests/tst_media_viewer_pages.cpp` | Model roles, selection retention when a page disappears, stale index → synthetic key, per-message rights isolation, page state transitions |
| `tests/tst_media_export.cpp` | `canExport` truth table (capabilities × viewOnce × secret × spoiler), display name/extension/mime repair |
| `tests/tst_video_player_state.cpp` | `afterPlaybackState` buffering/ready/ended, `afterFailure`, `primaryAction`, `seekPositionMs` (duration 0) |
| `tests/tst_media_file_state.cpp` | progress/failure/cancel signals, prefix bytes, `isReady` unchanged |
| `tests/tst_playback_engine.cpp` | toggle/pause/resume/stop, video pauses music, voice replaces item, speed per kind, device-removal pause |
| `tests/tst_voice_bubble.cpp` | `resampleWaveform`, `voiceDownloadFailed`, `voicePendingAfterError`, pending row identity |
| Extend `tests/tst_media_viewer.cpp` | new `openPages`/`savePageAs` guards, close clears source, export decision plumbing |

Register each in `tests/CMakeLists.txt` with `beygram_add_test(...)`; keep them `QTEST_GUILESS_MAIN` where
they touch no QML, and `QT_QPA_PLATFORM=offscreen` (already set by the helper) for anything that needs
`QGuiApplication`.

### 5.9 Staging

1. **Stage 1 (P0, viewer core):** store state + media roles → page model → pager → zoom/dismiss →
   in-viewer video with `VideoPlayerState` → keyboard map → fix playback close/sink.
2. **Stage 2 (P1, actions and audio):** chrome actions, Save/Share with capability re-check, download
   progress + retry, `PlaybackEngine`, voice bubble, video-note bubble, audio focus policy.
3. **Stage 3 (P2, policy):** spoiler/view-once/secret data + concealment + export denial, viewed
   receipts, auto-download policy, position counter, album wiring.
4. **Stage 4 (P3):** hero open/close, capture protection, reduced-motion handling, hover affordances.

---

## 6. Risks and verification checklist

- **Video sink:** `setVideoOutput(QQuickItem*)` is the first thing to verify; use `videoSink` if frames do
  not appear. Do not "fix" it by adding a second player.
- **Image decoding:** the provider decodes synchronously in `requestImage`; a 4096 px cap plus
  `sourceSize` is required before enabling a pager that can decode three neighbours.
- **Path leakage:** share/open-external must export a copy; never hand the TDLib cache path to other apps.
- **Pending playback race:** `AppController::openMediaPlayback`'s one-shot `fileReady` connection needs a
  generation counter, otherwise an older open can replace a newer one.
- **Page shrink:** the Android synthetic-key trick exists because the list can shrink under the pager;
  the Qt model must reproduce it (invalid index → empty delegate, never a null dereference).
- **View-once deletion:** TDLib deletes the message immediately after `openMessageContent`; without
  `retainViewOncePage` the Windows viewer will close mid-view.
- **Audio policy honesty:** Windows has no `AUDIOFOCUS_LOSS`; implement the documented approximation and
  label it as such in the code and in this doc.

Manual verification matrix (to be executed later, not now): photo page open/close, swipe both directions,
page at both edges, zoom + pan clamp, double-tap point accuracy, drag-dismiss threshold, keyboard-only
pass, video buffering/end/replay/seek, audio while video plays, voice pending download → auto-play,
voice download failure → retry, video note inline growth, secret/view-once export denial, Save into each
target folder, Share round-trip, and one large (>20 MP) photo.

---

## 7. Non-claims

- No build, configure, preview, or test execution was performed for this audit.
- Nothing here confirms the Windows media viewer or playback overlay currently works at runtime.
- Android behaviour is described from source; device-specific rendering (ExoPlayer codec support,
  `PlayerView` surface behaviour, MediaStore folder naming) was not observed on a device.
- Priorities are proposals for the porting sequence, not commitments.
