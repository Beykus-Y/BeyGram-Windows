# 18 — Music library, `.bgmusic`, BG Beat / BG Mix and the player

Parity audit of the Android music library, `.bgmusic` v1 container, queue/player,
BG Beat, BG Mix and their native boundary against the Windows client.

- **Mode:** read-only audit. No CMake configure, no build, no QML preview, no test run, no device run, no git operations.
- **Consequence:** every "current status" statement below is derived from source reading only. Nothing here
  asserts that the Windows client builds, runs, plays audio, or links BG Media.
- **Android side:** `app/src/main/java/com/beykus/beygram/{data/music,playback,media,ui/screens}`,
  `app/src/main/cpp/**`, `docs/formats/bgmusic-v1.md` (Compose, Media3/ExoPlayer, JNI).
- **Windows side:** `src/media/**`, `src/ui/MediaPlaybackController.*`, `src/telegram/**`, `qml/**`,
  `tests/**`, `cmake/BeyGramBgMedia.cmake` (Qt 6 Quick + Qt Multimedia).

| | |
|---|---|
| Audit date | 2026-11-11 |
| Android reference | `app/src/main/java/com/beykus/beygram/{data/music, playback, media, ui/screens/{music,bgmusic,playback}}`, `app/src/main/cpp`, `docs/formats/bgmusic-v1.md` |
| Windows target | `src/media`, `src/ui/MediaPlaybackController.*`, `src/telegram/*`, `qml/*`, `cmake/BeyGramBgMedia.cmake` |
| Overall Android status | Music library, playlists, `.bgmusic` v1, queue/player, equalizer and session restore are complete on `DEV`; BG Beat/BG Mix are complete in Kotlin but the native BG Media core is an uninitialised submodule (`third_party/bg-media` is empty in this clone, so `HAVE_BGMEDIA` is 0 and both JNI bridges compile to stubs) |
| Overall Windows status | Placeholder only: one static capability string plus a single-file, local-only `QMediaPlayer` wrapper. No library, no queue, no `.bgmusic`, no BG Media |
| Priority | **P3** in the audit table (`M14`, `M15`), i.e. Phase 4 of `docs/PORTING_ROADMAP.md`; the pure format/library slice is worth pulling forward to the end of Phase 3 because it needs no audio device and no TDLib audio plumbing |

---

## 1. Files reviewed and Android reference behaviour

### 1.1 Files reviewed

Music library (`data/music`, 2 128 lines):

| File | Lines | Role |
|---|---:|---|
| `MusicModels.kt` | 184 | Schema constants, `PlaylistTrack`, `MusicPlaylist`, `MusicTrackRef`, `MusicListenStats`, `MusicHistoryEntry`, `MusicLibrary`, `MusicFailure`, `MusicOutcome` |
| `MusicLibraries.kt` | 28 | Process-wide `MusicLibraryRepository` singleton |
| `MusicLibraryStore.kt` | 12 | `filesDir/music_library` + `MusicLibraryFileStore` |
| `MusicLibraryPersistence.kt` | 26 | `MusicLibraryLoad` / `MusicLibraryPersistence` seam |
| `MusicLibraryFileStore.kt` | 172 | Per-account JSON file, atomic replace, quarantine, 3 backups, account-hashed file names |
| `MusicLibraryJson.kt` | 274 | Encode/decode, per-entry tolerance, `skippedCount`, limits |
| `MusicLibraryEdits.kt` | 496 | Every mutation as a pure edit (playlists, tracks, favourites, stats, imports) |
| `MusicLibraryRepository.kt` | 363 | Mutex + write-then-publish, account switch, cover lifecycle, imports |
| `MusicConversions.kt` | 53 | `PlaybackItem` → `PlaylistTrack` / `MusicTrackRef` |
| `MusicPlayback.kt` | 137 | `playPlaylist`, windowed resolution, lazy pages |
| `MusicPlaybackWindow.kt` | 83 | Window/pager constants and pure range maths |
| `MusicHistoryRecorder.kt` | 62 | Counts a listen on the first `started` state per occurrence |
| `MusicSmartCollections.kt` | 66 | Frequently played, recently added, not played lately, from chat |
| `MusicReplacement.kt` | 75 | Match rules and candidate ordering for replacing a dead track |

`.bgmusic` (`data/music/bgmusic`, 1 907 lines) plus the 31 KB spec
`docs/formats/bgmusic-v1.md`:

| File | Lines | Role |
|---|---:|---|
| `BgMusicFormat.kt` | 198 | Signature, versions, size/string limits, sanitising, UUID/username/hash normalisation, magic bytes, archive sniffing |
| `BgMusicModels.kt` | 65 | `BgMusicManifest`, `BgMusicTrack`, `BgMusicSourceHint.TelegramPublic`, `BgMusicAuthor` |
| `BgMusicStrictJson.kt` | 229 | Strict JSON scan: duplicate keys, depth, literals |
| `BgMusicReader.kt` | 454 | Streaming ZIP scan, entry validation, budget, ordered manifest parse, `Unsupported`/`Invalid` reasons |
| `BgMusicWriter.kt` | 196 | Validate in memory, deterministic entry order/timestamps, deflated manifest, stored cover |
| `BgMusicCache.kt` | 59 | Cache key `(accountId, fileId, size)` and a generic LRU with eviction callback |
| `BgMusicFileParser.kt` | 75 | Parse-once cache (8 entries), cover file in cache, account-hashed cover names |
| `BgMusicExporter.kt` | 129 | `cacheDir/bgmusic_export`, single latest archive, cover re-encode to JPEG ≤ 1 MiB |
| `BgMusicImportDecision.kt` | 46 | `NewPlaylist` / `SameRevision` / `NewerRevision` / `OlderRevision` |
| `BgMusicImportPlanner.kt` | 40 | Pure matching against the local library |
| `BgMusicLibraryMapping.kt` | 207 | Local playlist ↔ manifest, export identity/revision rules, `telegram_public` hints, file names |
| `BgMusicSources.kt` | 100 | Public username lookup and hint resolution in the recipient account |

Player / queue (`playback`, 4 269 lines):

