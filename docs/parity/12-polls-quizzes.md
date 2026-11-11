# 12 - Polls and quizzes: Android to Windows parity audit

Status: read-only audit. No build, no test run, no GUI session, no git operation and
no code change was performed for this document. Every statement below comes from
reading the sources listed in §1 and §3.

Reference client: the Android sources kept in `app/` (Jetpack Compose, TDLib Java
bindings). Target client: the Qt/QML Windows client in `src/`, `qml/`, `tests/`.

Scope of this slice: poll and quiz messages as they arrive from TDLib; regular,
multiple-answer and anonymous polls; quizzes (correct answer, explanation, single
attempt); voting and changing a vote; closed polls and server vote restrictions;
results visibility and percentages; the creation flow (composer, validation,
`InputMessagePoll`); the voters list; adding an option; stopping a poll the user
owns. Out of scope: scheduled/silent send plumbing (audited separately), topics,
reactions, and any Telegram behaviour TDLib does not expose.

Companion documents: [PORTING_AUDIT.md](../PORTING_AUDIT.md),
[PORTING_ROADMAP.md](../PORTING_ROADMAP.md) (polls are Phase 5, item 6),
[WINDOWS_TDLIB.md](../WINDOWS_TDLIB.md) (pinned TDLib revision).

---

## 1. Android files reviewed

### Data / TDLib layer

| File | What it implements |
|---|---|
| `app/src/main/java/com/beykus/beygram/domain/model/MessageSpecial.kt` | Poll domain model: `MessagePoll`, `MessagePollOption`, `PollVoteRestriction`, `PollVoterInfo`, `PollVotersPage`, `OutgoingPoll`, `PollDraftProblem`, `PollOptionAddProblem`, `pollVotersNextOffset`, `appendPollVoters`. All poll rules live here. |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdMediaMapper.kt` | `TdApi.MessagePoll` to `MessageSpecialContent.Poll` (`toSpecialPoll`): quiz type, `optionOrder` shuffle, correct answers, explanation, restriction reason, `canAddOption`. |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdConversationActions.kt` | `sendPoll`, `votePoll`, `stopPoll`, `addPollOption`, `refreshMappedMessage`, `inputMessagePoll`, `PollDraftProblem.message()`, `PollOptionAddProblem.message()`. |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdRequests.kt` | `pollResult` and `pollErrorText`: the poll-specific TDLib error mapping. |
| `app/src/main/java/com/beykus/beygram/data/tdlib/TdLibTelegramRepository.kt` | Repository entry points `sendPoll`, `votePoll`, `stopPoll`, `pollVoters`, `addPollOption`; `TdApi.PollVoter` to `PollVoterInfo`. |
| `app/src/main/java/com/beykus/beygram/domain/repository/ConversationRepository.kt` | UI-facing contract for the five poll operations. |
| `app/src/main/java/com/beykus/beygram/data/unavailable/UnavailableTelegramRepository.kt` | Confirms the contract is satisfied (as `unavailable()`) without a session. |

### UI layer

| File | What it implements |
|---|---|
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/SpecialMessageViews.kt` | `PollCard`, `PollOptionRow`, `PollFooter`, `PollVotersDialog`, `AddPollOptionDialog`, `pollOptionProblemText`. The complete rendered behaviour. |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ComposerExtras.kt` | `PollComposerDialog`: question/options/quiz/anonymous/multiple/correct answer/explanation, code-point counter, validation surface. |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ComposerExtraPickers.kt` | Attachment entry point: `attachments.pickPoll = { pollComposerOpen = true }`, then `prepareExtra(MessageExtraDraft.Poll(poll))`. |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/ChatScreen.kt` | Wiring of `onPollVote`, `onPollStop`, `onLoadPollVoters`, `onAddPollOption`; draft send through `conversation.sendPoll(...)` with reply/forum/send options. |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/MessageThreadScreen.kt` | Same poll callbacks in the thread/channel view. |
| `app/src/main/java/com/beykus/beygram/ui/screens/chat/MessageContextDialog.kt` | Poll voting from the message context sheet. |

### Android tests reviewed (as behaviour documentation)

| File | What it pins down |
|---|---|
| `app/src/test/java/com/beykus/beygram/domain/model/MessageSpecialTest.kt` | Revoting, quiz correctness, results visibility, voters visibility, voters paging/de-duplication, draft validation, add-option validation. |
| `app/src/test/java/com/beykus/beygram/data/tdlib/PollSendTest.kt` | `inputMessagePoll` shape for regular polls and quizzes; nine poll-specific error strings. |
| `app/src/test/java/com/beykus/beygram/data/tdlib/SpecialContentMappingTest.kt` | Quiz mapping, server percentage, restriction, shuffled `optionOrder` preserving original ids. |
| `app/src/test/java/com/beykus/beygram/ui/screens/chat/AttachmentComposerTest.kt` | The attachment list hides the poll action when `allowsPolls` is false. |

---

