# Stories, gifts, Stars and Premium — Android → Windows parity audit

Status: read-only static audit of the repository at `F:\BeyGram Windows`.
Date: 2026-11-11.
Nothing in this document was produced by building, running, or testing either client.
Every statement below comes from reading the sources named in §2 and §4.

Companion documents: [PORTING_AUDIT.md](../PORTING_AUDIT.md) (feature matrix;
M10 reactions/stickers/gifts/Premium/Stars, M11 stories),
[PORTING_ROADMAP.md](../PORTING_ROADMAP.md) (Phase 5),
[WINDOWS_TDLIB.md](../WINDOWS_TDLIB.md),
[WINDOWS_FOUNDATION.md](../WINDOWS_FOUNDATION.md).

---

## 1. Scope and method

In scope:

- the story feed (bar on the chat list), story viewer, story publisher, story archive,
  story reactions, story replies and story privacy/lifetime options;
- gifts: catalog, received gifts, collections, gift sending;
- Telegram Stars: balance, transaction history, top-up hand-off, paid messages/media,
  paid reactions;
- Premium: status, feature list, plans, external checkout hand-off and gift-code activation;
- the entry points that expose all of the above from profiles.

Out of scope: Android-only platform surfaces that have no direct Windows counterpart
(the `FLAG_SECURE` story window, the stealth-mode policy engine, the Android visual media
picker and Media3 Transformer pipeline). These are described only where they affect the
Windows design.

Method: the Android client is the functional reference and is read first (§2–§3). The
Windows tree is then read to establish what actually exists (§4). Only then are gaps and
a plan written (§5–§9). Where Android and Windows disagree, Android behaviour is treated
as the specification unless the Windows deviation is deliberate and documented.

---

## 2. Android files reviewed

### 2.1 Domain models (portable, pure)

| File | Size | Contents |
|---|---|---|
| `app/src/main/java/com/beykus/beygram/domain/model/Stories.kt` | 7.1 KB | `StoryPreview`, `ChatStories`, `StoryMedia`, `StoryDetail`, `StoryPrivacy`, `StoryPostTarget`, `StoryArchivePage`, `StoryPostPermission`/`StoryPostDenial`, `StoryPostTicket`, `StorySubmissionState`, `StoryPostEvent`, `storyActivePeriodChoices`, `storyCropRect`, `orderedStoryBar`, `orderedStoryPostTargets` |
| `app/src/main/java/com/beykus/beygram/domain/model/Monetization.kt` | 31.8 KB | Premium info/plans/features/load state, gift codes, star balance/transactions/top-up options, external payment URL policy, `PremiumCheckoutStep`, `StarsTopUpStep`, `GiftCatalogItem`, `ReceivedGiftItem`, `GiftCollectionItem`, `GiftsInfo`, `giftBlockReason`, `GiftSendStep`, `PremiumGiftCodeStep`, price/star formatting |
| `app/src/main/java/com/beykus/beygram/domain/model/PaidContent.kt` | 5.1 KB | Paid message/media plan, pay gate, outcomes, insufficient-stars/error classification |
| `app/src/main/java/com/beykus/beygram/domain/model/PaidReaction.kt` | 1.4 KB | Paid reaction amount block, uncertain-payment exception |
| `app/src/main/java/com/beykus/beygram/domain/model/InteractionPolicy.kt` | 1.1 KB | `InteractionEffect.StoryView`, stealth block exception |
| `app/src/main/java/com/beykus/beygram/domain/repository/TelegramRepository.kt` | 23.1 KB | Story and monetization contract (lines 83–205) |
| `app/src/main/java/com/beykus/beygram/domain/UiText.kt`, `UserMessageException.kt` | — | Message plumbing used by every story/payment result |

### 2.2 TDLib layer

| File | Size | Contents |
|---|---|---|
| `data/tdlib/TdStories.kt` | 61.5 KB | Active-story list and paging, chat/block filtering, story detail cache, OpenStory/CloseStory, reactions, replies, delete, archive, caption limit, post permission/targets, publish with submission ledger and crash recovery, media upload tracking |
| `data/tdlib/TdPayments.kt` | 45.1 KB | Premium state/features/plans, external checkout resolution, Premium gift codes, Stars balance/history/top-up, gifts catalog/received/collections/send, paid content plan and Stars payment |
| `data/tdlib/TdPaidReactions.kt` | 5.1 KB | Add/commit/rollback of pending paid reactions, single-flight, uncertain-after-commit rule |
| `data/tdlib/TdLibTelegramRepository.kt` | 208 KB | Wiring: `stories`/`payments`/`paidReactions` construction, repository overrides, update fan-out (`stories.handle`), auth lifecycle (`stories.authorized()` / `signedOut()`) |
| `data/tdlib/TdStealthScheduler.kt` | 10.6 KB | Delayed sends; story replies use it |
| `data/tdlib/MediaFileStore.kt`, `TdMediaMapper.kt` | 5.3 / 20.2 KB | File registration for story photos/videos |
| `data/media/StoryMediaPreparer.kt` | 12.9 KB | Photo crop/scale to 1080×1920, video transcode to 720×1280 ≤60 s |
| `data/tdlib/OutgoingMediaContent.kt` + `data/media/OutgoingMediaImporter.kt` | — | Staged upload directory, `markSentUpload` eviction contract |

### 2.3 UI (Compose)