| File | Lines | Role |
|---|---:|---|
| `PlaybackEngine.kt` | 2 009 | Single process-wide ExoPlayer, queue, lazy sources, prefetch, BG Mix dispatch, sleep timer, MediaSession service |
| `PlaybackModels.kt` | 530 | `PlaybackOrigin`, `PlaybackItem`, `PlaybackState`, shuffle/repeat/next policies, play-next, queue sanitising |
| `PlaybackSources.kt` | 287 | Account-scoped lazy source handles, custom `DataSource` that waits for TDLib, growing-file reads |
| `PlaybackShuffleOrder.kt` | 69 | Explicit shuffle order, page insert/remove |
| `MusicSessionStore.kt` | 91 | `music_resume_v1` prefs: origins only, ≤ 80 identities, no paths/file ids |
| `MusicSessionRestorer.kt` | 91 | Re-resolve origins against current TDLib, ≤ 3 in flight, drop gone/protected |
| `MusicBotCache.kt` | 60 | Reserved copies of music-bot downloads, prune against the player |
| `PlaybackService.kt` | 43 | `MediaSessionService`, notification provider, equalizer start/stop |
| `BgMixQueueDriver.kt` | 82 | Eligibility and native-id ↔ queue-index mapping |
| `EqualizerController/Runtime/Store` | 559 | Android audio-session equalizer |

Native (`app/src/main/cpp`):

| File | Lines | Role |
|---|---:|---|
| `bgbeat_jni.cpp` | 480 | BG Beat JNI: version, cancel-token registry, `nativeAnalyzeFile` → JSON; stub when `HAVE_BGMEDIA=0` |
| `bgmix_jni.cpp` | 312 | BG Mix JNI: create/destroy, add/remove/clear queue, transport, seek, 50 ms `update`, `render`, JSON `status` |
| `CMakeLists.txt` | 84 | Builds both bridges into `libbgbeat_jni.so`; pinned BG Media commit `73507c8d…`, token fallback, stub mode |

UI (10 809 lines across `ui/screens/{music,bgmusic,playback}`): `MyMusicScreen`,
`MusicPlaylistScreen`, `AddToPlaylistSheet`, `ReplacementSheet`,
`ChatLibraryMusicRow`, `MusicLibraryUi`, `MusicScreenSupport`,
`MusicCoverFiles`; `BgMusicChatCard`, `BgMusicPreviewScreen`,
`BgMusicExportSheet`; `MusicPlayerSheet` (1 523), `NowPlayingBar`,
`MusicPlayerUiState`, `MusicSearchScreen`, `EqualizerScreen`, `TrackArtwork`,
`PlaybackSpeedMenu`, `PlaybackSourceLabel`.

Android tests used as expected-value references: 24 JVM suites under
`app/src/test/java/com/beykus/beygram/{data/music,data/music/bgmusic,playback,ui/screens/*}`,
including round-trip, archive-security, fuzz, manifest-compatibility,
writer-validation, import-planner/mapping, playback-window, queue-policy,
identity, metadata, growing-audio, interrupted-music and session-policy.

### 1.2 Library sources

A track can enter the library from four places, and all of them converge on the
same `PlaylistTrack` shape:

1. **Chat audio messages** — `orderedChatMusic()` turns sent, non-secret
   `messageAudio` items into `PlaybackItem`s ordered newest → oldest, with
   `PlaybackOrigin(chatId, messageId, accountId)`.
   `playlistTrackOf()` converts one such item into a playlist entry.
2. **Chat playlist queue** — tapping a bubble builds a queue from the loaded
   page, then appends older pages through
   `conversation.sharedMessages(chat.id, SharedMediaKind.Music, cursor)`
   (`ChatScreen.kt`, ~1752-1784). Empty filtered pages advance the cursor up to
   four times instead of stalling.
3. **Music search** — `MusicSearchScreen` runs two independent searches:
   the account's own messages through
   `searchGlobalMessages(query, offset, GlobalSearchFilter(kind = SharedMediaKind.Music))`,
   and public results through the optional music inline bot
   (`ConversationRepository.musicBotSearch` / `musicBotTrack`, TDLib
   `getInlineQueryResults` → send to Saved Messages → download → copy out →
   delete the temporary message and restore the draft).
4. **`.bgmusic` files** — a received or picked file is parsed and mapped through
   `BgMusicLibraryMapping.toPlaylistCopy`; tracks whose `telegram_public` hints
   resolve in the recipient account become playable, the rest stay listed but
   unavailable.

"Downloaded tracks" is not a separate store: `rememberDownloadedTracks()`
derives it from the TDLib file state of library entries. Favourites is a fixed
built-in playlist (`00000000-0000-4000-8000-00000000f4f0`) that can be neither
deleted nor renamed. Smart collections are pure functions over stats + playlists.

### 1.3 Library storage format

- One file per account: `filesDir/music_library/<sha256(accountId)>.json`.
  The account id itself never appears in a file name.
- Top level: `schemaVersion` (currently 1), `accountId`, `playlists[]`,
  `stats[]`, `recent[]`.
- `playlists[]`: `id`, `title`, `description`, `coverPath`, `tracks[]`,
  `createdAt`, `updatedAt`, `revision`, `sourcePlaylistId`, `sourceRevision`,
  `revisionAtSource`, `sortMode`.
- `tracks[]`: `itemId`, `title`, `performer`, `album`, `durationSeconds`,
  `origin{chatId,messageId,accountId}`, `addedAt`, `audioHash`.
- Limits: 500 playlists, 5 000 tracks per playlist, 200 recent entries,
  2 000 stats, 128-char title, 1 000-char description.
- Write discipline: temp file → `fd.sync()` → atomic move; publish in memory
  only after the write succeeded; a failed write leaves memory and disk
  unchanged. A corrupt or foreign file is renamed to `<name>.<stamp>.bak`;
  a file that loaded with skipped entries is backed up before its first
  overwrite; at most 3 backups per account.
- A newer `schemaVersion` yields `Unavailable`: nothing is written over it.
- `origin` is the identity anchor: `tg:<account>:<chat>:<message>`, else
  `hash:<sha256>`, else `item:<entryId>`. Covers must live inside
  `filesDir/music_covers`; anything else is refused, and unreferenced cover
  files are deleted only after the library write.

### 1.4 Queue and player