## 2. How Android behaves (the specification to port)

### 2.1 Wire data and mapping

`TdMediaMapper.special()` maps `TdApi.MessagePoll` through
`toSpecialPoll(canAddOption)` and produces:

- `id`, `question`, `description` (from `MessagePoll.description`);
- `options`: `id` (TDLib string id), `text`, `voterCount`,
  **`percentage` (server `vote_percentage`)**, `isChosen`, `isBeingChosen`,
  and **`optionIndex`** (position in TDLib's original array);
- `totalVoterCount`, `isAnonymous`, `allowsMultipleAnswers`,
  **`allowsRevoting`**, `isClosed`, **`canSeeResults`**, **`canGetVoters`**,
  **`canAddOption`**;
- `isQuiz`, **`correctOptionIndices`**, **`explanation`**;
- **`voteRestriction`** from `poll.vote_restriction_reason` (`Closed`,
  `CountryRestricted`, `MembershipRequired`, `Scheduled`, `YetUnsent`, `Other`,
  `None`).

Display order: TDLib may shuffle options for anti-bias (`poll.option_order`).
Android applies the order **only when it is a valid permutation** (same size, no
duplicates, every index in range); otherwise it falls back to wire order.
`optionIndex` always keeps the original id, because votes and quiz answers are
expressed in wire positions.

### 2.2 Poll kinds

| Kind | Detection | Behaviour |
|---|---|---|
| Regular | `poll.type is PollTypeRegular` | Vote once; change only when `allows_revoting` |
| Multiple-answer | `allows_multiple_answers` | Multi-select, explicit Vote / Change vote button, selections submitted together |
| Anonymous | `is_anonymous` | Short "Anonymous" label; voter list suppressed |
| Non-anonymous | `!is_anonymous` | Voter list available only if `can_get_voters` |
| Quiz | `poll.type is PollTypeQuiz` | "Quiz" header, anonymity forced, single attempt, correct answer and explanation |
| Closed | `is_closed` | Footer "Closed", no interaction, results shown if permitted |
| Restricted | `vote_restriction_reason != None` | Concrete reason line; `canVote` false |

### 2.3 Voting and multiple answers

`MessagePoll.canVote` is the single authority:

```
!isClosed
&& voteRestriction == None
&& !(isQuiz && hasVoted)          // a quiz is answered once
&& !(hasVoted && !allowsRevoting) // a regular poll needs the server flag
```

`hasVoted` is derived from `options.any { it.isChosen }`;
`chosenOptionIndices` maps chosen options back to `optionIndex`.

UI behaviour (`PollCard` / `PollOptionRow`):

- Single-choice: tapping an option immediately calls
  `onVote(messageId, listOf(optionIndex))`.
- Multiple-choice: tapping toggles a local `selected` set; a **Vote** (or
  **Change vote** when already voted) button is enabled only when the selection
  is non-empty and differs from the server choice; it submits
  `selected.sorted()`.
- `poll.options.any { it.isBeingChosen }` puts the card in a pending state
  (spinner on the affected row, no further interaction).
- `votePoll` de-duplicates, sorts and rejects an empty list before sending
  `SetPollAnswer(chatId, messageId, option_ids)`, then refreshes the message
  from TDLib (`GetMessage`) instead of guessing the new counts.

### 2.4 Results and percentages

`MessagePoll.percentageOf(index)`:

1. use the server `vote_percentage` when it is in `1..100`;
2. otherwise compute `voterCount * 100 / counted`, where `counted` is the
   **sum of option vote counts** when positive, else `totalVoterCount`;
3. clamp to `0..100`; never divide by zero.

The sum-of-options denominator matters: in a multiple-answer poll the sum of
option votes exceeds the number of voters, so dividing by `totalVoterCount`
would produce percentages that do not add up and can exceed 100 %.

Result visibility is server-controlled: `revealsResults = canSeeResults`. When
results are hidden, counts are not rendered and the voter list is suppressed
(`canShowVoters = canGetVoters && !isAnonymous && revealsResults`). The footer
shows `pluralStringResource(chat_poll_votes, totalVoterCount)` only when
`showResults && totalVoterCount > 0`.

### 2.5 Closed polls, revoting, restrictions

- Closed: `is_closed` makes `canVote` false and shows "Closed"; results still
  render when `canSeeResults`.
- **Stop poll** is shown only for `message.isOutgoing && !isClosed`, asks for
  confirmation (`chat_poll_stop_title` / `_text` / `_confirm`), sends
  `StopPoll(chatId, messageId, reply_markup = null)`, then refreshes.
- Restriction reasons are rendered verbatim in a warning tone
  (`chat_poll_restriction_closed`, `_country`, `_membership`, `_other`,
  `_scheduled`, `_unsent`).

### 2.6 Quizzes

- `isQuiz` drives the "Quiz" header and the composer's quiz mode.
- A correct option is marked (`chat_poll_correct`, green check) only when
  `isCorrect(index) && (isClosed || hasVoted)` - never before the user answered
  or the quiz closed.
- The explanation appears after the results section when
  `isQuiz && showResults && explanation.isNotBlank()`.
- A quiz is answered once: `canVote` excludes `isQuiz && hasVoted`.
- On send, quizzes force `is_anonymous = true` and `allows_revoting = false`
  regardless of the composer toggles.

### 2.7 Create flow

1. Attachment sheet to "Poll" (`attachments.pickPoll`) opens
   `PollComposerDialog`.
2. Dialog state: question, option list (starts with two empty options), `quiz`,
   `anonymous`, `multiple`, `correct` set, `explanation`.
   - question counter uses code points, capped at
     `OutgoingPoll.MAX_POLL_QUESTION_LENGTH` (255);
   - each option capped at `MAX_POLL_OPTION_LENGTH` (100);
   - 2..10 options (`MIN_POLL_OPTIONS` / `MAX_POLL_OPTIONS`);
   - quiz mode forces anonymity, disables the anonymous checkbox and shows a
     correct-answer toggle per option;
   - removing an option re-indexes the correct-answer set;
   - explanation is offered only in quiz mode.
3. `OutgoingPoll.problem()` validates and returns a `PollDraftProblem`
   (empty/too-long question, too few/many options, empty/too-long option,
   missing/invalid/too-many correct answers). The repository calls the same
   function, so UI and transport cannot disagree.
4. `prepareExtra(MessageExtraDraft.Poll(poll))` places the poll in the composer,
   so reply target, silent and schedule options still apply; the normal Send
   button sends it (`ChatScreen` to `conversation.sendPoll(...)`).
5. `inputMessagePoll` builds `InputMessagePoll`: `question`; `options`
   (formatted text, `media = null`); `description = null`; `media = null`;
   `is_anonymous = value.isAnonymous || value.isQuiz`;
   `allows_multiple_answers`; `allows_revoting = !isQuiz`; `members_only = false`;
   `country_codes = []`; `shuffle_options = false`;
   `hide_results_until_closes = false`;
   `type = InputPollTypeQuiz(correct_option_ids sorted, explanation or null)` or
   `InputPollTypeRegular(allowAddingOptions = false)`; `open_period = 0`;
   `close_date = 0`; `is_closed = false`.
6. Send failures go through `pollResult` / `pollErrorText`, which distinguishes:
   private chat with a person, channel direct messages, non-anonymous poll in a
   channel, missing `canSendPolls` right, invalid poll parameters, message
   unavailable, Premium required, schedule failure, flood wait, and a
   poll-specific fallback.

### 2.8 Voters, add option, stop

- **Voters**: `PollVotersDialog` appears only when `canShowVoters`. It has a
  per-option chip row, pages with `GetPollVoters(chatId, messageId, optionIndex,
  offset, limit 1..100)`, appends pages without duplicating a sender
  (`appendPollVoters` de-duplicates by `senderId`), and computes the next offset
  with `pollVotersNextOffset` (`-1` when the page is empty or the total is
  reached).
- **Add option**: offered only when `canAddOption && !isClosed`. The dialog
  validates locally (`addOptionProblem`: empty, too long, duplicate
  case-insensitive, limit reached); the repository re-reads the message with
  `GetMessage`, re-checks `canAddOption`/`isClosed`, sends `AddPollOption`, then
  refreshes.
- **Stop**: as in §2.5.

### 2.9 Errors and privacy

`pollErrorText` maps TDLib strings to user-facing text; unknown failures keep a
poll-specific fallback rather than a generic "action failed". Poll questions and
options are user text: Android's diagnostics redaction removes quoted text from
logged TDLib messages (`redactedTdlibMessage` test) and the event log strips
non-ASCII poll text. A Windows implementation must not log question, option or
explanation text.

### 2.10 Tests as the executable spec

- `MessageSpecialTest`: quiz answered once, revoting flag, draft validation,
  trimming and length cap, voter visibility matrix, voters paging and
  de-duplication, add-option validation.
- `PollSendTest`: regular poll field mapping, quiz anonymity/revoting/correct
  ids/explanation, and nine error-mapping cases.
- `SpecialContentMappingTest`: quiz mapping, server percentage, restriction,
  shuffled order preserving original ids.
- `AttachmentComposerTest`: poll action hidden when the chat forbids polls.

---

## 3. Current Windows status

### 3.1 Files

| File | State |
|---|---|
| `src/telegram/PollInfo.h` / `.cpp` | Parser plus two request builders. Compiled into `beygram_telegram` (`src/telegram/CMakeLists.txt`). |
| `tests/tst_poll_info.cpp` | Eight test slots, registered as `tst_poll_info` in `tests/CMakeLists.txt`. |
| `qml/components/PollBubble.qml` | A read-only card. Listed in the root `CMakeLists.txt` `QML_FILES`. |
| `qml/components/MessageBubble.qml` | No poll branch; no `PollBubble` instantiation. |
| `qml/pages/ChatsPage.qml` | Message delegate passes text/contentType/media only; the Attach button opens one file dialog. |
| `src/telegram/TelegramTypes.h` / `.cpp` | `MessageData` has no poll field. Polls become `contentType == "poll"` and `text == question` (or `"[Poll]"`). |
| `src/telegram/MessageListModel.h` / `.cpp` | No poll role; `updateContent` rewrites text, HTML and contentType only. |
| `src/telegram/TelegramRepository.h` / `.cpp` | No `sendPoll`, `votePoll`, `stopPoll`, `addPollOption` or voters request. |
| `src/ui/AppController.h` | No poll invokables. |

### 3.2 What exists and is correct

`PollInfo.cpp` is a careful parser for the fields it covers:

- `id` (number or string), `question` (formattedText or bare string);
- `options[]` with `text`, `voter_count` (negative clamped to 0), `is_chosen`;
- `total_voter_count` (negative clamped to 0), `is_anonymous` (default true),
  `is_closed`, `allows_multiple_answers`;
- `isQuiz` from `poll.type.@type == "pollTypeQuiz"`, with a legacy `is_quiz`
  fallback and an explicit "unknown type falls back to the flag" rule;
- `hasVoted` derived from any `is_chosen`;
- malformed input tolerated (non-object poll yields an empty struct, non-object
  options are skipped, a wrong `options` type yields an empty list).

`votePollRequest` builds `setPollAnswer(chat_id, message_id, option_ids)` and
passes positions through unchanged; `stopPollRequest` builds
`stopPoll(chat_id, message_id, reply_markup = null)`. `tst_poll_info` round-trips
both through JSON text and covers negative chat id and large message id.

`PollBubble.qml` is honest about results: it never invents a percentage, hides
the label when the total is unknown, derives the fill from `count / total`,
disables interaction when `closed`, uses `Accessible.RadioButton`/`CheckBox`
roles, and shows "Closed"/"Quiz"/"Multiple choice" pills.

### 3.3 What is not wired (evidence)

This is the decisive finding: **none of the poll code is reachable from the
running application.**

- `pollFromMessage` is called only from `tests/tst_poll_info.cpp`. No production
  translation unit includes `PollInfo.h`.
- `votePollRequest` and `stopPollRequest` are called only from the test. No
  repository method sends them.
- `PollBubble.qml` is referenced only by `CMakeLists.txt` (`QML_FILES`). No QML
  file instantiates it.
- `MessageBubble.qml` has no `contentType === "poll"` branch, so a poll message
  renders as an ordinary text bubble whose body is the question
  (`messageContentText` returns `poll.question.text`) or the literal `[Poll]`.
- `MessageData` carries no poll payload, so the UI could not render one even if
  `PollBubble` were instantiated.
- `TelegramRepository` has no poll operations; `AppController` exposes none; the
  Attach button (`ChatsPage.qml`, `attachDialog`) only opens a file picker.
- `MessageListModel::updateContent` does not refresh poll state: it rewrites
  text, HTML and contentType only, so a live `updateMessageContent` (vote
  counts, close) leaves the poll data stale.

Net effect for a Windows user today: a poll message is visible as a question
string; it cannot be read as a poll, voted on, closed, inspected or created.

### 3.4 Fields that do not exist at all

`PollInfo` has no equivalent for: `description`, poll media, per-option `id`,
server `vote_percentage`, `is_being_chosen`, `option_order`, `allows_revoting`,
`can_see_results`, `can_get_voters`, `can_add_option`, `correct_option_ids`,
`explanation`, `explanation_media`, `vote_restriction_reason`, `close_date`,
`open_period`. There is no `OutgoingPoll`, no draft validation, no voters model,
and no request builder for `sendPoll`, `addPollOption` or `getPollVoters`.

---

## 4. Gap matrix

Legend: **P1** = user-visible correctness gap in an existing chat surface;
**P2** = missing feature the roadmap already schedules; **P3** = polish.

| # | Capability | Android | Windows | Gap | Priority |
|---|---|---|---|---|---|
| 1 | Render a poll as a poll card | `PollCard` | text bubble only | total | P1 |
| 2 | Question, options, vote counts | yes | parsed but unused | total | P1 |
| 3 | Server `vote_percentage` | used first | not parsed; recomputed | wrong numbers | P1 |
| 4 | Multiple-answer percentages | sum of option votes | divides by `totalVoterCount` | can exceed 100 % / does not sum to 100 | P1 |
| 5 | Vote (single choice) | `SetPollAnswer` + refresh | none | total | P1 |
| 6 | Multi-select + Vote button | yes | none | total | P1 |
| 7 | Change vote / `allows_revoting` | enforced | absent | total | P1 |
| 8 | Quiz one-attempt rule | enforced | absent | total | P1 |
| 9 | Closed poll: no interaction | enforced | `closed` property only, never set | total | P1 |
| 10 | Vote restrictions | six reasons | absent | total | P1 |
| 11 | Results hidden until close | `canSeeResults` | absent | shows counts that should be hidden | P1 |
| 12 | Quiz correct answer marker | yes, after vote/close | absent | total | P1 |
| 13 | Quiz explanation | yes | absent | total | P2 |
| 14 | Poll description | yes | absent | total | P2 |
| 15 | Anonymous indicator | yes | absent | cosmetic | P3 |
| 16 | `option_order` shuffle | validated permutation, ids preserved | ignored (documented) | display only | P3 |
| 17 | Pending vote (`is_being_chosen`) | spinner | absent | polish | P3 |
| 18 | Create poll (regular) | `PollComposerDialog` | none | total | P2 |
| 19 | Create quiz + correct answers | yes | none | total | P2 |
| 20 | Draft validation (limits, trimming) | `OutgoingPoll.problem()` | none | total | P2 |
| 21 | Send `InputMessagePoll` | yes | none | total | P2 |
| 22 | Poll-specific send errors | nine mapped cases | none | total | P2 |
| 23 | Voters list (paged, de-duplicated) | yes | none | total | P2 |
| 24 | Add option | yes, re-checked server-side | none | total | P2 |
| 25 | Stop poll (owner) | confirm + refresh | builder only | unwired | P1 |
| 26 | Live refresh after vote/stop | `GetMessage` | none | total | P1 |
| 27 | Live `updateMessageContent` refresh | yes | text only | stale counts | P1 |
| 28 | Attachment "Poll" entry | yes | none | total | P2 |
| 29 | Accessibility (role, percent, keyboard) | role + percent + 48 dp | role/checked, mouse only, no percent | partial | P2 |
| 30 | Privacy: no poll text in logs | redaction | not applicable yet | must be honoured | P1 (when wiring) |

Two Windows-specific notes:

- The `PollBubble` denominator (`count / totalVoterCount`) is not merely missing
  parity; for multiple-answer polls it is mathematically wrong. It should be
  replaced, not extended.
- `PollBubble`'s click handler emits a signal but the rows are not focusable and
  have no `Keys` handler, so a poll cannot be answered from the keyboard on
  desktop. Android's radio/checkbox semantics alone do not fix that on Windows.

---

## 5. Implementation plan

The plan keeps the existing seam: pure parsing/building in `PollInfo.*`
(unit-testable without a session), repository methods that only speak JSON, and
QML that renders state it is given. No Android code changes.

### Stage A - Complete the poll model and derived rules (P1)

Files: `src/telegram/PollInfo.h`, `src/telegram/PollInfo.cpp`.

- `PollOption`: add `id` (TDLib string), `votePercentage` (`-1` when absent),
  `isBeingChosen`, and `index` (original wire position).
- `PollInfo`: add `description`, `allowsRevoting`, `canSeeResults`,
  `canGetVoters`, `canAddOption`, `correctOptionIds` (`QVector<int>`),
  `explanation`, `voteRestriction` (enum), `closeDate`, `openPeriod`.
- Keep the existing "wire order is vote identity" rule: store options in wire
  order and expose display order separately. Apply `option_order` only when it
  is a valid permutation, mirroring `TdMediaMapper`.
- Add derived rules as testable functions: `hasVoted`, `chosenOptionIndices`,
  `canVote`, `revealsResults`, `canShowVoters`, `isCorrect(index)`,
  `percentageOf(index)` (server percentage, then sum-of-options denominator,
  then total, then no value).
- Parse `poll.type.correct_option_ids`, `explanation.text`,
  `vote_restriction_reason.@type`, `allows_revoting`, `can_see_results`,
  `can_get_voters`, `messagePoll.can_add_option`, `messagePoll.description`.

Tests: extend `tests/tst_poll_info.cpp` (§6).

### Stage B - Carry the poll through the message model (P1)

Files: `src/telegram/TelegramTypes.h`, `src/telegram/TelegramTypes.cpp`,
`src/telegram/MessageListModel.h`, `src/telegram/MessageListModel.cpp`.

- `MessageData`: add `PollInfo poll;` (default-constructed means "no poll").
  `contentType == "poll"` stays the discriminator, so no existing role changes
  meaning.
- `messageFromJson`: when `messageContentLabel(content) == "poll"`, populate
  `data.poll = pollFromMessage(message)`.
- `MessageListModel`: add a `PollRole` named `poll` returning a `QVariantMap`
  built by a new `pollToVariantMap(const PollInfo&)` in `PollInfo.*`, so it is
  unit-testable and QML does not re-derive percentages. (The existing
  `reactionsJson` string is the alternative style; a map avoids a second JSON
  parse and duplicated arithmetic.)
- `updateContent`: also refresh `message.poll` from the new content and emit the
  poll role. This is what makes a live vote or close visible.

Tests: new `tests/tst_message_poll_model.cpp` (§6).

### Stage C - Render a real poll card (P1)

Files: `qml/components/PollBubble.qml`, `qml/components/MessageBubble.qml`,
`qml/pages/ChatsPage.qml`.

- `MessageBubble`: add `property var poll: null` and render `PollBubble` when
  `contentType === "poll" && poll !== null`; suppress the plain-text question in
  that case so it is not printed twice. Keep the text path as the fallback for
  an unparsable poll (`[Poll]`).
- `ChatsPage` delegate: pass `poll: model.poll`.
- `PollBubble`: align with `PollCard`:
  - header "Poll"/"Quiz" plus anonymous label;
  - question, description;
  - option rows: chosen, selected, correct (quiz, only after vote/close),
    percentage from the model, pending spinner, result fill;
  - multi-answer: local selection plus a Vote / Change vote button, disabled
    until the selection differs from the server choice;
  - footer: Closed / Multiple choice / votes plural / restriction reason;
  - quiz explanation after results;
  - Stop for outgoing polls with confirmation;
  - signals `optionVoteRequested(index)`, `voteSubmitted(list<int>)`,
    `stopRequested()`, `votersRequested(optionIndex)`, `addOptionRequested()`;
  - keyboard: focusable rows, Space/Enter activation, `Accessible.checked`, and
    the percentage included in `Accessible.name`;
  - never fabricate percentages; show nothing when `revealsResults` is false.

Tests: QML test where the project supports it (§6), otherwise the manual gate.

### Stage D - Wire voting, stopping and live refresh (P1)

Files: `src/telegram/PollInfo.h`/`.cpp`, `src/telegram/TelegramRepository.h`/`.cpp`,
`src/ui/AppController.h`/`.cpp`, `qml/pages/ChatsPage.qml`,
`qml/components/MessageBubble.qml`.

- Request builders in `PollInfo.*`, next to the existing two:
  `getMessageRequest(chatId, messageId)`,
  `addPollOptionRequest(chatId, messageId, text)`,
  `getPollVotersRequest(chatId, messageId, optionIndex, offset, limit)`.
  `votePollRequest` and `stopPollRequest` already exist.
- `TelegramRepository`:
  - `votePoll(chatId, messageId, optionIds)` - dedupe and sort, reject an empty
    list, send, then refresh;
  - `stopPoll(chatId, messageId)`;
  - `refreshMessage(chatId, messageId)` using `getMessageRequest`, guarded by
    `chatId == m_currentChatId`, feeding `messageFromJson` into `addOrUpdate`;
  - errors through the existing `setError` path with poll-specific text.
- `AppController`: `Q_INVOKABLE void votePoll(qint64 messageId,
  const QVariantList& optionIndices)` and `stopPoll(qint64 messageId)`, using
  `currentChatId`.
- QML: connect the `PollBubble` signals; disable the card while a vote is in
  flight (mirror Android's pending state with a local flag cleared on the next
  `dataChanged` if TDLib does not report `is_being_chosen` for our own vote).

### Stage E - Error mapping and privacy (P1)

Files: `src/telegram/PollInfo.cpp` or a small `PollErrors.*`,
`src/telegram/TelegramRepository.cpp`.

Port `pollErrorText`'s phrase checks: private chat with a person, channel direct
messages, non-anonymous poll in a channel, `canSendPolls`/admin rights, invalid
poll parameters, `MESSAGE_ID_INVALID`, Premium, schedule, flood wait, and the
poll-specific fallback. Never pass poll question/option/explanation text to the
logger or the diagnostics dump; treat it like a message body.

### Stage F - Creation flow (P2)

Files: new `src/telegram/PollDraft.h`/`.cpp` (or additions to `PollInfo.*`),
`src/telegram/TelegramRepository.*`, `src/ui/AppController.*`, new
`qml/components/PollComposerDialog.qml`, `qml/pages/ChatsPage.qml`.

- `OutgoingPoll` mirroring Android: `question`, `options`, `isAnonymous`,
  `allowsMultipleAnswers`, `isQuiz`, `correctOptionIndices`, `explanation`;
  `normalized()` (trim) and `problem()` returning a `PollDraftProblem` with the
  same rules and constants (2..10 options, 255/100 code points, quiz rules).
- `sendPollRequest(chatId, poll, replyToMessageId)` building
  `inputMessagePoll` with the exact Android field values from §2.7, including
  `allows_revoting = !isQuiz`, quiz-forced anonymity, sorted correct ids, and a
  null explanation when blank.
- `TelegramRepository::sendPoll(...)` following the `sendText`/`sendMedia`
  pattern: send, add the returned message, track media, resolve sender, mark
  read.
- `AppController::sendChatPoll(const QVariantMap& draft)` (or explicit
  arguments) returning a validation error for QML.
- `PollComposerDialog.qml`: question counter, 2..10 options with add/remove,
  poll/quiz toggle, anonymous and multiple checkboxes (anonymous disabled in
  quiz mode), per-option correct toggle, explanation in quiz mode, inline error
  text. Open it from a new "Poll" entry in the attachment UI; `ChatsPage.qml`
  currently opens `attachDialog` directly, so it needs a small attachment menu
  or a second button.
- Windows has no composer "extra draft" queue yet. The smallest faithful
  behaviour is compose, validate, then send with the current reply target. A
  later refinement can stage the draft in the composer like Android's
  `MessageExtraDraft.Poll`.

### Stage G - Voters list and add option (P2)

Files: `src/telegram/PollInfo.h`/`.cpp` (paging helpers),
`src/telegram/TelegramRepository.*`, `src/ui/AppController.*`, new
`qml/components/PollVotersDialog.qml` and `qml/components/AddPollOptionDialog.qml`
(or inline in `PollBubble`).

- Port `pollVotersNextOffset` and `appendPollVoters` (de-duplicate by sender id)
  as pure functions with tests.
- `TelegramRepository::requestPollVoters(...)` emits
  `pollVotersLoaded(requestId, voters, totalCount, nextOffset)`; `AppController`
  forwards the signal.
- Add option: re-read the message, re-check `canAddOption && !isClosed`, apply
  `addOptionProblem` locally, send `addPollOption`, refresh. The re-check is not
  optional: Android documents that the server can revoke the permission between
  render and click.

### Stage H - Polish (P3)

Anonymous indicator, `option_order` display, pending spinner, vote plural,
`Accessible` percentages, high-contrast tokens, RTL, and the five design styles.

---

## 6. Tests to add / extend

No test is claimed to have been run. Register new targets with
`beygram_add_test(...)` in `tests/CMakeLists.txt`, as the existing suites are.

### Extend `tests/tst_poll_info.cpp`

1. `description`, `allows_revoting`, `can_see_results`, `can_get_voters`,
   `can_add_option` parsing (present, absent, wrong type).
2. Server `vote_percentage` exposed and preferred; absent falls back.
3. Multiple-answer percentage: options with 3+2+2 votes and
   `total_voter_count = 5` must yield 43/29/29 (sum-of-options denominator), not
   60/40/40.
4. Total 0 with all counts 0 yields no percentage, not 0 %.
5. Quiz: `correct_option_ids`, `isCorrect`, explanation; `explanation_media`
   ignored safely.
6. `option_order` valid permutation changes display order and preserves
   original indices; duplicate / out-of-range / wrong-size order falls back to
   wire order.
7. `canVote` matrix: closed; restricted; quiz answered; voted with and without
   revoting; not voted.
8. `canShowVoters` matrix: anonymous, `can_get_voters = false`,
   `can_see_results = false`.
9. Restriction mapping for all six `pollVoteRestrictionReason*` classes and an
   unknown class (document the chosen fallback).
10. `is_being_chosen` parsed.
11. Malformed: options as object, negative percentages, missing type.

### New `tests/tst_poll_draft.cpp`

1. Valid regular poll and valid quiz produce no problem.
2. Question empty / longer than 255 code points (use a 4-byte emoji to prove
   code-point counting rather than `QString::size()`).
3. Fewer than 2 / more than 10 options; empty option; option longer than 100.
4. Quiz without a correct answer, with an out-of-range index, with two answers
   in single-choice mode.
5. `normalized()` trims question, options and explanation.
6. `sendPollRequest` shape: anonymity forced for a quiz, `allows_revoting`
   false for a quiz and true for a regular poll, correct ids sorted,
   `explanation` null when blank, `shuffle_options = false`,
   `hide_results_until_closes = false`, `open_period = 0`, `close_date = 0`,
   `is_closed = false`, `InputPollTypeRegular.allowAddingOptions = false`.

### New `tests/tst_message_poll_model.cpp`

1. `messageFromJson` on `messagePoll` fills `poll`; `messageText` leaves it
   empty.
2. The `poll` role returns a map with options in display order and original
   indices.
3. `updateContent` with new poll content refreshes counts and closed state.
4. `updateContent` with non-poll content clears the poll.
5. A poll message still produces a sensible chat-list preview (question, then
   the empty-poll fallback).

### Extend `tests/tst_telegram_requests.cpp` (or add `tst_poll_requests.cpp`)

1. `setPollAnswer` with empty ids (the repository must reject before sending).
2. `stopPoll` keeps `reply_markup` null.
3. `addPollOption` sends text as formatted text with `media = null`.
4. `getPollVoters` clamps offset to at least 0 and limit to 1..100.
5. `getMessage` shape for the refresh path.

### QML tests

If the project adopts Qt Quick Test, cover `PollBubble` states: hidden results,
multiple-answer percentages, closed disables clicks, quiz correct marker only
after voting, and keyboard activation.

---

## 7. Priority and sequencing

The roadmap files polls under Phase 5 (P2), but this audit splits the work:
items 1-5 are not a new feature. A poll message that renders as a bare question
string is a correctness defect in a chat surface that already ships.

| Order | Work | Priority | Depends on | Rough size |
|---|---|---|---|---|
| 1 | Complete `PollInfo` model and derived rules (Stage A) | P1 | - | 1-2 days |
| 2 | `MessageData`/`MessageListModel` poll role and `updateContent` (Stage B) | P1 | 1 | 1 day |
| 3 | `PollBubble` rewrite and `MessageBubble` branch (Stage C) | P1 | 1, 2 | 2-3 days |
| 4 | Vote/stop repository, `AppController`, live refresh (Stage D) | P1 | 1, 3 | 2 days |
| 5 | Error mapping and privacy pass (Stage E) | P1 | 4 | 0.5 day |
| 6 | `OutgoingPoll`, `sendPollRequest`, composer, attachment entry (Stage F) | P2 | 1 | 3-4 days |
| 7 | Voters dialog and add option (Stage G) | P2 | 4 | 2-3 days |
| 8 | Accessibility, anonymous label, shuffle, pending polish (Stage H) | P3 | 3 | 1-2 days |

Sequencing rule: stages A-E must land before any poll is advertised in the
composer. Shipping creation before voting would let a Windows user author a poll
they cannot read or answer.

---

## 8. Manual verification gates (not executed)

Never automate these against the user's real account. Use a test account and a
second device.

1. Create a regular poll in a group; vote from the other device; confirm the
   first device updates without a history reload.
2. Create a multiple-answer poll; select two options; confirm the Vote button
   enables, the submitted selection matches, and percentages use the
   sum-of-options denominator.
3. Create a quiz; confirm the correct answer is invisible before answering and
   appears with the explanation afterwards; confirm a second answer is refused.
4. Close an owned poll; confirm the footer, the disabled card and the preserved
   results.
5. Open the voters list on a non-anonymous poll; page to the end; confirm no
   duplicated sender in a multiple-answer poll.
6. Add an option to a poll that allows it; confirm the new option appears.
7. Send a poll to a private chat with a person and to a channel without
   anonymity; confirm the specific error text, not a generic failure.
8. Confirm the question, options and explanation never appear in the log file
   or the diagnostics dump.

---

## 9. Risks and open decisions

1. **TDLib field availability.** Stages A and G depend on classes the Windows
   parser does not read yet. The pinned revision (`4c6377aa...`, reported
   1.8.69) is newer than the field set Android relies on, but `td_api.tl` is not
   vendored in this repository. Verify `addPollOption`, `getPollVoters`,
   `allows_revoting`, `can_see_results`, `can_get_voters` and
   `vote_restriction_reason` against the exact checkout before coding. If one
   is missing, degrade explicitly (for example, no voters list) instead of
   sending an unsupported request.
2. **`updateMessageContent` for polls.** TDLib sends vote updates as
   `updateMessageContent` with a new `messagePoll`. Windows currently ignores
   the poll part. Relying on `getMessage` alone would leave a poll voted on by
   someone else stale until the next history reload, so the Stage B change is
   mandatory.
3. **Model ownership.** `MessageData` is copied across the parse worker and
   stored in `QVector`; `PollInfo` must stay trivially copyable (no pointers, no
   `QObject`). Confirm the queued-signal path (`applyHistory`,
   `applyParsedChat`) still registers the type correctly.
4. **Percentage semantics.** Android's sum-of-options denominator is a
   deliberate choice for multiple-answer polls. Port it and test it; do not
   "fix" it to `totalVoterCount`.
5. **No composer draft queue.** Android stages the poll as
   `MessageExtraDraft.Poll`. Windows has no equivalent; the immediate-send
   behaviour in Stage F is a documented, smaller-scope difference. Staged drafts
   are a separate composer feature.
6. **Desktop accessibility.** Radio/checkbox roles alone are insufficient
   without focus and key handling. The QML test should cover keyboard
   activation, not only pointer clicks.
7. **Privacy.** Poll text must never enter logs, crash reports or the
   diagnostics dump. Android already treats this as a rule with tests; Windows
   should add an explicit assertion when the wiring lands.

---

## 10. Out of scope

- Replying to a specific poll option (`pollOptionId`): Android always sends an
  empty string today, so Windows should not invent it.
- Poll option media (`InputPollOption.media`): Android always sends `null`.
- Local percentage invention when TDLib reports no results.
- Poll scheduling/`open_period`/`close_date` authoring: Android sends zeroes and
  leaves these to Telegram defaults.
- Changing the pinned TDLib revision for polls.

---

## 11. Verification statement

- Static source reading only. No configure, build, `ctest`, QML run or device
  test was executed for this document.
- No Android source, Windows source, test or build file was modified.
- The claim "the poll code is unreachable" is based on a repository-wide text
  search for `PollInfo`, `PollBubble`, `pollFromMessage`, `votePollRequest` and
  `stopPollRequest`: the only non-test references are the CMake source list
  (`src/telegram/CMakeLists.txt`), the root `QML_FILES` list, and the header
  itself.
- The Android behaviour described here is what the cited Kotlin files
  implement; it was not verified against a live Telegram account.
