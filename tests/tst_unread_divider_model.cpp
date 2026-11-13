#include "telegram/UnreadDividerModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class UnreadDividerModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsInDefaultState();
    void setFirstUnreadStoresPosition();
    void setFirstUnreadMovesInEitherDirection();
    void setFirstUnreadIgnoresNonPositiveIds();
    void shouldShowBeforeRequiresVisibility();
    void shouldShowBeforeMatchesOnlyTheDividerId();
    void setVisibleEmitsOnlyOnRealChange();
    void markReadConsumesDivider();
    void markReadIgnoresOtherIds();
    void markReadWithoutDividerIsNoOp();
    void markReadIsIdempotent();
    void markReadWorksWhileHidden();
    void clearResetsPositionAndVisibility();
    void clearOnDefaultStateIsSilent();
    void clearAllowsReuse();
    void changedOnlyOnRealChanges();
};

void UnreadDividerModelTest::startsInDefaultState()
{
    const UnreadDividerModel model;

    QCOMPARE(model.firstUnread(), qint64(0));
    QVERIFY(model.visible());
    QVERIFY(!model.shouldShowBefore(0));
    QVERIFY(!model.shouldShowBefore(1));
    QVERIFY(!model.shouldShowBefore(-1));
}

void UnreadDividerModelTest::setFirstUnreadStoresPosition()
{
    UnreadDividerModel model;

    model.setFirstUnread(10);

    QCOMPARE(model.firstUnread(), qint64(10));
    QVERIFY(model.shouldShowBefore(10));
    QVERIFY(!model.shouldShowBefore(9));
    QVERIFY(!model.shouldShowBefore(11));
}

void UnreadDividerModelTest::setFirstUnreadMovesInEitherDirection()
{
    UnreadDividerModel model;

    model.setFirstUnread(10);
    model.setFirstUnread(4);
    QCOMPARE(model.firstUnread(), qint64(4));
    QVERIFY(!model.shouldShowBefore(10));
    QVERIFY(model.shouldShowBefore(4));

    model.setFirstUnread(25);
    QCOMPARE(model.firstUnread(), qint64(25));
    QVERIFY(!model.shouldShowBefore(4));
    QVERIFY(model.shouldShowBefore(25));
}

void UnreadDividerModelTest::setFirstUnreadIgnoresNonPositiveIds()
{
    UnreadDividerModel model;
    QSignalSpy spy(&model, &UnreadDividerModel::changed);

    model.setFirstUnread(0);
    model.setFirstUnread(-1);
    model.setFirstUnread(-999);

    QCOMPARE(model.firstUnread(), qint64(0));
    QCOMPARE(spy.count(), 0);

    // A non-positive id must not remove a divider that already exists.
    model.setFirstUnread(5);
    QCOMPARE(spy.count(), 1);

    model.setFirstUnread(0);
    model.setFirstUnread(-3);
    QCOMPARE(model.firstUnread(), qint64(5));
    QVERIFY(model.shouldShowBefore(5));
    QCOMPARE(spy.count(), 1);
}

void UnreadDividerModelTest::shouldShowBeforeRequiresVisibility()
{
    UnreadDividerModel model;
    model.setFirstUnread(7);

    QVERIFY(model.shouldShowBefore(7));

    model.setVisible(false);
    QVERIFY(!model.shouldShowBefore(7));
    QVERIFY(!model.shouldShowBefore(6));
    QVERIFY(!model.shouldShowBefore(8));

    // Hiding keeps the position, so showing again restores the divider.
    QCOMPARE(model.firstUnread(), qint64(7));

    model.setVisible(true);
    QVERIFY(model.shouldShowBefore(7));
}

void UnreadDividerModelTest::shouldShowBeforeMatchesOnlyTheDividerId()
{
    UnreadDividerModel model;
    model.setFirstUnread(42);

    QVERIFY(!model.shouldShowBefore(41));
    QVERIFY(model.shouldShowBefore(42));
    QVERIFY(!model.shouldShowBefore(43));

    // Non-positive ids never match, even if the divider were at zero.
    QVERIFY(!model.shouldShowBefore(0));
    QVERIFY(!model.shouldShowBefore(-42));
}

void UnreadDividerModelTest::setVisibleEmitsOnlyOnRealChange()
{
    UnreadDividerModel model;
    QSignalSpy spy(&model, &UnreadDividerModel::changed);

    // Visible is already the default.
    model.setVisible(true);
    QCOMPARE(spy.count(), 0);

    model.setVisible(false);
    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.visible());

    model.setVisible(false);
    QCOMPARE(spy.count(), 1);

    model.setVisible(true);
    QCOMPARE(spy.count(), 2);
    QVERIFY(model.visible());
}

