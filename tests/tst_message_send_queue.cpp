#include "telegram/MessageSendQueue.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QueuedMessage makeMessage(qint64 tempId, qint64 chatId, const QString& text,
                          qint64 replyTo = 0)
{
    QueuedMessage queued;
    queued.tempId = tempId;
    queued.chatId = chatId;
    queued.text = text;
    queued.replyTo = replyTo;
    return queued;
}

QVector<qint64> tempIdsOf(const QVector<QueuedMessage>& messages)
{
    QVector<qint64> ids;
    ids.reserve(messages.size());
    for (const QueuedMessage& message : messages)
        ids.append(message.tempId);
    return ids;
}

} // namespace

class MessageSendQueueTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void enqueueStoresTheMessage();
    void enqueueRejectsNonPositiveTempId();
    void enqueueRejectsNonPositiveChatId();
    void enqueueRejectsBlankText();
    void enqueueKeepsTextVerbatim();
    void enqueueRejectsDuplicateTempId();
    void enqueueRejectsTempIdWhileInFlight();
    void enqueueAllowsTempIdAgainAfterMarkSent();
    void enqueueNormalisesAttempts();
    void pendingReturnsFifoOrder();
    void pendingReturnsACopy();
    void takeNextReturnsOldestAndRemoves();
    void takeNextOnEmptyReturnsDefault();
    void markFailedRequeuesAtTheBack();
    void markFailedRequeuesAWaitingMessage();
    void markFailedBackoffGrowsAndIsCapped();
    void markFailedDropsAtMaxAttempts();
    void markFailedDropsImmediatelyAtOneAttempt();
    void markFailedDropsWhenEnqueuedAttemptsReachTheLimit();
    void markFailedIgnoresUnknownId();
    void markSentRemovesInFlightMessage();
    void markSentRemovesQueuedMessage();
    void markSentIgnoresUnknownId();
    void clearEmptiesTheQueue();
    void clearOnEmptyQueueDoesNotSignal();
    void setMaxAttemptsClampsToRange();
    void setMaxAttemptsEmitsOnlyOnChange();
    void changedFiresOnlyOnRealChanges();
};

void MessageSendQueueTest::startsEmpty()
{
    const MessageSendQueue queue;

    QCOMPARE(queue.count(), 0);
    QVERIFY(queue.pending().isEmpty());
    QCOMPARE(queue.maxAttempts(), 3);
}

void MessageSendQueueTest::enqueueStoresTheMessage()
{
    MessageSendQueue queue;
    QSignalSpy spy(&queue, &MessageSendQueue::changed);

    QVERIFY(queue.enqueue(makeMessage(11, 42, QStringLiteral("hello"), 77)));

    QCOMPARE(queue.count(), 1);
    QCOMPARE(spy.count(), 1);

    const QueuedMessage stored = queue.pending().constFirst();
    QCOMPARE(stored.tempId, qint64(11));
    QCOMPARE(stored.chatId, qint64(42));
    QCOMPARE(stored.text, QStringLiteral("hello"));
    QCOMPARE(stored.replyTo, qint64(77));
    QCOMPARE(stored.attempts, 0);
}

void MessageSendQueueTest::enqueueRejectsNonPositiveTempId()
{
    MessageSendQueue queue;
    QSignalSpy spy(&queue, &MessageSendQueue::changed);

    QVERIFY(!queue.enqueue(makeMessage(0, 42, QStringLiteral("zero"))));
    QVERIFY(!queue.enqueue(makeMessage(-1, 42, QStringLiteral("negative"))));

    QCOMPARE(queue.count(), 0);
    QCOMPARE(spy.count(), 0);
}

void MessageSendQueueTest::enqueueRejectsNonPositiveChatId()
{
    MessageSendQueue queue;
    QSignalSpy spy(&queue, &MessageSendQueue::changed);

    QVERIFY(!queue.enqueue(makeMessage(1, 0, QStringLiteral("no chat"))));
    QVERIFY(!queue.enqueue(
        makeMessage(2, -1001234567890LL, QStringLiteral("negative chat"))));

    QCOMPARE(queue.count(), 0);
    QCOMPARE(spy.count(), 0);
}