- **One player per process.** `PlaybackEngine` owns a single `ExoPlayer`
  wrapped in a `ForwardingPlayer`; `PlaybackService` (a `MediaSessionService`)
  exposes it to the notification, lock screen, Bluetooth and the equalizer.
  The player survives screen changes and service stop-on-pause.
- **The queue is the single source of truth.** `PlaybackState.queue` and the
  ExoPlayer playlist are kept in lockstep; `queueIndex` is the *original* index
  even under shuffle. `queueForPlayback()` strips secret, secret-chat and
  view-once items and refuses a private selection.
- **Lazy, account-scoped sources.** `PlaybackQueueSource` carries
  `accountId`, `observeFile`, manual `download`, policy-aware `autoDownload`
  and an optional `loadMore` page. `PlaybackSourceHandle` resolves a path at
  open time; `PlaybackDataSource` blocks in BUFFERING (poll 50 ms, request every
  1 s, open timeout 60 s, stall timeout 45 s, cancelled-transfer timeout 10 s)
  instead of silently skipping. Growing Ogg/Opus files are read up to
  `readablePrefix()` and re-opened at the completed path without replacing the
  player.
- **Pagination.** A long playlist resolves a window (60 entries resolve fully;
  otherwise ±20 around the start) and appends pages of 30, tail first then
  head, so every entry eventually joins the queue. `appendMusicPage()` dedupes
  and enforces account + public-only. `queuePageLoading` is a real state, so
  the UI never claims "no more music" while a page is in flight.
- **Modes.** Repeat Off/All/One cycles Off→All→One. Shuffle uses an explicit
  `ShuffleOrder` so Play Next lands next in the *actual* order and appended
  pages keep the existing order. `PREVIOUS_RESTART_THRESHOLD_MS = 3000`:
  Previous restarts the track when past 3 s.
- **Prefetch.** At most two following public, unresolved tracks, only through
  the auto-download policy; a key is remembered only when policy allowed and
  the repository accepted it.
- **Interruption.** Music interrupted by a voice note or video note is stored
  (`InterruptedMusic`) with position, queue, index, shuffle, repeat and source,
  and resumes when the interrupting item ends; reselecting the track restores
  that context.
- **Privacy.** Secret attachments, secret chats and view-once media never reach
  the queue, the MediaSession, the notification or the saved session; they stop
  when the chat is left or the app is backgrounded (`stopSecret`). System media
  ids for private content are hashed.
- **Sleep timer.** 15/30/60 minutes on `elapsedRealtime`, account-scoped,
  cancelled by account switch, never restarted after process death.
- **Speeds.** Music and voice speeds are independent; music defaults to 1×.
- **Session restore.** `saveableMusicSession()` stores only origins,
  index, position (clamped ≤ 24 h), shuffle and repeat for the active account
  (≤ 80 identities, centred on the current index); restore re-resolves each
  origin against current TDLib, requires the selected track to still exist, and
  comes back **paused with no download**.
- **BG Mix dispatch.** `startQueue` tries BG Mix first when the experimental
  flag is on, repeat is Off, shuffle is off and every item is an eligible local
  file; otherwise ExoPlayer handles the queue.

### 1.5 BG Beat

- Kotlin: `BgBeatManager` watches `PlaybackEngine.state` + the enabled setting,
  analyses the current *local* music file, cancels on track/account change,
  exposes `Disabled/Idle/Ineligible/Analyzing/Ready/Error`, and stores results
  per account in `filesDir/bgbeat-cache/account_<safeId>` with a 50 MiB quota.
- Native: `nativeAnalyzeFile(path, cacheDir, minBpm, maxBpm, cancelToken, progress)`
  returns JSON with `schemaVersion`, `analyzerVersion`, `contentHash`,
  `configHash`, `duration`, `sampleRate`, `sourceChannels`, `bpm`,
  `bpmConfidence`, `bpmLevelConfidence`, `beatPhaseConfidence`,
  `downbeatConfidence`, `tempoStable`, `octaveAmbiguous`, `beatsPerBar`,
  `meterConfidence`, `analysisSeconds`, `activeRatio`, `bpmCandidates[]`,
  `beats[]` (`time`, `strength`, `confidence`, `beatInBar`, `barIndex`,
  `downbeat`, `extrapolated`), `waveform[]` (≤ 400 min/max bins) and
  `waveformPeaksPerSecond`.
- Decoding is miniaudio, chunked (8 192 frames), cancellable, bounded to 1 GiB
  of float samples, rejects zero-frame streams and streams that read less than
  half of a known length. The analysis cache is keyed by content hash + config
  hash.
- When `HAVE_BGMEDIA=0` every call returns
  `{"status":"unsupported","message":"BG Media C++ core is omitted…"}` and the
  version string is `BG Beat 0.1 (stub - private core omitted)`.

### 1.6 BG Mix

- Kotlin `BgMixPlayer` owns one native session and one `AudioTrack`
  (48 000 Hz, stereo, `ENCODING_PCM_FLOAT`, `MODE_STREAM`, ~200 ms buffer),
  a dedicated `MAX_PRIORITY` render thread pulling 1 024 frames, and a 50 ms
  control tick. `release()` stops/flushes the track, joins the render thread
  with a 1 s timeout, and destroys the native host on a separate thread so a
  running analysis cannot block the caller.
- `BgMixQueueDriver` accepts only complete local `wav`/`mp3`/`flac` files,
  refuses to start on a track it cannot decode (no silent substitution), maps
  native queue ids back to queue indices, and exposes status as a `StateFlow`.
- Native `MixHost` = one `MixEngine(48000)` + one `MixSession`; control calls are
  serialised by a mutex, `render()` is lock-free by design. Transport actions
  are 0 play, 1 pause, 2 next (planned transition), 3 stop. `status` JSON
  carries `paused`, `master`, `transitionActive/Available/Type/Progress`,
  `preparingNext/Name/Progress`, `lastError`, `status`, `decks[2]`
  (`loaded`, `playing`, `ended`, `positionSeconds`, `durationSeconds`, `bpm`)
  and `queue[]` (`id`, `name`, `state`, `progress`, `bpm`, `durationSeconds`,
  `error`).
