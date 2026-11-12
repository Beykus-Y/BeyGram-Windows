#include "telegram/PollInfo.h"
#include "telegram/PollVoteModel.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

/// The option_ids a vote would carry, as the request actually serializes them.
QVector<int> optionIdsOf(const QJsonObject& request)
{
    QVector<int> ids;
    const QJsonArray array = request.value(QStringLiteral("option_ids")).toArray();
    ids.reserve(array.size());
    for (const QJsonValue& value : array)
        ids.append(value.toInt());
    return ids;
}

} // namespace

class PollVoteModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsInactive();
    void setPollStoresState();
    void setPollRejectsInvalidArguments();
    void setPollReplacesPreviousPoll();
    void setPollResetsSelection();
    void setPollIgnoresIdenticalState();
    void toggleIgnoresOutOfRange();
    void toggleIgnoresEveryIndexWithoutPoll();
    void toggleSingleChoiceReplacesSelection();
    void toggleSingleChoiceDeselectsChosenOption();
    void toggleMultipleAddsAndRemoves();
    void toggleMultipleKeepsSelectionSorted();
    void toggleMultipleTwiceReturnsToEmpty();
    void clearSelectionDropsSelection();
    void clearSelectionIsNoOpWhenEmpty();
    void resetClearsEverything();
    void resetIsNoOpWhenAlreadyReset();
    void changedFiresOnlyOnRealChanges();
    void selectedFeedsVotePollRequest();
};

void PollVoteModelTest::startsInactive()
{
    PollVoteModel model;

    QVERIFY(!model.active());
    QCOMPARE(model.chatId(), qint64(0));
    QCOMPARE(model.messageId(), qint64(0));
    QCOMPARE(model.optionCount(), 0);
    QVERIFY(!model.multiple());
    QVERIFY(!model.hasSelection());
    QVERIFY(model.selected().isEmpty());
}

void PollVoteModelTest::setPollStoresState()
{
    PollVoteModel model;
    model.setPoll(42, 7, 3, true);

    QVERIFY(model.active());
    QCOMPARE(model.chatId(), qint64(42));
    QCOMPARE(model.messageId(), qint64(7));
    QCOMPARE(model.optionCount(), 3);
    QVERIFY(model.multiple());
    QVERIFY(!model.hasSelection());
}

void PollVoteModelTest::setPollRejectsInvalidArguments()
{
    PollVoteModel model;
    QSignalSpy spy(&model, &PollVoteModel::changed);

    model.setPoll(42, 7, 3, false);
    QCOMPARE(spy.count(), 1);

    // No chat id, no message id, no answers: each is an unparsed poll and must
    // leave the valid one in place.
    model.setPoll(0, 7, 3, true);
    model.setPoll(-1, 7, 3, true);
    model.setPoll(42, 0, 3, true);
    model.setPoll(42, -7, 3, true);
    model.setPoll(42, 7, 0, true);
    model.setPoll(42, 7, -3, true);

    QCOMPARE(spy.count(), 1);
    QVERIFY(model.active());
    QCOMPARE(model.chatId(), qint64(42));
    QCOMPARE(model.messageId(), qint64(7));
    QCOMPARE(model.optionCount(), 3);
    QVERIFY(!model.multiple());

    // A rejected call on a fresh model does not activate it either.
    PollVoteModel empty;
    QSignalSpy emptySpy(&empty, &PollVoteModel::changed);
    empty.setPoll(0, 0, 0, false);
    QVERIFY(!empty.active());
    QCOMPARE(emptySpy.count(), 0);
}

void PollVoteModelTest::setPollReplacesPreviousPoll()
{
    PollVoteModel model;
    model.setPoll(42, 7, 3, false);
    model.toggle(2);

    QSignalSpy spy(&model, &PollVoteModel::changed);
    model.setPoll(99, 8, 5, true);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.chatId(), qint64(99));
    QCOMPARE(model.messageId(), qint64(8));
    QCOMPARE(model.optionCount(), 5);
    QVERIFY(model.multiple());
    QVERIFY(!model.hasSelection());
}

void PollVoteModelTest::setPollResetsSelection()
{
    PollVoteModel model;
    model.setPoll(42, 7, 3, true);
    model.toggle(0);
    model.toggle(2);
    QCOMPARE(model.selected(), (QVector<int>{0, 2}));

    // The same poll may come back with a different vote, so its answer is a new
    // starting point rather than a reason to keep the old draft.
    model.setPoll(42, 7, 3, true);
    QVERIFY(!model.hasSelection());

    // Switching the mode of the same poll also drops the draft: the old
    // selection may be illegal under the new rules.
    model.toggle(1);
    model.setPoll(42, 7, 3, false);
    QVERIFY(!model.hasSelection());
    QVERIFY(!model.multiple());
}