void MessageSendQueueTest::enqueueRejectsBlankText()
{
    MessageSendQueue queue;
    QSignalSpy spy(&queue, &MessageSendQueue::changed);

    QVERIFY(!queue.enqueue(makeMessage(1, 42, QString())));
    QVERIFY(!queue.enqueue(makeMessage(2, 42, QStringLiteral("   "))));
    QVERIFY(!queue.enqueue(makeMessage(3, 42, QStringLiteral("\t\n "))));

    QCOMPARE(queue.count(), 0);
    QCOMPARE(spy.count(), 0);
}

void MessageSendQueueTest::enqueueKeepsTextVerbatim()
{
    MessageSendQueue queue;
    const QString text = QStringLiteral("  padded message  ");

    QVERIFY(queue.enqueue(makeMessage(1, 42, text)));

    QCOMPARE(queue.pending().constFirst().text, text);
}

void MessageSendQueueTest::enqueueRejectsDuplicateTempId()
{
    MessageSendQueue queue;
    QVERIFY(queue.enqueue(makeMessage(1, 42, QStringLiteral("first"))));

    QSignalSpy spy(&queue, &MessageSendQueue::changed);

    QVERIFY(!queue.enqueue(makeMessage(1, 43, QStringLiteral("second"))));

    QCOMPARE(queue.count(), 1);
    QCOMPARE(spy.count(), 0);

    const QueuedMessage stored = queue.pending().constFirst();
    QCOMPARE(stored.chatId, qint64(42));
    QCOMPARE(stored.text, QStringLiteral("first"));
}

void MessageSendQueueTest::enqueueRejectsTempIdWhileInFlight()
{
    MessageSendQueue queue;
    QVERIFY(queue.enqueue(makeMessage(1, 42, QStringLiteral("sending"))));
    QCOMPARE(queue.takeNext().tempId, qint64(1));

    QSignalSpy spy(&queue, &MessageSendQueue::changed);

    QVERIFY(!queue.enqueue(makeMessage(1, 42, QStringLiteral("again"))));

    QCOMPARE(queue.count(), 0);
    QCOMPARE(spy.count(), 0);
}

void MessageSendQueueTest::enqueueAllowsTempIdAgainAfterMarkSent()
{
    MessageSendQueue queue;
    QVERIFY(queue.enqueue(makeMessage(1, 42, QStringLiteral("first"))));
    QCOMPARE(queue.takeNext().tempId, qint64(1));
    queue.markSent(1);

    QVERIFY(queue.enqueue(makeMessage(1, 42, QStringLiteral("second"))));

    QCOMPARE(queue.count(), 1);
    QCOMPARE(queue.pending().constFirst().text, QStringLiteral("second"));
}

void MessageSendQueueTest::enqueueNormalisesAttempts()
{
    MessageSendQueue queue;

    QueuedMessage negative = makeMessage(1, 42, QStringLiteral("negative"));
    negative.attempts = -5;
    QVERIFY(queue.enqueue(negative));
    QCOMPARE(queue.pending().constFirst().attempts, 0);

    QueuedMessage counted = makeMessage(2, 42, QStringLiteral("counted"));
    counted.attempts = 2;
    QVERIFY(queue.enqueue(counted));
    QCOMPARE(queue.pending().constLast().attempts, 2);

    QCOMPARE(queue.takeNext().attempts, 0);
    QCOMPARE(queue.takeNext().attempts, 2);
}