- BG Mix is experimental and off by default (`bgmix_settings` /
  `bgmix_enabled`); the Settings screen toggles it and BG Beat. Repeat and
  shuffle force the ExoPlayer path.

### 1.7 Native boundary

| Aspect | Android |
|---|---|
| Loading | `System.loadLibrary("bgbeat_jni")` through `BgMediaSdk.ensureLoaded()`; both bridges live in one `.so` |
| Strings | BG Beat uses `jstring`; BG Mix deliberately uses UTF-8 `jbyteArray` because `NewStringUTF` breaks on 4-byte sequences (emoji in titles) |
| Cancellation | `CancelRegistry` maps `int64` handles to `shared_ptr<atomic<bool>>`; create/cancel/destroy are mutex-guarded (no use-after-free across threads) |
| Threading | Control calls serialised; render called from exactly one audio thread with no lock |
| Failure mode | Missing core = stub library that still loads, reports `unsupported`, and never pretends to play |
| Versions | `BGMEDIA_VERSION_STRING`, `kAnalysisSchemaVersion`, `kAnalyzerVersion` surfaced in the version string |

### 1.8 Caching

| Cache | Location | Policy |
|---|---|---|
| Music library | `filesDir/music_library/<sha256>.json` + ≤ 3 `.bak` | durable, atomic, quarantined on corruption |
| Playlist covers | `filesDir/music_covers` | app-private, copied on duplicate, deleted when unreferenced |
| `.bgmusic` parse result | in-memory LRU, 8 entries, key `(account, fileId, size)` | re-parsed when TDLib replaces the file |
| `.bgmusic` covers | `cacheDir/bgmusic_covers/<sha256(account)>_<fileId>_<size>.<ext>` | deleted on LRU eviction |
| `.bgmusic` export | `cacheDir/bgmusic_export` | only the latest archive; `.part` → rename |
| BG Beat analysis | `filesDir/bgbeat-cache/account_<safeId>` | 50 MiB quota, oldest-first eviction, cleared per account/all |
| BG Mix prepared tracks | `filesDir/bgmix-cache` | owned by the native session |
| Music-bot copies | cache dir `music-bot` | reserved until handed to the player, pruned against the live queue |
| Session resume | `SharedPreferences` `music_resume_v1` | origins only, per account |

---

## 2. Current Windows status

### 2.1 What exists

| Area | State | Evidence |
|---|---|---|
| BG Media capability | One static struct, no real probe | `src/media/BgMediaStatus.h`, `BgMediaStatus.cpp`: returns `linked=false` and "BG Media is not linked in this build" unless `BEYGRAM_HAS_BGMEDIA` is defined |
| BG Media build switch | Opt-in subdirectory, empty in this clone | `cmake/BeyGramBgMedia.cmake`; `BEYGRAM_WITH_BGMEDIA=OFF` by default; `third_party/bg-media` contains 0 entries; `beygram_bgmedia` is an empty `INTERFACE` target, so `BEYGRAM_HAS_BGMEDIA` is never set |
| UI surface | Diagnostics card only | `qml/pages/EnvironmentPage.qml:149-167` shows `BeyGramApp.bgMediaTitle` / `bgMediaDetail`; `AppController.cpp:1196-1204` forwards the static struct |
| Navigation | No music section | `qml/Shell.qml` has Chats, Profile, Settings, Network, Logs, Search only |
| Playback | Single local file, no queue | `src/ui/MediaPlaybackController.{h,cpp}`: `QMediaPlayer` + `QAudioOutput`, `open(QUrl, title)`, play/pause/toggle/stop/seek/volume; rejects remote URLs and missing files; strips paths from error text |
| Playback UI | Generic media overlay | `qml/components/MediaPlaybackOverlay.qml`: title, slider, Play/Pause, Close, error line; no queue, metadata, artwork, shuffle, repeat, speed or next/previous |
| Audio in TDLib mapping | Partial | `TelegramTypes.cpp:150-161, 207-213` recognises `messageAudio`, returns `album_cover_thumbnail` as the thumbnail and the audio file as the full file; `MessageData` has **no** title/performer/album/duration fields |
| File download | Complete-file only | `MediaFileStore` requests `downloadFile` with priority 32 and a 120 s timeout; no progress model, no growing-file reads, no per-account scoping |
| Music library / `.bgmusic` / BG Beat / BG Mix | Absent | No files under `src/media` besides `BgMediaStatus`, no `qml` pages, no tests |

### 2.2 What the existing playback controller does and does not do

Does: validates that the source is a local existing file, loads it into one
`QMediaPlayer`, exposes `position`, `duration`, `playing`, `volume` (clamped),
`error`, and a `videoOutput` property for the overlay's `VideoOutput`.

Does not: hold a queue, expose metadata (artist/album/artwork), integrate with
Windows System Media Transport Controls or media keys, restore a session,
remember a position, apply speed, shuffle, repeat, equalizer, sleep timer,
prefetch, account scoping, or any download. `openMediaPlayback(fileId, title)`
in `AppController.cpp:782-805` is the only entry point and it waits for one file
via `MediaFileStore::fileReady`.

### 2.3 Test coverage

`tests/tst_media_playback.cpp` covers exactly the single-file controller:
defaults, empty/remote/missing/directory rejection, volume clamp, transport
before open, `stateChanged`. There is no Windows test for a music library,
`.bgmusic`, queue order, session restore, BG Beat or BG Mix.

---

## 3. Gap analysis

Severity: **S1** = feature absent, **S2** = present but materially incomplete,
**S3** = polish/edge-case difference.

### 3.1 Library

| # | Gap | Android reference | Severity |
|---|---|---|---|
| L1 | No per-account library store, JSON schema, atomic write, backups or quarantine | `MusicLibrary*` | S1 |
| L2 | No playlist CRUD, ordering, sorting, duplicate, favourites | `MusicLibraryEdits` | S1 |
| L3 | No listen stats, recent history or smart collections | `MusicHistoryRecorder`, `MusicSmartCollections` | S1 |
| L4 | No track identity (`tg:`/`hash:`/`item:`) and no origin re-resolution | `musicTrackKey`, `MusicSessionRestorer` | S1 |
| L5 | No cover file rules (inside-root only, copy-on-duplicate, delete-unreferenced) | `MusicLibraryRepository` | S1 |
| L6 | No replacement search for dead tracks | `MusicReplacement` | S2 |
| L7 | No music search (own messages by audio filter + music bot) | `MusicSearchScreen`, `TdBotFeatures` | S2 |
| L8 | No "downloaded tracks" derived view | `rememberDownloadedTracks` | S2 |

