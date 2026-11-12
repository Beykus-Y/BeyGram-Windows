#include "telegram/PollResults.h"

#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

PollOption option(int votes, bool chosen = false)
{
    PollOption result;
    result.text = QStringLiteral("Option");
    result.voterCount = votes;
    result.chosen = chosen;
    return result;
}

PollInfo makePoll(const QVector<PollOption>& options, int totalVoters, bool quiz = false,
                  bool multiple = false, bool closed = false, bool voted = false)
{
    PollInfo poll;
    poll.question = QStringLiteral("Question");
    poll.options = options;
    poll.totalVoterCount = totalVoters;
    poll.isQuiz = quiz;
    poll.multipleChoice = multiple;
    poll.isClosed = closed;
    poll.hasVoted = voted;
    return poll;
}

} // namespace

class PollResultsTest final : public QObject {
    Q_OBJECT

private slots:
    void regularPollResults();
    void quizHidesResultsUntilAnswered();
    void closedPollResults();
    void multipleChoiceUsesOptionVotesWhenTotalMissing();
    void prefersReportedTotal();
    void zeroTotals();
    void emptyPoll();
    void optionPercentageRounds();
    void optionPercentageRejectsUnusableInput();
};

void PollResultsTest::regularPollResults()
{
    const PollResults results =
        pollResultsFromInfo(makePoll({option(4, true), option(6)}, 10));

    QCOMPARE(results.totalVoterCount, 10);
    QCOMPARE(results.options.size(), 2);
    QVERIFY(results.canVote);
    QVERIFY(!results.closed);
    QVERIFY(results.revealsResults);

    QCOMPARE(results.options.at(0).index, 0);
    QCOMPARE(results.options.at(0).voterCount, 4);
    QCOMPARE(results.options.at(0).percentage, 40.0);
    QVERIFY(results.options.at(0).chosen);

    QCOMPARE(results.options.at(1).index, 1);
    QCOMPARE(results.options.at(1).voterCount, 6);
    QCOMPARE(results.options.at(1).percentage, 60.0);
    QVERIFY(!results.options.at(1).chosen);
}

void PollResultsTest::quizHidesResultsUntilAnswered()
{
    // An open, unanswered quiz hides its counts but can still be answered.
    const PollResults unanswered =
        pollResultsFromInfo(makePoll({option(4), option(6)}, 10, true));
    QVERIFY(!unanswered.revealsResults);
    QVERIFY(unanswered.canVote);
    QVERIFY(!unanswered.closed);

    // Answering reveals the results; canVote only tracks the open/closed flag.
    const PollResults answered =
        pollResultsFromInfo(makePoll({option(4, true), option(6)}, 10, true, false, false, true));
    QVERIFY(answered.revealsResults);
    QVERIFY(answered.canVote);
    QCOMPARE(answered.options.at(0).percentage, 40.0);

    // A hand-built PollInfo may mark the choice only on the option.
    const PollResults chosenOnly =
        pollResultsFromInfo(makePoll({option(4, true), option(6)}, 10, true));
    QVERIFY(chosenOnly.revealsResults);
}

void PollResultsTest::closedPollResults()
{
    const PollResults regular =
        pollResultsFromInfo(makePoll({option(5), option(5)}, 10, false, false, true));
    QVERIFY(regular.closed);
    QVERIFY(!regular.canVote);
    QVERIFY(regular.revealsResults);
    QCOMPARE(regular.options.at(0).percentage, 50.0);
    QCOMPARE(regular.options.at(1).percentage, 50.0);

    // A closed quiz shows its results even though it was never answered.
    const PollResults quiz =
        pollResultsFromInfo(makePoll({option(2), option(8)}, 10, true, false, true));
    QVERIFY(quiz.closed);
    QVERIFY(!quiz.canVote);
    QVERIFY(quiz.revealsResults);
    QCOMPARE(quiz.options.at(0).percentage, 20.0);
    QCOMPARE(quiz.options.at(1).percentage, 80.0);
}