| File | Size | Contents |
|---|---|---|
| `ui/screens/stories/StoryBar.kt` | 6.9 KB | Horizontal bar: add-story cell, per-chat ring, unread border, load-more at end |
| `ui/screens/stories/StoryViewerScreen.kt` | 53.9 KB | Full-screen viewer, progress, pause/hold, tap thirds, prefetch, Open/Close, reactions, reply, delete, edit, offline/cached states |
| `ui/screens/stories/StoryViewerState.kt` | 4.7 KB | Pure viewer state rules: empty phase, pinned story, removal confirmation, successor index, close logging |
| `ui/screens/stories/StoryRules.kt` | 4.1 KB | Pure rules: prefetch targets, initial index, index after update, duration, tap target |
| `ui/screens/stories/StoryPublisherScreen.kt` | 47.1 KB | Publish-as target, media pick/prepare, caption, privacy, lifetime, protect, keep-in-profile, permission gate, archive dialog, submission reconnect |
| `ui/screens/stories/StoryArchiveViewerScreen.kt` | 19.4 KB | Expired-story viewer: views/reactions, caption edit, privacy edit, keep-in-profile toggle, delete |
| `ui/screens/stories/StoryEditDialog.kt` | 9.3 KB | Shared caption/privacy editor |
| `ui/screens/stories/StoryMediaViews.kt` | 7.0 KB | Secure window flag, reduced motion, photo/video rendering |
| `ui/screens/profile/GiftsScreen.kt` | 41.3 KB | Balance card, catalog, send dialog, received gifts paging, collections, Stars link |
| `ui/screens/profile/StarsScreen.kt` | 36.3 KB | Balance, top-up options, confirm/method dialogs, transaction history paging |
| `ui/screens/profile/PremiumScreen.kt` | 56.3 KB | Status, features, plans, gift-code workflow, checkout dialog, stale/TTL handling, skeleton |
| `ui/screens/profile/PaymentExternalLink.kt` | 2.8 KB | Opens validated URL outside the app, excludes own deep-link handlers |
| `ui/screens/chat/PaidContentPayment.kt` | 9.3 KB | Process-scoped paid-message flow (price read → confirm → pay) |
| `ui/screens/chats/ChatsScreen.kt` | — | Story bar placed on the main list only (not search/folders) |
| `ui/screens/profile/ProfileScreen.kt` | — | Premium/Stars/Gifts entry rows, Premium badge |
| `BeyGramApp.kt` | — | Routes `Stories`, `StoryPublisher`, `Premium`, `Stars`, `Gifts`; return-route bookkeeping; open helpers |
| `navigation/AppRoute.kt` | 1.8 KB | Route enum entries |

### 2.4 Android tests used as behaviour specifications

`ui/screens/stories/StoryRulesTest.kt`, `StoryViewerStateTest.kt`, `StoryAddLabelWidthTest.kt`; `domain/model/GiftEligibilityTest.kt`, `GiftSendFlowTest.kt`, `GiftTextAndPagingTest.kt`, `MonetizationFormattingTest.kt`, `PaidContentPaymentTest.kt`, `PaidReactionTest.kt`, `PaymentFlowTest.kt`, `PremiumGiftCodeTest.kt`, `PremiumLoadStateTest.kt`; `data/tdlib/GiftSendTextLimitTest.kt`, `GiftServiceTextTest.kt`, `PaidContentMappingTest.kt`, `PremiumInfoTimeoutTest.kt`, `TdPaidReactionsTest.kt`, `StealthBoundaryTest.kt`; `ui/screens/chat/PaidReactionStoreTest.kt`.

---

## 3. Android behaviour (the specification to match)

### 3.1 Stories feed — the bar

- Metadata only. A full `Story` is fetched by the viewer; the bar carries `StoryPreview {storyId, date, isForCloseFriends}`.
- Sources: `updateChatActiveStories` for `storyListMain` with `order > 0`, plus a direct `getChatActiveStories` for the signed-in user's own chat (`createPrivateChat(getMe)`).
- Ordering: own stories first, then descending `(order, chatId)` (`orderedStoryBar`).
- Filtering before any title/photo reaches the UI: secret chats dropped; live stories dropped; blocked senders dropped (`updateChatBlockList` / `BlockListMain`); deleted stories dropped (`updateStoryDeleted`); chats without a title dropped.
- Paging: `loadActiveStories(storyListMain)` returns one page per call; **404 is the end marker**. Up to 5 pages on refresh; 1 page when the bar reaches its end; a refresh resets the exhausted flag.
- Unread state: `hasUnread = any(storyId > maxReadStoryId)`; the viewer optimistically raises `maxReadStoryId` on a successful `openStory` and the server watermark wins again on the next update.
- Placement: main chat list only — not in search results, not in a server folder.
- Cell: 66 dp avatar ring (accent when unread), 88 dp cell, add-story cell first.

### 3.2 Story viewer

- Entry: `(posterChatId, initialStoryId)`; the index is the requested story, else the first unread, else 0.
- The open story is **pinned by id**, not by index. A partial or empty list update keeps the last known entry; a numeric shift never silently switches the story.
- Removal is confirmed, not guessed: if the author disappears from the bar while connected, the viewer asks `getChatActiveStories` **twice** (750 ms apart). Only two empty answers close it (`Unavailable`); a failure is `Unknown` and changes nothing. A single present-but-missing story moves to its successor.
- Media: full current file + its preview + neighbour previews, at most 4 files (`storyPrefetchTargets`). Neighbour full videos are never prefetched; they are loaded when the viewer moves there.
- Timing: photo 5 s; video uses TDLib duration, clamped to 1–60 s; a missing duration falls back to 5 s.
- Interaction: tap left 34 % = previous, rest = next; press-and-hold pauses; explicit previous/pause/next buttons.
- Read receipts: `openStory` is sent **only after** the detail is loaded, the media is on screen and the window is resumed. `closeStory` is sent in a `NonCancellable` block for a story whose open was *dispatched*, even if the open result was a timeout. This is deliberate: a failed/offline load must not consume the unread marker.
- Overlays: reaction picker (`getStoryAvailableReactions`, `setStoryReaction`), reply (`sendMessage` with `inputMessageReplyToStory`, private chats only, stealth-scheduled when applicable), delete (`deleteStory` + confirm), edit (`editStory` caption + `setStoryPrivacySettings`), close.
- Offline: cached `StoryDetail` is shown with a "cached" notice and a retry; no read mark is sent.
- Every close is logged with a stable reason code (`back`, `close_button`, `completed`, `deleted`, `unavailable`) and the story count only.
- Android-specific: `FLAG_SECURE` while the viewer/publisher is visible.

### 3.3 Story publisher and archive