### 3.2 `.bgmusic`

| # | Gap | Android reference | Severity |
|---|---|---|---|
| B1 | No reader/writer at all | `BgMusicReader`, `BgMusicWriter` | S1 |
| B2 | No strict-JSON scanner, no duplicate-key/depth rules | `BgMusicStrictJson` | S1 |
| B3 | No archive security (entry-name, duplicate, nesting, budget, magic bytes, cover mismatch) | `BgMusicFormat`, `BgMusicReader` | S1 |
| B4 | No import decision/planner and no revision identity rules | `BgMusicImportDecision`, `BgMusicImportPlanner`, `BgMusicLibraryMapping` | S1 |
| B5 | No `telegram_public` hint resolution | `BgMusicSources` | S2 |
| B6 | No chat card, preview dialog, import or export | `BgMusicChatCard`, `BgMusicPreviewScreen`, `BgMusicExportSheet` | S2 |
| B7 | No deterministic export (fixed entry order/timestamps) or cover re-encode | `BgMusicWriter`, `BgMusicExporter` | S2 |
| B8 | No file association / drag-drop import for `.bgmusic` | Android has no OS-open intent either (it arrives as a document); Windows should add shell association as a deliberate extra | S3 |

### 3.3 Player and queue

| # | Gap | Android reference | Severity |
|---|---|---|---|
| P1 | No queue model, no `PlaybackItem`/`PlaybackState`, no next/previous | `PlaybackModels` | S1 |
| P2 | No shuffle order, repeat modes, Play Next, reorder, remove | `PlaybackShuffleOrder`, `PlaybackEngine` | S1 |
| P3 | No lazy/partial playback; a track must be fully downloaded first | `PlaybackSources`, `PlaybackEngine` | S2 |
| P4 | No pagination of long chat playlists or playlists | `MusicPlayback`, `MusicPlaybackWindow` | S2 |
| P5 | No prefetch policy | `prefetchCandidates` | S3 |
| P6 | No session persistence/restore | `MusicSessionStore`, `MusicSessionRestorer` | S2 |
| P7 | No sleep timer | `setSleepTimer` | S3 |
| P8 | No speed control | `applySpeed` | S3 |
| P9 | No Windows SMTC / media keys / taskbar controls | `PlaybackService` + Media3 | S2 |
| P10 | No equalizer (and `QMediaPlayer` cannot host one; needs custom DSP) | `Equalizer*` | S3 |
| P11 | No interrupted-music resume policy | `InterruptedMusic` | S3 |
| P12 | No private/secret/view-once exclusion rules in a queue | `queueForPlayback`, `stopSecret` | S1 (security) |

### 3.4 BG Beat / BG Mix

| # | Gap | Android reference | Severity |
|---|---|---|---|
| N1 | No BG Media core linked; submodule empty; status is hardcoded | `cmake/BeyGramBgMedia.cmake`, `BgMediaStatus.cpp` | S1 |
| N2 | No in-process BG Beat analysis API, cancel tokens, cache or progress | `BgBeatManager`, `bgbeat_jni.cpp` | S1 |
| N3 | No BG Mix engine, session, queue, transitions or status model | `BgMixPlayer`, `bgmix_jni.cpp` | S1 |
| N4 | No Windows audio output decision (WASAPI vs miniaudio device vs `QAudioSink` pull) | Android `AudioTrack` | S1 (design) |
| N5 | No capability reporting that reflects the actually linked core | `BgMediaStatus` | S2 |
| N6 | No BG Beat UI (waveform/beats) or BG Mix toggle in Settings | `BgBeatSection`, `SettingsScreen` | S2 |

### 3.5 TDLib audio plumbing (prerequisite for 3.1-3.3)

| # | Gap | Severity |
|---|---|---|
| T1 | `MessageData` lacks audio metadata (`title`, `performer`, `album`, `duration`, `mime`, `size`, album-cover file id) | S1 |
| T2 | No audio-filtered history/search request (`searchChatMessages` + `SearchMessagesFilterAudio`), only unfiltered `getChatHistory` | S1 |
| T3 | No message-by-id lookup for origin re-resolution | S1 |
| T4 | No per-account file identity; `MediaFileStore` keys only by `int fileId` | S1 |
| T5 | No download progress/cancel, no auto-download policy, no growing-file state | S2 |
| T6 | No bot inline-query path (`getInlineQueryResults`, send-then-download, temporary message cleanup) | S2 |

---

## 4. Implementation plan

Order matters: **A → B → C → D**, with C unblocking B. Each slice is
independently testable and honest about what is not there.

### Phase A — pure port: library + `.bgmusic` (no TDLib, no audio device)

Why first: it is a direct, high-value port, fully covered by pure tests, and it
makes the product useful (import/export/preview) before any audio backend work.

New files:

