#include "PrefetchPlanner.h"

#include <QHash>
#include <QSet>

#include <algorithm>

namespace beygram::telegram {
namespace {
constexpr int MinBudget = 1;
constexpr int MaxBudget = 64;

/// Where a chat sits in the plan. tier 0 is the visible list, tier 1 the recent
/// one; index is the position the caller gave the chat inside that list.
struct ChatRank {
    int tier = 1;
    int index = 0;
};

/// A candidate together with the rank of the chat it belongs to.
struct RankedRequest {
    PrefetchRequest request;
    ChatRank rank;
};

/// The within-chat rules: higher priority first, then the newer message. fileId
/// is only a final tie-break, so the order never depends on the input order.
bool outranksWithinChat(const PrefetchRequest& a, const PrefetchRequest& b)
{
    if (a.priority != b.priority)
        return a.priority > b.priority;
    if (a.messageId != b.messageId)
        return a.messageId > b.messageId;
    return a.fileId < b.fileId;
}

/// The order of the finished plan: chat relevance first, then the within-chat
/// rules.
bool plannedBefore(const RankedRequest& a, const RankedRequest& b)
{
    if (a.rank.tier != b.rank.tier)
        return a.rank.tier < b.rank.tier;
    if (a.rank.index != b.rank.index)
        return a.rank.index < b.rank.index;
    return outranksWithinChat(a.request, b.request);
}

/// The order used to choose the copy of a duplicated file. Priority decides
/// first, because that is the rule, and the chat rank only breaks a tie.
bool duplicateWinnerBefore(const RankedRequest& a, const RankedRequest& b)
{
    if (a.request.priority != b.request.priority)
        return a.request.priority > b.request.priority;
    return plannedBefore(a, b);
}
} // namespace

QVector<PrefetchRequest> PrefetchPlanner::plan(const QVector<qint64>& visibleChatIds,
                                               const QVector<qint64>& recentChatIds,
                                               const QVector<PrefetchRequest>& candidates,
                                               int budget)
{
    if (candidates.isEmpty())
        return {};

    // Build the chat ranking once. A chat already known from the visible list
    // keeps its visible rank when it also appears in the recent list.
    QHash<qint64, ChatRank> ranks;
    ranks.reserve(visibleChatIds.size() + recentChatIds.size());
    for (int i = 0; i < visibleChatIds.size(); ++i) {
        const qint64 chatId = visibleChatIds.at(i);
        if (!ranks.contains(chatId))
            ranks.insert(chatId, ChatRank{0, i});
    }
    for (int i = 0; i < recentChatIds.size(); ++i) {
        const qint64 chatId = recentChatIds.at(i);
        if (!ranks.contains(chatId))
            ranks.insert(chatId, ChatRank{1, i});
    }
    if (ranks.isEmpty())
        return {};

    // Keep only candidates that can actually be fetched, for a chat the caller
    // asked about.
    QVector<RankedRequest> ranked;
    ranked.reserve(candidates.size());
    for (const PrefetchRequest& candidate : candidates) {
        if (candidate.fileId <= 0)
            continue;
        const auto rank = ranks.constFind(candidate.chatId);
        if (rank == ranks.cend())
            continue;
        ranked.push_back(RankedRequest{candidate, rank.value()});
    }
    if (ranked.isEmpty())
        return {};

    // Drop duplicate files before the final sort, so the surviving copy is the
    // highest priority one rather than whichever chat happens to sort first.
    std::sort(ranked.begin(), ranked.end(), duplicateWinnerBefore);
    QSet<int> seenFiles;
    seenFiles.reserve(ranked.size());
    QVector<RankedRequest> unique;
    unique.reserve(ranked.size());
    for (const RankedRequest& entry : ranked) {
        if (seenFiles.contains(entry.request.fileId))
            continue;
        seenFiles.insert(entry.request.fileId);
        unique.push_back(entry);
    }

    std::sort(unique.begin(), unique.end(), plannedBefore);

    const int limit = qBound(MinBudget, budget, MaxBudget);
    const qsizetype count = std::min(unique.size(), static_cast<qsizetype>(limit));

    QVector<PrefetchRequest> result;
    result.reserve(count);
    for (qsizetype i = 0; i < count; ++i)
        result.push_back(unique.at(i).request);
    return result;
}

} // namespace beygram::telegram