void PollVoteModelTest::setPollIgnoresIdenticalState()
{
    PollVoteModel model;
    model.setPoll(42, 7, 3, true);
    QSignalSpy spy(&model, &PollVoteModel::changed);

    model.setPoll(42, 7, 3, true);

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.active());
    QCOMPARE(model.optionCount(), 3);
}

void PollVoteModelTest::toggleIgnoresOutOfRange()
{
    PollVoteModel model;
    model.setPoll(42, 7, 3, true);
    QSignalSpy spy(&model, &PollVoteModel::changed);

    QVERIFY(!model.toggle(-1));
    QVERIFY(!model.toggle(3));
    QVERIFY(!model.toggle(1000));

    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.hasSelection());
}

void PollVoteModelTest::toggleIgnoresEveryIndexWithoutPoll()
{
    PollVoteModel model;
    QSignalSpy spy(&model, &PollVoteModel::changed);

    // Without a poll m_count is 0, so even index 0 names no answer.
    QVERIFY(!model.toggle(0));
    QVERIFY(!model.toggle(1));

    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.hasSelection());

    // reset() drops the poll but keeps the model usable, not the selection.
    model.setPoll(42, 7, 3, false);
    model.toggle(1);
    model.reset();
    QVERIFY(!model.toggle(1));
    QVERIFY(!model.hasSelection());
}

void PollVoteModelTest::toggleSingleChoiceReplacesSelection()
{
    PollVoteModel model;
    model.setPoll(42, 7, 3, false);

    QVERIFY(model.toggle(2));
    QCOMPARE(model.selected(), (QVector<int>{2}));

    QVERIFY(model.toggle(0));
    QCOMPARE(model.selected(), (QVector<int>{0}));
    QCOMPARE(model.selected().size(), 1);

    QVERIFY(model.toggle(1));
    QCOMPARE(model.selected(), (QVector<int>{1}));
}

void PollVoteModelTest::toggleSingleChoiceDeselectsChosenOption()
{
    PollVoteModel model;
    model.setPoll(42, 7, 3, false);
    model.toggle(1);

    // Tapping the chosen answer again clears the draft; clearSelection() is not
    // the only way back to no answer.
    QVERIFY(model.toggle(1));
    QVERIFY(!model.hasSelection());

    QVERIFY(model.toggle(1));
    QCOMPARE(model.selected(), (QVector<int>{1}));
}

void PollVoteModelTest::toggleMultipleAddsAndRemoves()
{
    PollVoteModel model;
    model.setPoll(42, 7, 4, true);

    QVERIFY(model.toggle(2));
    QCOMPARE(model.selected(), (QVector<int>{2}));

    QVERIFY(model.toggle(0));
    QCOMPARE(model.selected(), (QVector<int>{0, 2}));

    QVERIFY(model.toggle(2));
    QCOMPARE(model.selected(), (QVector<int>{0}));

    QVERIFY(model.toggle(0));
    QVERIFY(!model.hasSelection());
}

void PollVoteModelTest::toggleMultipleKeepsSelectionSorted()
{
    PollVoteModel model;
    model.setPoll(42, 7, 5, true);

    model.toggle(4);
    model.toggle(1);
    model.toggle(3);
    model.toggle(0);

    QCOMPARE(model.selected(), (QVector<int>{0, 1, 3, 4}));

    model.toggle(1);
    QCOMPARE(model.selected(), (QVector<int>{0, 3, 4}));

    model.toggle(2);
    QCOMPARE(model.selected(), (QVector<int>{0, 2, 3, 4}));
}

void PollVoteModelTest::toggleMultipleTwiceReturnsToEmpty()
{
    PollVoteModel model;
    model.setPoll(42, 7, 3, true);
    QSignalSpy spy(&model, &PollVoteModel::changed);

    QVERIFY(model.toggle(1));
    QVERIFY(model.toggle(1));

    QCOMPARE(spy.count(), 2);
    QVERIFY(!model.hasSelection());
    QVERIFY(model.selected().isEmpty());
}

void PollVoteModelTest::clearSelectionDropsSelection()
{
    PollVoteModel model;
    model.setPoll(42, 7, 4, true);
    model.toggle(3);
    model.toggle(1);
    QSignalSpy spy(&model, &PollVoteModel::changed);

    model.clearSelection();

    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.hasSelection());
    QVERIFY(model.selected().isEmpty());
    // Only the draft is cleared; the poll itself stays active.
    QVERIFY(model.active());
    QCOMPARE(model.chatId(), qint64(42));
    QCOMPARE(model.messageId(), qint64(7));
    QCOMPARE(model.optionCount(), 4);
    QVERIFY(model.multiple());
}