- Publish-as target: self first, then `getChatsToPostStories`; secret chats and untitled chats dropped; `canPostStory` gate with explicit denial reasons (Premium required, active-story limit, weekly, monthly, boost required, live story active, unavailable). For non-self targets privacy is forced to `Everyone` and the chat controls it.
- Media: photo cropped/scaled to exactly **1080×1920**; video re-encoded to **720×1280**, duration validated 1–60 s; invalid media is refused, never sent.
- Caption: limit from `getOption("story_caption_length_max")`, default 1024, ceiling 4096, counted in code points.
- Privacy: `Everyone`, `Contacts`, `CloseFriends`, `SelectedUsers` (+ contact picker; empty selection is an error).
- Lifetime: 6/12/24/48 h; **only 24 h without Premium** (`storyActivePeriodChoices`).
- Options: `protect_content`, `keep_in_profile`.
- Submission is process-owned and crash-safe: a submission id + TSV ledger (`story-submissions-<account>.tsv`) records `submitting` / `pending` / `published` / `indeterminate` plus a media fingerprint (kind, size, width, height, caption). `postStory` returns a temporary story; `updateStoryPostSucceeded/Failed` is the terminal event. After process death the ledger is reconciled with `getChatActiveStories` + `getStory`, and a timeout becomes `indeterminate` rather than a duplicate post. Leaving the screen hands file cleanup to the repository (`abandonUpload`/`abandonSubmission`).
- Archive: `getChatArchivedStories(selfChatId, fromStoryId, 100)`, newest first, `nextFromStoryId == 0` means end; being-posted stories excluded. Archived viewer shows view/reaction counts, caption/privacy edit, keep-in-profile toggle (`toggleStoryIsPostedToChatPage`), delete. Telegram cannot return an archived story to the active list, and the UI says so.

### 3.4 Gifts

- Catalog: `getAvailableGifts`, sorted by star price; `isPremium`, remaining count (min of user/overall limits), sold-out, auction-only.
- Balance: `getStarTransactions(me, "", null, "", 1)` read only for `starAmount`; a failure leaves the balance unknown instead of inventing one.
- Received gifts (own profile only): `getReceivedGifts(..., offset, 30)` with a cursor; pages merged by id; an unchanged cursor means the end. Sender names resolved via `getUser`/`getChat` in parallel.
- Collections: `getGiftCollections(owner)`.
- Send: exactly one in flight; text validated against `getOption("gift_text_length_max")` (default 128, ceiling 4096); `canSendGift` first; balance re-read; `sendGift(giftId, owner, text, isPrivate, false)` with a 60 s timeout. A timeout or unknown failure is **Uncertain** and is never retried automatically. `STARGIFT_USAGE_LIMITED` maps to sold out. Gifts are not offered for groups; a private/secret chat resolves to its user id.
- Client-side gate before send: own profile, busy, auction-only, sold out, Premium required, balance unknown, insufficient stars.
- Send state is process-scoped (`RetainedGiftSendStore`) so rotation/navigation cannot lose a purchase; an uncertain result survives process death as a warning until acknowledged.

### 3.5 Telegram Stars and payments

- Balance + first page: `getStarTransactions(me, "", null, offset, 30)`.
- Top-up packages: `getStarPaymentOptions`; a failure is surfaced as "options unavailable", not as an empty balance.
- Top-up hand-off: re-read the package, then `getPaymentForm(inputInvoiceTelegram(telegramPaymentPurposeStars(...)))`. Only `paymentFormTypeRegular.additionalPaymentOptions` URLs are exposed. Card-entry forms (`methods.isEmpty()`), test invoices, subscriptions and `paymentFormTypeStars` (which would *spend* Stars) are refused. `sendPaymentForm` is **not** called for top-ups.
- Premium checkout: re-read `getPremiumState`, require the same plan/price/currency/current/upgrade flags, resolve `paymentLink` via `getInternalLink`, validate the HTTPS URL, then open the system browser. `clickPremiumSubscriptionButton` is best-effort analytics. `sendPaymentForm` is never called for Premium.
- Paid content (paid media / invoice messages): `getPaymentForm(inputInvoiceMessage)` → Stars form shows price + balance and requires confirmation → price re-read → `sendPaymentForm(invoice, formId, "", "", null, 0)` only for a matching Stars form. Timeout/unknown = Uncertain, never retried. Real-money forms only expose validated external pages.
- Paid reactions: `addPendingPaidMessageReaction` → `commitPendingPaidMessageReactions`, rollback via `removePendingPaidMessageReactions` in `NonCancellable`; one send at a time; a failure after commit is reported uncertain because Stars may already be spent.
- History: transaction kinds mapped exhaustively (top-up, gift purchase/sale/upgrade/transfer/auction, Premium, paid message send/receive, paid reaction send/receive, paid media, subscriptions, invoices, withdrawal); refund rows keep a distinct identity key.
- URL policy (`validateExternalPaymentUrl`): https only, absolute, no user info, no whitespace/control characters, no encoded CR/LF, no non-443 port, host labels validated, length-bounded. Payment URLs and gift codes are never logged or persisted.

### 3.6 Premium

- Read: `getPremiumState` + `getPremiumFeatures` + `getMe` in parallel, bounded by a 20 s timeout.
- Snapshot is account-bound and expires after 5 minutes; checkout requires a fresh, same-account snapshot with no failed refresh. Status/features/prices are hidden when stale, but the gift-code section stays composed because it owns in-flight work and typed input.
- Features are mapped to a closed enum; unknown server features degrade to `Other`.
- Plans are sorted by months/price/currency; each row is blocked when it is the current plan, has no external checkout link, or another checkout is in flight.
- Gift-code workflow: normalize bare code or `t.me/giftcode/...` link → `checkPremiumGiftCode` → show creator, date, duration, giveaway flag, already-used state, "originally for another user" note → explicit apply → `applyPremiumGiftCode`. Used/expired/invalid/other-user errors are distinct; a timeout is Uncertain.
- Checkout is one browser hand-off at a time; the URL never survives rotation; on return the status is re-read and a success notice appears only if `isPremium` flipped false→true.

---

## 4. Current Windows status

**There is no stories, gifts, Stars, Premium, paid-content or paid-reaction implementation.** The Windows client is the Phase 2 slice: auth, chat list, folders/archive, text history, text send/edit/delete, replies, read receipts, reactions (emoji/custom/paid **display** only), media send with thumbnails, media viewer/playback, profile panel, drafts, global search, settings/theme.

