#include "telegram/PrefetchPlanner.h"

#include <QVector>
#include <QtTest>

#include <algorithm>

using namespace beygram::telegram;

namespace {
PrefetchRequest request(qint64 chatId, qint64 messageId, int fileId, int priority)
{
    return PrefetchRequest{chatId, messageId, fileId, priority};
}

QVector<int> fileIds(const QVector<PrefetchRequest>& plan)
{
    QVector<int> ids;
    ids.reserve(plan.size());
    for (const PrefetchRequest& entry : plan)
        ids.push_back(entry.fileId);
    return ids;
}

QVector<qint64> chatIds(const QVector<PrefetchRequest>& plan)
{
    QVector<qint64> ids;
    ids.reserve(plan.size());
    for (const PrefetchRequest& entry : plan)
        ids.push_back(entry.chatId);
    return ids;
}
} // namespace

class PrefetchPlannerTest final : public QObject {
    Q_OBJECT

private slots:
    void emptyCandidatesProduceEmptyPlan();
    void emptyChatListsProduceEmptyPlan();
    void singleCandidateIsReturnedUnchanged();
    void visibleChatsRankAboveRecentChats();
    void earlierChatInAListWins();
    void chatInBothListsKeepsItsVisibleRank();
    void repeatedChatIdsAreCountedOnce();
    void higherPriorityWinsWithinAChat();
    void newestMessageWinsAtEqualPriority();
    void chatRankOutranksPriority();
    void duplicateFileIdsKeepTheHighestPriority();
    void duplicateWinnerIsChosenByPriorityNotChatRank();
    void duplicateAtEqualPriorityKeepsTheBestRankedCopy();
    void duplicateAtEqualPriorityWithinAChatKeepsTheNewestMessage();
    void nonPositiveFileIdsAreDropped();
    void candidatesFromUnlistedChatsAreDropped();
    void budgetTruncatesThePlan();
    void budgetBelowOneIsRaisedToOne();
    void budgetAboveSixtyFourIsCapped();
    void defaultBudgetIsTwenty();
    void budgetLargerThanThePoolReturnsEverything();
    void negativePriorityIsKeptButRankedLast();
    void planIsIndependentOfCandidateOrder();
};

void PrefetchPlannerTest::emptyCandidatesProduceEmptyPlan()
{
    QVERIFY(PrefetchPlanner::plan({1, 2}, {3}, {}, 20).isEmpty());
    QVERIFY(PrefetchPlanner::plan({}, {}, {}, 20).isEmpty());
}

void PrefetchPlannerTest::emptyChatListsProduceEmptyPlan()
{
    const QVector<PrefetchRequest> candidates{request(1, 1, 10, 5)};
    QVERIFY(PrefetchPlanner::plan({}, {}, candidates, 20).isEmpty());
}

void PrefetchPlannerTest::singleCandidateIsReturnedUnchanged()
{
    const PrefetchRequest candidate = request(42, 7, 99, 3);
    const QVector<PrefetchRequest> result = PrefetchPlanner::plan({42}, {}, {candidate}, 20);
    QCOMPARE(result.size(), 1);
    QCOMPARE(result.first(), candidate);
}

void PrefetchPlannerTest::visibleChatsRankAboveRecentChats()
{
    // The recent candidate outranks the visible one on every other rule; the
    // chat tier still decides.
    const QVector<PrefetchRequest> candidates{
        request(20, 500, 2, 99),
        request(10, 1, 1, 0),
    };
    const QVector<PrefetchRequest> result = PrefetchPlanner::plan({10}, {20}, candidates, 20);
    const QVector<qint64> expected{10, 20};
    QCOMPARE(chatIds(result), expected);
}

void PrefetchPlannerTest::earlierChatInAListWins()
{
    const QVector<PrefetchRequest> visible{
        request(30, 1, 3, 0),
        request(10, 1, 1, 0),
        request(20, 1, 2, 0),
    };
    const QVector<qint64> visibleOrder{30, 10, 20};
    QCOMPARE(chatIds(PrefetchPlanner::plan({30, 10, 20}, {}, visible, 20)), visibleOrder);

    const QVector<PrefetchRequest> recent{
        request(3, 1, 3, 0),
        request(1, 1, 1, 0),
        request(2, 1, 2, 0),
    };
    const QVector<qint64> recentOrder{3, 1, 2};
    QCOMPARE(chatIds(PrefetchPlanner::plan({}, {3, 1, 2}, recent, 20)), recentOrder);
}

