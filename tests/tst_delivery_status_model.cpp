#include "telegram/DeliveryStatusModel.h"

#include <QSignalSpy>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

class DeliveryStatusModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void glyphFollowsState();
    void ignoresNonPositiveMessageIds();
    void repeatedStateIsSilent();
    void transitionsEmitChanged();
    void chatChangeIsARealChange();
    void noneForgetsMessage();
    void countsPendingAndFailed();
    void markChatReadUpgradesMatchingChat();
    void markChatReadLeavesOtherStatesAlone();
    void markChatReadEmitsOncePerBatch();
    void markChatReadHandlesNegativeChatIds();
    void removeForgetsOnlyTheMessage();
    void removeUnknownIsSilent();
    void clearForgetsEverything();
    void keepsLargeMessageIds();
};

void DeliveryStatusModelTest::startsEmpty()
{
    const DeliveryStatusModel model;

    QCOMPARE(model.state(1), DeliveryState::None);
    QVERIFY(model.glyph(1).isEmpty());
    QCOMPARE(model.pendingCount(), 0);
    QCOMPARE(model.failedCount(), 0);
}

void DeliveryStatusModelTest::glyphFollowsState()
{
    DeliveryStatusModel model;

    model.setState(10, 1, DeliveryState::Pending);
    QCOMPARE(model.glyph(1), QStringLiteral("…"));

    model.setState(10, 1, DeliveryState::Sent);
    QCOMPARE(model.glyph(1), QStringLiteral("✓"));

    model.setState(10, 1, DeliveryState::Read);
    QCOMPARE(model.glyph(1), QStringLiteral("✓✓"));

    model.setState(10, 1, DeliveryState::Failed);
    QCOMPARE(model.glyph(1), QStringLiteral("!"));

    model.setState(10, 1, DeliveryState::None);
    QVERIFY(model.glyph(1).isEmpty());

    // An unknown message has no glyph either.
    QVERIFY(model.glyph(999).isEmpty());
}

void DeliveryStatusModelTest::ignoresNonPositiveMessageIds()
{
    DeliveryStatusModel model;
    QSignalSpy spy(&model, &DeliveryStatusModel::changed);

    model.setState(10, 0, DeliveryState::Sent);
    model.setState(10, -1, DeliveryState::Pending);
    model.setState(10, -100, DeliveryState::Failed);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.state(0), DeliveryState::None);
    QCOMPARE(model.state(-1), DeliveryState::None);
    QCOMPARE(model.pendingCount(), 0);
    QCOMPARE(model.failedCount(), 0);
    QVERIFY(model.glyph(0).isEmpty());

    // An invalid id must not disturb a message that is already tracked.
    model.setState(10, 5, DeliveryState::Sent);
    QCOMPARE(spy.count(), 1);
    model.setState(10, 0, DeliveryState::Read);
    model.setState(10, -5, DeliveryState::Failed);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.state(5), DeliveryState::Sent);
}

void DeliveryStatusModelTest::repeatedStateIsSilent()
{
    DeliveryStatusModel model;
    QSignalSpy spy(&model, &DeliveryStatusModel::changed);

    model.setState(10, 1, DeliveryState::Sent);
    QCOMPARE(spy.count(), 1);

    model.setState(10, 1, DeliveryState::Sent);
    QCOMPARE(spy.count(), 1);

    // A different message is a real change.
    model.setState(10, 2, DeliveryState::Pending);
    QCOMPARE(spy.count(), 2);
    model.setState(10, 2, DeliveryState::Pending);
    QCOMPARE(spy.count(), 2);
}

void DeliveryStatusModelTest::transitionsEmitChanged()
{
    DeliveryStatusModel model;
    QSignalSpy spy(&model, &DeliveryStatusModel::changed);

    model.setState(10, 1, DeliveryState::Pending);
    QCOMPARE(spy.count(), 1);

    model.setState(10, 1, DeliveryState::Sent);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.state(1), DeliveryState::Sent);

    model.setState(10, 1, DeliveryState::Read);
    QCOMPARE(spy.count(), 3);
    QCOMPARE(model.state(1), DeliveryState::Read);

    model.setState(10, 1, DeliveryState::Failed);
    QCOMPARE(spy.count(), 4);
    QCOMPARE(model.state(1), DeliveryState::Failed);

    // Retrying a failed message is a normal transition.
    model.setState(10, 1, DeliveryState::Pending);
    QCOMPARE(spy.count(), 5);
    QCOMPARE(model.state(1), DeliveryState::Pending);
}