void MessageSendQueueTest::pendingReturnsFifoOrder()
{
    MessageSendQueue queue;
    QVERIFY(queue.enqueue(makeMessage(3, 10, QStringLiteral("three"))));
    QVERIFY(queue.enqueue(makeMessage(1, 20, QStringLiteral("one"))));
    QVERIFY(queue.enqueue(makeMessage(2, 30, QStringLiteral("two"))));

    const QVector<QueuedMessage> messages = queue.pending();

    QCOMPARE(tempIdsOf(messages), (QVector<qint64>{3, 1, 2}));
    QCOMPARE(messages.at(0).chatId, qint64(10));
    QCOMPARE(messages.at(1).chatId, qint64(20));
    QCOMPARE(messages.at(2).chatId, qint64(30));
}

void MessageSendQueueTest::pendingReturnsACopy()
{
    MessageSendQueue queue;
    QVERIFY(queue.enqueue(makeMessage(1, 42, QStringLiteral("one"))));
    QVERIFY(queue.enqueue(makeMessage(2, 42, QStringLiteral("two"))));

    QVector<QueuedMessage> copy = queue.pending();
    copy[0].text = QStringLiteral("mutated");
    copy.removeLast();

    QCOMPARE(queue.count(), 2);
    QCOMPARE(queue.pending().constFirst().text, QStringLiteral("one"));
    QCOMPARE(tempIdsOf(queue.pending()), (QVector<qint64>{1, 2}));
}

void MessageSendQueueTest::takeNextReturnsOldestAndRemoves()
{
    MessageSendQueue queue;
    QVERIFY(queue.enqueue(makeMessage(1, 42, QStringLiteral("one"))));
    QVERIFY(queue.enqueue(makeMessage(2, 42, QStringLiteral("two"))));
    QVERIFY(queue.enqueue(makeMessage(3, 42, QStringLiteral("three"))));

    QSignalSpy spy(&queue, &MessageSendQueue::changed);

    QCOMPARE(queue.takeNext().tempId, qint64(1));
    QCOMPARE(queue.count(), 2);
    QCOMPARE(tempIdsOf(queue.pending()), (QVector<qint64>{2, 3}));
    QCOMPARE(spy.count(), 1);

    QCOMPARE(queue.takeNext().tempId, qint64(2));
    QCOMPARE(queue.takeNext().tempId, qint64(3));
    QVERIFY(queue.pending().isEmpty());
    QCOMPARE(spy.count(), 3);
}

void MessageSendQueueTest::takeNextOnEmptyReturnsDefault()
{
    MessageSendQueue queue;
    QSignalSpy spy(&queue, &MessageSendQueue::changed);

    const QueuedMessage next = queue.takeNext();

    QCOMPARE(next.tempId, qint64(0));
    QCOMPARE(next.chatId, qint64(0));
    QVERIFY(next.text.isEmpty());
    QCOMPARE(next.replyTo, qint64(0));
    QCOMPARE(next.attempts, 0);
    QCOMPARE(spy.count(), 0);
}

void MessageSendQueueTest::markFailedRequeuesAtTheBack()
{
    MessageSendQueue queue;
    QVERIFY(queue.enqueue(makeMessage(1, 10, QStringLiteral("one"))));
    QVERIFY(queue.enqueue(makeMessage(2, 10, QStringLiteral("two"))));
    QVERIFY(queue.enqueue(makeMessage(3, 10, QStringLiteral("three"))));

    QCOMPARE(queue.takeNext().tempId, qint64(1));

    QSignalSpy changedSpy(&queue, &MessageSendQueue::changed);
    QSignalSpy retrySpy(&queue, &MessageSendQueue::retryScheduled);

    queue.markFailed(1);

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(retrySpy.count(), 1);
    QCOMPARE(tempIdsOf(queue.pending()), (QVector<qint64>{2, 3, 1}));
    QCOMPARE(queue.pending().constLast().attempts, 1);
    QCOMPARE(queue.pending().constLast().text, QStringLiteral("one"));

    QCOMPARE(retrySpy.at(0).at(0).toLongLong(), qint64(1));
    QCOMPARE(retrySpy.at(0).at(1).toInt(), 1);
    QCOMPARE(retrySpy.at(0).at(2).toInt(), 1000);
}

