# Parity 05 — Media download, cache and thumbnails

Read-only parity audit of the Android media download/cache/thumbnail/auto-download/limit
behaviour against the current Windows client.

- **Scope:** file state tracking, file selection, download priorities, auto-download
  policy and settings, TDLib projection, thumbnails, retry/error/cancel, traffic
  accounting, cache limits, cleanup and storage UI.
- **Android reference:** `app/src/main/java/com/beykus/beygram/...` (source of truth).
- **Windows target:** `src/telegram`, `src/ui`, `qml`, `tests`.
- **Method:** source reading only. No build, CMake configure, test run, GUI run or
  git operation was performed for this document. Statements below describe code as
  written, not verified runtime behaviour.

| | |
|---|---|
| Audit date | 2026-11-11 |
| Android reference | `data/media/*`, `data/tdlib/{MediaFileStore,TdMediaMapper,TdStorage,StorageCleanupRules,TdCacheMaintenance,ListedMediaFiles,TelegramNetworkMonitor}`, `data/settings/AutoDownloadSettingsStore`, `data/storage/*`, settings screens |
| Windows target | `src/telegram/MediaFileStore.*`, `src/telegram/TelegramRepository.*`, `src/ui/MediaImageProvider.*`, `src/ui/MediaViewerController.*`, `src/ui/MediaPlaybackController.*`, `qml/components/*`, `tests/*` |
| Overall Android status | Complete: stateful file store with manual/auto origins, per-account auto-download policy, TDLib projection, traffic accounting, storage usage/limits/cleanup with a 6-hour maintenance pass |
| Overall Windows status | Early: a path-only cache that requests `downloadFile` for avatars and message thumbnails, serves images to QML and opens one downloaded file in the viewer. No progress, error, cancel, auto-download policy, network awareness, storage limits or cleanup |
| Priority | **P0/P1** — `M06` in `docs/PORTING_AUDIT.md`; download state is a prerequisite for the viewer (`docs/parity/06-media-viewer-playback.md`) |
| Related audits | `docs/parity/06-media-viewer-playback.md` (viewer/playback UI), `docs/parity/14-albums-attachments.md` (outgoing media), `docs/parity/18-music-bgmedia.md` (music prefetch), `docs/parity/10-appearance-settings.md` (settings framework) |

The viewer and player UI themselves are out of scope here and are covered by audit 06;
this document covers the download/cache mechanics they depend on.

Status legend used in this document:

| Mark | Meaning |
|---|---|
| Present | Implemented on Windows and structurally comparable to Android |
| Partial | Some of the behaviour exists, important parts are missing |
| Missing | No Windows implementation found |
| N/A | Deliberately not applicable on Windows |

---

## 1. Android files reviewed

### 1.1 Download state and file lifecycle

| File | What it defines |
|---|---|
| `data/tdlib/MediaFileStore.kt` | Per-file `MediaFileState` flows, history-safe registration, download begin/cancel/fail, manual-vs-auto origin, traffic hooks |
| `data/tdlib/TdMediaMapper.kt` | `TdApi.File.toMediaFileState()`, photo full/preview selection, thumbnail registration for every media kind |
| `data/tdlib/ListedMediaFiles.kt` | 30-minute protection window for files opened from shared media |
| `data/tdlib/TdLibTelegramRepository.kt` | `downloadFile`, `downloadFileInBackground`, `autoDownloadFile`, `cancelFileDownload`, `UpdateFile` routing, auto-download cancellation/re-evaluation, TDLib settings sync |
| `domain/model/MessageMedia.kt` | `MediaFileState` (size, localPath, downloadedBytes, downloadPath, downloadedPrefixBytes, upload fields), `AutoDownloadRequest` |
| `domain/repository/TelegramRepository.kt` | Repository contract for `observeFile`, `downloadFile`, `downloadFileInBackground`, `autoDownloadFile`, `cancelFileDownload` |
| `domain/repository/StorageRepository.kt` | `storageUsage`, `clearStorage(categories, progress)`, `applyCachePolicy` |

### 1.2 Auto-download policy, network and settings

| File | What it defines |
|---|---|
| `data/media/AutoDownloadPolicy.kt` | Network classes, categories, rules, defaults, `decideAutoDownload`, denial reasons, `MediaKind` → category |
| `data/media/AutoDownloadNetwork.kt` | Transport classification (Wi-Fi/metered Wi-Fi/mobile/roaming/other/none), VPN underlying-network handling, process-wide monitor, `AutoDownloadRuntime` gate |
| `data/media/AutoDownloadTdlibMapping.kt` | Projection of local rules onto `TdApi.AutoDownloadSettings` |
| `data/settings/AutoDownloadSettingsStore.kt` | Per-account persisted rules (SharedPreferences), defaults, corrupt-value fallback |
| `data/tdlib/TelegramNetworkMonitor.kt` | Default-route observation and `SetNetworkType` pushes |
| `ui/screens/settings/AutoDownloadSettingsScreen.kt` | Network tabs, per-category switch + limit picker (1/2/5/10/20/50/100 MB/unlimited), data saver, traffic summary |

### 1.3 Thumbnails, traffic and cache