void DeliveryStatusModelTest::chatChangeIsARealChange()
{
    DeliveryStatusModel model;
    QSignalSpy spy(&model, &DeliveryStatusModel::changed);

    model.setState(10, 1, DeliveryState::Sent);
    QCOMPARE(spy.count(), 1);

    // Same state, different chat: the owner changed, so readers must repaint.
    model.setState(20, 1, DeliveryState::Sent);
    QCOMPARE(spy.count(), 2);

    // The old chat no longer owns the message.
    model.markChatRead(10, {1});
    QCOMPARE(model.state(1), DeliveryState::Sent);
    QCOMPARE(spy.count(), 2);

    model.markChatRead(20, {1});
    QCOMPARE(model.state(1), DeliveryState::Read);
    QCOMPARE(spy.count(), 3);
}

void DeliveryStatusModelTest::noneForgetsMessage()
{
    DeliveryStatusModel model;
    QSignalSpy spy(&model, &DeliveryStatusModel::changed);

    model.setState(10, 1, DeliveryState::Failed);
    QCOMPARE(model.failedCount(), 1);
    QCOMPARE(spy.count(), 1);

    model.setState(10, 1, DeliveryState::None);
    QCOMPARE(model.state(1), DeliveryState::None);
    QVERIFY(model.glyph(1).isEmpty());
    QCOMPARE(model.failedCount(), 0);
    QCOMPARE(spy.count(), 2);

    // Forgetting an already absent message changes nothing.
    model.setState(10, 1, DeliveryState::None);
    QCOMPARE(spy.count(), 2);
}

void DeliveryStatusModelTest::countsPendingAndFailed()
{
    DeliveryStatusModel model;

    model.setState(1, 1, DeliveryState::Pending);
    model.setState(1, 2, DeliveryState::Pending);
    model.setState(1, 3, DeliveryState::Sent);
    model.setState(1, 4, DeliveryState::Read);
    model.setState(1, 5, DeliveryState::Failed);
    model.setState(1, 6, DeliveryState::Failed);

    QCOMPARE(model.pendingCount(), 2);
    QCOMPARE(model.failedCount(), 2);

    model.setState(1, 1, DeliveryState::Sent);
    QCOMPARE(model.pendingCount(), 1);

    // A failed message that is retried no longer counts as failed.
    model.setState(1, 5, DeliveryState::Pending);
    QCOMPARE(model.pendingCount(), 2);
    QCOMPARE(model.failedCount(), 1);

    model.setState(1, 2, DeliveryState::Read);
    QCOMPARE(model.pendingCount(), 1);

    model.remove(1);
    QCOMPARE(model.pendingCount(), 1);

    model.setState(1, 5, DeliveryState::Read);
    QCOMPARE(model.pendingCount(), 0);

    model.setState(2, 7, DeliveryState::Failed);
    model.clear();
    QCOMPARE(model.pendingCount(), 0);
    QCOMPARE(model.failedCount(), 0);
}

void DeliveryStatusModelTest::markChatReadUpgradesMatchingChat()
{
    DeliveryStatusModel model;
    model.setState(7, 1, DeliveryState::Pending);
    model.setState(7, 2, DeliveryState::Sent);
    model.setState(7, 3, DeliveryState::Read);
    model.setState(7, 4, DeliveryState::Failed);
    model.setState(8, 5, DeliveryState::Sent);
    model.setState(8, 6, DeliveryState::Pending);

    QSignalSpy spy(&model, &DeliveryStatusModel::changed);
    model.markChatRead(7, {1, 2, 3, 4, 5, 6, 0, -3, 999});

    QCOMPARE(model.state(1), DeliveryState::Read);
    QCOMPARE(model.state(2), DeliveryState::Read);
    QCOMPARE(model.glyph(1), QStringLiteral("✓✓"));

    // Already read, failed, other-chat and unknown messages stay untouched.
    QCOMPARE(model.state(3), DeliveryState::Read);
    QCOMPARE(model.state(4), DeliveryState::Failed);
    QCOMPARE(model.state(5), DeliveryState::Sent);
    QCOMPARE(model.state(6), DeliveryState::Pending);
    QCOMPARE(model.state(999), DeliveryState::None);

    QCOMPARE(spy.count(), 1);
}

void DeliveryStatusModelTest::markChatReadLeavesOtherStatesAlone()
{
    DeliveryStatusModel model;
    model.setState(7, 1, DeliveryState::Read);
    model.setState(7, 2, DeliveryState::Failed);

    QSignalSpy spy(&model, &DeliveryStatusModel::changed);
    model.markChatRead(7, {1, 2});
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.state(1), DeliveryState::Read);
    QCOMPARE(model.state(2), DeliveryState::Failed);

    // An empty batch and a chat without tracked messages are no-ops.
    model.markChatRead(7, {});
    model.markChatRead(99, {1, 2});
    model.markChatRead(99, {});
    QCOMPARE(spy.count(), 0);

    // Non-positive ids inside a batch are skipped without affecting the rest.
    model.setState(7, 3, DeliveryState::Pending);
    spy.clear();
    model.markChatRead(7, {0, -1, 3});
    QCOMPARE(model.state(3), DeliveryState::Read);
    QCOMPARE(spy.count(), 1);
}

