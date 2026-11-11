# 14 — Albums and attachments: Android → Windows parity audit

Status: read-only static audit of the repository at `F:\BeyGram Windows`.
Date: 2026-11-11.
Nothing in this document was produced by building, running, or testing either client. Every
statement below comes from reading the sources named in §1 and §3.

Companion documents: [PORTING_AUDIT.md](../PORTING_AUDIT.md) (M07 composer/attachments),
[PORTING_ROADMAP.md](../PORTING_ROADMAP.md) (album rendering listed as still open in Phase 2),
[WINDOWS_TDLIB.md](../WINDOWS_TDLIB.md) (pinned TDLib revision).

Method: the Android client is the functional reference and is read first (§1–§2). The Windows
tree is then read to establish what actually exists (§3). Only then are gaps and a plan written
(§4–§6). Where the two disagree, Android behaviour is the specification unless the Windows
deviation is deliberate and documented.

Scope: multi-photo/video albums, album captions, the staged attachment tray, the send pipeline,
upload progress, cancel/retry, file pickers, drag-and-drop and paste, album rendering, album
selection and album paging in the viewer. Camera capture, photo editing, per-file quality modes,
poll/contact/dice/location/music extras and the incoming share target are covered only where they
touch the attachment tray.

---

## 1. Android files reviewed

All paths relative to the repository root.