void PrefetchPlannerTest::chatInBothListsKeepsItsVisibleRank()
{
    const QVector<PrefetchRequest> candidates{
        request(9, 100, 1, 50),
        request(5, 1, 2, 0),
    };
    const QVector<PrefetchRequest> result = PrefetchPlanner::plan({5}, {9, 5}, candidates, 20);
    const QVector<qint64> expected{5, 9};
    QCOMPARE(chatIds(result), expected);
    QCOMPARE(result.size(), 2); // The chat is not planned twice.
}

void PrefetchPlannerTest::repeatedChatIdsAreCountedOnce()
{
    const QVector<PrefetchRequest> candidates{
        request(7, 1, 1, 0),
        request(3, 1, 2, 0),
    };
    const QVector<PrefetchRequest> result = PrefetchPlanner::plan({7, 7, 3}, {7}, candidates, 20);
    const QVector<qint64> expected{7, 3};
    QCOMPARE(chatIds(result), expected);
    QCOMPARE(result.size(), 2);
}

void PrefetchPlannerTest::higherPriorityWinsWithinAChat()
{
    const QVector<PrefetchRequest> candidates{
        request(1, 100, 1, 1),
        request(1, 1, 2, 9),
        request(1, 50, 3, 5),
    };
    const QVector<int> expected{2, 3, 1};
    QCOMPARE(fileIds(PrefetchPlanner::plan({1}, {}, candidates, 20)), expected);
}

void PrefetchPlannerTest::newestMessageWinsAtEqualPriority()
{
    const QVector<PrefetchRequest> candidates{
        request(1, 5, 1, 7),
        request(1, 500, 3, 7),
        request(1, 50, 2, 7),
    };
    const QVector<int> expected{3, 2, 1};
    QCOMPARE(fileIds(PrefetchPlanner::plan({1}, {}, candidates, 20)), expected);
}

void PrefetchPlannerTest::chatRankOutranksPriority()
{
    const QVector<PrefetchRequest> candidates{
        request(2, 1, 2, 100),
        request(1, 1, 1, 0),
    };
    const QVector<int> expected{1, 2};
    QCOMPARE(fileIds(PrefetchPlanner::plan({1}, {2}, candidates, 20)), expected);
}

void PrefetchPlannerTest::duplicateFileIdsKeepTheHighestPriority()
{
    const PrefetchRequest loser = request(1, 1, 7, 2);
    const PrefetchRequest winner = request(1, 9, 7, 8);
    const QVector<PrefetchRequest> result =
        PrefetchPlanner::plan({1}, {}, {loser, winner}, 20);
    QCOMPARE(result.size(), 1);
    QCOMPARE(result.first(), winner);
}

void PrefetchPlannerTest::duplicateWinnerIsChosenByPriorityNotChatRank()
{
    // The same file is offered by a visible chat with a low priority and by a
    // recent chat with a high one. The rule says priority wins, so the recent
    // request is the one that survives.
    const QVector<PrefetchRequest> candidates{
        request(1, 5, 7, 1),
        request(2, 6, 7, 9),
    };
    const QVector<PrefetchRequest> result = PrefetchPlanner::plan({1}, {2}, candidates, 20);
    QCOMPARE(result.size(), 1);
    QCOMPARE(result.first(), request(2, 6, 7, 9));
}

void PrefetchPlannerTest::duplicateAtEqualPriorityKeepsTheBestRankedCopy()
{
    const QVector<PrefetchRequest> candidates{
        request(2, 9, 7, 5),
        request(1, 1, 7, 5),
    };
    const QVector<PrefetchRequest> result = PrefetchPlanner::plan({1}, {2}, candidates, 20);
    QCOMPARE(result.size(), 1);
    QCOMPARE(result.first(), request(1, 1, 7, 5));
}

void PrefetchPlannerTest::duplicateAtEqualPriorityWithinAChatKeepsTheNewestMessage()
{
    const QVector<PrefetchRequest> candidates{
        request(1, 3, 7, 5),
        request(1, 8, 7, 5),
    };
    const QVector<PrefetchRequest> result = PrefetchPlanner::plan({1}, {}, candidates, 20);
    QCOMPARE(result.size(), 1);
    QCOMPARE(result.first(), request(1, 8, 7, 5));
}