void DeliveryStatusModelTest::markChatReadEmitsOncePerBatch()
{
    DeliveryStatusModel model;
    model.setState(7, 1, DeliveryState::Pending);
    model.setState(7, 2, DeliveryState::Sent);

    QSignalSpy spy(&model, &DeliveryStatusModel::changed);
    model.markChatRead(7, {1, 2});

    // Two upgrades in one call are one visible change.
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.state(1), DeliveryState::Read);
    QCOMPARE(model.state(2), DeliveryState::Read);

    // Repeating the same batch changes nothing.
    model.markChatRead(7, {1, 2});
    QCOMPARE(spy.count(), 1);

    // Duplicate ids in a batch still produce a single signal.
    model.setState(7, 3, DeliveryState::Sent);
    spy.clear();
    model.markChatRead(7, {3, 3, 3});
    QCOMPARE(spy.count(), 1);
}

void DeliveryStatusModelTest::markChatReadHandlesNegativeChatIds()
{
    // Supergroups and channels use negative chat ids.
    constexpr qint64 kGroup = -1001234567890LL;
    DeliveryStatusModel model;
    QSignalSpy spy(&model, &DeliveryStatusModel::changed);

    model.setState(kGroup, 1, DeliveryState::Sent);
    QCOMPARE(spy.count(), 1);

    // A neighbouring id is a different chat.
    model.markChatRead(kGroup + 1, {1});
    QCOMPARE(model.state(1), DeliveryState::Sent);
    QCOMPARE(spy.count(), 1);

    model.markChatRead(kGroup, {1});
    QCOMPARE(model.state(1), DeliveryState::Read);
    QCOMPARE(spy.count(), 2);
}

void DeliveryStatusModelTest::removeForgetsOnlyTheMessage()
{
    DeliveryStatusModel model;
    model.setState(1, 1, DeliveryState::Sent);
    model.setState(1, 2, DeliveryState::Failed);

    QSignalSpy spy(&model, &DeliveryStatusModel::changed);
    model.remove(1);

    QCOMPARE(model.state(1), DeliveryState::None);
    QVERIFY(model.glyph(1).isEmpty());
    QCOMPARE(model.state(2), DeliveryState::Failed);
    QCOMPARE(spy.count(), 1);

    model.remove(1);
    QCOMPARE(spy.count(), 1);
}

void DeliveryStatusModelTest::removeUnknownIsSilent()
{
    DeliveryStatusModel model;
    QSignalSpy spy(&model, &DeliveryStatusModel::changed);

    model.remove(999);
    model.remove(0);
    model.remove(-42);
    QCOMPARE(spy.count(), 0);

    model.setState(1, 5, DeliveryState::Sent);
    QCOMPARE(spy.count(), 1);

    // Removing an unknown id must not disturb the tracked message.
    model.remove(4);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.state(5), DeliveryState::Sent);
}

void DeliveryStatusModelTest::clearForgetsEverything()
{
    DeliveryStatusModel model;
    QSignalSpy spy(&model, &DeliveryStatusModel::changed);

    // Already empty: nothing changed, nothing emitted.
    model.clear();
    QCOMPARE(spy.count(), 0);

    model.setState(1, 1, DeliveryState::Sent);
    model.setState(2, 2, DeliveryState::Pending);
    model.setState(3, 3, DeliveryState::Failed);
    spy.clear();

    model.clear();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.state(1), DeliveryState::None);
    QCOMPARE(model.state(2), DeliveryState::None);
    QCOMPARE(model.state(3), DeliveryState::None);
    QCOMPARE(model.pendingCount(), 0);
    QCOMPARE(model.failedCount(), 0);
    QVERIFY(model.glyph(2).isEmpty());

    model.clear();
    QCOMPARE(spy.count(), 1);
}

void DeliveryStatusModelTest::keepsLargeMessageIds()
{
    constexpr qint64 kMaxId = std::numeric_limits<qint64>::max();
    constexpr qint64 kChat = std::numeric_limits<qint64>::max() - 1;

    DeliveryStatusModel model;
    model.setState(kChat, kMaxId, DeliveryState::Sent);

    QCOMPARE(model.state(kMaxId), DeliveryState::Sent);
    QCOMPARE(model.glyph(kMaxId), QStringLiteral("✓"));

    model.markChatRead(kChat, {kMaxId});
    QCOMPARE(model.state(kMaxId), DeliveryState::Read);

    model.remove(kMaxId);
    QCOMPARE(model.state(kMaxId), DeliveryState::None);
    QCOMPARE(model.glyph(kMaxId), QString());
}

QTEST_GUILESS_MAIN(DeliveryStatusModelTest)
#include "tst_delivery_status_model.moc"
