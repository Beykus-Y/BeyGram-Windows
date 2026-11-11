# 09 — Profiles, contacts, blocking, reporting parity: Android → Windows

Read-only parity audit of **user / group / channel profiles**, **own-profile editing**, **contacts**,
**blocking**, **reporting** and **shared media**.

Method: static reading of the Android Kotlin sources and the Windows C++/QML sources only. No build,
CMake configure, test run, GUI run, device interaction or `git` command was executed for this
document, so every statement below describes *code as written*, not observed behaviour. Nothing was
verified against a live TDLib instance.

Reference items: `docs/PORTING_AUDIT.md` — `M12 | Profiles, contacts, groups, channels` (priority
**P2**, "permission checks, profile mapping"), `docs/PORTING_ROADMAP.md` Phase 3 items 3–4
("Profiles: own/user/group/channel, photo update, shared media, members" and "Contacts, blocking,
privacy and active sessions"), and [08-search.md](08-search.md) for the people-search sources this
document defers to Stage 8.

- Android reference: `app/src/main/java/com/beykus/beygram/**`
- Windows target: `src/telegram/**`, `src/ui/**`, `qml/**`
- Date of audit: 2026-11-11

---

## 1. Files reviewed

### 1.1 Android (reference behaviour)

| Area | Files |
| --- | --- |
| Profile model | `domain/model/ProfileInfo.kt`, `domain/model/ProfilePhotos.kt`, `domain/model/SharedMedia.kt`, `domain/model/MessageReportReason.kt` |
| Profile data layer | `data/tdlib/TdProfiles.kt`, `data/tdlib/TdProfilePhotos.kt` |
| Contacts model | `domain/model/Contacts.kt`, `domain/model/GlobalSearchPeople.kt` |
| Contacts data layer | `data/tdlib/TdContacts.kt`, `data/tdlib/ContactRequests.kt`, `data/tdlib/ContactCard.kt` |
| Blocking / reporting / privacy | `data/tdlib/TdPrivacy.kt`, `data/tdlib/TdLibTelegramRepository.kt` (block + report paths), `domain/model/Privacy.kt` |
| Shared media | `data/tdlib/TdConversationActions.kt` (`shared()`, filters), `data/tdlib/ChatSearchRequest.kt` |
| Profile UI | `ui/screens/profile/ProfileScreen.kt` (incl. `EditProfileScreen`), `GroupProfile.kt`, `ChatInfoMenu.kt`, `ProfilePhotoGallery.kt`, `SharedMediaSection.kt`, `ChatMembersScreen.kt`, `GroupMembersScreen.kt`, `MemberRow.kt`, `GroupManagementScreen.kt`, `GroupSettingsScreen.kt`, `GroupJoinRequestsScreen.kt`, `GroupInvitesScreen.kt`, `ChannelStatisticsScreen.kt`, `PremiumScreen.kt`, `StarsScreen.kt`, `GiftsScreen.kt` |
| Contacts UI | `ui/screens/contacts/ContactsScreen.kt`, `ui/screens/contacts/ContactsScreenState.kt` |
| Report UI | `ui/screens/chat/MessageReportDialog.kt`, `ui/screens/chat/ChatScreen.kt` (report invocation) |
| Host wiring | `BeyGramApp.kt` (routes `MyProfile`, `EditProfile`, `Profile`, `Contacts`) |
| Manifest | `app/src/main/AndroidManifest.xml` |
| Tests | `app/src/test/java/.../ui/screens/profile/*`, `.../ui/screens/contacts/*`, `.../data/tdlib/ContactCardTest.kt`, `ContactRequestsTest.kt`, `PrivacyRulesTest.kt`, `ChatSearchRequestTest.kt`, `app/src/androidTest/.../profile/ProfileLoadErrorTest.kt` |

### 1.2 Windows (current state)

| Area | Files |
| --- | --- |
| Profile model | `src/telegram/UserProfile.h`, `src/telegram/UserProfile.cpp` |
| Profile requests | `src/telegram/TelegramRepository.cpp` (`requestProfile`, ~L528–599), `src/telegram/TelegramRepository.h` |
| Profile UI state | `src/ui/AppController.h` (L113–122, L207–216, L278–279, L358–363), `src/ui/AppController.cpp` (L297, L341–359, L611–626, L957–964, L1113–1131) |
| Profile UI | `qml/pages/ChatsPage.qml` (header button L343–347, drawer L737–839), `qml/Shell.qml` (L141–146) |
| Profile edit | `src/telegram/ProfileEdit.h`, `src/telegram/ProfileEdit.cpp`, `qml/components/ProfileEditDialog.qml` (**unused**) |
| Contacts | `src/telegram/ContactSearch.h`, `src/telegram/ContactSearch.cpp` (**not wired**), `qml/pages/GlobalSearchPage.qml` |
| Shared media | none |
| Blocking / reporting | none |
| Tests | `tests/tst_user_profile.cpp`, `tests/tst_contact_search.cpp`, `tests/tst_profile_edit.cpp`, `tests/CMakeLists.txt` |

---

## 2. Android reference behaviour

### 2.1 Profile data model (`ProfileInfo`)

One immutable snapshot per chat/user, published through `StateFlow<ProfileState>`:

| Group | Fields |
| --- | --- |
| Identity | `id`, `title`, `kind` (`Person`/`Bot`/`Group`/`Channel`/`Secret`), `firstName`, `lastName`, `initials` |
| Photo | `photo: AvatarPhoto(smallFileId, bigFileId, photoId)` |
| Handles | `username` (editable), `usernames` (all active) |
| Contact data | `phone`, `bio`, `birthday(day, month, year?)`, `address` (business account) |
| Presence | `status`: `Bot`, `DeletedAccount`, `Online`, `Recently`, `LastWeek`, `LastMonth`, `LongAgo`, `LastSeen(wasOnlineMillis)` |
| Social proof | `commonGroupCount`, `isVerified`, `isPremium`, `personalChatId` / `personalChatTitle` |
| Group/channel | `memberCount`, `onlineMemberCount`, `administratorCount`, `inviteLink`, `canListMembers`, `canViewStatistics`, `groupAdmin: GroupAdminRights?`, `administrators`, `memberPreview` |
| Shared media | `sharedCounts: SharedCounts(media, files, links, voice, music)` |
| Self / block | `isSelf`, `isBlocked` |

`isBlocked` comes from `userFullInfo.blockList is BlockListMain` and is what flips the profile menu
between Block and Unblock.

### 2.2 What is loaded, and with which TDLib calls

`TdProfiles` keeps a per-chat cache (`chats`, `users`, `userFull`, `groups`, `groupFull`, `supers`,
`superFull`, `onlineCounts`, `sharedCounts`, `chatAdmins`, `chatMemberPreview`) behind one lock and
republishes immutable snapshots. `refreshChat(chatId)` runs:

1. `GetChat(chat_id)`.
2. By chat type:
   - `ChatTypePrivate` / `ChatTypeSecret` → `GetUser(user_id)`, `GetUserFullInfo(user_id)`, plus
     `GetChat(personalChatId)` when `userFullInfo.personalChatId != 0` (failure tolerated).
   - `ChatTypeBasicGroup` → `GetBasicGroup(basic_group_id)` + `GetBasicGroupFullInfo(basic_group_id)`.
   - `ChatTypeSupergroup` → `GetSupergroup(supergroup_id)` + `GetSupergroupFullInfo(supergroup_id)`.
3. All of the above under a 25 s timeout; a failure becomes a localized `UiText` error while cached
   data stays visible.
4. Three *independent* follow-ups (each with its own timeout, each failure ignored):
   - shared counts: `GetChatMessageCount(chatId, null, filter, false)` for
     `SearchMessagesFilterPhotoAndVideo`, `…Document`, `…Url`, `…VoiceNote`, `…Audio`;
   - administrators: `GetChatAdministrators(chatId)` (max 20, owner/admin names resolved);
   - member preview: `GetSupergroupMembers(SupergroupMembersFilterRecent, 0, 10)` or the basic
     group's full member list, only when `canGetMembers`/basic group.

Own profile: `refreshMe()` = `GetMe` + `GetUserFullInfo` + `GetOption("bio_length_max")`.

Live updates handled by `TdProfiles.handle`: `UpdateUser`, `UpdateUserStatus`,
`UpdateUserFullInfo`, `UpdateBasicGroup(FullInfo)`, `UpdateSupergroup(FullInfo)`, `UpdateChatPhoto`,
`UpdateChatTitle`, `UpdateChatPermissions`, `UpdateChatOnlineMemberCount`, `UpdateChatAction`
(typing) and `UpdateOption` (`bio_length_max`).

### 2.3 Profile screen composition

`ProfileScreen` (person/bot/secret) renders, top to bottom:

- Toolbar: back, overflow `ChatInfoMenu` for non-self profiles.
- Avatar (tap opens `ProfilePhotoGallery`), title, subtitle by kind:
  - group → "N members" or "N members, M online";
  - channel → "N subscribers";
  - secret → presence inside a secret-chat status string;
  - bot → "Bot"; person → presence or "your Telegram account".
- Badges: Verified / Premium.
- Own profile: "Edit" + "Change photo" buttons (stacked when narrow or font scale ≥ 1.3).
- Other person/secret: "Send a gift".
- Secret-chat entry when eligible (`SecretChatEligibility.canStart`: person, not self, not bot, not
  deleted) with a confirmation dialog.
- Own profile only: Premium / Stars / Gifts entry rows.
- `ProfileRowsCard` rows: phone, every username, bio ("About me", or "Description" for
  group/channel), birthday, business address, common groups, invite link, member/subscriber count,
  administrator count, type. Shared counts move into tab headers when the tabs are present.
- Group management entry when `groupAdmin.canManageAnything`.
- Personal (linked) channel row when `personalChatId > 0`.
- Administrators section, members preview section (`canListMembers`).
- Shared media tabs.
- Hidden-tabs section and notification entry (own profile).

Error handling is explicit: `ProfileLoadError(message, loading, onRetry)` keeps already-loaded data
visible and offers Retry. A missing profile with no error shows `profile_info_not_loaded`.

### 2.4 Own-profile edit flow

`EditProfileScreen` → `TelegramRepository.updateMyProfile(ProfileEdit(firstName, lastName, username, bio))`
→ `TdProfiles.edit()`:

- Guard: profile must be loaded (`err_profile_not_loaded`).
- First name required (`err_name_empty`), first/last ≤ 64 characters (`err_name_too_long`).
- Bio length counted in **code points** against the server-provided `bio_length_max`
  (`err_bio_too_long`); the screen shows a live `n/limit` counter and disables Save past the limit.
- Username trimmed, `@` stripped, must be empty, unchanged, or match `[A-Za-z][A-Za-z0-9_]{4,31}`
  (`err_username_format`).
- Only changed fields are sent: `SetUsername`, `SetName`, `SetBio`.
- `finally { refreshMe() }` — a partial failure still refreshes, and the UI appends
  `profile_save_partial_hint` to the error.
- Unsaved-changes guard: Back shows "Discard changes?" instead of leaving.

Photo management:

- `setPhoto(OutgoingMedia)` → `SetProfilePhoto(InputChatPhotoStatic(InputFileLocal(path)), false)`,
  60 s timeout, then `refreshMe`.
- `deleteMyPhoto(photoId)` → `DeleteProfilePhoto`.
- `setPreviousPhoto(photoId)` → `SetProfilePhoto(InputChatPhotoPrevious(photoId), false)`.
- Gallery: `GetUserProfilePhotos(userId, offset, limit)` for users, merged with the current full-info
  photo; for groups/channels only the current photo is available.
- The picker uses `ActivityResultContracts.PickVisualMedia` (Android Photo Picker, no storage
  permission) and `OutgoingMediaImporter` (square crop for avatars).

### 2.5 Contacts

`TdContacts`:

| Operation | TDLib | Notes |
| --- | --- | --- |
| List | `GetContacts()` → ids → parallel `GetUser` | one failing user is skipped; `distinctBy(id)`; sorted by lower-cased title |
| Search | `SearchContacts(query, limit)` | keeps TDLib relevance order; returns `totalCount` for paging-by-larger-limit |
| Add | `ImportContacts([ImportedContact])` | rejects all-zero ids as "not on Telegram"; phone normalized, ≥ 7 digits |
| Edit | `AddContact(userId, importedContact, sharePhoneNumber = false)` | jar has no `EditImportedContact`; never shares the account's own number |
| Delete | `RemoveContacts([userId])` | removes the address-book entry only; does not block or delete history |
| Share | `GetUser` → `Contact(phone, first, last, vCard, userId)` | vCard built locally with RFC 6350 escaping |

Validation: names ≤ 64 chars, at least one of first/last non-blank, phone either blank or ≥ 7 digits.
TDLib errors are mapped: `PHONE_NUMBER_INVALID`, `PHONE_NUMBER_OCCUPIED`,
`PHONE_NUMBER_NOT_OCCUPIED`/`UNOCCUPIED`, `USER_NOT_FOUND`/`USER_ID_INVALID`.

Contacts screen:

- Header with "add contact"; search field filters **locally** on title, subtitle (`@username` or
  phone) and saved phone digits (only for phone-like queries, ≥ 3 digits).
- Alphabet sections (`A`…`Z`, `#` for symbols/non-Latin).
- Row: avatar, title, subtitle; long-press or accessibility custom action opens a menu with Edit,
  Share, Copy number (disabled when hidden), Copy username (only when the subtitle is a username),
  Delete.
- Dialogs: add, edit (with "changes sync to Telegram" hint), delete confirmation, share-to-chat
  (only chats with `canPost`).
- Tapping a row opens the private chat; a result from a previous account is dropped via
  `canUseContactResult(requestAccountId, activeAccountId)`.
- Loaded contacts live in a `ViewModel` keyed by account, so switching tabs keeps the list.

**Permissions:** the manifest declares **no** `READ_CONTACTS`. Contacts are the Telegram address
book only; nothing is read from the device.

### 2.6 Blocking, reporting, privacy

Blocking:

- Entry points: profile overflow (`ChatInfoMenu`) and the block-list screen.
- `setPrivateChatBlocked(chatId, blocked)`: `GetChat` → must be `ChatTypePrivate` → not self →
  `TdPrivacy.setBlocked(User, userId, blocked)` → `SetMessageSenderBlockList(MessageSenderUser(id),
  BlockListMain | null)`.
- `ChatInfoMenu` requires an explicit confirmation dialog before any destructive call, shows a busy
  state, and surfaces the error in a second dialog.
- Block list: `GetBlockedMessageSenders(BlockListMain, offset, limit)` with concurrent name/photo
  resolution, paging via `fetched`; rows can be users or chats.
- `isBlocked` on the profile drives the Block/Unblock label.

Reporting:

- Profile "Report spam" → `reportSpam(chatId)` → `ReportChat(chatId)`:
  - `ReportChatResultOptionRequired` → pick the canonical spam option only (`id`/`text` containing
    `spam`/`спам`); never substitute an unrelated reason, otherwise error.
  - `ReportChatResultTextRequired` → retry with empty text only when `isOptional`.
  - `ReportChatResultMessagesRequired` → localized "requires messages" error.
- Block-list "Report" for a sender → `reportSpamForUser(userId)` creates the private chat first
  (`CreatePrivateChat(userId, false)`), so a blocked user can be reported without opening the chat.
- Message reporting in chat: `reportMessages(chatId, [messageId], reason)` with
  `MessageReportReason.Spam | Violence | PersonalDetails`, mapped to TDLib options by keyword in the
  option id/text; `MessageReportDialog` collects the reason. Same two-step `ReportChat` protocol.

Privacy settings (adjacent but same data layer): 9 `PrivacySetting` values read/written with TDLib
privacy rules; un-round-trippable rules are marked `isEditable = false` and shown read-only.

### 2.7 Shared media

- `SharedMediaKind { Media, Files, Links, Voice, Music }`.
- Filters: `PhotoAndVideo`, `Document`, `Url`, `VoiceNote`, `Audio`.
- Page: `SearchChatMessages(chatId, null, "", null, fromMessageId, offset, 20, filter)`; `offset = 0`
  for older, `-(limit-1)` for newer. Results are merged by message id in `SharedMediaWindow`.
- Counts come from `GetChatMessageCount` (same five filters) and appear in the tab headers.
- UI: header, scrollable tab bar with counts, `HorizontalPager` (70 % of screen height, min 360 dp),
  media grid (3 columns) for Media, list rows for Files/Links/Voice/Music, infinite scroll,
  retry/footer states, empty states per kind, full-screen `SharedMediaViewer` for Media with
  export-confirmation, forward rights and protected-content rules.
- Forum messages open in their own topic (`topicOf`).

### 2.8 UI permission gating (server remains the authority)

| Action | Android gate |
| --- | --- |
| Block / Unblock | `kind ∈ {Person, Bot}` and `!isSelf` |
| Report spam | same as block |
| Delete chat | always offered from the profile menu |
| Members list | `supergroupFullInfo.canGetMembers` (basic groups always true) |
| Member preview | same flag, 10 rows |
| Statistics | `kind == Group && canViewStatistics` (`canGetStatistics`) |
| Group management | `groupAdmin.canManageAnything` |
| Invite link | shown only when TDLib returned one |
| Secret chat | person, not self, not bot, not deleted |
| Edit / change photo | own profile only |

---

## 3. Windows current status

### 3.1 What exists

**`UserProfile` (read-only value type).** Fields: `userId`, `chatId`, `displayName`, `username`,
`bio`, `phoneNumber`, `photoFileId`, `isPremium`, `isBot`, `isContact`, `isChannel`, `isGroup`,
`memberCount`, `hasPhoto`. Request builders: `getUserRequest`, `getUserFullInfoRequest`,
`getChatRequest`, `getSupergroupFullInfoRequest`. Parsers: `profileFromUser`, `profileFromChat`.
The struct is deliberately pointer-free and thread-safe (documented in the header).

**Repository `requestProfile(chatId)`.** `getChat` → private/secret: `getUser` + `getUserFullInfo`
(sets `chatId` back to the chat id) → `profileLoaded`; supergroup: `getSupergroupFullInfo` →
`profileLoaded(profileFromChat(chat, full))`; everything else (including **basic groups**) emits
`profileFromChat(chat)` with no full info.

**Controller state.** `m_profile`, `profileVisible`, `profileLoading`, and ten read-only properties
(`profileName`, `profileUsername`, `profileBio`, `profilePhone`, `profilePhotoFileId`,
`profileIsGroup`, `profileIsChannel`, `profileMemberCount`). `requestProfile(chatId)` sets visible +
loading and forwards to the repository; `closeProfile()` clears visible; `openChat()` force-closes
the drawer; `profileLoaded` sets `m_profile`, clears loading and **sets visible = true**.

**Profile drawer (QML).** A 320 px right-hand `Rectangle` inside `ChatsPage`, opened from a
"Profile" button in the chat header. Shows: avatar (from `currentChatPhotoFileId`/`Ready`/`Revision`,
i.e. the **chat-list** avatar, not `profilePhotoFileId`), name (`profileName` falling back to
`currentChatTitle`), `@username`, `"Group/Channel · N members"`, a divider, bio, phone, and a
`BusyIndicator` bound to `profileLoading`. Close button only.

**`ProfileEdit` request builders.** `setName`, `setBio` (whitespace folded to one line),
`setUsername` (whitespace removed, `@` stripped), `deleteProfilePhoto`. No `setProfilePhoto`
(documented as intentionally out of scope for this layer).

**`ProfileEditDialog.qml`.** A complete modal editor (first/last/username/bio, Save/Cancel, error
area, `saveRequested` signal, `saving` state) — **instantiated nowhere**.

**`ContactSearch`.** Pure `searchContactsRequest(query, limit)` (clamped 1–100) and
`contactsFromResponse()` (expects an array under `"users"`; skips malformed rows; strips `@`;
prefers `usernames.active_username` → `active_usernames` → `username`). **Not referenced by the
repository, controller or QML.**

**Global search.** Messages only (`GlobalSearchModel`, `searchMessages`); no people/contacts section.

### 3.2 What does not exist

- Block, unblock, report (chat or message), block list, privacy settings — no occurrence of these
  TDLib methods anywhere in `src/` or `qml/`.
- Contact list (`GetContacts`), add/edit/delete contact, share contact card, contact alphabet
  grouping, contact search UI.
- Shared media tabs, counts, media grid, `SearchChatMessages` with an attachment filter,
  `GetChatMessageCount`.
- Member lists, administrators, member preview, invite link, group management, statistics.
- Presence / last seen, verified badge, birthday, common groups, business address, personal
  (linked) channel, own-account profile page, profile photo gallery, secret chat entry.
- Profile photo upload/delete wiring (builders exist for delete only).

### 3.3 Behavioural defects in the existing slice

1. **Basic groups never load full info.** `requestProfile` has no `ChatTypeBasicGroup` branch, so
   `description` and `memberCount` are always empty for basic groups (Android calls
   `GetBasicGroupFullInfo`).
2. **Supergroups skip `getSupergroup`.** Only `getSupergroupFullInfo` is fetched, so usernames,
   verification and the `memberCount` fallback available on the `supergroup` object are lost.
3. **Profile errors are invisible.** A failed profile request leaves `profileLoading == true`
   forever (no `profileLoaded`), so the drawer spins indefinitely; the message goes to
   `m_chatError`, which is not rendered in the drawer and has no Retry.
4. **Late responses reopen a closed drawer.** `profileLoaded` unconditionally sets
   `m_profileVisible = true`, so closing the drawer during a slow request makes it reappear.
   There is also no request-generation guard: a response for chat A can be applied after chat B was
   requested.
5. **Avatar mismatch.** The drawer shows the chat-list avatar, not the profile photo
   (`profilePhotoFileId` is exposed but unused in QML).
6. **No live profile updates.** `updateUser` only refreshes sender names; `updateUserStatus`,
   `updateUserFullInfo`, `updateSupergroupFullInfo`, `updateChatPhoto` are not handled, so an open
   profile goes stale (presence, photo, member count, bio).
7. **`ProfileEditDialog` is dead UI**, and `ProfileEdit` builders are only exercised by unit tests —
   there is no repository method, no validation, no bio limit, no partial-failure refresh.
8. **`ContactSearch` is dead code** outside tests, and its parser contract (array of user objects)
   does not match TDLib's `searchContacts` result (`user_ids`), which the header acknowledges but
   nothing implements.
9. **Navigation label mismatch.** `Shell.qml` shows a "Profile" nav item (`currentSection == 1`)
   that opens `OverviewPage` ("Account overview"), not a profile page. The real profile is only the
   in-chat drawer.

---

## 4. Gap matrix

Priority: **P0** = blocks usable profile parity / correctness bug; **P1** = core Android feature
missing from the daily flow; **P2** = important but secondary; **P3** = advanced/large surface.

| ID | Area | Gap | Android reference | Windows today | Prio |
| --- | --- | --- | --- | --- | --- |
| P-01 | Profile data | No presence, verified, usernames[], birthday, common groups, business address, personal channel, self flag, block flag | `ProfileInfo`, `TdProfiles.userInfo/chatProfile` | `UserProfile` fields | P0 |
| P-02 | Profile data | Basic groups: no `GetBasicGroup`/`GetBasicGroupFullInfo` | `refreshChat` | missing branch | P0 |
| P-03 | Profile data | Supergroups: no `GetSupergroup` (usernames/verified/memberCount fallback) | `refreshChat` | missing call | P0 |
| P-04 | Profile UI | No error state, no Retry; spinner can hang forever | `ProfileLoadError` | drawer only has `BusyIndicator` | P0 |
| P-05 | Profile UI | Stale/late response reopens drawer; no request generation | route-scoped state | `AppController::profileLoaded` | P0 |
| P-06 | Profile UI | No live updates while open | `TdProfiles.handle` | only `updateUser` sender names | P1 |
| P-07 | Profile UI | No actions panel (block/report/delete), no rows card, no badges, no copy actions | `ProfileScreen` + `ChatInfoMenu` | read-only drawer | P1 |
| P-08 | Profile UI | Avatar uses chat-list photo, not profile photo | `ProfileScreen` | `currentChatPhotoFileId` | P1 |
| P-09 | Own profile | No own-profile page; nav "Profile" opens Overview | `AppRoute.MyProfile` | `OverviewPage` | P1 |
| P-10 | Edit | Edit dialog not wired; no validation/bio limit/partial refresh | `EditProfileScreen`, `TdProfiles.edit` | `ProfileEdit` builders + dead QML | P1 |
| P-11 | Edit | No profile photo set/delete (only `deleteProfilePhoto` builder) | `setPhoto`, `deleteMyPhoto`, gallery | none | P2 |
| P-12 | Contacts | No contact list, alphabet grouping, local search | `ContactsScreen`, `TdContacts.load` | none | P1 |
| P-13 | Contacts | No add/edit/delete/share contact | `TdContacts`, `ContactRequests` | none | P1 |
| P-14 | Contacts | `ContactSearch` unwired; parser shape ≠ `searchContacts` result | `TdContacts.search` | dead code + tests | P1 |
| P-15 | Contacts | No people results in global search | unified search people source | messages only | P2 |
| P-16 | Block | No block/unblock, no block state on profile | `setPrivateChatBlocked`, `isBlocked` | none | P1 |
| P-17 | Block | No block-list screen, no privacy settings | `TdPrivacy`, block-list page | none | P2 |
| P-18 | Report | No chat spam report, no message report with reasons | `reportSpam`, `reportMessages` | none | P1 |
| P-19 | Shared media | No tabs/counts/grid/viewer | `SharedMediaSection` | none | P2 |
| P-20 | Members | No member list/preview/admins/invite link | `ChatMembersScreen`, `GroupMembersScreen` | none | P2 |
| P-21 | Group mgmt | No admin actions, join requests, invites, statistics | `GroupManagement*`, `ChannelStatistics*` | none | P3 |
| P-22 | Monetization | No Premium/Stars/Gifts entries | `PremiumScreen`, `StarsScreen`, `GiftsScreen` | none | P3 |
| P-23 | Secret chats | No secret-chat start from profile | `SecretChatEligibility` | none | P3 |
| P-24 | Tests | No repository/UI-level profile or contacts tests | VM + Compose tests | 3 pure-function suites | P1 (with each stage) |

---

## 5. Implementation plan

Conventions observed in the codebase: pure request/parse helpers in `src/telegram/*.{h,cpp}` (no
session, no logging, no `@extra`), repository orchestration in `TelegramRepository`, QML-facing state
in `AppController`, view-only QML in `qml/`, one `beygram_add_test(...)` per file in
`tests/CMakeLists.txt`. Tests must be new-style QtTest slots with JSON fixtures, mirroring
`tests/tst_user_profile.cpp`.

### Stage 1 — Complete the profile value type and load path (P0)

**Files:** `src/telegram/UserProfile.h`, `src/telegram/UserProfile.cpp`,
`src/telegram/TelegramRepository.cpp`, `src/telegram/TelegramRepository.h`,
`tests/tst_user_profile.cpp`.

1. Extend `UserProfile`:
   - `enum class Presence { Unknown, Bot, DeletedAccount, Online, Recently, LastWeek, LastMonth, LongAgo, LastSeen }`
     plus `qint64 lastSeenUnix` (TDLib `user.status`; `was_online` seconds → keep seconds, format in QML).
   - `QStringList usernames`, `QString editableUsername` (keep `username` as the editable one),
     `bool isVerified`, `bool isSelf`, `bool isBlocked`, `bool isDeleted`.
   - `int onlineMemberCount`, `int administratorCount`, `QString inviteLink`,
     `bool canListMembers`, `bool canViewStatistics`.
   - `QString personalChatTitle`, `qint64 personalChatId`, `int commonGroupCount`,
     `QString birthdayText` (or structured day/month/year), `QString address`.
   - Keep `isContact` (already parsed) and `hasPhoto`.
2. Add request builders: `getBasicGroupRequest`, `getBasicGroupFullInfoRequest`,
   `getSupergroupRequest`, `getChatAdministratorsRequest` (needed in Stage 6),
   `getChatMessageCountRequest(chatId, filter)` (Stage 7), `getUserProfilePhotosRequest`.
3. Parsers:
   - `profileFromUser`: read `usernames.active_usernames` (fallback to `active_username`/`username`),
     `verification_status.is_verified`, `type.userTypeDeleted`, `status` → `Presence`,
     `userFullInfo.block_list` → `isBlocked` (`blockListMain` only; `blockListMain` + `blockList`
     variants must be handled defensively), `birthdate`, `business_info.location.address`,
     `group_in_common_count` (only when not self), `personal_chat_id`.
   - `profileFromChat`: basic group description/member count from `basicGroupFullInfo`
     (`description`, `members.size()`), supergroup `usernames`, `verification_status`,
     `member_count` fallback from `supergroup`, `invite_link.invite_link`,
     `can_get_members`, `can_get_statistics`, `administrator_count`, `online_member_count` where
     present; private chats inherit the user profile.
4. Repository `requestProfile`: add the `chatTypeBasicGroup` branch
   (`getBasicGroup` + `getBasicGroupFullInfo`), add `getSupergroup` before
   `getSupergroupFullInfo`, keep a per-request generation counter (`m_profileRequestId`) and ignore
   stale callbacks. Emit a new `profileFailed(chatId, message)` signal on timeout/error so the
   controller can clear loading.
5. Handle `updateUserStatus`, `updateUserFullInfo`, `updateSupergroupFullInfo`,
   `updateBasicGroupFullInfo`, `updateChatPhoto` by re-emitting the current profile when the update
   targets the open chat/user (Stage 3 depends on this; it can land here).

**Tests (`tst_user_profile.cpp` additions):**
`parsesPresence`, `parsesVerifiedAndUsernames`, `parsesBlockedUser`, `parsesDeletedAccount`,
`parsesBasicGroupFullInfo`, `parsesSupergroupUsernamesAndInviteLink`,
`parsesPersonalChatAndCommonGroups`, `buildsBasicGroupRequests`, `buildsSupergroupRequest`,
`ignoresMalformedFullInfo`.

**Risk:** TDLib 1.8.69 field shapes are pinned in `docs/WINDOWS_TDLIB.md`; confirm
`verification_status`, `block_list`, `birthdate`, `business_info`, `personal_chat_id` exist in the
pinned schema before writing parsers, and keep every read optional so older payloads/fixtures still
parse.

### Stage 2 — Real profile panel with states (P0)

**Files:** `src/ui/AppController.h`, `src/ui/AppController.cpp`, `qml/pages/ChatsPage.qml`
(or a new `qml/components/ProfilePanel.qml`), `qml/components/Avatar.qml` (reuse).

1. Controller: add `profileError`, `profileIsSelf`, `profileIsVerified`, `profileIsBlocked`,
   `profilePresence`, `profileLastSeen`, `profileUsernames`, `profileInviteLink`,
   `profileAdministratorCount`, `profileOnlineMemberCount`, `profilePersonalChatId/Title`,
   `profilePhotoReady/Revision`; `retryProfile()` invokable.
2. Guard the state machine: `requestProfile` stores the requested chat id; `profileLoaded` only
   applies when `profile.chatId == m_profileChatId`, and **does not** set visible when the user has
   closed the panel. `profileFailed` clears loading and sets `profileError`. `closeProfile` clears
   error and increments a generation so a late reply is dropped.
3. Split the drawer into `qml/components/ProfilePanel.qml` (props in, `retryRequested` /
   `closeRequested` / `blockRequested` / `reportRequested` / `deleteRequested` out) so it can be
   reused by the own-profile page and covered by a QML smoke test. Show:
   - avatar from `profilePhotoFileId`/`profilePhotoReady` with initials fallback;
   - title, `@username` (first editable username), presence/verified/premium badges;
   - type + member/subscriber count + administrators/online where available;
   - `ProfileRowsCard`-equivalent rows (phone, usernames, bio/description, invite link, birthday,
     address, common groups, personal channel);
   - error banner + Retry;
   - action buttons row (Stage 4/5 gated).
4. Keep the header "Profile" button and add `Esc` to close.

**Tests:** `tests/tst_profile_state.cpp` (new) covering: late response for another chat is ignored;
`profileFailed` clears loading and sets error; closing then receiving a response keeps the panel
closed; self profile has no block/report actions. These can be pure controller-state tests only if
the controller logic is factored into a small helper; otherwise document them as manual QML checks
and add the repository generation test in `tst_telegram_requests.cpp` style.

### Stage 3 — Own profile page and edit flow (P1)

**Files:** `src/telegram/ProfileEdit.h/.cpp`, `src/telegram/TelegramRepository.cpp`,
`src/ui/AppController.cpp/.h`, `qml/components/ProfileEditDialog.qml`, `qml/pages/ProfilePage.qml`
(new), `qml/Shell.qml`, `tests/tst_profile_edit.cpp`.

1. Add `ProfileEditValidation` (pure, testable): trim names; first name required; ≤ 64 chars;
   username empty/unchanged/regex `^[A-Za-z][A-Za-z0-9_]{4,31}$`; bio code-point count ≤ limit;
   returns a typed error enum so QML can show a localized message.
2. Add `bioLengthMax` to the controller, sourced from `GetOption("bio_length_max")` once the
   session is ready (default 70, clamp 1–4096 like Android).
3. Repository `updateOwnProfile(firstName, lastName, username, bio)`:
   - compare against the cached own profile and send only changed methods, in Android's order
     (`setUsername` → `setName` → `setBio`);
   - `finally` re-request `getMe` + `getUserFullInfo` so partial success is reflected;
   - map TDLib errors (`USERNAME_OCCUPIED`, `USERNAME_INVALID`, `FIRSTNAME_INVALID`,
     `FLOOD_WAIT`) to readable text.
4. Own-profile page: reuse `ProfilePanel` in "self" mode (Edit / Change photo buttons, no
   block/report), replacing the misleading `Shell.qml` "Profile" → `OverviewPage` mapping; move the
   account/session diagnostics from `OverviewPage` into a "Session" card or keep `OverviewPage`
   reachable from Settings.
5. Wire `ProfileEditDialog.qml` from the own-profile page: pass current values, `saving`,
   `errorText`; on `saveRequested` call the invokable; keep the dialog open on failure. Add a
   discard-changes confirmation (Android parity) and a live bio counter.
6. Photo: add `setOwnProfilePhoto(path)` (`setProfilePhoto` + `InputChatPhotoStatic` +
   `InputFileLocal`) and `deleteOwnProfilePhoto(photoId)`; expose via file dialog. Defer the
   gallery (Stage 9).

**Tests (`tst_profile_edit.cpp` additions):** `validationRejectsEmptyFirstName`,
`validationRejectsLongNames`, `validationAcceptsEmptyUsername`, `validationRejectsBadUsername`,
`countsBioInCodePoints`, `sendsOnlyChangedFields` (needs the diff helper extracted as a pure
function), `mapsUsernameErrors`.

### Stage 4 — Contacts list, search, add/edit/delete/share (P1)

**Files:** `src/telegram/ContactSearch.h/.cpp` (extend), `src/telegram/TelegramRepository.cpp/.h`,
`src/ui/AppController.cpp/.h`, `qml/pages/ContactsPage.qml` (new), `qml/Shell.qml`,
`tests/tst_contact_search.cpp`.

1. Fix the parser contract: `searchContacts` returns `user_ids`, so add
   `contactUserIdsFromResponse()` (ints) and keep `contactsFromResponse()` for resolved payloads;
   document both. Add `getContactsRequest()` and `importContactsRequest(...)`,
   `addContactRequest(userId, contact, sharePhoneNumber=false)`, `removeContactsRequest(userId)`,
   `contactVCard(...)` (RFC 6350 escaping as in Android's `ContactCard`).
2. Repository: `loadContacts()` (`getContacts` → batched `getUser`, skip failures, dedupe, sort),
   `searchContacts(query)` (`searchContacts` → resolve ids, keep relevance order),
   `addContact` / `updateContact` / `deleteContact` / `shareContact(chatId, userId)`.
3. Controller: expose `QAbstractListModel* contacts`, `contactsLoading`, `contactsError`,
   invokables for the five operations; keep a `QHash<qint64, ContactInfo>` cache and clear it on
   logout/account switch.
4. QML `ContactsPage`: search field with **local** filtering (title, `@username`, phone digits when
   the query is phone-like and ≥ 3 digits), alphabet sections, row with avatar/name/subtitle,
   context menu (Edit / Share / Copy number / Copy username / Delete), add/edit dialogs with
   validation, delete confirmation, share-to-chat picker limited to writable chats.
5. Add a "Contacts" nav entry (and keep the existing "Profile" entry for the own profile), plus
   `Ctrl+K`-style entry from global search later (Stage 8).

**Tests:** `parsesContactUserIds`, `buildsGetContactsRequest`, `buildsImportContactsRequest`,
`addContactNeverSharesOwnNumber`, `buildsRemoveContactsRequest`, `escapesVCardText`,
`validatesPhoneDigits`, `mapsContactErrors`, `contactMatchesQueryIgnoresSeparators`,
`contactAlphabetGroupHandlesNonLatin`.

### Stage 5 — Block, unblock, report (P1)

**Files:** `src/telegram/UserActions.h/.cpp` (new: pure builders + option pickers),
`src/telegram/TelegramRepository.cpp/.h`, `src/ui/AppController.cpp/.h`,
`qml/components/ProfilePanel.qml`, `qml/components/MessageBubble.qml` or the message context menu,
`tests/tst_user_actions.cpp` (new).

1. Builders: `setMessageSenderBlockListRequest(userId, blocked)` (`SetMessageSenderBlockList`,
   `BlockListMain` / null), `reportChatRequest(chatId, optionId, messageIds, text)`,
   `chooseSpamReportOption(options)`, `chooseMessageReportOption(options, reason)` — copy Android's
   keyword matching (`spam`/`спам`, `violence`/`насили`, `personal`/`личн`).
2. Repository:
   - `setUserBlocked(chatId, blocked)`: `getChat` → `chatTypePrivate` → not self →
     `setMessageSenderBlockList(userId, blocked)`; reject group/channel and self with a typed error.
   - `reportChatSpam(chatId)`: two-step `ReportChat` exactly as Android (`OptionRequired` → canonical
     spam option only; `TextRequired` → empty text only when optional; `MessagesRequired` → error).
   - `reportMessages(chatId, ids, reason)`: reason picker + same protocol.
   - `deleteChatForSelf(chatId)` for the profile "Delete chat" action: private/secret or
     `can_be_deleted_only_for_self` → `deleteChatHistory(chatId, true, false)`; otherwise
     `leaveChat` (never `revoke = true`).
3. Controller: `blockCurrentProfile()`, `unblockCurrentProfile()`, `reportCurrentProfile()`,
   `deleteCurrentChat()`; each returns a typed result the QML shows.
4. QML: profile overflow menu (Block/Unblock, Report spam, Delete chat) with a confirmation dialog
   before every destructive call, busy state, and an error dialog — mirror `ChatInfoMenu.kt`
   semantics (danger colour on Report/Delete, no RPC before confirmation).
5. Message report: add "Report message" to the message context menu with a reason dialog
   (Spam / Violence / Personal details).

**Tests:** `buildsBlockRequest`, `unblockClearsBlockList`, `picksOnlySpamOption`,
`failsWhenSpamOptionMissing`, `mapsReasonKeywords`, `reportRequiresMessages`,
`deleteChatUsesDeleteHistoryForPrivate`, `deleteChatLeavesGroups`,
`refusesBlockingOwnAccount`, `refusesBlockingGroupChat`.

### Stage 6 — Members, administrators, invite link (P2)

**Files:** `src/telegram/GroupMembers.h/.cpp` (new), `TelegramRepository.*`, `AppController.*`,
`qml/pages/GroupMembersPage.qml` (new), `tests/tst_group_members.cpp` (new).

- `getSupergroupMembersRequest(supergroupId, offset, limit)` (Recent filter) and
  `getBasicGroupFullInfoRequest` for basic groups; `getChatAdministratorsRequest`.
- Resolve users in parallel (repository already has a user cache; extend it to return full
  `UserProfile`s), map role Owner/Admin/Member, custom titles via `getChatAdministrators`.
- Gate on `canListMembers`; hide the section otherwise. Preview = first 10 in the profile panel.
- Tests: `mapsRoles`, `pagesBasicGroupLocally`, `skipsUnresolvableUsers`, `capsAdministrators`.

### Stage 7 — Shared media tabs and counts (P2)

**Files:** `src/telegram/SharedMedia.h/.cpp` (new), `TelegramRepository.*`, `AppController.*`,
`qml/components/SharedMediaTabs.qml` (new), `tests/tst_shared_media.cpp` (new).

- `searchChatMessagesRequest(chatId, fromMessageId, offset, limit, filter)` with
  `offset = 0` / `-(limit-1)`; `getChatMessageCountRequest(chatId, filter)`.
- Five filters: PhotoAndVideo, Document, Url, VoiceNote, Audio.
- Per-kind paging state with merge-by-message-id; expose as five models or one model with a `kind`
  role. Counts load independently and never fail the profile.
- QML: tab bar with counts, 3-column grid for media (reuse `MediaImageProvider`/`Avatar` patterns),
  list rows for files/links/voice, "load more" on scroll, empty/error states, click opens the
  existing `MediaViewerOverlay` with the chat's media set.
- Tests: `buildsSearchRequestOlder`, `buildsSearchRequestNewerUsesNegativeOffset`,
  `mapsFilters`, `mergesPagesById`, `countFailureIsNull`.

### Stage 8 — People in global search (P2)

**Files:** `src/telegram/GlobalSearchModel.*` or a new `PeopleSearchModel`, `TelegramRepository.*`,
`qml/pages/GlobalSearchPage.qml`.

- Add a "People" section backed by `searchContacts` (contacts) and, if desired later, `searchChats`
  for public chats. Merge pages by id using a larger limit (TDLib has no offset for people search),
  exactly as Android's `GlobalSearchPage<T>`.
- QML: sectioned results (Contacts / Messages), row opens the private chat.

### Stage 9 — Advanced profile surfaces (P3)

Profile photo gallery (`getUserProfilePhotos`, delete, set-previous), linked/personal channel row,
Premium/Stars/Gifts entries (external URLs only), secret-chat entry, group management (rights,
join requests, invites), channel statistics, birthday/common-group presentation. Each is a separate
slice; none should block Stages 1–7.

---

## 6. Cross-cutting rules to preserve

1. **Never invent data.** Android's parsers only copy what TDLib returned (no derived phone numbers,
   usernames or photos). `UserProfile.h` states the same; keep it.
2. **Server is the authority for permissions.** Gate UI on TDLib flags (`can_get_members`,
   `can_get_statistics`, admin rights, `block_list`) and still surface TDLib errors.
3. **No destructive RPC before confirmation.** Block, unblock, report and delete must each show a
   confirmation dialog; block/report are only offered for person/bot chats that are not self.
4. **Delete chat must never revoke for others.** `deleteChatHistory(chatId, true, /*revoke*/ false)`
   for private chats; `leaveChat` for groups/channels.
5. **Per-account isolation.** Contact/profile caches and in-flight requests must be keyed by the
   active account and dropped on account switch (Android does this with `accountId` keys and
   `canUseContactResult`).
6. **No `@extra` in builders.** The transport adds correlation ids; `tst_profile_edit.cpp` asserts
   this today — keep the assertion for every new builder.
7. **Phone numbers stay as TDLib returned them.** Do not format or normalize for display; only
   normalize for `ImportContacts`.

---

## 7. Suggested test plan (not executed)

| Layer | Tests |
| --- | --- |
| Pure builders/parsers | `tst_user_profile`, `tst_contact_search`, `tst_profile_edit`, `tst_user_actions`, `tst_group_members`, `tst_shared_media` — JSON fixtures only, no session |
| Repository behaviour | Extend `tst_telegram_requests` style suites: stale profile response ignored, basic-group branch issues two requests, contact failure isolation, report two-step retry |
| QML | `tests/CMakeLists.txt` QML smoke test for `ProfilePanel` (loading / error+retry / self / group) and `ContactsPage` (empty / grouped / no-results) using the existing QML test harness if present; otherwise document as manual checks |
| Manual acceptance | Own profile edit round trip (name/bio/username incl. occupied username error); block → verify chat disappears from senders/unblock; report spam on a bot; contacts add/edit/delete/share; shared media tabs page in both directions; account switch clears caches |

Acceptance criteria for "profile parity" (Stages 1–5):

- Opening a private, basic-group and supergroup/channel profile shows the same information set as
  Android (presence or member counts, verified, usernames, bio/description, invite link where
  available, block state).
- A failed load shows an error with Retry and does not leave a spinner.
- Own profile can be edited with Android's validation rules and partial-failure refresh.
- Contacts can be listed, filtered locally, added, edited, deleted and shared.
- Block/unblock and report (chat spam + message reason) work with confirmation and readable errors.
- No build or test claim is made here; all of the above requires the normal CMake/CTest run.

---

## 8. Priority summary

| Priority | Items |
| --- | --- |
| **P0** | P-01…P-05 — complete `UserProfile` + correct load path (basic group, supergroup), profile panel error/retry, stale-response guard |
| **P1** | P-06…P-10, P-12…P-14, P-16, P-18, P-24 — live updates, actions panel, own-profile page, edit wiring, contacts, block, report, tests alongside each |
| **P2** | P-11, P-15, P-17, P-19, P-20 — photo gallery/upload, people search, block list + privacy, shared media, members/admins |
| **P3** | P-21…P-23 — group management, statistics, monetization entries, secret chats |

Stages 1–2 are small and fix correctness bugs in code that already ships; they should come first.
Stages 3–5 deliver the features users will notice missing next (edit own profile, contacts, block and
report). Stages 6–7 are the largest UI additions and depend on Stages 1–2 for accurate group data.