void MessageSendQueueTest::markFailedRequeuesAWaitingMessage()
{
    MessageSendQueue queue;
    QVERIFY(queue.enqueue(makeMessage(1, 42, QStringLiteral("one"))));
    QVERIFY(queue.enqueue(makeMessage(2, 42, QStringLiteral("two"))));

    QSignalSpy changedSpy(&queue, &MessageSendQueue::changed);
    QSignalSpy retrySpy(&queue, &MessageSendQueue::retryScheduled);

    queue.markFailed(1);

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(retrySpy.count(), 1);
    QCOMPARE(tempIdsOf(queue.pending()), (QVector<qint64>{2, 1}));
    QCOMPARE(queue.pending().constLast().attempts, 1);
}

void MessageSendQueueTest::markFailedBackoffGrowsAndIsCapped()
{
    MessageSendQueue queue;
    queue.setMaxAttempts(10);
    QVERIFY(queue.enqueue(makeMessage(7, 42, QStringLiteral("retry"))));

    QSignalSpy retrySpy(&queue, &MessageSendQueue::retryScheduled);

    const QVector<int> expectedDelays{1000, 2000, 4000, 8000, 16000,
                                      30000, 30000, 30000, 30000};
    QVector<int> attempts;
    QVector<int> delays;
    for (int i = 0; i < expectedDelays.size(); ++i) {
        QCOMPARE(queue.takeNext().tempId, qint64(7));
        queue.markFailed(7);

        QCOMPARE(retrySpy.count(), i + 1);
        attempts.append(retrySpy.at(i).at(1).toInt());
        delays.append(retrySpy.at(i).at(2).toInt());
    }

    QCOMPARE(attempts, (QVector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9}));
    QCOMPARE(delays, expectedDelays);
    QCOMPARE(queue.count(), 1);
    QCOMPARE(queue.pending().constFirst().attempts, 9);
}

void MessageSendQueueTest::markFailedDropsAtMaxAttempts()
{
    MessageSendQueue queue;
    QCOMPARE(queue.maxAttempts(), 3);
    QVERIFY(queue.enqueue(makeMessage(5, 42, QStringLiteral("drop"))));

    QSignalSpy changedSpy(&queue, &MessageSendQueue::changed);
    QSignalSpy retrySpy(&queue, &MessageSendQueue::retryScheduled);

    // The first two failures re-queue the message; the third reaches the
    // default limit of three total attempts and drops it.
    for (int attempt = 1; attempt <= 2; ++attempt) {
        QCOMPARE(queue.takeNext().tempId, qint64(5));
        queue.markFailed(5);
        QCOMPARE(queue.count(), 1);
        QCOMPARE(queue.pending().constFirst().attempts, attempt);
        QCOMPARE(retrySpy.count(), attempt);
    }

    QCOMPARE(queue.takeNext().tempId, qint64(5));
    queue.markFailed(5);

    QCOMPARE(queue.count(), 0);
    QVERIFY(queue.pending().isEmpty());
    QCOMPARE(retrySpy.count(), 2);
    QCOMPARE(changedSpy.count(), 6);
}

void MessageSendQueueTest::markFailedDropsImmediatelyAtOneAttempt()
{
    MessageSendQueue queue;
    queue.setMaxAttempts(1);
    QVERIFY(queue.enqueue(makeMessage(9, 42, QStringLiteral("once"))));

    QSignalSpy changedSpy(&queue, &MessageSendQueue::changed);
    QSignalSpy retrySpy(&queue, &MessageSendQueue::retryScheduled);

    QCOMPARE(queue.takeNext().tempId, qint64(9));
    queue.markFailed(9);

    QCOMPARE(queue.count(), 0);
    QCOMPARE(retrySpy.count(), 0);
    QCOMPARE(changedSpy.count(), 2);
}