void PollResultsTest::multipleChoiceUsesOptionVotesWhenTotalMissing()
{
    // With no usable total the sum of option votes is the denominator, so a
    // multiple-answer poll still produces shares that add up to 100 %.
    const PollResults results =
        pollResultsFromInfo(makePoll({option(3), option(2), option(1)}, 0, false, true));
    QCOMPARE(results.totalVoterCount, 6);
    QCOMPARE(results.options.at(0).percentage, 50.0);
    QCOMPARE(results.options.at(1).percentage, 33.0);
    QCOMPARE(results.options.at(2).percentage, 17.0);

    // The same fallback applies to a quiz whose server total is missing.
    const PollResults quiz =
        pollResultsFromInfo(makePoll({option(2), option(2)}, 0, true, false, true));
    QCOMPARE(quiz.totalVoterCount, 4);
    QCOMPARE(quiz.options.at(0).percentage, 50.0);
    QCOMPARE(quiz.options.at(1).percentage, 50.0);
}

void PollResultsTest::prefersReportedTotal()
{
    // The sum fallback is only for a missing total; a reported total wins even
    // when the option votes sum past it (multiple answers).
    const PollResults results =
        pollResultsFromInfo(makePoll({option(3), option(2)}, 4, false, true));
    QCOMPARE(results.totalVoterCount, 4);
    QCOMPARE(results.options.at(0).percentage, 75.0);
    QCOMPARE(results.options.at(1).percentage, 50.0);
}

void PollResultsTest::zeroTotals()
{
    const PollResults noVotes =
        pollResultsFromInfo(makePoll({option(0), option(0)}, 0));
    QCOMPARE(noVotes.totalVoterCount, 0);
    QCOMPARE(noVotes.options.at(0).percentage, 0.0);
    QCOMPARE(noVotes.options.at(1).percentage, 0.0);

    // Negative counts are clamped instead of skewing the denominator.
    const PollResults negativeVotes =
        pollResultsFromInfo(makePoll({option(-3), option(2)}, -5));
    QCOMPARE(negativeVotes.totalVoterCount, 2);
    QCOMPARE(negativeVotes.options.at(0).voterCount, 0);
    QCOMPARE(negativeVotes.options.at(0).percentage, 0.0);
    QCOMPARE(negativeVotes.options.at(1).percentage, 100.0);

    const PollResults allNegative =
        pollResultsFromInfo(makePoll({option(-1), option(-2)}, -5, false, true));
    QCOMPARE(allNegative.totalVoterCount, 0);
    QCOMPARE(allNegative.options.at(0).percentage, 0.0);
    QCOMPARE(allNegative.options.at(1).percentage, 0.0);
}

void PollResultsTest::emptyPoll()
{
    const PollResults regular = pollResultsFromInfo(makePoll({}, 0));
    QVERIFY(regular.options.isEmpty());
    QCOMPARE(regular.totalVoterCount, 0);
    QVERIFY(regular.canVote);
    QVERIFY(!regular.closed);
    QVERIFY(regular.revealsResults);

    const PollResults quiz = pollResultsFromInfo(makePoll({}, 0, true));
    QVERIFY(quiz.options.isEmpty());
    QVERIFY(!quiz.revealsResults);
}

void PollResultsTest::optionPercentageRounds()
{
    QCOMPARE(optionPercentage(4, 10, 2), 40);
    QCOMPARE(optionPercentage(3, 4, 2), 75);
    QCOMPARE(optionPercentage(1, 3, 3), 33);
    QCOMPARE(optionPercentage(2, 3, 3), 67);
    QCOMPARE(optionPercentage(5, 8, 2), 63);  // 62.5 rounds up
    QCOMPARE(optionPercentage(1, 8, 2), 13);  // 12.5 rounds up
    QCOMPARE(optionPercentage(1, 200, 2), 1); // 0.5 rounds up
    QCOMPARE(optionPercentage(1, 400, 2), 0); // 0.25 rounds down
    QCOMPARE(optionPercentage(15, 10, 2), 100); // clamped
}

void PollResultsTest::optionPercentageRejectsUnusableInput()
{
    QCOMPARE(optionPercentage(0, 10, 2), 0);
    QCOMPARE(optionPercentage(5, 0, 2), 0);
    QCOMPARE(optionPercentage(5, -10, 2), 0);
    QCOMPARE(optionPercentage(-3, 10, 2), 0);
    QCOMPARE(optionPercentage(5, 10, 0), 0);
    QCOMPARE(optionPercentage(5, 10, -1), 0);
    QCOMPARE(optionPercentage(0, 0, 0), 0);
}

QTEST_GUILESS_MAIN(PollResultsTest)
#include "tst_poll_results.moc"