Evidence of absence (searched all of `src/` and `qml/`):

- No request builder or call for `loadActiveStories`, `getChatActiveStories`, `getStory`, `openStory`, `closeStory`, `postStory`, `getChatArchivedStories`, `canPostStory`, `getChatsToPostStories`, `setStoryReaction`, `deleteStory`, `editStory`, `setStoryPrivacySettings`, `toggleStoryIsPostedToChatPage`.
- No call for `getPremiumState`, `getPremiumFeatures`, `getStarTransactions`, `getStarPaymentOptions`, `getPaymentForm`, `sendPaymentForm`, `getAvailableGifts`, `canSendGift`, `sendGift`, `getReceivedGifts`, `getGiftCollections`, `checkPremiumGiftCode`, `applyPremiumGiftCode`, `getInternalLink`, `addPendingPaidMessageReaction`, `commitPendingPaidMessageReactions`.
- No story/gift/star/premium model, controller, QML page, QML component or route.
- `AppController` exposes no story/monetization property or invokable (`src/ui/AppController.h`).
- `TelegramRepository` exposes no story/monetization method (`src/telegram/TelegramRepository.h`).
- No `docs/parity/` entry existed before this file.

What Windows *does* have that this work must reuse:

| Existing piece | Relevance |
|---|---|
| `TelegramSession::sendRequest` + `TdJsonTransport` (`@extra` correlation, per-request timeout) | All new TDLib calls go through this; no QML→TDLib. |
| `TelegramRepository::sendRequest(request, handler, description, timeoutMs)` | Existing repository-side helper pattern for one-shot calls. |
| `MediaFileStore::trackFileObject/trackFileId`, `path`, `isReady`, `fileReady` | Story photo/video and gift sticker assets. Currently only previews are requested; full-size downloads must be added. |
| `MediaImageProvider` (`image://beygrammedia/<fileId>?v=<revision>`) | Story thumbnails, avatars, gift stickers. |
| `MediaViewerController` / `MediaPlaybackController` + `MediaViewerOverlay.qml` / `MediaPlaybackOverlay.qml` | Full-screen overlay pattern the story viewer should follow. |
| `ChatListModel` / `MessageListModel` (`QAbstractListModel` + role enums) | Model shape for the story bar, gift catalog, plans and transactions. |
| `MessageReactions` (`reactionTypePaid` → `"paid"`), `ReactionBar.qml` | Paid reactions already render as a star; only sending is missing. |
| `ReplyInfo` (`messageReplyToStory`, `storyPosterChatId`, `storyId`) | Story replies already parse when they arrive in history; only sending is missing. |
| `AppController` Q_PROPERTY/Q_INVOKABLE + `stateChanged` pattern | QML-facing surface for the new controllers. |
| `tests/CMakeLists.txt` + `beygram_add_test` (Qt Test, offscreen) | Test harness for the pure rules. |
| `docs/WINDOWS_TDLIB.md` — pinned TDLib commit dated 2026-10-27, client reports 1.8.69 | Story/gift/Star API availability must be confirmed against the shipped `tdjson.dll`; the audit did not run it. |

Windows-specific structural facts that shape the plan:

- `qml/Shell.qml` is a `StackLayout` of six fixed sections (`ChatsPage`, `OverviewPage`, `SettingsPage`, `EnvironmentPage`, `LogsPage`, `GlobalSearchPage`). There is **no route stack**; `qml/Main.qml` swaps auth/shell with a `Loader` and already hosts two full-screen overlays.
- `qml/pages/ChatsPage.qml` has a chat-profile side panel but no own-profile screen; `OverviewPage` is the account page.
- Windows has no stealth mode, no Android `FLAG_SECURE`, and no protocol-handler exclusion mechanism. Those Android safeguards need Windows equivalents or explicit documented differences.

---

## 5. Gap analysis

Severity: **S1** blocks a usable feature, **S2** degrades correctness/safety, **S3** polish/parity.

