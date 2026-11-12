#pragma once

#include "PollInfo.h"

#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One option of a poll result as the UI renders it. PollResults::options
/// keeps PollInfo::options order, so index is the 0-based position a vote
/// request names.
struct PollResultOption {
    int index = 0;
    int voterCount = 0;
    /// Whole percentage, rounded to the nearest integer and clamped to 0..100.
    /// It stays 0 when the poll has no counted votes, so hidden or unanswered
    /// results never show a fabricated share.
    double percentage = 0;
    bool chosen = false;
};

/// The results of one poll, prepared for rendering without touching the UI.
///
/// totalVoterCount is TDLib's total when it is positive and the sum of the
/// option vote counts otherwise, so a poll whose server total is unavailable
/// still reports the votes it actually carries.
struct PollResults {
    int totalVoterCount = 0;
    QVector<PollResultOption> options;
    /// Voting is possible while the poll is open. Revoting and vote
    /// restrictions are server flags that a caller may still apply.
    bool canVote = false;
    /// A quiz hides its counts until it is closed or this account has voted;
    /// a regular poll always shows them.
    bool revealsResults = true;
    bool closed = false;
};

/// Percentage of one option out of totalVotes, rounded to the nearest whole
/// percent and clamped to 0..100.
///
/// totalVotes is the resolved denominator: TDLib's total_voter_count when it
/// is positive, otherwise the sum of the option vote counts, which is what a
/// quiz or multiple-answer poll needs when the server total is unavailable.
/// Zero or negative votes, totals or optionCount yield 0, so a poll without
/// votes never fabricates a share.
[[nodiscard]] int optionPercentage(int optionVotes, int totalVotes, int optionCount);

/// Builds the rendered results of a poll.
///
/// totalVoterCount falls back to the sum of the option vote counts when
/// TDLib's total is not positive, and percentages use that same denominator.
/// Chosen options and their positions are passed through unchanged. canVote is
/// true only while the poll is open, closed mirrors PollInfo::isClosed, and
/// revealsResults is true for regular polls and for quizzes that are closed or
/// already answered. Option text is never copied here, so the results carry no
/// user content beyond the counts.
[[nodiscard]] PollResults pollResultsFromInfo(const PollInfo& poll);

} // namespace beygram::telegram
