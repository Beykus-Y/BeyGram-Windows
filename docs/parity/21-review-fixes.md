# Review fixes — 13 November 2026

Follow-up to the `DEV` review at commit `47f84c2e`. The list below maps every
review finding to the change that addresses it and the test that guards it.

## P0 — messages

| Finding | Fix | Guard |
| --- | --- | --- |
| Delegate redeclared `MessageBubble` properties with `required property`, hiding the component's own properties | `ChatsPage.qml` now assigns `messageId: model.messageId`, `text: model.text`, ... directly. Only components that own their contract (`ChatTrackerEntry`, chat rows, search rows) keep `required property`. | `tst_message_bubble_render` |
| `markCurrentChatRead()` returned while `m_lastReadMessageId == 0`, so opening a chat never sent `viewMessages` | The early return now only checks `m_currentChatId`; the newest visible message id drives the read marker. | existing repository/read tests |
| Older history page could roll back an edited or downloaded message | `applyHistory()` merges into `MessageHistoryStore` first and skips ids already present in `MessageListModel`. | `tst_message_model_merge` |
| `updateContent()` changed the text type but left attachment ids/sizes from the old content | The method recomputes `mediaFileId`, `mediaFullFileId`, `mediaWidth`, `mediaHeight`, clears `mediaReady`/`mediaFailed` and bumps `mediaRevision`. | `tst_message_model_merge` |
| Retry button called `openChat()` which returned early | `openChat()` only short-circuits when the same chat is loaded, has messages, no error and no history request is in flight. | manual retry path |
| Message bubbles rendered empty/left-aligned on the screenshot | `MessageBubble` positioned the bubble by toggling `anchors.left`/`anchors.right`. A delegate reused for the other direction kept both anchors, stretching every bubble to the full row. It now uses an explicit `x` binding. | `tst_message_bubble_render::outgoingTextBubbleAlignsRight` |

## P0 — account isolation

| Finding | Fix | Guard |
| --- | --- | --- |
| Chat and message caches lived in shared `state/` directories | Both cache paths are built from `m_registry.accountDirectory(m_accountId)`. | existing cache tests |
| Drafts lived in one global file | `DraftStore::switchTo()` drops the in-memory drafts and loads the account file; `AppController::ensureRepository()` calls it with the account path. | `tst_draft_store::switchesAccountsWithoutLeakingDrafts` |
| `MediaFileStore::setSession()` kept file ids across accounts | A real session change clears paths, ready flags, pending requests and failure flags; callbacks carry a session generation and stale responses are ignored. | `tst_media_file_store_session` |

## P1 — history, media, search

| Finding | Fix | Guard |
| --- | --- | --- |
| Media download failure left an endless spinner | `MediaFileStore` emits `fileFailed`, `MessageListModel` exposes `mediaFailed`, and `MessageBubble` replaces the spinner with a danger-tinted retry label. | model merge test + bubble render test |
| Late download of an older attachment replaced the newer one | `AppController::openMediaPlayback()` remembers the last requested file id and ignores older `fileReady` callbacks. | manual |
| In-chat search result click only closed the panel | The result calls `page.jumpToMessage(messageId)` before closing. | manual |
| Global search ignored `messageId` | `Shell.qml` calls `chatsPage.jumpToMessage(messageId)` after `openChat()`. | manual |
| Auto-scroll jumped to the end on every insert | `onCountChanged` scrolls only when the user was within 120 px of the bottom or the last message is outgoing. | manual |
| Compact inline meta was never used because `textHtml` is always filled | `canInlineMeta` checks the plain text length/line count and ignores `textHtml`. | `tst_message_bubble_render` |
| Sticker with an emoji fallback drew a bubble | Any `contentType === "sticker"` is sticker-only; `StickerBubble` renders the emoji until the file arrives. | `tst_message_bubble_render::emojiStickerRendersGlyph` |

## P1 — features that only changed local state

| Subsystem | Fix |
| --- | --- |
| Proxy | `NetworkFacade::proxyApplyRequested` → `TelegramRepository::applyProxy()` → `TelegramSession::applyProxy()` sends TDLib `addProxy` with `enable=true` and keeps the settings for the next start. |
| Stories | `StoriesFacade::seen` → `openStory()`; `reactionRequested` → `setStoryReaction` with the current chat as `story_poster_chat_id`. |
| Gifts, privacy, sessions | Already connected; verified in `AppController::ensureRepository()`. |
| Storage | `StorageFacade::cleanRequested` → `optimizeStorage` with the selected file types; `getStorageStatistics` fills the breakdown and the running flag is released when the request finishes. |
| Notifications | Per-chat sound and preview are no longer discarded: `setChatNotificationSettings()` writes both fields. |
| Music | `MusicPlayerFacade` now owns a `QMediaPlayer`/`QAudioOutput`, registers track sources (`setSourceFor`), plays a requested queue entry (`playTrack`), follows play/pause/next/previous/seek/speed and advances on end-of-media. `MusicLibraryPage`'s `playRequested`/`playlistPlayRequested` are handled by `AppController`. |

## CI

`.github/workflows/windows-foundation.yml` now runs on pushes to `DEV` and
`main` in addition to pull requests.

## Tests

The three new test targets are registered in `tests/CMakeLists.txt`:

- `tst_media_file_store_session`
- `tst_message_model_merge`
- `tst_message_bubble_render`

`tst_draft_store` gained the account-switch case and `tst_music_player_facade`
gained `playTrackSelectsRequestedTrack`.