| # | Capability | Android evidence | Windows today | Gap | Sev |
|---|---|---|---|---|---|
| 1 | Active-story feed and ordering | `TdStories.loadActivePages/publish`, `orderedStoryBar` | none | Whole read path | S1 |
| 2 | Story filtering (secret chats, blocked, live, deleted, untitled) | `TdStories.publish`, `updateChatBlockList`, `updateStoryDeleted` | none | Privacy filter must exist before any UI | S1 |
| 3 | Story bar UI + paging + unread ring + add cell | `StoryBar.kt`, `ChatsScreen` item | none | Component + model + placement rules | S1 |
| 4 | Story detail load/cache/prefetch | `TdStories.story`, `storyCache` (64), `storyPrefetchTargets` (≤4) | none | Requests + cache + prefetch policy | S1 |
| 5 | OpenStory/CloseStory read marking | `openStory` after media ready; `closeStory` in `NonCancellable` | none | Correctness-critical; easy to get wrong | S1 |
| 6 | Viewer playback (5 s photo, video duration clamp, pause/hold, tap thirds) | `StoryRules`, `StoryViewerScreen`, `StoryMediaViews` | generic viewer/player only | Story-specific overlay + timing | S1 |
| 7 | Viewer resilience (pinned story, two-empty confirmation, offline cached detail, empty phases) | `StoryViewerState.kt` | none | Pure rules + UI states | S2 |
| 8 | Story reactions | `getStoryAvailableReactions`, `setStoryReaction` | emoji reactions in chats only | Story-scoped reaction picker | S2 |
| 9 | Story reply | `sendMessage` + `inputMessageReplyToStory` | `ReplyInfo` parses it; no send | Send path + composer reuse | S2 |
| 10 | Story delete / caption edit / privacy edit | `deleteStory`, `editStory`, `setStoryPrivacySettings` | none | Own-story actions | S2 |
| 11 | Story archive list + archived viewer + keep-in-profile toggle | `archive()`, `StoryArchiveViewerScreen`, `toggleStoryIsPostedToChatPage` | none | Paging + viewer + toggle | S3 |
| 12 | Story publishing (media prep, caption, privacy, lifetime, protect, keep) | `StoryPublisherScreen`, `StoryMediaPreparer`, `postStory` | media send exists; no 9:16 prep, no transcode | New media pipeline (crop + re-encode) | S1 |
| 13 | Post permission and publish-as targets | `canPostStory`, `getChatsToPostStories`, denial enum | none | Gate + target picker | S2 |
| 14 | Crash-safe story submission | submission ledger, `updateStoryPostSucceeded/Failed`, recovery windows | none | Ledger + reconciliation | S2 |
| 15 | Premium read-only (status, features, plans) | `TdPayments.premiumInfo` | none | Requests + models + screen | S1 |
| 16 | Premium snapshot TTL/account binding/stale rules | `PremiumLoadState`, `PremiumSnapshot` | none | Pure rules + UI gating | S2 |
| 17 | Premium external checkout hand-off | `premiumPaymentUrl`, `getInternalLink`, URL policy | none | Re-read + validate + open browser | S1 (if checkout is offered) |
| 18 | Premium gift-code workflow | `checkPremiumGiftCode`, `applyPremiumGiftCode`, `PremiumGiftCodeStep` | none | Two-step workflow + error classes | S2 |
| 19 | Stars balance + history paging | `starsInfo`, `starTransactions` | none | Requests + model | S1 |
| 20 | Stars top-up via external pages | `getStarPaymentOptions`, `getPaymentForm`, `StarsTopUpStep` | none | Package re-check + method chooser | S2 |
| 21 | Gifts catalog + received + collections | `getAvailableGifts`, `getReceivedGifts`, `getGiftCollections` | none | Requests + models + screen | S1 |
| 22 | Gift send with single-flight/uncertain semantics | `sendGift`, `canSendGift`, `giftBlockReason`, retained store | none | Money-safety rules | S1 |
| 23 | Paid message/media purchase | `paidContentPlan`, `payPaidContentWithStars` | none (no paid-media rendering either) | Plan/confirm/pay + bubble state | S2 |
| 24 | Paid reactions | `TdPaidReactions` add/commit/rollback | display only | Send path + rollback | S2 |
| 25 | Profile entry points and Premium badge | `ProfileScreen` rows, `profile_badge_premium` | account page has no monetization links | Entry points | S3 |
| 26 | Payment URL hardening | `validateExternalPaymentUrl`, `PaymentExternalLink` | `AppController::openLink` exists but has no payment-specific policy | Add explicit payment URL policy | S1 (security) |
| 27 | Secure display of personal content | `FLAG_SECURE` in `StoryMediaViews` | no equivalent | Document the difference or add a Windows policy | S2 |
| 28 | Reduced-motion support | `rememberReducedMotion` | theme/motion settings exist for chats | Honour the existing motion setting in the viewer | S3 |
| 29 | Story/gift state across account switch and restart | ledger, account-bound snapshots, retained send store | single-account build | Must be account-scoped from day one | S2 |
| 30 | Test coverage of the pure rules | `StoryRulesTest` (14 cases), `StoryViewerStateTest` (8), `StoryAddLabelWidthTest` (3); 11 monetization suites | none | Port the rule suites | S2 |

---

## 6. Implementation plan

The order below is deliberate: pure rules first (cheap, testable, no TDLib), then read-only screens, then anything that spends money, then anything that uploads.

### Phase 0 — Shared plumbing (no user-visible feature)

**Files (new):**

- `src/telegram/StoryTypes.h` — `StoryPreview`, `ChatStories`, `StoryMedia`, `StoryDetail`, `StoryPrivacy`, `StoryPostTarget`, `StoryArchivePage`, `StoryPostPermission/Denial`, `StoryPostTicket`, `StorySubmissionState`, `StoryPostEvent`.
- `src/telegram/MonetizationTypes.h` — Premium, Stars, gifts, paid-content and paid-reaction value types.
- `src/telegram/PaymentUrl.h/.cpp` — `validateExternalPaymentUrl` equivalent returning `std::optional<QUrl>`; reject non-https, user info, whitespace/control characters, `%0d`/`%0a`, non-443 ports, malformed labels, over-long values.
- `src/telegram/StoryRules.h/.cpp` — `storyPrefetchTargets`, `storyInitialIndex`, `storyIndexAfterUpdate`, `storyDurationMillis`, `storyTapTarget`, `storyCropRect`, `storyActivePeriodChoices`, `orderedStoryBar`, `orderedStoryPostTargets`.
- `src/telegram/StoryViewerRules.h/.cpp` — `storyViewerEmptyPhase`, `storyViewerEntryUpdate`, `storyViewerPinnedStory`, `needsStoryRemovalConfirmation`, `storySuccessorIndex`, `confirmStoryListState` (as a callback-driven helper).
- `src/telegram/MonetizationRules.h/.cpp` — `premiumPlanBlock`, `PremiumCheckoutStep`/`StarsTopUpStep`/`GiftSendStep`/`PremiumGiftCodeStep` transition functions, `giftBlockReason`, `giftTextLimit`, `validateGiftText`, `mergeReceivedGifts`, `nextReceivedGiftsOffset`, `formatPriceMinorUnits`, `formatStarAmount`, `classifyPremiumGiftCodeError`, `normalizePremiumGiftCode`, `validatePremiumGiftCode`.

**Modify:** `src/telegram/CMakeLists.txt`, `tests/CMakeLists.txt`.

**Tests (new):** `tests/tst_story_rules.cpp`, `tests/tst_story_viewer_rules.cpp`, `tests/tst_monetization_rules.cpp`, `tests/tst_payment_url.cpp` — direct ports of `StoryRulesTest`, `StoryViewerStateTest`, `PaymentFlowTest`, `PremiumLoadStateTest`, `PremiumGiftCodeTest`, `GiftEligibilityTest`, `GiftSendFlowTest`, `GiftTextAndPagingTest`, `MonetizationFormattingTest`, `PaidContentPaymentTest`, `PaidReactionTest`.

**Exit:** rules compile and the ported expectations hold; no TDLib, no UI.

### Phase 1 — Story read path (repository)

**Files (new):**