| File | Lines | What it owns |
| --- | --- | --- |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/MediaComposer.kt` | 1877 | `AttachmentState` (staging, per-file quality prep, send), `AttachmentPanel`, compact tray, `attachmentSendBatches`, `reorderPendingAttachment`, pickers wiring |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/AttachmentMenu.kt` | 216 | Attachment action grid (Camera, Photo/Video, File, Audio, Music, Location, Contact, Poll, Dice) |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/AttachmentQuality.kt` | 466 | `MediaQualityMode`/`MediaDeliveryMode`, `MediaPreparation`, quality panel, estimates |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/MessageAlbum.kt` | 328 | `groupTelegramMediaAlbums`, `MessageAlbumBubble`, per-tile progress, album meta/retry |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ChatScreen.kt` | 2935 | Composer ↔ attachment wiring, caption field/limit, album row in the feed, viewer paging, retry entry point |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/AttachmentMediaHost.kt` | 48 | Camera/photo-editor overlays owned by the composer |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/CameraCaptureController.kt` | 496 | CameraX photo/video capture, flash/zoom/timer |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/MediaViews.kt` | 1721 | Single-media bubbles with upload/download progress |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/MediaViewerPager.kt` | 23 | Viewer page selection helpers |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/MessageSelection.kt` | 50 | Album-as-one-unit selection, forward id ordering |
| `app/src/main/java/com/beykus/beygram/data/media/OutgoingMediaImporter.kt` | 329 | Staging copy, photo normalization, video probe/metadata, thumbnails, discard |
| `app/src/main/java/com/beykus/beygram/data/media/MediaLimits.kt` | 34 | 10 attachments, 10 MiB photo, 2 GiB / 1 h video, 2560 px, aspect 20 |
| `app/src/main/java/com/beykus/beygram/data/tdlib/OutgoingMediaContent.kt` | 93 | `OutgoingMedia.toInputContent` (inputPhoto/Video/Document + thumbnail/dimensions/duration) |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdConversationActions.kt` | 1118 | `sendMedia`, `sendMediaAlbum` (`SendMessageAlbum`), `retryMessages` (`ResendMessages`) |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdLibTelegramRepository.kt` | 3695 | `UpdateFile`/`UpdateMessageSendSucceeded`/`Failed`, `mediaAlbumId` mapping, staged-upload cleanup |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdMediaMapper.kt` | 369 | `TdApi.File.toMediaFileState()` (uploaded bytes / upload-active) |
| `app/src/main/java/com/beykus/beygram/domain/model/ChatMessage.kt` | — | `mediaAlbumId`, `reactionMessage(album)` |
| `app/src/main/java/com/beykus/beygram/domain/repository/ConversationRepository.kt` | — | `sendMediaAlbum` contract, album adjacency for forwarding |
| `app/src/main/java/com/beykus/beygram/share/ShareSession.kt` | 199 | Incoming share: 2..N photo/video becomes one album |
| `app/src/test/java/com/beykus/beygram/ui/screens/chat/CompactAttachmentSendPolicyTest.kt` | 85 | Batch policy + caption code-point counting |
| `app/src/test/java/com/beykus/beygram/ui/screens/chat/AttachmentReorderTest.kt` | 40 | Send-order reorder semantics |
| `app/src/test/java/com/beykus/beygram/ui/screens/chat/AttachmentComposerTest.kt`, `CompactMixedAttachmentsTest.kt`, `CompactAttachmentTrayTest.kt`, `AttachmentExitPreviewTest.kt`, `AttachmentLocationRequestTest.kt` | — | Composer state/policy unit tests |

Adjacent Android code read for context (not attachment-specific): `CameraCaptureScreen.kt`,
`PhotoEditorScreen.kt`, `ComposerExtras.kt`, `ComposerExtraPickers.kt`, `MessageViews.kt`.

---

## 2. Android behaviour (the specification to port)

### 2.1 Staging and import

`AttachmentState` (`MediaComposer.kt:156-1062`) is the single owner of the pending list.

- Pending items are `OutgoingMedia` (`path`, `fileName`, `mimeType`, `kind`, `width`, `height`,
  `durationSeconds`, `thumbnailPath`, `size`, `stagingKey`). State survives activity recreation
  through `rememberSaveable` (`MediaComposer.kt:1094-1118`).
- Hard cap `MAX_PENDING_ATTACHMENTS = 10` (`:1080`) plus `MediaLimits.MAX_ATTACHMENTS`; the
  picker itself is capped at 10 (`PickMultipleVisualMedia(MediaLimits.MAX_ATTACHMENTS)`, `:1127`).
- `prepare(uris, asDocument)` (`:366-396`) imports **sequentially**, appending each item to the
  pending list as soon as it is ready, so the tray fills progressively. Cancellation discards the
  partially staged item (`importer.discard`).
- `OutgoingMediaImporter.prepare` copies the source into
  `filesDir/uploads/<uuid>/original/<name>`, sanitizes the file name, and then classifies by MIME:
  - photo → decode, downscale to ≤2560 px, pad extreme panoramas to ≤20:1, re-encode JPEG q88
    (PNG when alpha must survive), 320 px thumbnail, reject >10 MiB (`OutgoingMediaImporter.kt:219-253`);
  - video → probe tracks; a container/codec that cannot be a video message becomes a **document**
    (`:255-281`), otherwise read rotation-corrected width/height, duration and a first-frame thumbnail;
  - audio → document with duration; anything else → document.
- Per-file quality preparation is keyed by the staged original's path and runs under a `Mutex`
  so several videos never exhaust hardware codecs (`:205-211`, `:796-843`). Changing one file's
  mode cancels only that file's job. Send is blocked while any file is still preparing
  (`canSend`, `:280`).
- Removal cancels the file's job, deletes its upload directory (`discard`, `:685-693`), and
  `discardAll()` (`:974-996`) also clears the camera-retry paths and the editor session.
- Camera takes are imported through the same pipeline (`importCaptureFile`, `:536-571`); a failed
  import keeps its cache path in a saveable retry state (`captureRetry`, `:146-155`, `:581-591`).

### 2.2 Grouping, order, captions

- **Order** is the tray order. `reorderPendingAttachment` (`:1463-1472`) moves one item by ±1 in
  the *pending list*, and the comment at `:1462` states explicitly that the same list defines the
  album order. There is no drag gesture: long-press opens a "move left/right" menu
  (`:1598-1612`) and TalkBack gets equivalent custom actions (`:1562-1569`).
- **Batching** is decided by `attachmentSendBatches` (`:1072-1078`): if the prepared list has 2..10
  items and every prepared item is `Photo` or `Video`, it is one album batch; otherwise each item
  becomes its own single-message batch. Consequences:
  - a document, audio file, GIF (`Animation`) or a video switched to *File* delivery breaks the
    album and forces sequential sends (`CompactAttachmentSendPolicyTest.kt:38-59`);
  - 11 items can never be one album (`:61-64`).
- **Caption**: the composer text field *is* the caption while attachments are staged
  (`ChatScreen.kt:1971`, `:2039`, `:2060-2065`). It is counted in Unicode code points
  (`attachmentCaptionLength`, `:1065`) against `limits.caption`, and it is applied only to the
  **first** batch and only to the **first** content of an album
  (`TdConversationActions.kt:462`); after a batch succeeds the caption is cleared for the rest
  (`MediaComposer.kt:1054-1055`).
- **Reply/topic**: reply target is validated (`GetMessageProperties.canBeReplied`) and its
  `topicId` wins; a forum topic is used only without a reply (`TdConversationActions.kt:453-468`).
- **View-once is refused for albums** (`:451`, `err_view_once_single_media`), and view-once is
  dropped when any item is switched to file delivery (`MediaComposer.kt:741`).
- On the receive side, grouping uses only a **nonzero** `mediaAlbumId` **and** a non-null media
  (`MessageAlbum.kt:84-93`); members are sorted by `(date, id)` inside the bubble (`:145`).

### 2.3 Send pipeline

`AttachmentState.send` (`MediaComposer.kt:998-1061`):

1. Refuses when `sending || importing || locating || preparing`.
2. Claims `sending = true` **synchronously** before launching, so a double tap cannot send twice.
3. Snapshots the pending originals and pairs each with its prepared outgoing media.
4. Iterates batches in order; per batch calls the `sendMedia` lambda supplied by `ChatScreen`.
5. On failure: optionally offers the stealth-schedule fallback, otherwise sets the composer error
   and **breaks** — the failed and all later items stay staged, the caption is kept.
6. On success: removes exactly the sent originals (identity via `samePendingAttachment`,
   `:1086-1092`), drops their quality entries, clears the caption, continues.

`ChatScreen.kt:480-507` chooses `sendMediaAlbum` when `media.size > 1`, otherwise `sendMedia`;
both receive `draftEntities` and the forum topic, and `clearReply()` runs on success.

`TdConversationActions.sendMediaAlbum` (`:448-472`):

- `require(media.size in 2..10 && all Photo|Video)`, `requireFutureSchedule`,
  `requireViewOnceSupported`;
- caption bounded to `limits.caption` with entities;
- reply target re-read and re-checked;
- one `TdApi.SendMessageAlbum` with `inputMessageContents` in staged order, caption+entities on
  index 0 only, `topicId`/`replyTo` as above;
- scheduled albums are not merged into visible history (`:470-471`);
- the returned messages are mapped into history and replies hydrated.

Each content carries `InputFileLocal` (or `InputFileId` for server GIFs), an `InputThumbnail`
when one exists, real `width`/`height`, `duration`, and `supportsStreaming` probed from the MP4
`moov` position (`OutgoingMediaContent.kt:10-93`).

### 2.4 Upload progress

- TDLib `updateFile` → `TdApi.File.toMediaFileState()` exposes `uploadedBytes` and
  `isUploadingActive` (`TdMediaMapper.kt:28-45`); `MediaFileState.uploadProgress` is derived from
  them.
- Album tiles render `LinearProgressIndicator(progress = uploadProgress)` at the tile bottom while
  `file.isUploading` (`MessageAlbum.kt:318-321`); single-media bubbles do the same
  (`MediaViews.kt:619-620`, `:716-718`, `:817-818`).
- Message-level send state comes from `sending_state`: `Pending → Sending`,
  `Failed → Failed` (`TdLibTelegramRepository.kt:3407-3409`).
- On `UpdateMessageSendSucceeded` the staged upload directory is cleaned up
  (`cleanupSentUploads`, `:2682`).

### 2.5 Cancel and retry

- **Retry** is explicit and per album: `retryMessages(chatId, ids)` → `TdApi.ResendMessages`
  (`TdConversationActions.kt:479-486`). The album bubble shows retry when any member is `Failed`
  (`MessageAlbum.kt:229`) and passes the failed members' ids (`ChatScreen.kt:1646`); TDLib resends
  the album as a unit, which is why forwarding/resend must keep members adjacent
  (`ConversationRepository.kt:227-231`).
- **Cancel before send** is composer-level: remove one item, or "clear all" behind a confirm
  dialog (`MediaComposer.kt:1307-1324`). **Cancel of an in-flight upload is not a UI action** —
  there is no per-message "cancel upload"; the user deletes the message.
- **Retry of a failed import** exists for camera takes only (`retryCapture`, `:581-591`); failed
  quality preparation falls back to the untouched original with a notice (`:820-835`).
- Poll/extra sends keep the prepared item after failure and offer Retry/Cancel
  (`:657-669`, `:1264-1279`).

### 2.6 Pickers, camera, drag-drop, paste, share

- Pickers (`MediaComposer.kt:1127-1130`): `PickMultipleVisualMedia(10)` for photo/video,
  `OpenMultipleDocuments()` for File and for Audio. No `GetContent`, no single-select path.
- Camera: `openCamera()` → `CameraCaptureScreen` (`AttachmentMediaHost.kt:24-31`); a photo opens
  `PhotoEditorScreen` first, "send original" is one tap away (`:414-459`); a video is staged
  directly.
- **Drag-and-drop**: not an Android concept — absent.
- **Paste**: no clipboard-image paste path. Clipboard use in the chat is text-copy only
  (`ChatScreen.kt:840`, `:1501`, `:2305`). `ClipData` appears in the *outgoing share* helper
  (`MediaViews.kt:1683`), not in the composer.
- **Incoming share** is the Android substitute for desktop drag-drop: `ShareSession` sends 2..N
  photo/video as one album, anything else as separate messages (`share/ShareSession.kt:144-145`).

### 2.7 Rendering, viewer, selection

- `MessageAlbumBubble` (`MessageAlbum.kt:117-247`): 2 columns for ≤4 items, 3 columns for 5+
  (`:149`), square tiles with 2 px gaps, one bubble for the whole album; the reply block comes
  from the first member that has one; **all** non-blank member captions are rendered; reactions
  are owned by one member (`reactionMessage`); meta row is the latest member's.
- Viewer: pages are media-window pages with the selected id, so swiping crosses album members
  (`ChatScreen.kt:2201-2215`, `MediaViewerPager.kt`); every album member opens from the same
  bubble rectangle (`onMediaBounds`, `MessageAlbum.kt:140-141`).
- Selection selects the whole loaded album (`MessageSelection.kt:18-30`); forwarding sorts ids
  ascending and relies on album contiguity (`:32-42`).
- Per-tile state: spoiler reveal, secret-media suppression, auto-download policy, download
  spinner/icon, duration badge, video play badge (`MessageAlbum.kt:249-323`).

---

## 3. Current Windows status

| Area | Windows implementation | State |
| --- | --- | --- |
| Send builder | `src/telegram/MediaSend.cpp:30-140` — `sendMessage` only, `inputMessagePhoto`/`Video`/`Document` | Single item only |
| Media metadata in request | width/height `0`, `thumbnail: null`, `duration 0`, `supports_streaming: false` (`MediaSend.cpp:30-90`) | No thumbnails, no dimensions, no duration |
| Caption | plain `formattedText`, empty entities (`MediaSend.cpp:13-20`), `caption.trimmed()` | No entities |
| Album request | none — no `sendMessageAlbum` anywhere outside the Android tree | **Missing** |
| Validation | `isSupportedMediaPath` extension allow-lists (`MediaSend.cpp:106-160`) | Present, single path |
| Repository send | `TelegramRepository::sendMedia` (`TelegramRepository.cpp:685-719`), 120 s timeout, adds the message only from the response | No batching |
| Controller API | `AppController::sendChatMedia(const QUrl&, const QString&)` (`AppController.cpp:1060-1092`), kind chosen by extension probing | One file per call |
| QML attach flow | one "Attach" button + `FileDialog { fileMode: FileDialog.OpenFile }` (`qml/pages/ChatsPage.qml:691-695`, `:919-935`) | Single-select, no tray |
| Attachment tray | none | **Missing** |
| Album rendering | `qml/components/AlbumGrid.qml` exists and is registered in `CMakeLists.txt:90`, but **no QML file references it** | Dead component |
| Album data | `MessageData` has no album id (`TelegramTypes.h:30-59`); `MessageListModel` exposes no album role (`MessageListModel.h:15-48`) | **Missing** |
| Grouping helper | `src/telegram/AlbumGrouper.{h,cpp}` + `tests/tst_album_grouper.cpp` | Present, **unwired** |
| Message bubbles | `qml/components/MessageBubble.qml` — one media per bubble, "sending…"/"Not sent" text (`:292-303`) | Single media |
| Upload progress | `MediaFileStore::handleFile` returns unless `is_downloading_completed` (`MediaFileStore.cpp:96-126`); only `path`/`isReady` are exposed | **No upload progress** |
| Pending/failed | parsed from `sending_state` (`TelegramTypes.cpp:527-528`); `updateMessageSendSucceeded`/`Failed` handled (`TelegramRepository.cpp:1063-1093`) | Present, no local echo |
| Retry / cancel | no `resendMessages` builder, no delete-based cancel, no retry affordance | **Missing** |
| Viewer | `MediaViewerController` holds one `fileId` (`MediaViewerController.h:18-55`); `MediaViewerOverlay.qml` shows one image | No album paging |
| Drag-drop | no `DropArea`/`Drag` in `qml/**` | **Missing** |
| Paste | clipboard is text-copy only (`AppController.cpp:1169`, `:1302`) | **Missing** |
| Camera | none | **Missing** |
| Staging copies | Windows sends the user's original path directly | No staging, no normalization, no cleanup |

Notes that matter for the plan:

- The Windows send path relies on TDLib's own `updateNewMessage` echo for the outgoing bubble: that
  update is parsed into a message whose `pending` flag comes from `sending_state`
  (`TelegramTypes.cpp:527-528`), and `updateMessageSendSucceeded` later replaces the temporary id
  (`MessageListModel::replaceId`, `MessageListModel.cpp:146-163`). The state scaffolding therefore
  exists; the missing parts are upload byte tracking and a retry/cancel affordance.
- `docs/PORTING_ROADMAP.md:95` already lists "album rendering" as open for Phase 2, and
  `docs/PORTING_AUDIT.md:299` (M07) lists "paste/drag-drop; IME … album send" as the composer
  risk surface. This audit is consistent with both.

---

## 4. Gap matrix (ranked by risk)

Priority: **P1** = blocks a normal Telegram desktop send/receive of a photo set; **P2** =
expected desktop affordance or resilience; **P3** = polish.

| # | Gap | Android reference | Windows reference | Priority |
| --- | --- | --- | --- | --- |
| G1 | No `sendMessageAlbum`; 2..10 photos/videos are sent as N separate messages | `TdConversationActions.kt:448-472` | `MediaSend.cpp` (whole file) | **P1** |
| G2 | No batch policy (album vs sequential, 2..10, Photo/Video only, video-as-file excluded) | `MediaComposer.kt:1072-1078` | — | **P1** |
| G3 | No staged tray: no multi-select, no remove, no reorder, no per-item preview | `MediaComposer.kt:1484-1719` | `ChatsPage.qml:919-935` | **P1** |
| G4 | Album members render as separate bubbles; `AlbumGrid` unused; no album id in the model | `MessageAlbum.kt:84-93`, `:117-247` | `TelegramTypes.h:30-59`, `MessageListModel.h:15-48`, `qml/components/AlbumGrid.qml` | **P1** |
| G5 | Album caption only on the first content; Windows has no album path to enforce it | `TdConversationActions.kt:462` | — | **P1** |
| G6 | No reply/topic validation for media sends (reply id is passed through unchecked) | `TdConversationActions.kt:453-468` | `TelegramRepository.cpp:685-719` | P2 |
| G7 | No thumbnail/width/height/duration in the request → slower recipient previews, wrong aspect until download | `OutgoingMediaContent.kt:18-49` | `MediaSend.cpp:30-73` | **P1** (cheap, high value) |
| G8 | No caption entities on media | `OutgoingMediaContent.kt:17` | `MediaSend.cpp:13-20` | P2 |
| G9 | No upload progress (downloads only) | `TdMediaMapper.kt:28-45`, `MessageAlbum.kt:318-321` | `MediaFileStore.cpp:96-126` | P2 |
| G10 | No retry for failed sends | `TdConversationActions.kt:479-486`, `MessageAlbum.kt:229` | — | P2 |
| G11 | No cancel of a queued/in-flight send | `MediaComposer.kt:974-996` (pre-send only) | — | P2 |
| G12 | No drag-and-drop of files onto the conversation | n/a (Android) | `qml/**` (no `DropArea`) | P2 |
| G13 | No clipboard-image paste | n/a (Android) | `AppController.cpp:1169` | P2 |
| G14 | No camera capture / photo editor | `AttachmentMediaHost.kt:24-31`, `CameraCaptureController.kt` | — | P3 (separate slice M08) |
| G15 | No album paging in the viewer | `ChatScreen.kt:2201-2215`, `MediaViewerPager.kt` | `MediaViewerController.h:18-55` | P2 |
| G16 | No staging copy / photo normalization / upload-dir cleanup; sending the original path means a move/delete mid-upload fails the send | `OutgoingMediaImporter.kt:33-50`, `TdLibTelegramRepository.kt:2682` | `AppController.cpp:1060-1092` | P2 |
| G17 | Album-aware selection/forwarding (album members stay adjacent) | `MessageSelection.kt:18-42` | `ForwardMessages.h:17-23` | P3 |
| G18 | Per-file quality modes (Compact/Balanced/Original, video File delivery) | `AttachmentQuality.kt`, `MediaComposer.kt:699-843` | — | P3 |
| G19 | Poll/contact/dice/location/music extras share the tray | `AttachmentMenu.kt:59-93`, `ComposerExtras.kt` | — | P3 (other slice) |
| G20 | Incoming share → album detection | `share/ShareSession.kt:144-145` | — | P3 (share slice) |

---

## 5. Implementation plan

Conventions to respect (from the existing code): builders in `src/telegram/*` are pure — no
logging, no `@extra`, no file access beyond `QFileInfo`, correlation is added by the transport;
models live beside their siblings and are registered in `src/telegram/CMakeLists.txt`; QML files
must be added to `qt_add_qml_module(... QML_FILES ...)` in the root `CMakeLists.txt`; QtTest files
are added as `beygram_add_test(...)` in `tests/CMakeLists.txt`.

### Phase A — album request + batch policy (P1)

Files:
- `src/telegram/MediaSend.h/.cpp`
  - add `struct OutgoingMediaItem { OutgoingMediaKind kind; QString localPath; int width; int height; int durationSeconds; QString thumbnailPath; bool supportsStreaming; }`;
  - `QJsonObject sendMediaAlbumRequest(qint64 chatId, const QVector<OutgoingMediaItem>& items, const QString& caption, qint64 replyToMessageId = 0)`;
  - `sendMediaMessageRequest` gains an optional `OutgoingMediaItem` overload so single and album
    contents are built by one function (thumbnail, width/height, duration, streaming flag);
  - `QVector<QVector<OutgoingMediaItem>> partitionMediaBatches(const QVector<OutgoingMediaItem>&)` —
    the `attachmentSendBatches` equivalent: one batch iff `2..10` and every item is Photo/Video,
    else one item per batch;
  - keep `isSupportedMediaPath` and extend the photo allow-list only if the importer adds formats.
- `src/telegram/TelegramRepository.h/.cpp`
  - `void sendMediaBatch(const QVector<OutgoingMediaItem>&, const QString& caption, qint64 replyToMessageId)`:
    partition, then per batch issue the matching request through the existing `sendRequest`
    (one response handler per album; the response is `messages`, so iterate and `addOrUpdate` each).
- `src/ui/AppController.h/.cpp`
  - `Q_INVOKABLE bool sendChatMediaList(const QList<QUrl>& files, const QString& caption)`;
    keep `sendChatMedia` as a one-element wrapper so the current QML keeps working.

APIs used: `sendMessageAlbum { chat_id, topic_id, reply_to, options, input_message_contents }`
(TDLib 1.8.69, see `docs/WINDOWS_TDLIB.md:15`); `sendMessage` for the singleton path.

Tests (`tests/tst_media_send.cpp`, extended):
- album request has `@type == sendMessageAlbum`, one content per item, order preserved;
- caption text on index 0 only, empty on the rest;
- `2` and `10` items → one batch; `1` and `11` → sequential;
- a Document or a video-as-file item breaks the album;
- reply target null when `replyToMessageId <= 0`; `topic_id` null;
- no `http`/`https`/`://` in any request (mirrors the existing `requestContainsNoNetworkUrl`);
- new `tests/tst_attachment_batches.cpp` for `partitionMediaBatches` alone.

### Phase B — album data and rendering (P1)

Files:
- `src/telegram/TelegramTypes.h/.cpp`: `MessageData::mediaAlbumId` parsed from `media_album_id`
  (guarded: absent → 0). Album members keep their own ids.
- `src/telegram/MessageListModel.h/.cpp`: `MediaAlbumIdRole`.
- new `src/telegram/MessageAlbumModel.h/.cpp` (proxy over `MessageListModel`):
  - rows are either a singleton or an album (reuse `groupAlbums` on `(id, mediaAlbumId)`), exposing
    `albumId`, `messageIds`, `count`, `caption`, `replyPreview`, `outgoing`, `failed`, `media` list
    (fileId/fullFileId/width/height/ready/revision per item);
  - must react to `dataChanged`, `rowsInserted`, `rowsRemoved`, `modelReset`, and to
    `replaceId` (temporary → real id inside an album).
- `qml/components/MessageAlbumBubble.qml`: `AlbumGrid` + caption block + reply block + meta
  ("sending…"/"Not sent"/time), tile click → viewer at that index.
- `qml/pages/ChatsPage.qml`: replace `delegate: MessageBubble` with a `DelegateChooser`
  (`modelData.kind === "album"` → `MessageAlbumBubble`, else `MessageBubble`), bound to the new
  model.

Tests: extend `tests/tst_album_grouper.cpp` for the `media_album_id` path; new
`tests/tst_message_album_model.cpp` covering: singleton vs album rows, order by first id, caption
from the first non-blank member, `replaceId` inside an album, removal of one member, reset.

### Phase C — staged attachment tray and composer (P1)

Files:
- new `src/media/OutgoingStaging.h/.cpp`: `AttachmentItem` record (`id`, `sourcePath`,
  `stagedPath`, `kind`, `width`, `height`, `durationSeconds`, `size`, `thumbnailPath`, `state`,
  `error`); `addPaths()`, `remove(id)`, `move(id, offset)`, `clear()`, `items()`; copies into
  `QStandardPaths::CacheLocation/attachments/<uuid>/`; caps at 10 items, 10 MiB photo, 2 GiB / 1 h
  video (`MediaLimits` equivalent); reads dimensions with `QImageReader`, duration later via the
  media slice; deletes the staging directory on remove/clear/after a successful send.
- new `src/ui/AttachmentListModel.h/.cpp` (`QAbstractListModel` over `OutgoingStaging`).
- `src/ui/AppController`: `Q_PROPERTY(QAbstractItemModel* attachments …)`,
  `attachFiles(QList<QUrl>)`, `removeAttachment(QString id)`, `moveAttachment(QString id, int offset)`,
  `clearAttachments()`, `sendAttachments(const QString& caption)`.
- `qml/components/AttachmentTray.qml` (thumbnail rail, remove button, move-left/right actions —
  same model as Android's long-press menu), `qml/components/AttachmentMenu.qml` (Photo/Video,
  File, Audio entries; Camera only once Phase F lands), and the `ChatsPage.qml` composer:
  `FileDialog { fileMode: FileDialog.OpenFiles }` plus tray above the `TextArea`; caption
  placeholder/counter while the tray is non-empty.

Tests: new `tests/tst_outgoing_staging.cpp` (limit enforcement, move semantics, remove deletes the
staged copy, clear is idempotent, same source twice yields distinct ids) and
`tests/tst_attachment_list_model.cpp` (row signals after add/move/remove).

### Phase D — drag-and-drop, paste, album viewer (P2)

Files:
- `qml/pages/ChatsPage.qml`: `DropArea` over the conversation + composer, `onDropped` reads
  `drop.urls` → `BeyGramApp.attachFiles(...)`; visible highlight while `containsDrag`.
- `src/ui/AppController`: `Q_INVOKABLE bool attachFromClipboard()` — `QGuiApplication::clipboard()`
  → `mimeData()->hasUrls()` (attach local files) else `hasImage()` (save `QImage` to the staging
  directory as PNG and attach).
- `ChatsPage.qml` composer: `Keys.onPressed` for `Ctrl+V` before the `TextArea` consumes it
  (only when the clipboard holds a URL/image; plain text paste stays native).
- `src/ui/MediaViewerController.h/.cpp`: `openAlbum(const QVector<int>& fileIds, int index, const QString& title)`,
  `next()`, `previous()`, `index`, `count`; `qml/components/MediaViewerOverlay.qml` left/right
  keys and arrows. `MessageAlbumBubble` tiles pass the album's ordered full-size ids.

Tests: `tests/tst_media_viewer.cpp` (album open at index, next/prev clamping, single-item
behaviour unchanged); `tests/tst_clipboard_attach.cpp` with a synthetic `QMimeData`
(urls vs image vs text). QML `DropArea` behaviour is a manual check only — do not claim it as
tested.

### Phase E — upload progress, retry, cancel (P2)

Files:
- `src/telegram/MediaFileStore.h/.cpp`: stop discarding `updateFile` for non-completed downloads;
  keep `remote.uploaded_size` / `remote.is_uploading_active` per file id, expose
  `uploadProgress(int fileId)`, `isUploading(int fileId)`, and a `uploadChanged(int fileId)` signal
  (coalesced like the existing revision counter).
- `MessageData`: keep `mediaFileId` (already the preview id) and ensure the *full* file id is
  tracked for progress; `MessageAlbumModel` and `MessageBubble` read the store through
  `AppController`.
- `src/telegram/TelegramRequests.h/.cpp`: `resendMessagesRequest(chatId, ids)` and
  `deleteMessagesRequest` reuse for cancel.
- `AppController`: `Q_INVOKABLE void retryChatMessages(const QList<qint64>& ids)`; bubble/meta
  button on failed rows; "cancel sending" on pending rows → delete the temporary message.
- Optional but recommended: an optimistic local echo
  (`MessageListModel::addPendingLocal(localId, MessageData)`) so the bubble appears before the
  TDLib round trip; if added, reconcile in `updateMessageSendSucceeded` and add a
  duplication test.

Tests: `tests/tst_media_send.cpp` (resend request shape), `tests/tst_chat_models.cpp`
(pending → sent replacement, failed → retry keeps row), `tests/tst_message_album_model.cpp`
(progress role does not reorder rows).

### Phase F — camera and quality (P3, separate slices)

Camera capture and the photo editor belong to the Media Foundation slice (M08 in
`docs/PORTING_AUDIT.md`); per-file quality modes (G18) only make sense after Phase C exists.
Do not stub a camera entry before capture works.

---

## 6. Priority summary

1. **P1 — make a photo set behave like Telegram**: Phase A (album request + batch policy) and
   Phase B (album id in the model + album bubble) are independent of each other and can land in
   either order, but together they are the parity unit. Phase A alone already fixes the most
   visible defect (a 5-photo send becoming 5 messages) and is the smallest change with tests.
2. **P1 — staged tray**: Phase C. Without it there is no place to choose an order, remove one
   item, or attach more than one file, so Phase A's batching is only reachable from C++.
3. **P1 (cheap) — media metadata**: G7 (`width`/`height`/`duration`/thumbnail in the request) is a
   few lines in `MediaSend.cpp` and removes the "recipient sees a square placeholder" symptom.
4. **P2 — desktop affordances**: drag-drop, paste, album viewer paging, upload progress, retry,
   cancel.
5. **P3 — camera, quality modes, extras, share-target albums**: separate slices.

Recommended order: G7 → Phase A → Phase B → Phase C → Phase D → Phase E.

---

## 7. Verification status

- Source-only audit; **no build, no CMake configure, no test run, no GUI launch** was performed.
- Nothing in this document asserts that existing tests pass; `tests/tst_album_grouper.cpp` and
  `tests/tst_media_send.cpp` were read, not executed.
- `AlbumGrid.qml` being unreferenced was verified by searching `qml/**` for `AlbumGrid`
  (no matches) while `CMakeLists.txt:90` registers it.
- The `media_album_id` JSON field name follows the TDLib 1.8.69 schema used by the Android
  client (`TdApi.Message.mediaAlbumId`). It is not present anywhere in the Windows sources today,
  so the parser change in Phase B must be covered by a unit test with a hand-written message
  object.

## 8. Open questions for the owner

1. Should Windows send the **original** file (current behaviour, server-side transcode) or stage
   and normalize like Android (≤2560 px JPEG q88)? Staging costs disk and a copy but gives
   deterministic dimensions, thumbnails and safe cleanup.
2. Album order control: Android has no drag gesture, only move-left/right. For a desktop port, is
   drag-to-reorder in the tray required, or is the same button-based reorder acceptable?
3. Is "cancel an in-flight upload" in scope? The cheapest implementation is deleting the temporary
   message; a dedicated cancel would need TDLib-side handling and is not an Android parity item.
4. Should the existing `AlbumGrouper` keep grouping by an explicit `groupedId` argument, or be
   changed to read `MessageData::mediaAlbumId` directly once the field exists? Keeping the pure
   signature and feeding it from the model preserves the current tests.