```
src/media/music/
  MusicModels.h                 PlaylistTrack, MusicPlaylist, MusicLibrary, MusicTrackRef,
                                MusicListenStats, MusicHistoryEntry, MusicFailure/MusicOutcome
  MusicIds.h/.cpp               musicTrackKey(), newMusicItemId(), favourite id constants
  MusicLibraryJson.h/.cpp       encode/decode, per-entry tolerance, limits
  MusicLibraryStore.h/.cpp      per-account file, atomic replace, quarantine, 3 backups
  MusicLibraryEdits.h/.cpp      pure mutations (playlists, tracks, favourites, stats, imports)
  MusicLibraryRepository.h/.cpp QObject + mutex, write-then-publish, account switch, covers
  MusicSmartCollections.h/.cpp  frequentlyPlayed/recentlyAdded/notPlayedLately/fromChat
  MusicReplacement.h/.cpp       normalise/isMusicReplacement/replacementCandidates
  MusicConversions.h/.cpp       PlaybackItem <-> PlaylistTrack/TrackRef (Phase B type)

src/media/bgmusic/
  BgMusicFormat.h/.cpp          limits, sanitise, UUID/username/hash, magic bytes, sniffing
  BgMusicStrictJson.h/.cpp      duplicate keys, depth, literals
  BgMusicModels.h               Manifest/Track/SourceHint/Author
  BgMusicReader.h/.cpp          QIODevice streaming ZIP scan (minizip or QZipReader-like
                                in-house scanner), budget, reasons, Unsupported
  BgMusicWriter.h/.cpp          validate-in-memory, deterministic order/time, stored cover
  BgMusicCache.h/.cpp           LRU (8) keyed by (accountId, fileId, size), cover eviction
  BgMusicFileParser.h/.cpp      parse-once + cover file naming (SHA-256 account segment)
  BgMusicExporter.h/.cpp        cache export dir, single latest, QImage → JPEG ≤ 1 MiB
  BgMusicImportDecision.h/.cpp  New/Same/Newer/Older
  BgMusicImportPlanner.h/.cpp   pure matching, favourites excluded
  BgMusicLibraryMapping.h/.cpp  export identity/revision, hints, file names, toPlaylistCopy
```

APIs to mirror one-to-one (same names, `std::expected`-style result types):
`MusicLibraryRepository::createPlaylist/renamePlaylist/updateDescription/setCover/`
`deletePlaylist/duplicatePlaylist/addTracks/removeTrack/replaceTrack/moveTrack/`
`sortPlaylist/toggleFavorite/recordPlay/importPlaylistCopy/replaceImportedPlaylist`;
`BgMusicReader::read(QIODevice*, std::optional<qint64> declaredSize)`;
`BgMusicWriter::write(manifest, cover, QIODevice*)`;
`BgMusicImportPlanner::decide(playlistId, revision, library)`;
`BgMusicLibraryMapping::toManifest/toPlaylistCopy/exportIdentity/publicHints/fileNameFor`.

Storage mapping (must use `AppPaths`):

| Android | Windows |
|---|---|
| `filesDir/music_library` | `AppPaths::dataDirectory()/music/<accountHash>/library.json` |
| `filesDir/music_covers` | `…/music/<accountHash>/covers/` |
| `cacheDir/bgmusic_covers` | `AppPaths::cacheDirectory()/bgmusic/covers/` |
| `cacheDir/bgmusic_export` | `AppPaths::cacheDirectory()/bgmusic/export/` |
| `filesDir/bgbeat-cache` | `AppPaths::cacheDirectory()/bgbeat/<accountHash>/` |
| `filesDir/bgmix-cache` | `AppPaths::cacheDirectory()/bgmix/` |

Tests (Qt Test, `tests/CMakeLists.txt` `beygram_add_test(...)`):

- `tst_music_library_json` — encode/decode, skipped entries, newer schema,
  corrupt root, duplicate ids, limits, null optionals.
- `tst_music_library_store` — atomic replace, backup-before-overwrite, ≤ 3
  backups, quarantine on corruption/foreign account, clear removes temp + backups.
- `tst_music_repository` — write-then-publish, failed save leaves state, account
  switch placeholder, cover root enforcement, delete-unreferenced, import.
- `tst_music_playback_window` — window/full-resolve/shuffle/pages/pager.
- `tst_music_smart_collections` — ordering, dedupe, 30-day rule, limit 50.
- `tst_music_replacement` — normalisation, duration delta 3 s, candidate order.
- `tst_bgmusic_roundtrip` — writer→reader equality, deterministic bytes, cover
  entry name per magic bytes.
- `tst_bgmusic_security` — bad entry names, `..`, drive letters, duplicate
  (case-insensitive) entries, second cover, nested archives, > 8 entries,
  compressed/uncompressed budgets, manifest > 2 MiB, cover > 1 MiB,
  non-UTF-8, duplicate JSON keys, depth 16, bad signature, `Unsupported`.
- `tst_bgmusic_writer_validation` — bad UUID, empty title, out-of-range
  revision/duration, > 5 000 tracks, > 8 sources, unsupported cover.
- `tst_bgmusic_import_planner` / `tst_bgmusic_library_mapping` — revision
  identity, favourites excluded, hint rules (account, public chat, low bits 0,
  post number range), file-name sanitising.
- `tst_bgmusic_fuzz` — deterministic pseudo-random corpus (fixed seed) for the
  reader; assert every input yields a result and bounded memory.

Exit criteria: Android v1 vectors and the Android round-trip/security/fuzz
expectations reproduce exactly; a hostile archive is rejected without
unbounded allocation; no audio backend is required to run the suite.

### Phase B — queue, player and session (needs T1/T3/T4 from Phase C, or a fake source)

New files:

```
src/media/playback/
  PlaybackItem.h/.cpp      origin, key/occurrenceId, systemPrivate, systemMediaId,
                           metadata, account scoping
  PlaybackQueue.h/.cpp     queueForPlayback, appendMusicPage, playNext, reorder/remove
  PlaybackState.h/.cpp     playing/intentPlaying, buffering, canNext/canPrevious,
                           fraction, timeline availability
  PlaybackPolicy.h/.cpp    canGoNext/Previous, resumePosition, interruptedMusic,
                           prefetchCandidates, shouldHandleEnded, intent rules
  PlaybackShuffleOrder.h/.cpp
  MusicQueueModel.h/.cpp   QAbstractListModel for QML (original order + playback order)
  PlaybackController.h/.cpp  QMediaPlayer/QAudioOutput, queue load, next/previous,
                           seek, speed, volume, sleep timer, signals
  MusicSessionStore.h/.cpp   JSON per account under dataDirectory, ≤ 80 origins
  MusicSessionRestorer.h/.cpp re-resolve via repository, ≤ 3 in flight, paused restore
  PlaybackServiceWindows.h/.cpp  SMTC bridge (ITfThreadMgr / SystemMediaTransportControls)
```

QML:

```
qml/pages/MyMusicPage.qml            favourites, recent, downloaded, playlists, smart
qml/pages/MusicPlaylistPage.qml      list, reorder, sort, cover, play
qml/pages/MusicSearchPage.qml        own messages + (later) bot results
qml/components/NowPlayingBar.qml     mini player
qml/components/MusicPlayerSheet.qml  full player: artwork, seek, modes, speed, queue
qml/components/MusicQueueView.qml    queue rows, drag handle, play-next/remove
qml/components/AddToPlaylistSheet.qml
qml/components/TrackArtwork.qml
qml/components/EqualizerSheet.qml    only if a DSP backend exists; otherwise a
                                     truthful "not available" card
```