- `src/telegram/StoryRequests.h/.cpp` — pure JSON builders: `loadActiveStories`, `getChatActiveStories`, `getStory`, `openStory`, `closeStory`, `getStoryAvailableReactions`, `setStoryReaction`, `deleteStory`, `editStory`, `setStoryPrivacySettings`, `toggleStoryIsPostedToChatPage`, `canPostStory`, `getChatsToPostStories`, `getChatArchivedStories`, `getOption("story_caption_length_max")`, `sendMessage` with `inputMessageReplyToStory`.
- `src/telegram/Stories.h/.cpp` — the `TdStories` equivalent: active map, ordering/filtering, detail cache (LRU 64), read overrides, `openedStories`, generation counter, paging with 404 end marker (5 pages refresh / 1 page append), `authorized()`/`signedOut()`.
- `src/telegram/StoryListModel.h/.cpp` — `QAbstractListModel` roles: `ChatIdRole`, `TitleRole`, `InitialsRole`, `PhotoFileIdRole`, `PhotoReadyRole`, `IsSelfRole`, `UnreadRole`, `OrderRole`, `StoryCountRole`, `MaxReadStoryIdRole`.
- `src/telegram/StoryDetailModel.h/.cpp` (optional) — single-story detail for QML binding, or expose detail as `Q_PROPERTY`s on the controller.

**Modify:**

- `src/telegram/TelegramRepository.h/.cpp` — own a `Stories`, route `updateChatActiveStories`, `updateChatBlockList`, `updateChatPhoto`, `updateChatTitle`, `updateStoryDeleted`; add `refreshActiveStories`, `loadMoreActiveStories`, `cachedActiveStory`, `cachedStory`, `loadStory`, `confirmActiveStoryIds`, `openStory`, `closeStory`, `storyReactions`, `setStoryReaction`, `replyToStory`, `deleteStory`, `editStoryCaption`, `setStoryPrivacy`, `storyCaptionLimit`.
- `src/telegram/MediaFileStore.h/.cpp` — add a full-size download request (`trackFileId(fileId, priority, fullSize)`) so story photos/videos can be fetched; keep preview behaviour unchanged.
- `src/ui/AppController.h/.cpp` — expose `activeStories` model, `storyViewerOpen`, `storyPosterChatId`, invokables `openStories(chatId, storyId)`, `closeStories()`, `storyLoadMore()`, plus a `storyChanged` signal.

**Tests (new):** `tests/tst_story_requests.cpp` (JSON shape and `@type` names), `tests/tst_story_list_model.cpp` (ordering, filtering inputs, unread flag), `tests/tst_media_file_store.cpp` additions for full-size requests.

**Exit:** the bar can be populated from a live session, and `getStory`/`openStory`/`closeStory` work without any UI.

### Phase 2 — Story viewer and bar UI

**Files (new):**

- `qml/components/StoryBar.qml` — `ListView` horizontal, add cell, ring, unread border, load-more at end, keyboard navigation.
- `qml/components/StoryViewerOverlay.qml` — follows `MediaViewerOverlay.qml` conventions (`visible: BeyGramStories.viewerOpen`, `z: 1000`, `Keys.onEscapePressed`), progress row, header (avatar/title/time/privacy), caption gradient, controls row, reaction picker, reply box, delete confirm, edit dialog.
- `qml/components/StoryProgressRow.qml`, `qml/components/StoryEditDialog.qml`, `qml/components/StoryReactionPicker.qml`.
- `src/ui/StoryController.h/.cpp` — QML-facing viewer state: current index/story, progress-driving duration, pause, `previous()/next()`, `openStoryForCurrent()`, `closeStoryForCurrent()`, prefetch targets, offline/cached/empty phase, notice text.

**Modify:** `qml/pages/ChatsPage.qml` (insert the bar above pinned chats, only when no search and no folder is active), `qml/Main.qml` (host `StoryViewerOverlay`), `src/ui/CMakeLists.txt`, `src/app/main.cpp` (context property registration).

**Tests:** `tests/tst_story_controller.cpp` for state transitions that do not need a window; QML logic kept thin so it is testable in C++.

**Exit:** feed → viewer → read mark → close, with reactions/reply/delete/edit, works on a real session; offline and partial-update behaviour matches §3.2.

### Phase 3 — Premium read-only and gift codes

**Files (new):**

- `src/telegram/PaymentsRequests.h/.cpp` — `getPremiumState`, `getPremiumFeatures`, `getMe`, `getInternalLink`, `clickPremiumSubscriptionButton`, `checkPremiumGiftCode`, `applyPremiumGiftCode`.
- `src/telegram/Premium.h/.cpp` — `premiumInfo()` (parallel reads, 20 s bound), `premiumPaymentUrl(plan)` (re-read + `getInternalLink` + URL policy + best-effort click), `checkPremiumGiftCode`, `applyPremiumGiftCode`, `PremiumLoadState`/`PremiumSnapshot` with 5-minute TTL and account binding.
- `src/telegram/PremiumPlanModel.h/.cpp` — plan rows with `isCurrent`, `isUpgrade`, `hasExternalLink`, discount.
- `src/ui/MonetizationController.h/.cpp` — QML surface: `premiumLoading`, `premiumStatusText`, `premiumIsPremium`, `premiumStale`, `premiumFeatures` (list), `premiumPlans` (model), invokables `refreshPremium()`, `selectPlan(monthCount)`, `confirmCheckout()`, `cancelCheckout()`, `checkGiftCode(text)`, `applyGiftCode()`, `openPaymentPage()`.

**Modify:** `qml/pages/OverviewPage.qml` (Premium/Stars/Gifts rows), `qml/Main.qml` (payment-page open handler), `src/ui/AppController.*`, `src/app/main.cpp`.

**QML (new):** `qml/pages/PremiumPage.qml` (status card, features card, plan rows, gift-code section, stale/refresh-failed cards, skeleton), `qml/components/PremiumCheckoutDialog.qml`.

**Opening the browser:** use `QDesktopServices::openUrl` only after `PaymentUrl` validation. Today `AppController::openLink` already hands `https`/`tg` URLs to `QDesktopServices` with no payment-specific policy; once `tg://` and `t.me` protocol registration lands (deep links, roadmap Phase 5), a Telegram-hosted payment URL could be routed back into the app. Either fail closed on Telegram-hosted payment links or add an explicit browser-only hand-off that never re-enters the app's link handling. This needs an explicit decision — see §9.