void PrefetchPlannerTest::nonPositiveFileIdsAreDropped()
{
    const QVector<PrefetchRequest> candidates{
        request(1, 4, 0, 9),
        request(1, 3, -1, 9),
        request(1, 2, -42, 9),
        request(1, 1, 3, 1),
    };
    const QVector<PrefetchRequest> result = PrefetchPlanner::plan({1}, {}, candidates, 20);
    const QVector<int> expected{3};
    QCOMPARE(fileIds(result), expected);
}

void PrefetchPlannerTest::candidatesFromUnlistedChatsAreDropped()
{
    const QVector<PrefetchRequest> candidates{
        request(99, 1, 2, 100),
        request(1, 1, 1, 0),
        request(2, 1, 3, 0),
    };
    const QVector<PrefetchRequest> result = PrefetchPlanner::plan({1}, {2}, candidates, 20);
    const QVector<qint64> expected{1, 2};
    QCOMPARE(chatIds(result), expected);
}

void PrefetchPlannerTest::budgetTruncatesThePlan()
{
    QVector<PrefetchRequest> candidates;
    for (int i = 5; i >= 1; --i)
        candidates.push_back(request(1, i, i, i * 10));

    const QVector<PrefetchRequest> result = PrefetchPlanner::plan({1}, {}, candidates, 3);
    const QVector<int> expected{5, 4, 3};
    QCOMPARE(fileIds(result), expected);
}

void PrefetchPlannerTest::budgetBelowOneIsRaisedToOne()
{
    const QVector<PrefetchRequest> candidates{
        request(1, 1, 1, 9),
        request(1, 2, 2, 8),
    };
    for (const int budget : {0, -1, -7}) {
        const QVector<PrefetchRequest> result = PrefetchPlanner::plan({1}, {}, candidates, budget);
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.first().fileId, 1);
    }
}

void PrefetchPlannerTest::budgetAboveSixtyFourIsCapped()
{
    QVector<PrefetchRequest> candidates;
    for (int messageId = 1; messageId <= 70; ++messageId)
        candidates.push_back(request(1, messageId, messageId, 0));

    const QVector<PrefetchRequest> result = PrefetchPlanner::plan({1}, {}, candidates, 1000);
    QCOMPARE(result.size(), 64);
    QCOMPARE(result.first().messageId, qint64(70));
    QCOMPARE(result.last().messageId, qint64(7));
}

void PrefetchPlannerTest::defaultBudgetIsTwenty()
{
    QVector<PrefetchRequest> candidates;
    for (int i = 1; i <= 25; ++i)
        candidates.push_back(request(1, i, i, 0));

    const QVector<PrefetchRequest> result = PrefetchPlanner::plan({1}, {}, candidates);
    QCOMPARE(result.size(), 20);
}

void PrefetchPlannerTest::budgetLargerThanThePoolReturnsEverything()
{
    const QVector<PrefetchRequest> candidates{
        request(1, 2, 2, 0),
        request(1, 1, 1, 0),
    };
    const QVector<int> expected{2, 1};
    QCOMPARE(fileIds(PrefetchPlanner::plan({1}, {}, candidates, 64)), expected);
}

void PrefetchPlannerTest::negativePriorityIsKeptButRankedLast()
{
    const QVector<PrefetchRequest> candidates{
        request(1, 1, 1, -5),
        request(1, 2, 2, 0),
    };
    const QVector<PrefetchRequest> result = PrefetchPlanner::plan({1}, {}, candidates, 20);
    const QVector<int> expected{2, 1};
    QCOMPARE(fileIds(result), expected);
    QCOMPARE(result.last().priority, -5);
}

void PrefetchPlannerTest::planIsIndependentOfCandidateOrder()
{
    const QVector<PrefetchRequest> candidates{
        request(2, 7, 70, 4),
        request(1, 9, 90, 1),
        request(1, 3, 30, 4),
        request(2, 8, 80, 4),
        request(1, 5, 50, 4),
        request(1, 9, 91, 1),
        request(3, 2, 20, 9),
        request(1, 1, 10, 0),
        request(2, 1, 90, 1),
    };

    const QVector<PrefetchRequest> forward =
        PrefetchPlanner::plan({1, 3}, {2}, candidates, 6);
    QVector<PrefetchRequest> reversed = candidates;
    std::reverse(reversed.begin(), reversed.end());
    const QVector<PrefetchRequest> backward =
        PrefetchPlanner::plan({1, 3}, {2}, reversed, 6);

    QCOMPARE(forward, backward);
    QCOMPARE(forward.size(), 6);
}

QTEST_GUILESS_MAIN(PrefetchPlannerTest)
#include "tst_prefetch_planner.moc"