Wiring: register `MusicPlayerController` and `MusicLibraryController` as QML
singletons in `src/app/main.cpp` next to `BeyGramPlayback`; add a Music nav
entry in `qml/Shell.qml`; extend `AppController` with
`musicLibrary()`, `musicPlayer()`, `openMusicPlaylist(id)`, `importBgMusic(url)`.

Platform decisions to record in the doc/PR:

- `QMediaPlayer` covers MP3/AAC/WAV through Media Foundation; FLAC/OGG/Opus
  depend on installed codecs. Do not claim support before a device matrix is
  run; fall back to BG Media decoding if it is linked.
- Media keys and the lock-screen/taskbar panel are **not** provided by Qt
  Multimedia. They need a Windows SMTC bridge (`ISystemMediaTransportControls`),
  which is a separate slice with its own manual test.
- An equalizer cannot be attached to `QMediaPlayer`. Either route audio through
  a custom `QAudioSink`/BG Media graph or ship the equalizer as unavailable.

Tests: `tst_playback_queue` (sanitising, dedupe, play-next order, reorder),
`tst_playback_order` (shuffle cloneAndInsert/Remove, page append keeps order),
`tst_playback_policy` (next/previous, 3 s restart, ended token, intent after
focus loss), `tst_music_session_store` (origins only, ≤ 80, corrupt file
dropped), `tst_music_queue_model` (roles, index stability under shuffle).

Exit criteria: a playlist plays, pauses, seeks, next/previous, shuffle, repeat,
speed and sleep timer with a fake source; restore is silent and paused; a
secret item never enters the queue, model or SMTC.

### Phase C — TDLib audio plumbing

Files to touch:

- `src/telegram/TelegramTypes.h/.cpp`: add `AudioMeta { title, performer, album,
  durationSeconds, mimeType, sizeBytes, fileId, albumCoverFileId }` to
  `MessageData`; parse `messageAudio` fully; keep the existing thumbnail/full
  file ids.
- `src/telegram/MessageListModel.cpp`: expose `audioTitle`, `audioPerformer`,
  `audioAlbum`, `audioDuration`, `audioFileId`, `audioCoverFileId` roles.
- `src/telegram/TelegramRequests.h/.cpp`: `searchChatMessagesRequest(chatId,
  query, fromMessageId, limit, SearchMessagesFilterAudio)`,
  `getMessageRequest(chatId, messageId)`,
  `downloadFileRequest(fileId, priority, offset, limit, synchronous)`.
- `src/telegram/TelegramRepository.h/.cpp`: `requestMusicPage(chatId,
  fromMessageId, limit)`, `requestMessage(chatId, messageId)`,
  `musicPageLoaded` signal, per-account guard on every response.
- `src/telegram/MediaFileStore.h/.cpp`: key files by `(accountId, fileId)`,
  add `progress(accountId, fileId)`, `cancel`, and an explicit
  `trackFullFile(file, priority)` that does not rely on the 120 s default.
- `src/telegram/AutoDownloadPolicy.h/.cpp` (if not already ported): policy
  gate for prefetch; prefetch must never use the manual download path.

Tests: `tst_audio_messages` (metadata, cover id, absent fields),
`tst_music_origin` (re-resolution drops gone/protected/foreign/non-audio),
`tst_music_page_merge` (dedupe, cursor advance, no cross-chat/topic bleed),
`tst_media_file_scope` (same numeric id in two accounts never collides).

Exit criteria: a real chat's audio messages form an ordered, paginated queue
newest → oldest; a deleted message disappears on restore; no path or file id
reaches logs.

### Phase D — BG Media in-process

Files:

```
cmake/BeyGramBgMedia.cmake        link bgmedia_core (+ the Windows device adapter)
                                  when third_party/bg-media is present; define
                                  BEYGRAM_HAS_BGMEDIA and a version string
src/media/bgmedia/BgMediaEngine.h/.cpp   BG Beat analyse(file, cacheDir, min/max BPM,
                                          cancel, progress) -> result struct
src/media/bgmedia/BgMediaMix.h/.cpp      queue add/remove/clear, play/pause/next/stop,
                                          seek, 50 ms update, render into a float buffer,
                                          status snapshot
src/media/bgmedia/BgMediaSettings.h/.cpp enabled flags + cache quota (50 MiB)
src/media/BgMediaStatus.cpp              real version/schema/analyzer/capabilities
src/ui/BgBeatController.h/.cpp           QML-facing state machine (Disabled/Idle/
                                          Ineligible/Analyzing/Ready/Error)
src/ui/BgMixController.h/.cpp            QML-facing status + transport
```

Key design points:

- No JNI. Kotlin's `BgMixPlayer`/`BgBeatManager` are replaced by direct C++
  objects owned by `AppController`, with Qt signals instead of `StateFlow`.
- Audio output: BG Mix renders interleaved stereo float at 48 kHz. On Windows,
  prefer BG Media's own device adapter if the pinned commit provides a WASAPI/
  miniaudio desktop backend; otherwise pull frames into a `QAudioSink`
  (`QAudioFormat::Float`, 48 kHz, stereo) on a dedicated thread. Record the
  decision and the licence in `THIRD_PARTY_NOTICES.md`.
- Cancellation: port `CancelRegistry` as a small C++ registry keyed by
  `quint64`, with `shared_ptr<atomic_bool>`; no raw pointers across threads.
- Cache: per-account directories under `AppPaths::cacheDirectory()`, 50 MiB
  quota with oldest-first eviction, cleared per account.
- Truthfulness: if the core is missing, `BgMediaStatus` must say so with the
  real reason, and BG Mix/BG Beat controllers must expose `Disabled` rather
  than a silent fallback that looks like playback.

Tests: `tst_bgmedia_capability` (skip when not linked; assert the reported
version is real), `tst_bgbeat_analysis` (deterministic small WAV → BPM/beats
sanity, cancellation, cache hit), `tst_bgmix_queue` (id mapping, state machine,
transition flags with a null render), `tst_bgmedia_cache` (quota, eviction,
per-account isolation).