void UnreadDividerModelTest::markReadConsumesDivider()
{
    UnreadDividerModel model;
    model.setFirstUnread(12);
    QSignalSpy spy(&model, &UnreadDividerModel::changed);

    model.markRead(12);

    QCOMPARE(model.firstUnread(), qint64(0));
    QVERIFY(model.visible());
    QVERIFY(!model.shouldShowBefore(12));
    QCOMPARE(spy.count(), 1);
}

void UnreadDividerModelTest::markReadIgnoresOtherIds()
{
    UnreadDividerModel model;
    model.setFirstUnread(12);
    QSignalSpy spy(&model, &UnreadDividerModel::changed);

    // Ids before the divider were already read; ids after it belong to a
    // later setFirstUnread() from TDLib. Neither may move the divider.
    model.markRead(11);
    model.markRead(13);
    model.markRead(0);
    model.markRead(-1);

    QCOMPARE(model.firstUnread(), qint64(12));
    QVERIFY(model.shouldShowBefore(12));
    QCOMPARE(spy.count(), 0);
}

void UnreadDividerModelTest::markReadWithoutDividerIsNoOp()
{
    UnreadDividerModel model;
    QSignalSpy spy(&model, &UnreadDividerModel::changed);

    model.markRead(1);
    model.markRead(0);
    model.markRead(-5);

    QCOMPARE(model.firstUnread(), qint64(0));
    QVERIFY(model.visible());
    QCOMPARE(spy.count(), 0);
}

void UnreadDividerModelTest::markReadIsIdempotent()
{
    UnreadDividerModel model;
    model.setFirstUnread(8);
    QSignalSpy spy(&model, &UnreadDividerModel::changed);

    model.markRead(8);
    QCOMPARE(spy.count(), 1);

    // The divider is gone, so marking the same id read again changes nothing.
    model.markRead(8);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.firstUnread(), qint64(0));
}

void UnreadDividerModelTest::markReadWorksWhileHidden()
{
    UnreadDividerModel model;
    model.setFirstUnread(3);
    model.setVisible(false);

    model.markRead(3);

    // Consuming the divider restores the default state, including visibility.
    QCOMPARE(model.firstUnread(), qint64(0));
    QVERIFY(model.visible());
    QVERIFY(!model.shouldShowBefore(3));
}

void UnreadDividerModelTest::clearResetsPositionAndVisibility()
{
    UnreadDividerModel model;
    model.setFirstUnread(9);
    model.setVisible(false);
    QSignalSpy spy(&model, &UnreadDividerModel::changed);

    model.clear();

    QCOMPARE(model.firstUnread(), qint64(0));
    QVERIFY(model.visible());
    QVERIFY(!model.shouldShowBefore(9));
    QCOMPARE(spy.count(), 1);
}

void UnreadDividerModelTest::clearOnDefaultStateIsSilent()
{
    UnreadDividerModel model;
    QSignalSpy spy(&model, &UnreadDividerModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);

    model.setFirstUnread(2);
    model.clear();
    QCOMPARE(spy.count(), 2);

    // Already back at the default state: clearing again is silent.
    model.clear();
    QCOMPARE(spy.count(), 2);
}

void UnreadDividerModelTest::clearAllowsReuse()
{
    UnreadDividerModel model;
    model.setFirstUnread(4);
    model.clear();

    model.setFirstUnread(20);

    QCOMPARE(model.firstUnread(), qint64(20));
    QVERIFY(model.shouldShowBefore(20));
    QVERIFY(!model.shouldShowBefore(4));
}

void UnreadDividerModelTest::changedOnlyOnRealChanges()
{
    UnreadDividerModel model;
    QSignalSpy spy(&model, &UnreadDividerModel::changed);

    model.setFirstUnread(5);
    QCOMPARE(spy.count(), 1);

    // Re-applying the same id or a non-positive id changes nothing.
    model.setFirstUnread(5);
    model.setFirstUnread(0);
    model.setFirstUnread(-2);
    QCOMPARE(spy.count(), 1);

    model.setFirstUnread(6);
    QCOMPARE(spy.count(), 2);

    // Visibility toggles count only when the value really changes.
    model.setVisible(true);
    QCOMPARE(spy.count(), 2);
    model.setVisible(false);
    QCOMPARE(spy.count(), 3);
    model.setVisible(false);
    QCOMPARE(spy.count(), 3);

    // Reading an unrelated id is silent.
    model.markRead(7);
    QCOMPARE(spy.count(), 3);

    // Reading the divider itself clears it and restores visibility.
    model.markRead(6);
    QCOMPARE(spy.count(), 4);
    QCOMPARE(model.firstUnread(), qint64(0));
    QVERIFY(model.visible());

    // Nothing left to clear.
    model.clear();
    QCOMPARE(spy.count(), 4);
}

QTEST_GUILESS_MAIN(UnreadDividerModelTest)
#include "tst_unread_divider_model.moc"