void MessageSendQueueTest::markFailedDropsWhenEnqueuedAttemptsReachTheLimit()
{
    MessageSendQueue queue;
    QCOMPARE(queue.maxAttempts(), 3);

    QueuedMessage almost = makeMessage(1, 42, QStringLiteral("almost"));
    almost.attempts = 2;
    QVERIFY(queue.enqueue(almost));

    QSignalSpy retrySpy(&queue, &MessageSendQueue::retryScheduled);

    QCOMPARE(queue.takeNext().attempts, 2);
    queue.markFailed(1);

    QCOMPARE(queue.count(), 0);
    QCOMPARE(retrySpy.count(), 0);
}

void MessageSendQueueTest::markFailedIgnoresUnknownId()
{
    MessageSendQueue queue;
    QVERIFY(queue.enqueue(makeMessage(1, 42, QStringLiteral("kept"))));

    QSignalSpy changedSpy(&queue, &MessageSendQueue::changed);
    QSignalSpy retrySpy(&queue, &MessageSendQueue::retryScheduled);

    queue.markFailed(99);

    QCOMPARE(queue.count(), 1);
    QCOMPARE(queue.pending().constFirst().attempts, 0);
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(retrySpy.count(), 0);
}

void MessageSendQueueTest::markSentRemovesInFlightMessage()
{
    MessageSendQueue queue;
    QVERIFY(queue.enqueue(makeMessage(1, 42, QStringLiteral("one"))));
    QVERIFY(queue.enqueue(makeMessage(2, 42, QStringLiteral("two"))));
    QVERIFY(queue.enqueue(makeMessage(3, 42, QStringLiteral("three"))));

    QCOMPARE(queue.takeNext().tempId, qint64(1));

    QSignalSpy spy(&queue, &MessageSendQueue::changed);

    queue.markSent(1);

    QCOMPARE(queue.count(), 2);
    QCOMPARE(tempIdsOf(queue.pending()), (QVector<qint64>{2, 3}));
    QCOMPARE(spy.count(), 1);

    // Settling the same message twice is not another change.
    queue.markSent(1);
    QCOMPARE(spy.count(), 1);
}

void MessageSendQueueTest::markSentRemovesQueuedMessage()
{
    MessageSendQueue queue;
    QVERIFY(queue.enqueue(makeMessage(1, 42, QStringLiteral("one"))));
    QVERIFY(queue.enqueue(makeMessage(2, 42, QStringLiteral("two"))));
    QVERIFY(queue.enqueue(makeMessage(3, 42, QStringLiteral("three"))));

    QSignalSpy spy(&queue, &MessageSendQueue::changed);

    queue.markSent(2);

    QCOMPARE(queue.count(), 2);
    QCOMPARE(tempIdsOf(queue.pending()), (QVector<qint64>{1, 3}));
    QCOMPARE(spy.count(), 1);
}

void MessageSendQueueTest::markSentIgnoresUnknownId()
{
    MessageSendQueue queue;
    QVERIFY(queue.enqueue(makeMessage(1, 42, QStringLiteral("kept"))));

    QSignalSpy spy(&queue, &MessageSendQueue::changed);

    queue.markSent(2);

    QCOMPARE(queue.count(), 1);
    QCOMPARE(tempIdsOf(queue.pending()), (QVector<qint64>{1}));
    QCOMPARE(spy.count(), 0);
}

void MessageSendQueueTest::clearEmptiesTheQueue()
{
    MessageSendQueue queue;
    queue.setMaxAttempts(7);
    QVERIFY(queue.enqueue(makeMessage(1, 42, QStringLiteral("one"))));
    QVERIFY(queue.enqueue(makeMessage(2, 42, QStringLiteral("two"))));
    QCOMPARE(queue.takeNext().tempId, qint64(1));

    QSignalSpy spy(&queue, &MessageSendQueue::changed);

    queue.clear();

    QCOMPARE(queue.count(), 0);
    QVERIFY(queue.pending().isEmpty());
    QCOMPARE(spy.count(), 1);

    // The retry limit is a setting, not queued data.
    QCOMPARE(queue.maxAttempts(), 7);

    // The in-flight message was dropped too, so a late failure cannot bring it
    // back after the caller abandoned the queue.
    queue.markFailed(1);
    queue.markSent(1);
    QCOMPARE(queue.count(), 0);
    QCOMPARE(spy.count(), 1);

    // Clearing an already empty queue is not a change.
    queue.clear();
    QCOMPARE(spy.count(), 1);
}