Exit criteria: with the core linked, BG Beat analyses a local track with
progress and cancellation and caches the result; BG Mix plays a 3-file queue
with a planned transition and truthful status; with the core absent, both
report unavailable and music still plays through Phase B.

---

## 5. Privacy and security parity requirements

These are not optional polish; they are part of the feature contract:

1. Secret attachments, secret chats and view-once media never enter a queue, a
   saved session, a model, a notification, SMTC metadata or a log.
2. Every identity is account-scoped: `(accountId, chatId, messageId)` or
   `(accountId, fileId)`. A bare TDLib numeric id is never treated as global.
3. `.bgmusic` reading enforces limits while reading bytes, never from declared
   sizes; no nested archives; no traversal; no duplicate (case-insensitive)
   entries; strict UTF-8; strict JSON; cover magic bytes must match the entry
   name.
4. Export writes only the user's own text plus what the user explicitly
   consented to (`includePublicLinks`, `includeAuthor`); the audio hash is never
   written; a non-UUID local id is replaced by a fresh or name-based UUID so no
   Telegram data leaks into a file.
5. A library file from a newer schema is never overwritten; a corrupt or foreign
   file is moved aside, never deleted silently.
6. Logs never contain file system paths, message text, titles or account ids;
   use the existing `core/Redaction` helpers.
7. Import never merges, replaces or saves silently: the decision is shown and
   the user chooses.

---

## 6. Priority and sequencing

| Slice | Priority | Depends on | Why |
|---|---|---|---|
| A — library + `.bgmusic` pure port | **P2** (pull forward) | nothing | No audio device, no TDLib audio; high test value; import/export/preview is real user value |
| C — TDLib audio metadata + file scoping | **P2** | Phase 3 media | Also improves the chat bubble (artist/title/duration/artwork) |
| B — queue/player/session | **P3** | A + C | The audible feature; needs the platform decisions above |
| D — BG Beat / BG Mix | **P3** | B + BG Media availability | Blocked on a non-empty `third_party/bg-media` and a Windows audio backend decision |

The audit table in `docs/PORTING_AUDIT.md` lists `M14` (library + `.bgmusic`)
and `M15` (player/BG/equalizer) as P3 with an estimated 2-3 engineer-months
together. That estimate stands; Phase A is the part that can be delivered
sooner and should be scheduled at the end of Phase 3 rather than waiting for the
whole Phase 4.

Do not start D before the BG Media submodule is pinned and its Windows desktop
backend is known to build; otherwise the work degenerates into another stub.

---

## 7. Risks and open questions

1. **BG Media availability.** `third_party/bg-media` is empty and the Android
   build falls back to a token-based `FetchContent` of a pinned commit. Confirm
   the submodule can be initialised for the Windows build and that its CMake
   supports a desktop target (`BGMEDIA_BUILD_DESKTOP` exists upstream but is
   unverified here).
2. **Windows audio output for BG Mix.** BG Mix needs a 48 kHz float stereo sink
   and a control tick. `QAudioSink` pull is the least risky path; the upstream
   device adapter may be better but must be evaluated and licensed.
3. **Codec coverage.** `QMediaPlayer` relies on Media Foundation codecs. OGG/
   Opus (Telegram voice and some music) and FLAC may be missing on a clean
   Windows install. Decide the fallback before promising format parity.
4. **Media keys / SMTC.** Not part of Qt Multimedia; needs a dedicated COM
   bridge and a manual test on Windows 10 and 11.
5. **Equalizer.** Impossible on top of `QMediaPlayer`; either custom DSP or an
   honest "unavailable on this build" state. Do not fake it.
6. **Growing-file playback.** Android can start before a download completes.
   Windows can ship complete-file-only first, but the UI must then show
   buffering/downloading instead of a false "cannot play".
7. **ZIP implementation.** Qt has no public streaming ZIP reader. Choose
   minizip/zlib (already a transitive dependency of many stacks) or an in-house
   scanner, and keep the same byte-level limits and error taxonomy.
8. **Multi-account.** The Windows account registry exists; the library, caches,
   session store and BG caches must be keyed by account from day one, not
   retrofitted.

---

## 8. Manual verification checklist (not executed)

For the eventual acceptance run, not for this audit:

- Import a `.bgmusic` exported by Android; verify identical title, description,
  order, durations and the same `playlist_id`/`revision` decision.
- Re-import the same file → "same revision"; edit locally → export → re-import
  on a second account → "newer revision" with merge/replace/copy choices.
- Feed a renamed ZIP, a truncated archive, a 5 MiB archive, an archive with
  `../` entries, a duplicate entry, a second cover and a manifest with duplicate
  keys; each must produce the mapped reason and leave no temp file.
- Play a 100-track playlist: window load, page append at the boundary, no
  duplicate rows, no mass download, position preserved on append.
- Kill the app mid-track, relaunch: paused at roughly the previous position, no
  audio, no speculative download, then Play works.
- Secret chat audio: no notification, no SMTC metadata, stops on leaving the
  chat; view-once audio never queued.
- Account switch: no cross-account queue, artwork, history or session.
- With BG Media linked: BG Beat progress/cancel/cache; BG Mix 3-track queue with
  a transition; status matches audible output. With it absent: both report
  unavailable and ExoPlayer-equivalent path still plays.
- Media keys, taskbar panel and a Bluetooth headset: play/pause/next/previous/
  seek stay in sync (only after the SMTC slice lands).

---

## 9. Non-claims

- No CMake configure, build, QML preview, unit test or device test was run for
  this audit.
- Nothing here confirms that the Windows client currently plays audio, links BG
  Media, or renders any music UI.
- Android behaviour is described from source. Device-specific rendering
  (Media3 codec support, `AudioTrack` output, lock-screen/Bluetooth behaviour,
  equalizer effect) was not observed on a device.
- The Android BG Beat/BG Mix Kotlin layers are complete, but the native BG Media
  core is absent from this clone; the JNI stubs' real-device behaviour was not
  exercised.
- Priorities and phase order are proposals for the porting sequence, not
  commitments.