**Tests:** `tests/tst_premium.cpp` (TTL, account binding, stale gating, plan matching, gift-code validation/classification), `tests/tst_payments_requests.cpp`.

**Exit:** Premium status/features/plans and gift-code activation work; checkout either opens a validated external page or is honestly unavailable. No card data anywhere.

### Phase 4 — Stars read-only and top-up

**Files (new):**

- `src/telegram/Stars.h/.cpp` — `starsInfo()` (balance + first page + options, options failure non-fatal), `starTransactions(offset)`, `starBalance()`, `starsTopUpPayment(option)` (re-read package, `getPaymentForm(telegramPaymentPurposeStars)`, reject card forms/test invoices/subscriptions/Stars-spending forms, expose only validated `additionalPaymentOptions`).
- `src/telegram/StarTransactionModel.h/.cpp`, `src/telegram/StarTopUpModel.h/.cpp`.

**Modify:** `PaymentsRequests`, `TelegramRepository`, `MonetizationController` (`starBalance`, `starTransactions` model, `topUpOptions`, `topUpStep`, `prepareTopUp`, `openTopUpMethod`).

**QML (new):** `qml/pages/StarsPage.qml` (balance card, packages, history with load-more, confirm dialog, method chooser, notices), `qml/components/StarTopUpDialog.qml`.

**Tests:** `tests/tst_stars.cpp` — package matching, form-type rejection, cursor termination, refund identity keys, balance delta notice after return.

**Exit:** balance and history read correctly; a top-up either opens a validated external page or reports "unavailable" without charging anything.

### Phase 5 — Gifts

**Files (new):**

- `src/telegram/Gifts.h/.cpp` — catalog (`getAvailableGifts`), balance, received paging (`getReceivedGifts`, 30), collections (`getGiftCollections`), owner resolution (`getChat` → `messageSenderUser`; groups rejected), `sendGift` with single-flight, `canSendGift`, `getOption("gift_text_length_max")`, 60 s timeout, `STARGIFT_USAGE_LIMITED` → sold out, unknown-after-send → uncertain.
- `src/telegram/GiftCatalogModel.h/.cpp`, `src/telegram/ReceivedGiftModel.h/.cpp`.

**Modify:** `PaymentsRequests`, `TelegramRepository`, `MonetizationController` (`giftsInfo`, `receivedGifts`, `sendGift`, retained-send state keyed by account + profile).

**QML (new):** `qml/pages/GiftsPage.qml` (balance card, catalog grid, send dialog with message + hide-name, received list with paging, collections, Stars link), `qml/components/GiftSendDialog.qml`.

**Entry points:** profile panel "Send gift" for private chats; own-profile "Gifts" row.

**Tests:** `tests/tst_gifts.cpp` — eligibility gate, text limit, paging merge, sold-out, single-flight, uncertain persistence across a simulated restart.

**Exit:** gifts can be browsed, received gifts paged, and exactly one gift sent with an honest outcome.

### Phase 6 — Story publishing and archive

**Files (new):**

- `src/telegram/StoryPublisher.h/.cpp` — permission/targets, caption limit, publish with submission ledger (`story-submissions-<account>.tsv` equivalent, or `QSettings`/JSON in the account directory), pending/indeterminate reconciliation, upload abandonment.
- `src/media/StoryMediaPrepare.h/.cpp` — 9:16 crop to 1080×1920 for photos; video re-encode to 720×1280, ≤60 s. **This is the largest new dependency**: Android uses Media3 Transformer; Windows needs either FFmpeg (adds a binary + licence review) or Media Foundation (`IMFTransform`/`IMFSinkWriter`) with an H.264 encoder. Decision required — see §9.
- `src/telegram/StoryArchive.h/.cpp` — `getChatArchivedStories` paging (100), archived detail mapping, `toggleStoryIsPostedToChatPage`.

**QML (new):** `qml/components/StoryPublisherOverlay.qml` (media pick via `FileDialog`, preview, caption + counter, privacy chips, recipient picker, lifetime chips, protect/keep switches, permission/denial block, publish button, archive panel), `qml/components/StoryArchivePanel.qml`, `qml/components/StoryArchiveViewerOverlay.qml`, `qml/components/StoryRecipientsDialog.qml`.

**Tests:** `tests/tst_story_publisher.cpp` — permission denial mapping, period validation, privacy requirement, ledger round-trip, recovery decisions, target ordering; `tests/tst_story_media_prepare.cpp` — crop maths and dimension validation (pure parts only; the codec path needs an integration test).

**Exit:** a photo and a video can be published to self and to an eligible channel, survive a process restart mid-upload, and appear in the archive with edit/delete.

### Phase 7 — Paid content and paid reactions (optional, last)

**Files (new):** `src/telegram/PaidContent.h/.cpp` (plan/confirm/pay, Stars-only `sendPaymentForm`, price re-check), `src/telegram/PaidReactions.h/.cpp` (add/commit/rollback, single-flight).

**Modify:** message model/roles for paid-media and invoice messages (currently absent), `MessageBubble.qml`/`ReactionPicker.qml`.

**Tests:** `tests/tst_paid_content.cpp`, `tests/tst_paid_reactions.cpp` (rollback on failure, uncertain after commit, no double charge).

**Note:** this phase also requires rendering paid media/invoice messages, which Windows does not do today. It is therefore larger than it looks and should not block Phases 1–6.

---

## 7. Test plan

**Pure rule tests (Qt Test, `QT_QPA_PLATFORM=offscreen`, no TDLib):** the Phase 0 suites. These are near one-to-one ports and give the highest confidence per hour. They must cover the cases the Android suites already pin: prefetch de-duplication and the 4-file cap, requested/unread index selection, live-update index stability, video duration clamps, tap thirds, two-empty removal confirmation, successor selection, plan/package/price mismatch, one-checkout-at-a-time, gift eligibility order, gift text limits, cursor termination, refund identity keys, gift-code validation and error classification, URL policy.

**Request-shape tests:** assert `@type` and field names for every new builder (`tests/tst_story_requests.cpp`, `tests/tst_payments_requests.cpp`). This is how the existing `tst_telegram_requests.cpp` guards the conversation slice.

