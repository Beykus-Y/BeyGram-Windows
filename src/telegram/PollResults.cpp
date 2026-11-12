#include "PollResults.h"

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int kPercentScale = 100;
constexpr int kMinPercent = 0;
constexpr int kMaxPercent = 100;

/// Sum of the option vote counts. A multiple-answer poll needs this as its
/// denominator when TDLib's total is unavailable: the sum exceeds the number
/// of voters, so percentages still add up to 100 %.
int sumOfOptionVotes(const PollInfo& poll)
{
    int total = 0;
    for (const PollOption& option : poll.options)
        total += std::max(option.voterCount, 0);
    return total;
}

/// PollInfo::hasVoted is derived from is_chosen when the poll is parsed, but a
/// hand-built PollInfo may only set one of the two, so both are honoured.
bool hasVoted(const PollInfo& poll)
{
    if (poll.hasVoted)
        return true;

    for (const PollOption& option : poll.options) {
        if (option.chosen)
            return true;
    }
    return false;
}

} // namespace

int optionPercentage(int optionVotes, int totalVotes, int optionCount)
{
    // No counted votes, no usable denominator or no options means there is no
    // share to show; never fabricate one.
    if (optionVotes <= 0 || totalVotes <= 0 || optionCount <= 0)
        return kMinPercent;

    const double percentage =
        static_cast<double>(optionVotes) * kPercentScale / static_cast<double>(totalVotes);
    return std::clamp(qRound(percentage), kMinPercent, kMaxPercent);
}

PollResults pollResultsFromInfo(const PollInfo& poll)
{
    PollResults results;

    results.closed = poll.isClosed;
    results.canVote = !poll.isClosed;
    results.revealsResults = !poll.isQuiz || poll.isClosed || hasVoted(poll);

    // A missing or negative server total is replaced by the votes the options
    // carry. The same denominator feeds every percentage so the shares agree
    // with the total shown in the footer.
    results.totalVoterCount =
        poll.totalVoterCount > 0 ? poll.totalVoterCount : sumOfOptionVotes(poll);

    const int optionCount = static_cast<int>(poll.options.size());
    results.options.reserve(optionCount);
    for (int index = 0; index < optionCount; ++index) {
        const PollOption& source = poll.options.at(index);

        PollResultOption option;
        option.index = index;
        option.voterCount = std::max(source.voterCount, 0);
        option.percentage =
            optionPercentage(option.voterCount, results.totalVoterCount, optionCount);
        option.chosen = source.chosen;
        results.options.append(option);
    }

    return results;
}

} // namespace beygram::telegram