void MessageSendQueueTest::clearOnEmptyQueueDoesNotSignal()
{
    MessageSendQueue queue;
    QSignalSpy spy(&queue, &MessageSendQueue::changed);

    queue.clear();

    QCOMPARE(spy.count(), 0);
}

void MessageSendQueueTest::setMaxAttemptsClampsToRange()
{
    MessageSendQueue queue;

    queue.setMaxAttempts(0);
    QCOMPARE(queue.maxAttempts(), 1);

    queue.setMaxAttempts(-7);
    QCOMPARE(queue.maxAttempts(), 1);

    queue.setMaxAttempts(11);
    QCOMPARE(queue.maxAttempts(), 10);

    queue.setMaxAttempts(1000);
    QCOMPARE(queue.maxAttempts(), 10);

    queue.setMaxAttempts(5);
    QCOMPARE(queue.maxAttempts(), 5);

    queue.setMaxAttempts(1);
    QCOMPARE(queue.maxAttempts(), 1);

    queue.setMaxAttempts(10);
    QCOMPARE(queue.maxAttempts(), 10);
}

void MessageSendQueueTest::setMaxAttemptsEmitsOnlyOnChange()
{
    MessageSendQueue queue;
    QSignalSpy spy(&queue, &MessageSendQueue::changed);

    queue.setMaxAttempts(3);  // default, no change
    QCOMPARE(spy.count(), 0);

    queue.setMaxAttempts(4);  // 1
    queue.setMaxAttempts(4);  // no change
    QCOMPARE(spy.count(), 1);

    queue.setMaxAttempts(0);  // clamps to 1, 2
    queue.setMaxAttempts(0);  // still 1, no change
    QCOMPARE(spy.count(), 2);

    queue.setMaxAttempts(11);  // clamps to 10, 3
    QCOMPARE(spy.count(), 3);
    QCOMPARE(queue.maxAttempts(), 10);
}

void MessageSendQueueTest::changedFiresOnlyOnRealChanges()
{
    MessageSendQueue queue;
    QSignalSpy spy(&queue, &MessageSendQueue::changed);

    // Every rejection is silent.
    QVERIFY(!queue.enqueue(makeMessage(0, 42, QStringLiteral("bad id"))));
    QVERIFY(!queue.enqueue(makeMessage(1, 0, QStringLiteral("bad chat"))));
    QVERIFY(!queue.enqueue(makeMessage(1, 42, QString())));
    QVERIFY(!queue.enqueue(makeMessage(1, 42, QStringLiteral("   "))));
    QCOMPARE(spy.count(), 0);

    QVERIFY(queue.enqueue(makeMessage(1, 42, QStringLiteral("hello"))));  // 1
    QVERIFY(!queue.enqueue(makeMessage(1, 42, QStringLiteral("dup"))));
    QCOMPARE(spy.count(), 1);

    QCOMPARE(queue.takeNext().tempId, qint64(1));  // 2
    QCOMPARE(spy.count(), 2);

    // The id is in flight, so it is still a duplicate.
    QVERIFY(!queue.enqueue(makeMessage(1, 42, QStringLiteral("in flight"))));
    QCOMPARE(spy.count(), 2);

    // Unknown ids are ignored.
    queue.markSent(99);
    queue.markFailed(99);
    QCOMPARE(spy.count(), 2);

    queue.markSent(1);  // 3
    QCOMPARE(spy.count(), 3);

    // Empty queue: neither takeNext() nor clear() changes anything.
    QCOMPARE(queue.takeNext().tempId, qint64(0));
    queue.clear();
    QCOMPARE(spy.count(), 3);
}

QTEST_GUILESS_MAIN(MessageSendQueueTest)
#include "tst_message_send_queue.moc"