**Model tests:** ordering/filtering/unread in `StoryListModel`; catalog ordering and sold-out flags; transaction merge and `hasMore`.

**Integration (opt-in, real session):** extend the existing `tst_tdlib_integration` pattern only behind the same opt-in gate. Suggested checks: active list loads and pages; `getStory` returns media ids; `openStory`/`closeStory` accepted; `getPremiumState` returns plans; `getStarTransactions` returns a balance; `getAvailableGifts` returns a catalog. **Do not** run payment mutations in automated tests.

**Manual acceptance (documented, not claimed here):** story bar renders and pages; viewer marks read only after media is visible; offline viewer shows cached content and sends no read mark; gift send is disabled with an unknown balance; Premium checkout opens the system browser and never an embedded view; a top-up that offers only card fields reports "unavailable"; account switch clears every snapshot.

**Never automated:** `sendGift`, `applyPremiumGiftCode`, `sendPaymentForm`, `postStory`. These spend money or publish content; they are manual, explicit and single-shot by design.

---

## 8. Priority and sequencing

**Overall priority: P3.** This matches `docs/PORTING_AUDIT.md` (M10 gifts/Premium/Stars and M11 stories are both P3) and Phase 5 of `docs/PORTING_ROADMAP.md`. It is above "nice to have" but below the P0/P1/P2 work that makes the client usable at all (auth, chat list, history, media, notifications, settings/security). Do not start Phase 1 before the chat list and conversation slices are stable: stories reuse the same profile/avatar/media plumbing and would otherwise be built twice.

**Within this scope:**

| Order | Slice | Why first | Effort |
|---|---|---|---|
| 1 | Phase 0 rules + tests | Zero risk, unlocks everything, catches design mistakes cheaply | S |
| 2 | Phase 1 story read path | Highest user-visible value per unit of work; no money, no uploads | M |
| 3 | Phase 2 viewer + bar | Completes the story read experience | L |
| 4 | Phase 3 Premium read-only + gift codes | Cheap, read-only, exercises the payment-URL policy safely | M |
| 5 | Phase 4 Stars read-only + top-up | Needed before gifts make sense (balance/price display) | M |
| 6 | Phase 5 Gifts | First money-spending path; needs the safety rules from Phase 0 | L |
| 7 | Phase 6 publishing + archive | Largest risk: media transcode + crash-safe submission | XL |
| 8 | Phase 7 paid content/reactions | Depends on message rendering that does not exist yet | L |

Rough size: Phases 0–2 ≈ 3–4 weeks; Phases 3–5 ≈ 3–4 weeks; Phase 6 ≈ 4–6 weeks (dominated by the video pipeline); Phase 7 ≈ 2–3 weeks. These are planning estimates, not measured work.

**Recommended first deliverable:** Phases 0 + 1 + a read-only Premium card. That combination proves the TDLib wiring, the model pattern, the URL policy and the test harness with no financial or upload risk.

---

## 9. Risks and open questions

1. **TDLib API generation.** The pinned `tdjson` (client reports 1.8.69, commit 2026-10-27) should expose the story/gift/Star methods used by Android, but this audit did not run the client to confirm. Verify with `getOption("version")` and a `getAvailableGifts` call before committing to Phase 5.
2. **Video re-encode dependency.** Android relies on Media3 Transformer. Windows needs FFmpeg or Media Foundation. This is a licensing, packaging and QA decision that must be made before Phase 6; it is the single largest unknown in this plan.
3. **Payment link vs. protocol registration.** `AppController::openLink` hands `https`/`tg` URLs to `QDesktopServices`, and `tg://` and `t.me` protocol registration is planned for deep links. Opening a `t.me` payment URL through that path could bounce back into the app. Decide between failing closed on Telegram-hosted payment links or a browser-only hand-off that bypasses the app's handler.
4. **No `FLAG_SECURE` equivalent.** Android hides story content from Recents and screenshots. Windows has no portable equivalent. Either accept and document the difference, or add an optional "hide from capture" policy (Windows 10 2004+ `SetWindowDisplayAffinity`), which affects the whole window and must be a product decision.
5. **Stealth mode.** Android suppresses story views and schedules story replies. Windows has no stealth feature; if it is ported later, `Stories` must accept the same policy hook from day one rather than retrofitting it.
6. **Account scoping.** Premium snapshots, gift-send state and the story submission ledger are account-bound in Android. Windows is currently single-account, but the multi-account docs describe per-account sessions; keying state by account id now avoids a migration later.
7. **Paid media rendering.** Phase 7 is blocked by the absence of paid-media/invoice message rendering in the Windows message model. Confirm whether that is in scope before scheduling Phase 7.
8. **Story reply composer.** Reusing the chat composer across a full-screen overlay may fight focus/keyboard handling. Decide between an embedded mini-composer in the overlay (Android's dialog) and routing to the chat with a pre-filled reply.
9. **Privacy of cached story media.** Android keeps full story files in the media cache and relies on `FLAG_SECURE` for display. Windows should decide whether story files are cached like message media or evicted when the viewer closes.

---

## 10. Verification status

- **Build:** not run. No CMake configure or compile of the Windows tree, no Gradle task for the Android tree.
- **Tests:** not run. No `ctest`, no Android unit test, no instrumentation test.
- **Device/emulator:** not used. No `adb`, no GUI launch, no TDLib session.
- **Static review only:** the Android and Windows sources, QML, CMake/test registration and the porting docs named above.

What would need verification before this plan is treated as complete:

1. The shipped `tdjson.dll` exposes the story/gift/Star/Premium methods listed in §6 (start with `getOption("version")`, `getAvailableGifts`, `getPremiumState`).
2. The ported rule suites in §7 compile and pass against the Qt Test harness.
3. A live session renders the story bar, marks a story read only after its media is visible, and keeps the viewer open through a partial list update.
4. Premium and Stars screens show server data and open only validated external pages; no card data is ever collected.
5. A gift send and a Premium gift-code activation are single-shot, correctly report uncertain outcomes, and survive a restart.
6. A published story survives a process restart mid-upload without creating a duplicate.