void PollVoteModelTest::clearSelectionIsNoOpWhenEmpty()
{
    PollVoteModel model;
    model.setPoll(42, 7, 4, true);
    QSignalSpy spy(&model, &PollVoteModel::changed);

    model.clearSelection();
    QCOMPARE(spy.count(), 0);

    // Also silent before any poll is set.
    PollVoteModel empty;
    QSignalSpy emptySpy(&empty, &PollVoteModel::changed);
    empty.clearSelection();
    QCOMPARE(emptySpy.count(), 0);
}

void PollVoteModelTest::resetClearsEverything()
{
    PollVoteModel model;
    model.setPoll(42, 7, 4, true);
    model.toggle(0);
    QSignalSpy spy(&model, &PollVoteModel::changed);

    model.reset();

    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.active());
    QCOMPARE(model.chatId(), qint64(0));
    QCOMPARE(model.messageId(), qint64(0));
    QCOMPARE(model.optionCount(), 0);
    QVERIFY(!model.multiple());
    QVERIFY(!model.hasSelection());
    QVERIFY(model.selected().isEmpty());
}

void PollVoteModelTest::resetIsNoOpWhenAlreadyReset()
{
    PollVoteModel model;
    QSignalSpy spy(&model, &PollVoteModel::changed);

    model.reset();
    QCOMPARE(spy.count(), 0);

    // A poll without a selection is still something to forget.
    model.setPoll(42, 7, 3, false);
    QCOMPARE(spy.count(), 1);
    model.reset();
    QCOMPARE(spy.count(), 2);
    model.reset();
    QCOMPARE(spy.count(), 2);
}

void PollVoteModelTest::changedFiresOnlyOnRealChanges()
{
    PollVoteModel model;
    QSignalSpy spy(&model, &PollVoteModel::changed);

    // Silent: nothing to change.
    model.clearSelection();
    model.reset();
    QCOMPARE(spy.count(), 0);

    // One signal per accepted setPoll().
    model.setPoll(42, 7, 3, true);
    QCOMPARE(spy.count(), 1);
    model.setPoll(42, 7, 3, true);
    QCOMPARE(spy.count(), 1);
    model.setPoll(42, 7, 3, false);
    QCOMPARE(spy.count(), 2);

    // Rejected input is silent.
    model.setPoll(0, 7, 3, true);
    model.setPoll(42, 0, 3, true);
    model.setPoll(42, 7, 0, true);
    QCOMPARE(spy.count(), 2);

    // Every real toggle, then a no-op toggle of the same index in a
    // single-choice poll clears it instead of repeating the old selection.
    model.toggle(2);
    QCOMPARE(spy.count(), 3);
    model.toggle(-1);
    model.toggle(3);
    QCOMPARE(spy.count(), 3);
    model.toggle(2);
    QCOMPARE(spy.count(), 4);

    model.clearSelection();
    QCOMPARE(spy.count(), 4);

    model.toggle(0);
    QCOMPARE(spy.count(), 5);
    model.clearSelection();
    QCOMPARE(spy.count(), 6);
    model.clearSelection();
    QCOMPARE(spy.count(), 6);

    model.reset();
    QCOMPARE(spy.count(), 7);
    model.reset();
    QCOMPARE(spy.count(), 7);
}

void PollVoteModelTest::selectedFeedsVotePollRequest()
{
    // The stored indices are exactly the option_ids votePollRequest() sends, so
    // no renumbering happens between the model and the wire.
    // 2^53 keeps the id exact through the double-backed JSON value.
    const qint64 chatId = 9007199254740992LL;
    PollVoteModel model;
    model.setPoll(chatId, 42, 4, true);
    model.toggle(3);
    model.toggle(1);

    const QJsonObject request =
        votePollRequest(model.chatId(), model.messageId(), model.selected());

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setPollAnswer"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), chatId);
    QCOMPARE(request.value(QStringLiteral("message_id")).toVariant().toLongLong(), qint64(42));
    QCOMPARE(optionIdsOf(request), (QVector<int>{1, 3}));

    // An empty draft still builds a valid request, which is how a vote is
    // retracted: setPollAnswer with no option_ids.
    model.clearSelection();
    QVERIFY(optionIdsOf(votePollRequest(model.chatId(), model.messageId(),
                                        model.selected()))
                .isEmpty());
}

QTEST_GUILESS_MAIN(PollVoteModelTest)
#include "tst_poll_vote_model.moc"