| File | What it defines |
|---|---|
| `data/media/TrafficUsage.kt` | `TrafficOrigin`, immutable snapshot, high-water-mark accumulator, per-account `TrafficUsageStore` |
| `data/media/TrafficUsageFormat.kt` | Traffic size formatting |
| `data/tdlib/StorageCleanupRules.kt` | TDLib file type → `StorageCategory`, `optimizeStorage` request, clear-all-of-type request, int53/int32 limits |
| `data/tdlib/TdStorage.kt` | Usage statistics, per-category clear with progress, policy application, sent-upload cache eviction, `deleteFile` + `getFile` verification, owner markers |
| `data/tdlib/TdCacheMaintenance.kt` | Immediate + 6-hour policy application while authorized |
| `data/storage/StorageSettings.kt` | Size limits 1/2/5/10 GB/unlimited, retention 3/7/30 days/forever, default 10 GB + forever |
| `data/storage/StorageSettingsStore.kt` | Persisted storage settings with rollback on failed write |
| `ui/screens/settings/StorageSettingsScreen.kt` | Usage card, category bar, per-category selection, clear with progress, limit/retention pickers |
| `ui/screens/settings/StorageSettingsViewModel.kt` | Usage load, clear, apply policy, save failure handling |

### 1.4 Tests used as expected values

| Test | Covers |
|---|---|
| `test/.../MediaFileStoreTest.kt` | Stale history, shared requests, retry after failure, bounded progress, cancel, partial state, snapshot |
| `test/.../MediaFileStoreTrafficTest.kt` | Auto/manual attribution, takeover, resume watermarks, cancel settlement |
| `test/.../data/media/AutoDownloadPolicyTest.kt` | Per-network limits, data saver, secret/spoiler/offline/unknown-size denials, sticker bypass, unlimited |
| `test/.../data/media/AutoDownloadNetworkTest.kt` | Transport classification |
| `test/.../data/media/AutoDownloadTdlibMappingTest.kt` | TDLib projection, disabled network, data saver, int clamp, three networks |
| `test/.../data/media/TrafficUsageTest.kt` | No double counting, resume, takeover, terminal progress, per-account sessions |
| `test/.../data/tdlib/StorageCleanupRulesTest.kt` | Category mapping, no-limit policy, size/age limits, clear request |
| `test/.../data/tdlib/ListedMediaFilesTest.kt` | 30-minute protection expiry |

---

## 2. Android behaviour reference

### 2.1 File state model

`MediaFileState` carries `id`, `size` (`max(size, expectedSize)`), `localPath` (only
when `isDownloadingCompleted`), `downloadedBytes`, `isDownloading`
(`local.isDownloadingActive`), `uploadedBytes`, `isUploading`, `error`, plus
`downloadPath` and `downloadedPrefixBytes` for progressive playback. Progress is a
clamped float and guards `size == 0`.

Two write paths exist:

- `register(file)` — history snapshots; only applies when the id is still unknown, so a
  late history page can never roll back a newer `updateFile` result.
- `update(file)` — live TDLib state; always wins.

`MediaFileStore` keeps `origins` for in-flight transfers and a `settled` set for
cancelled transfers whose terminal update may still add bytes.

### 2.2 Download entry points and ownership

| Entry point | Policy gate | Origin |
|---|---|---|
| `downloadFile(fileId)` | none (manual) | Manual; takes over a running automatic transfer |
| `downloadFileInBackground(fileId)` | none (avatars/thumbnails) | Auto, counted as background traffic |
| `autoDownloadFile(request)` | full `decideAutoDownload` | Auto |
| `cancelFileDownload(fileId)` | none | Sends `CancelDownloadFile(onlyIfPending=false)` and hides the transfer |

`beginDownload` returns `false` when the file is complete or already downloading, so
duplicate UI requests share one TDLib transfer. A manual tap on an automatic transfer
promotes the origin; bytes already counted stay automatic.

`sendDownload` uses `DownloadFile(fileId, priority = 16, offset = 0, limit = 0,
synchronous = false)`. A `TdApi.Error` records a diagnostic event and sets a
user-visible `err_media_download_failed`; a cancelled transfer is cancelled instead of
failed, so a manual retry stays possible.

`UpdateFile` routing:

1. `mediaFiles.update(state)`.
2. If the file stopped downloading with no local path and it was cancelled, mark cancelled.
3. If it belongs to a tracked auto-download and its size changed, refresh the request and
   re-evaluate the policy (TDLib sometimes reports the real size only after the request).

### 2.3 Network classification

`AutoDownloadNetwork` = `Wifi`, `WifiMetered`, `Mobile`, `Roaming`, `Other`, `None`.
Only `Wifi` is unmetered; `Other` (external VPN, Ethernet, unprovable transport) follows
the mobile policy. For a VPN, only its own underlying networks are authoritative; if no
Wi-Fi/cellular transport can be proven the route is `Other`. The local Mihomo SOCKS
listener does not change the classification.

`AutoDownloadNetworkStatus` is a process-wide flow fed by a default-network callback.
`reevaluateAutoDownloads` cancels automatic transfers that the new network/settings no
longer allow.

`TelegramNetworkMonitor` separately pushes `SetNetworkType` (`networkTypeWiFi`,
`networkTypeMobile`, `networkTypeMobileRoaming`, `networkTypeOther`, `networkTypeNone`)
to TDLib.

### 2.4 Rules, categories and defaults

Categories: `Photos`, `Videos`, `Files`, `Music`. Mapping:
`Photo→Photos`; `Video/Animation/VideoNote→Videos`; `Audio/Voice→Music`;
`Document/Sticker→Files` (stickers bypass the rules entirely).

`decideAutoDownload` order: secret content → no network → sticker allow → secret chat →
spoiler → data saver on metered → unknown size (`<= 0`) → rule disabled → size above
`maxBytes` → allow.

Defaults:

| Network | Photos | Videos | Files | Music |
|---|---|---|---|---|
| Wi-Fi | 10 MB | 50 MB | 10 MB | 20 MB |
| Mobile | 1 MB | off (5 MB) | off (1 MB) | off (5 MB) |
| Roaming | off | off | off | off |
| Data saver | off on all metered networks | | | |

Rules are stored per Telegram account; a corrupt negative `max_bytes` falls back to the
default instead of becoming unlimited.

### 2.5 TDLib projection

`AutoDownloadTdlibMapping.toTdlib` sends only limits and preload switches, never content
or credentials. TDLib has one "other" bucket, so the larger enabled limit of
`files`/`music` is sent; the exact per-category decision stays local. `preloadStories` is
always `false`, `useLessDataForCalls` follows data saver. Three networks are configured
(`networkTypeWiFi`, `networkTypeMobile`, `networkTypeMobileRoaming`). The template is
`GetAutoDownloadSettingsPresets().high`.

### 2.6 Thumbnails and file selection

- Photo: full file = largest size by area; preview = smallest size whose long edge is
  ≥ 240 px, else the smallest size.
- Video/animation/document/audio/video note/sticker: TDLib `thumbnail`
  (`album_cover_thumbnail` for audio); sticker thumbnails are only accepted as WebP/JPEG.
- Minithumbnail bytes are kept for instant blur placeholders.
- Thumbnails are requested by visible composables through the same auto-download gate
  (`MediaViews` `LaunchedEffect`), while avatars/previews use
  `downloadFileInBackground` and bypass size rules.
- The image layer reads `localPath`; while only a prefix exists, `downloadPath` +
  `downloadedPrefixBytes` feed the progressive player.

### 2.7 Traffic accounting

`TrafficUsageAccumulator` counts only bytes above each transfer's high-water mark, so
repeated/decreasing progress cannot double count. `begin` records the origin and an
optional baseline; `finish` hides a transfer but keeps its watermark for the terminal
update; `release` forgets it. `promote` upgrades an automatic transfer to manual without
moving already-counted bytes. Sessions are per account.

### 2.8 Cache limits, cleanup and protection

- Defaults: 10 GB cap, no age-based deletion. User choices: 1/2/5/10 GB/unlimited and
  3/7/30 days/forever.
- `TdStorage.usage()` = TDLib `GetStorageStatistics` per category +
  `GetStorageStatisticsFast` (database, language packs, logs) + on-disk upload staging.
- `clear(categories)` deletes one category at a time so progress is truthful; empty file
  type lists are never sent (TDLib would treat them as defaults).
- `applyPolicy` uses a `latestOnly` lane so a stale policy cannot run after a newer one;
  size/age limits are applied with `immunity_delay = -1` (new files protected).
- Files shown by loaded chats (`MessageHistoryStore.mediaFileIds()`) and files opened
  from shared media in the last 30 minutes (`ListedMediaFiles`) are protected from
  sent-upload eviction.
- Sent upload copies are evicted by age and size with a 10-minute in-use grace; folders
  with only an owner mark are removed after 24 hours. Each eviction asks TDLib to
  `deleteFile` and verifies with `getFile` before deleting the folder.
- `TdCacheMaintenance` applies the policy immediately after authorization and every
  6 hours; a failure is logged and does not stop later passes.

---

## 3. Current Windows status

### 3.1 What exists

| Area | Windows implementation | Status |
|---|---|---|
| File path cache | `src/telegram/MediaFileStore.{h,cpp}`: `QHash<int, QString> m_paths`, `QSet<int> m_requested`, revision counter | Partial |
| Download request | `MediaFileStore::requestDownload` sends `downloadFile` with `priority` clamped 1–32, `offset 0`, `limit 0`, 120 s timeout | Partial |
| Update handling | `MediaFileStore` connects directly to `TelegramSession::updateReceived` and reacts only to `updateFile` where `is_downloading_completed == true` | Partial |
| Priorities | Avatars `8`, message thumbnails `16`, viewer/playback `32`; default `16` | Present (tiering differs from Android, which uses 16 for all) |
| Thumbnail selection | `TelegramTypes.cpp`: photo preview scored around 320 px, full = largest area; other kinds use `thumbnail`; link previews use smallest ≥ 240 px | Present |
| Image serving | `src/ui/MediaImageProvider.{h,cpp}`: reads local path, auto-transform, width-based scaling, circular mask for `round=1` | Present |
| Still viewer | `src/ui/MediaViewerController.{h,cpp}`: requests priority 32, exposes `image://beygrammedia/<id>?v=<revision>`, save-as via `QSaveFile` | Present |
| Playback | `src/ui/MediaPlaybackController.{h,cpp}`: local files only; `AppController::openMediaPlayback` requests priority 32 and waits for `fileReady` | Present |
| Ready propagation | `TelegramRepository` marks chat avatar and message media ready; `MessageListModel::markMediaReady`, `ChatListModel::markPhotoReady` | Present |
| TDLib file storage | Per-account `files_directory`; `enable_storage_optimizer = true` | Partial (library defaults only) |
| Tests | `tests/tst_media_image.cpp` (2 cases), `tests/tst_media_viewer.cpp` (11 cases), `tests/tst_media_playback.cpp` | Partial |

### 3.2 What the Windows store actually does

`MediaFileStore` is a path cache, not a file-state store:

- `trackFileObject` reads `local.path` + `is_downloading_completed`; complete files go
  straight into `m_paths`, everything else becomes a `downloadFile` request.
- `trackFileId` skips ids already in `m_paths` and otherwise requests a download.
- `handleUpdate` only accepts `updateFile`; `handleFile` returns immediately unless the
  download is completed and the path is non-empty.
- A `downloadFile` error (including the transport's synthetic timeout error) only clears
  `m_requested`; no signal, no error text, no retry affordance.
- `setSession` disconnects/reconnects the update signal but does not clear `m_paths` or
  `m_requested`, so state survives logout and account switches.
- `m_paths` is unbounded and a path is never invalidated when TDLib forgets/deletes a
  file, so `path()`/`isReady()` can keep returning a file that no longer exists.

`TelegramUpdateScheduler::isIgnored` explicitly drops `updateFile`, so all file traffic is
consumed only by `MediaFileStore` on the session signal.

### 3.3 Where downloads are triggered

| Trigger | Priority | Policy |
|---|---|---|
| Chat avatar (`TelegramRepository` line ~296) | 8 | none |
| Message thumbnail at history parse (`trackMessageMedia`) | 16 | none, for every parsed message, including off-screen history |
| Still viewer open (`MediaViewerController::open`) | 32 | none |
| Playback (`AppController::openMediaPlayback`) | 32 | none |
| Link preview thumbnail | — | **never tracked/downloaded** |
| Sticker picker/bubble thumbnail | — | **never tracked/downloaded** |

There is no user-facing "download" or "cancel" action; clicking a ready thumbnail opens
the viewer/player, which then requests the full file at priority 32.

---

## 4. Gap analysis

| ID | Gap | Evidence | Severity |
|---|---|---|---|
| G01 | No file-state model exposed to the UI: only `fileReady`/`revisionChanged`, no size, downloaded bytes, progress, upload state or partial path | `MediaFileStore.h` has `path`, `isReady`, `revision` only | High |
| G02 | Download failures are silent; the bubble spinner (`BusyIndicator` on `!mediaReady`) can run forever | `requestDownload` error branch only removes `m_requested`; no `fileFailed` | High |
| G03 | No cancellation; a user cannot stop a large automatic/full-size download | no `cancelDownloadFile` anywhere in `src/telegram` | High |
| G04 | Stale paths are never invalidated; deleted/forgotten TDLib files stay "ready" and render blank | `handleFile` early-returns on `!is_downloading_completed`; `m_paths` never erased | High |
| G05 | Every thumbnail in every parsed history page is downloaded regardless of visibility, network or size | `TelegramRepository::trackMessageMedia` called from all history/update paths | High |
| G06 | No network classification, metered/roaming handling, VPN underlying-network logic or data saver | no `QNetworkInformation`, no network enum | High |
| G07 | No auto-download settings, store, UI or TDLib projection | no `AutoDownload*` equivalent; `getAutoDownloadSettingsPresets`/`setAutoDownloadSettings` unused | High |
| G08 | No storage usage screen: no per-category bytes, database bytes or upload bytes | `getStorageStatistics`/`getStorageStatisticsFast` unused | High |
| G09 | No app-enforced cache size/retention limits; only TDLib's default optimizer | `enable_storage_optimizer = true` only | High |
| G10 | No protection set for in-use files (loaded chats, recently opened shared media) before any cleanup exists | no `mediaFileIds` equivalent | Medium (blocks G09) |
| G11 | No traffic accounting (auto vs manual bytes, active transfer count) | no `TrafficUsage` equivalent | Medium |
| G12 | TDLib is never told the network type, so its own preloading/auto-download behaviour is unaware of metered/roaming | no `setNetworkType` call | Medium |
| G13 | Link preview thumbnails never get a `downloadFile`, so `LinkPreviewCard` can render an empty image | `linkPreviewFileId` set but never tracked | Medium |
| G14 | Sticker thumbnails never get a `downloadFile`; picker/bubble images stay blank unless TDLib preloaded them | no tracking in `StickerSetModel`/repository | Medium |
| G15 | Media state survives logout/account switch; file ids are account-scoped | `setSession` does not reset `m_paths`/`m_requested` | High (multi-account) |
| G16 | Unbounded memory in `m_paths`/`m_requested` for long sessions | no eviction or cap | Low |
| G17 | No progressive playback/streaming from a partial file (`downloadPath` + `downloadedPrefixBytes`) | `MediaPlaybackController` requires a complete local file | Low |
| G18 | Outgoing media has no size/duration validation before send (Android caps photo at 10 MB, video at 2 GB/1 h) | `MediaSend::isSupportedMediaPath` checks extension only | Low (adjacent) |

Deliberate or acceptable differences (see §8): no upload staging folder on Windows, so
Android's sent-upload eviction has no counterpart; no Android `SharedPreferences`
migration is needed.

---

## 5. Implementation plan

Ordering is dependency-first: state model → policy → settings → TDLib wiring → storage →
UI. Each phase is independently testable with Qt Test and needs no real Telegram account.

### Phase M1 — File state store (P0)

**Files**

- `src/telegram/MediaFileState.h` (new): `MediaFileState` struct
  (`id`, `size`, `localPath`, `downloadedBytes`, `isDownloading`, `uploadedBytes`,
  `isUploading`, `downloadPath`, `downloadedPrefixBytes`, `error`), `downloadProgress()`,
  `uploadProgress()`.
- `src/telegram/MediaFileStore.{h,cpp}` (rewrite): `QHash<int, MediaFileState>` guarded by
  the existing `QReadWriteLock`; `QSet<int> m_requested`; `reset()`.
- `src/telegram/MediaTrafficUsage.{h,cpp}` (new, see M2) is injected so `update()` can
  account bytes.

**API**

```cpp
enum class MediaDownloadOrigin { Manual, Auto };

struct MediaFileState { /* as above */ };

void trackFileObject(const QJsonObject& file, int priority = 16);
void trackFileId(int fileId, int priority = 16);
bool beginDownload(int fileId, MediaDownloadOrigin origin);
bool beginAutoDownload(int fileId);
void cancelDownload(int fileId);
void fail(int fileId, const QString& message);
void reset();

[[nodiscard]] MediaFileState state(int fileId) const;
[[nodiscard]] bool isReady(int fileId) const;
[[nodiscard]] QString path(int fileId) const;
[[nodiscard]] int revision() const;

signals:
    void fileStateChanged(int fileId);
    void fileReady(int fileId);
    void fileFailed(int fileId, const QString& message);
    void revisionChanged();
```

**Behaviour to port**

- `register`-equivalent: history snapshots must not overwrite a known id.
- Accept every `updateFile`; map `local.path`, `is_downloading_completed`,
  `is_downloading_active`, `downloaded_size`, `downloaded_prefix_size`, `remote.uploaded_size`,
  `remote.is_uploading_active` (JSON snake_case).
- When a completed file becomes incomplete/empty (TDLib deleted or forgot it), erase the
  path and emit `fileStateChanged` — this closes G04.
- `beginDownload` returns false for complete/already-downloading files; manual promotes a
  running automatic transfer.
- `cancelDownload` hides the transfer, keeps the partial state, and sends
  `cancelDownloadFile` (see M4 wiring); the terminal update must not resurrect it.
- `reset()` on `setSession(nullptr)` and on account change (G15).

**Tests:** `tests/tst_media_file_store.cpp`

- `staleHistoryCannotReplaceCompletedDownload`
- `concurrentRequestsShareOneDownload`
- `failureAllowsRetryAndClearsError`
- `progressIsBoundedAndUnknownSizeDoesNotDivideByZero`
- `cancelHidesTransferButKeepsPartialState`
- `cancelledFileCanBeRequestedAgain`
- `cancellingLeavesCompletedFileAlone`
- `deletedFileIsNoLongerReady`
- `resetForgetsPathsAndRequests`
- `stateAndSnapshotAgree`

### Phase M2 — Traffic accounting (P1, independent)

**Files:** `src/telegram/MediaTrafficUsage.{h,cpp}`.

**API**

```cpp
enum class MediaTrafficOrigin { Auto, Manual };

struct MediaTrafficSnapshot {
    qint64 autoBytes = 0;
    qint64 manualBytes = 0;
    QHash<int, MediaTrafficOrigin> activeOrigins;
    qint64 totalBytes() const;
    bool hasActiveDownloads() const;
};

class MediaTrafficSession {
public:
    void begin(int fileId, MediaTrafficOrigin origin, qint64 alreadyDownloaded = 0,
               bool preserveWatermark = false);
    void promote(int fileId);
    void progress(int fileId, qint64 downloadedBytes);
    void finish(int fileId);
    void release(int fileId);
    bool isActive(int fileId) const;
    bool hasTransfer(int fileId) const;
    MediaTrafficSnapshot snapshot() const;
signals: void changed();
};

class MediaTrafficUsageStore { static MediaTrafficSession* session(const QString& accountId); };
```

Port the high-water-mark rules exactly: no double counting, resume baseline, manual
takeover keeps already-counted automatic bytes, automatic begin never demotes a manual
transfer, terminal progress still counted after `finish`, negative progress treated as 0,
per-account sessions.

**Tests:** `tests/tst_media_traffic_usage.cpp` with the Android case names:
`repeatedAndDecreasingProgressNeverDoubleCounts`, `resumedAttemptStartsFromKnownBytes`,
`aNewAttemptAfterFinishCountsNewTrafficAgain`,
`manualTakeoverKeepsAlreadyCountedAutomaticBytes`,
`automaticBeginDoesNotDemoteAManualTransfer`,
`progressWithoutBeginIsCountedAsAutomaticBackgroundTraffic`,
`resumingWithALowerReportedValueKeepsTheHighWaterMark`,
`finishHidesTransferButKeepsWatermarkForTerminalProgress`,
`finishingOneTransferKeepsOtherTransfersActive`, `negativeProgressIsTreatedAsZero`,
`sessionsAreScopedByAccount`.

### Phase M3 — Download policy and network (P0)

**Files**

- `src/telegram/MediaDownloadPolicy.{h,cpp}` (pure, no Qt GUI/network).
- `src/telegram/MediaNetworkMonitor.{h,cpp}` (Qt Network).
- `src/telegram/CMakeLists.txt`: add files and link `Qt6::Network`.

**API**

```cpp
// New incoming-media enum: the Windows client currently carries only a
// `contentType` string (TelegramTypes) and `OutgoingMediaKind` (MediaSend), so the
// policy needs its own MediaKind and a `MediaKind kindOfContentType(const QString&)`
// mapping (messagePhoto, messageVideo, messageAnimation, messageSticker,
// messageAudio, messageDocument, messageVoiceNote, messageVideoNote).
enum class MediaKind { Photo, Video, Animation, Sticker, Audio, Document, Voice, VideoNote };

enum class MediaNetwork { Wifi, WifiMetered, Mobile, Roaming, Other, None };
enum class MediaCategory { Photos, Videos, Files, Music };

struct MediaDownloadRule { bool enabled = false; qint64 maxBytes = MaxBytes; bool allows(qint64) const; };
struct MediaDownloadNetworkPolicy { MediaDownloadRule photos, videos, files, music; /* rule(), with() */ };
struct MediaDownloadSettings {
    MediaDownloadNetworkPolicy wifi, mobile, roaming;
    bool dataSaver = false;
    static MediaDownloadSettings defaults();
    MediaDownloadNetworkPolicy policyFor(MediaNetwork) const;
    MediaDownloadNetworkPolicy effectivePolicyFor(MediaNetwork) const;
};

enum class MediaDownloadDenial { SecretContent, SecretChat, Spoiler, NoNetwork,
                                 DataSaver, SizeUnknown, Disabled, TooLarge };
struct MediaDownloadDecision { bool allowed; MediaDownloadDenial denial; };

MediaCategory categoryOf(MediaKind);
MediaDownloadDecision decideAutoDownload(const MediaDownloadSettings&, MediaNetwork,
                                         const AutoDownloadRequest&);
```

`MediaNetworkMonitor` wraps `QNetworkInformation`:

- `QNetworkInformation::loadDefaultBackend()`; `transportMedium()` → Wi-Fi/cellular/
  Ethernet/unknown; `isMetered()` → `WifiMetered`; `reachability()` → `None`.
- Windows does not expose roaming; classify roaming as `Mobile` and document the
  difference (see §8).
- Emit `networkChanged(MediaNetwork)`; expose `setNetworkTypeRequest(MediaNetwork)` so the
  repository can push `setNetworkType`.

`AutoDownloadRequest` on Windows: `{ int fileId; MediaKind kind; qint64 sizeBytes;
bool isSecret; bool isSecretChat; bool hasSpoiler; }` in `MediaDownloadPolicy.h`.

**Tests:** `tests/tst_media_download_policy.cpp` and `tests/tst_media_network.cpp`.
Policy cases mirror Android one-for-one:
`wifiPhotoWithinLimitIsAllowed`, `wifiVideoRespectsItsOwnLimit`,
`mobileVideoIsDisabledByDefault`, `roamingDownloadsNothingByDefault`,
`mobilePhotoAboveLimitIsRejected`, `dataSaverDisablesMeteredDownloadsButKeepsWifi`,
`secretAndSpoilerContentNeverAutoDownloads`, `unknownSizeIsNeverAssumedSmall`,
`offlineNeverDownloads`, `channelsMapToTheExpectedCategories`,
`stickersDownloadOnEveryNetwork`, `unlimitedRuleAllowsAnyKnownSize`,
`editingOneNetworkDoesNotChangeAnother`.
Network cases: Wi-Fi, metered Wi-Fi, cellular, unknown/Ethernet → `Other`, no route →
`None`, metered flag propagation (inject a fake capabilities snapshot; do not read the
host adapter in tests).

### Phase M4 — Repository wiring and TDLib settings (P0)

**Files:** `src/telegram/TelegramRepository.{h,cpp}`,
`src/telegram/TelegramRequests.{h,cpp}` (new builders), `src/telegram/MediaFileStore.{h,cpp}`.

**API added to `TelegramRepository`**

```cpp
void downloadFile(int fileId);
void downloadFileInBackground(int fileId);
void autoDownloadFile(const AutoDownloadRequest& request);
void cancelFileDownload(int fileId);
MediaFileStore* mediaFiles();
void requestMediaForMessage(int fileId, MediaKind kind, qint64 sizeBytes,
                            bool secret, bool secretChat, bool spoiler);
```

**Request builders (testable without a session)**

- `downloadFileRequest(fileId, priority = 16)` → `downloadFile`
- `cancelDownloadFileRequest(fileId, onlyIfPending = false)` → `cancelDownloadFile`
- `getAutoDownloadSettingsPresetsRequest()`
- `setAutoDownloadSettingsRequest(settings, networkType)` → nested `settings` + `type`
- `setNetworkTypeRequest(MediaNetwork)` → `networkTypeWiFi|Mobile|MobileRoaming|Other|None`
- `deleteFileRequest(fileId)`, `getFileRequest(fileId)`
- `getStorageStatisticsRequest(chatLimit = 0)`, `getStorageStatisticsFastRequest()`
- `optimizeStorageRequest(policy, fileTypes)` (see M5)

**Behaviour**

- Thumbnail tracking must move from "every parsed message" to a policy-gated request:
  `trackMessageMedia` records the id/size/kind but only calls `autoDownloadFile` when the
  message is in the visible window (the repository already knows the current chat; add a
  visible-range signal from QML or track the last N message ids). Avatars use
  `downloadFileInBackground`; link previews and sticker thumbnails use
  `downloadFileInBackground` too (closes G13/G14).
- `updateFile` handling mirrors Android: store update, cancel settlement, size refinement
  and policy re-evaluation for tracked automatic downloads.
- `reevaluateAutoDownloads(MediaNetwork)` cancels transfers that the new network/settings
  no longer allow via `cancelDownloadFile(onlyIfPending=false)`.
- `applyAutoDownloadSettings` collects the settings flow, fetches presets (`high` template
  fallback), and sends three `setAutoDownloadSettings` calls; a TDLib failure must not
  change local state or block manual downloads.
- `setNetworkType` on every monitor change and after (re)authorization.
- `cancelFileDownload` sends the request and calls `MediaFileStore::cancelDownload`.

**Tests:** extend `tests/tst_telegram_requests.cpp` with request-shape cases
(`buildsDownloadFileRequest`, `buildsCancelDownloadFileRequest`,
`buildsSetAutoDownloadSettingsRequest`, `buildsSetNetworkTypeRequest`,
`buildsStorageStatisticsRequests`), plus `tests/tst_media_download_wiring.cpp` using a fake
session object that emits `updateReceived` to assert:
`autoDownloadIsDeniedByPolicy`, `manualDownloadIsNeverBlocked`,
`networkChangeCancelsDisallowedTransfers`, `sizeRefinementReevaluatesPolicy`,
`stickerAndLinkPreviewThumbnailsAreRequested`.

### Phase M5 — Storage usage, limits and cleanup (P0/P1)

**Files**

- `src/telegram/StorageCleanup.{h,cpp}` (pure rules).
- `src/telegram/StorageService.{h,cpp}` (TDLib calls, serialized queue).
- `src/telegram/StorageMaintenance.{h,cpp}` (6-hour timer).
- `src/ui/StorageSettingsStore.{h,cpp}` (persisted size/retention).

**API**

```cpp
enum class StorageCategory { Photos, Videos, Files, VoiceAndVideoNotes, Music, Other };

struct CachePolicy { std::optional<qint64> maxBytes; std::optional<int> maxAgeSeconds; };
struct StorageUsage { QHash<StorageCategory, qint64> bytesByCategory;
                      qint64 databaseBytes = 0; qint64 uploadBytes = 0; };

StorageCategory storageCategoryOf(const QString& fileType);
QJsonObject optimizeStorageRequest(const CachePolicy&, const QStringList& fileTypes);
QJsonObject clearFileTypesRequest(const QStringList& fileTypes);

class StorageService {
    void usage(std::function<void(Result<StorageUsage>)>);
    void clear(const QSet<StorageCategory>&, std::function<void(int done, int total)>,
               std::function<void(Result<qint64>)>);
    void applyPolicy(const CachePolicy&, std::function<void(Result<qint64>)>);
};
```

**Rules to port**

- No limits → send nothing (`optimizeStorage` is only sent when `maxBytes` or
  `maxAgeSeconds` is set).
- Size limit → `size = maxBytes`, `ttl = INT_MAX`, `count = INT_MAX`,
  `immunity_delay = -1`, `return_deleted_file_statistics = true`, `chat_limit = 0`.
- Age limit → `size = (1<<53)-1`, `ttl = maxAgeSeconds`, same rest.
- Clear → `size = 0`, `ttl = 0`, `count = 0`, `immunity_delay = 0`.
- File types must be the ones returned by `getStorageStatistics` (distinct by type), never
  an empty array; this preserves Android's "thumbnails/stickers untouched by default"
  behaviour.
- Serialize clear/apply so TDLib never runs two deletions at once; a newer policy skips a
  queued older one.
- Protection set: ids of media in the loaded chat history (add
  `MessageHistory`-equivalent id collection to `MessageListModel`/`TelegramRepository`)
  plus ids opened in the viewer for 30 minutes (new `ListedMediaFiles` equivalent).
- `StorageMaintenance`: apply immediately after `AuthStage::Ready` and every 6 hours; log a
  failure and continue.
- No sent-upload eviction: Windows sends from the user's path and creates no app-private
  upload staging (see §8). `uploadBytes` therefore reports 0.

**Tests:** `tests/tst_storage_cleanup.cpp`:
`fileTypesFallIntoTheCategoriesShownToTheUser`, `policyWithoutLimitsAsksTdlibForNothing`,
`sizeLimitLeavesAgeAndCountUnlimitedAndCountsEveryGivenType`,
`ageLimitDoesNotCutBySize`, `clearingRemovesEverythingOfTheGivenTypes`,
`emptyFileTypeListIsNeverSent`.
`tests/tst_storage_service.cpp` with a fake request sink:
`usageSumsCategoriesAndDatabase`, `clearReportsProgressPerCategory`,
`applyPolicySkipsStaleRequest`, `protectionSetIsExcludedFromEviction`,
`failedOptimizeKeepsPreviousState`.

### Phase M6 — Settings and QML surface (P1)

**Files**

- `src/telegram/MediaDownloadSettingsStore.{h,cpp}`: JSON `{"version":1,...}`,
  per-account path `<accountDirectory>/state/media-download-settings.json`, atomic
  `QSaveFile`, typed validation, corrupt values → defaults, `changed()` only on real change.
- `src/ui/MediaDownloadSettingsController.{h,cpp}`: QML-facing properties
  (`network`, `photosEnabled`, `photosLimit`, …, `dataSaver`, `trafficAutoBytes`,
  `trafficManualBytes`, `activeDownloads`) and `Q_INVOKABLE` setters.
- `src/ui/StorageSettingsController.{h,cpp}`: `usage`, `loading`, `error`,
  `clearCategories`, `sizeLimit`, `retention`, `applyPolicy`.
- `qml/pages/MediaSettingsPage.qml`: network tabs, four category rows with switch + limit
  menu (1/2/5/10/20/50/100 MB/unlimited), data-saver switch, traffic summary, save-failed
  text, manual/streaming notes.
- `qml/pages/StorageSettingsPage.qml`: usage card with stacked category bar, per-category
  checkboxes, clear-selected/clear-all with progress and freed bytes, size and retention
  pickers, footnote that message history is never deleted.
- `qml/pages/SettingsPage.qml`: links to both pages; `qml/Shell.qml` section/navigation
  entry as appropriate.
- `qml/components/MessageBubble.qml`: progress ring/percentage from
  `mediaDownloadedBytes`/`mediaSize`, a cancel action while downloading, and an error/retry
  state (closes G01–G03 in the UI).
- `qml/components/LinkPreviewCard.qml`, `qml/components/StickerPicker.qml`,
  `qml/components/StickerBubble.qml`: rely on the new tracking; no URL changes needed.

**Store API**

```cpp
class MediaDownloadSettingsStore final : public QObject {
public:
    void load();
    MediaDownloadSettings settings() const;
    void setRule(MediaNetwork, MediaCategory, MediaDownloadRule);
    void setDataSaver(bool);
signals:
    void changed();
};
```

**Tests:** `tests/tst_media_download_settings.cpp`:
`missingFileUsesDefaults`, `damagedJsonFallsBackToDefaults`,
`wrongTypesAreRejected`, `negativeLimitUsesDefault`, `roundTripPreservesAllNetworks`,
`writeFailureKeepsPreviousSettings`, `changedOnlyOnRealChange`,
`unknownVersionIsIgnoredSafely`.
`tests/tst_storage_settings.cpp`: limit/retention mapping to `CachePolicy`, persistence,
rollback on failed write.
QML smoke: add a Qt Quick Test page check only if a QML test harness exists at
implementation time; otherwise keep this as a manual checklist item and say so.

### Phase M7 — Message model and progressive playback (P2)

**Files:** `src/telegram/MessageListModel.{h,cpp}` (new roles
`mediaDownloadedBytes`, `mediaSize`, `mediaDownloading`, `mediaError`),
`src/telegram/TelegramTypes.cpp` (populate from `MediaFileStore` on `fileStateChanged`),
`src/ui/MediaPlaybackController.cpp` (optional partial-file source for audio).

**Tests:** extend `tests/tst_chat_models.cpp` with
`mediaProgressRolesFollowFileState`, `mediaErrorClearsOnRetry`,
`readyFlagOnlyForCompletedFiles`.

---

## 6. Test parity map

| Android test | Windows target |
|---|---|
| `MediaFileStoreTest` (7 cases) | `tests/tst_media_file_store.cpp` (10 cases, §M1) |
| `MediaFileStoreTrafficTest` | `tests/tst_media_traffic_usage.cpp` |
| `AutoDownloadPolicyTest` (14 cases) | `tests/tst_media_download_policy.cpp` |
| `AutoDownloadNetworkTest` | `tests/tst_media_network.cpp` |
| `AutoDownloadTdlibMappingTest` (5 cases) | `tests/tst_telegram_requests.cpp` + `tests/tst_media_download_wiring.cpp` |
| `TrafficUsageTest` (11 cases) | `tests/tst_media_traffic_usage.cpp` |
| `StorageCleanupRulesTest` (5 cases) | `tests/tst_storage_cleanup.cpp` |
| `ListedMediaFilesTest` | `tests/tst_storage_service.cpp` (protection window) |
| `StorageSettingsScreen` behaviour | `tests/tst_storage_settings.cpp` |
| — (no Android equivalent) | `tests/tst_media_viewer.cpp`, `tests/tst_media_image.cpp` (keep, extend) |

All suites register through `beygram_add_test(...)` in `tests/CMakeLists.txt`; the fake
session tests must not create a real TDLib client (same rule as
`tests/tst_telegram_session.cpp`).

---

## 7. Priority and sequencing

| Priority | Work | Why |
|---|---|---|
| P0 | M1 file state + M4 wiring | Fixes silent failures, stale "ready" paths and the endless spinner; everything else depends on a real state model |
| P0 | M3 policy + network monitor | Prevents eager download of entire history and gives metered/roaming safety |
| P0 | M4 TDLib settings/network push | Keeps TDLib's own preloading consistent with the app policy |
| P1 | M5 storage usage/limits/cleanup | The user-visible "storage" contract; required for a usable desktop client |
| P1 | M6 settings + QML | Makes the policy controllable and observable |
| P1 | M2 traffic accounting | Feeds the settings summary; small and independent |
| P2 | M7 progress roles/progressive playback | Polish; the state model already carries the data |

Suggested split: one change for M1+M2+M3 (pure logic + tests), one for M4 (wiring),
one for M5 (storage), one for M6 (UI), one for M7.

---

## 8. Deliberate platform differences

| Android behaviour | Windows decision |
|---|---|
| SharedPreferences per account | JSON file per account directory; same defaults and corrupt-value fallback |
| `ConnectivityManager` transport + roaming + VPN underlying networks | `QNetworkInformation` transport + metered; Windows exposes no roaming flag, so roaming maps to `Mobile` and is documented in the settings UI |
| Upload staging folder with `.sent`/`.owner` markers and eviction | Not applicable: Windows sends from the user's chosen path and creates no private copy, so no upload cache is counted or evicted |
| `SetNetworkType` from a default-network callback | Same calls, driven by `QNetworkInformation` changes and re-sent after authorization |
| Priority 16 for every `downloadFile` | Tiered 8 (avatars) / 16 (thumbnails) / 32 (open media); higher number = higher TDLib priority |
| Progressive audio/video from a partial TDLib file | P2; the file state will expose `downloadPath`/`downloadedPrefixBytes` even before playback uses them |
| Android "Other" transport follows mobile policy | Same rule; Windows Ethernet and unprovable transports map to `Other` |
| Sent-upload bytes included in storage usage | Windows reports 0 for that row; the storage UI must say so instead of showing a fake number |

---

## 9. Verification limits of this document

- No build, CMake configure, `ctest`, QML run or device test was executed.
- No claim is made that the described Windows behaviour was observed at runtime; all
  statements come from reading the listed sources.
- Android behaviour is described from source and unit-test expectations, not from a
  device run.
- Line numbers are omitted on purpose so the document does not rot; symbols and file paths
  are stable references.
- When M1–M7 land, update §3 status marks and record the actual test command output in the
  implementing change, not here.
