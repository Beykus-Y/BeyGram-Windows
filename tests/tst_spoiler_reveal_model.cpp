#include "telegram/SpoilerRevealModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class SpoilerRevealModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void revealAddsMessages();
    void ignoresNonPositiveIds();
    void revealIgnoresDuplicates();
    void duplicateRevealDoesNotRefreshEvictionOrder();
    void hideRemovesOneMessage();
    void hideUnknownIdIsSilent();
    void hideAllClearsEverything();
    void hideAllOnEmptyIsNoOp();
    void trimsOldestAtCap();
    void defaultCapIsTwoHundred();
    void setMaxClampsRange();
    void setMaxTrimsOldest();
    void setMaxEmitsOnlyOnRealChange();
    void revealAfterHideWorksAgain();
    void changedOnlyOnRealChanges();
};

void SpoilerRevealModelTest::startsEmpty()
{
    const SpoilerRevealModel model;

    QCOMPARE(model.revealedCount(), 0);
    QCOMPARE(model.maxRevealed(), 200);
    QVERIFY(!model.isRevealed(1));
    QVERIFY(!model.isRevealed(0));
    QVERIFY(!model.isRevealed(-1));
}

void SpoilerRevealModelTest::revealAddsMessages()
{
    SpoilerRevealModel model;

    model.reveal(11);
    QVERIFY(model.isRevealed(11));
    QCOMPARE(model.revealedCount(), 1);

    model.reveal(12);
    QVERIFY(model.isRevealed(11));
    QVERIFY(model.isRevealed(12));
    QCOMPARE(model.revealedCount(), 2);

    // Neighbouring ids are independent.
    QVERIFY(!model.isRevealed(10));
    QVERIFY(!model.isRevealed(13));
}

void SpoilerRevealModelTest::ignoresNonPositiveIds()
{
    SpoilerRevealModel model;
    QSignalSpy spy(&model, &SpoilerRevealModel::changed);

    model.reveal(0);
    model.reveal(-1);
    model.reveal(-999);

    QCOMPARE(model.revealedCount(), 0);
    QVERIFY(!model.isRevealed(0));
    QVERIFY(!model.isRevealed(-1));
    QCOMPARE(spy.count(), 0);
}

void SpoilerRevealModelTest::revealIgnoresDuplicates()
{
    SpoilerRevealModel model;
    QSignalSpy spy(&model, &SpoilerRevealModel::changed);

    model.reveal(42);
    QCOMPARE(spy.count(), 1);

    model.reveal(42);
    model.reveal(42);
    QCOMPARE(model.revealedCount(), 1);
    QVERIFY(model.isRevealed(42));
    QCOMPARE(spy.count(), 1);
}

void SpoilerRevealModelTest::duplicateRevealDoesNotRefreshEvictionOrder()
{
    SpoilerRevealModel model;
    model.setMaxRevealed(2);

    model.reveal(1);
    model.reveal(2);
    // Re-revealing the oldest entry must not make it the newest one.
    model.reveal(1);

    model.reveal(3);

    QCOMPARE(model.revealedCount(), 2);
    QVERIFY(!model.isRevealed(1));
    QVERIFY(model.isRevealed(2));
    QVERIFY(model.isRevealed(3));
}

void SpoilerRevealModelTest::hideRemovesOneMessage()
{
    SpoilerRevealModel model;

    model.reveal(5);
    model.reveal(6);

    model.hide(5);
    QVERIFY(!model.isRevealed(5));
    QVERIFY(model.isRevealed(6));
    QCOMPARE(model.revealedCount(), 1);
}

void SpoilerRevealModelTest::hideUnknownIdIsSilent()
{
    SpoilerRevealModel model;
    model.reveal(1);
    QSignalSpy spy(&model, &SpoilerRevealModel::changed);

    model.hide(2);
    model.hide(0);
    model.hide(-7);

    QCOMPARE(model.revealedCount(), 1);
    QVERIFY(model.isRevealed(1));
    QCOMPARE(spy.count(), 0);
}

void SpoilerRevealModelTest::hideAllClearsEverything()
{
    SpoilerRevealModel model;

    model.reveal(1);
    model.reveal(2);
    model.reveal(3);

    model.hideAll();

    QCOMPARE(model.revealedCount(), 0);
    QVERIFY(!model.isRevealed(1));
    QVERIFY(!model.isRevealed(2));
    QVERIFY(!model.isRevealed(3));

    // The model accepts new reveals after clearing.
    model.reveal(4);
    QVERIFY(model.isRevealed(4));
    QCOMPARE(model.revealedCount(), 1);
}

void SpoilerRevealModelTest::hideAllOnEmptyIsNoOp()
{
    SpoilerRevealModel model;
    QSignalSpy spy(&model, &SpoilerRevealModel::changed);

    model.hideAll();
    QCOMPARE(spy.count(), 0);

    model.reveal(9);
    model.hideAll();
    QCOMPARE(spy.count(), 2);

    // Already empty again: still silent.
    model.hideAll();
    QCOMPARE(spy.count(), 2);
}

void SpoilerRevealModelTest::trimsOldestAtCap()
{
    SpoilerRevealModel model;
    model.setMaxRevealed(3);

    model.reveal(10);
    model.reveal(20);
    model.reveal(30);
    QCOMPARE(model.revealedCount(), 3);

    model.reveal(40);
    QCOMPARE(model.revealedCount(), 3);
    QVERIFY(!model.isRevealed(10));
    QVERIFY(model.isRevealed(20));
    QVERIFY(model.isRevealed(30));
    QVERIFY(model.isRevealed(40));

    model.reveal(50);
    QCOMPARE(model.revealedCount(), 3);
    QVERIFY(!model.isRevealed(20));
    QVERIFY(model.isRevealed(30));
    QVERIFY(model.isRevealed(40));
    QVERIFY(model.isRevealed(50));
}

void SpoilerRevealModelTest::defaultCapIsTwoHundred()
{
    SpoilerRevealModel model;
    QCOMPARE(model.maxRevealed(), 200);

    for (qint64 id = 1; id <= 201; ++id)
        model.reveal(id);

    QCOMPARE(model.revealedCount(), 200);
    QVERIFY(!model.isRevealed(1));
    QVERIFY(model.isRevealed(2));
    QVERIFY(model.isRevealed(201));
}

void SpoilerRevealModelTest::setMaxClampsRange()
{
    SpoilerRevealModel model;

    model.setMaxRevealed(0);
    QCOMPARE(model.maxRevealed(), 1);

    model.setMaxRevealed(-50);
    QCOMPARE(model.maxRevealed(), 1);

    model.setMaxRevealed(10000);
    QCOMPARE(model.maxRevealed(), 10000);

    model.setMaxRevealed(10001);
    QCOMPARE(model.maxRevealed(), 10000);

    model.setMaxRevealed(250);
    QCOMPARE(model.maxRevealed(), 250);
}

void SpoilerRevealModelTest::setMaxTrimsOldest()
{
    SpoilerRevealModel model;
    model.setMaxRevealed(5);

    for (qint64 id = 1; id <= 5; ++id)
        model.reveal(id);
    QCOMPARE(model.revealedCount(), 5);

    model.setMaxRevealed(2);

    QCOMPARE(model.revealedCount(), 2);
    QVERIFY(!model.isRevealed(3));
    QVERIFY(model.isRevealed(4));
    QVERIFY(model.isRevealed(5));

    // Raising the cap again does not resurrect trimmed reveals.
    model.setMaxRevealed(5);
    QCOMPARE(model.revealedCount(), 2);
    QVERIFY(!model.isRevealed(1));
    QVERIFY(model.isRevealed(4));
    QVERIFY(model.isRevealed(5));
}

void SpoilerRevealModelTest::setMaxEmitsOnlyOnRealChange()
{
    SpoilerRevealModel model;
    QSignalSpy spy(&model, &SpoilerRevealModel::changed);

    // Same as the default: no signal.
    model.setMaxRevealed(200);
    QCOMPARE(spy.count(), 0);

    model.setMaxRevealed(300);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.maxRevealed(), 300);

    model.setMaxRevealed(300);
    QCOMPARE(spy.count(), 1);

    // 50000 clamps to 10000, which differs from 300 and therefore emits.
    model.setMaxRevealed(50000);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.maxRevealed(), 10000);
}

void SpoilerRevealModelTest::revealAfterHideWorksAgain()
{
    SpoilerRevealModel model;
    QSignalSpy spy(&model, &SpoilerRevealModel::changed);

    model.reveal(7);
    QCOMPARE(spy.count(), 1);

    model.hide(7);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!model.isRevealed(7));
    QCOMPARE(model.revealedCount(), 0);

    model.reveal(7);
    QCOMPARE(spy.count(), 3);
    QVERIFY(model.isRevealed(7));
    QCOMPARE(model.revealedCount(), 1);
}

void SpoilerRevealModelTest::changedOnlyOnRealChanges()
{
    SpoilerRevealModel model;
    QSignalSpy spy(&model, &SpoilerRevealModel::changed);

    model.reveal(5);
    QCOMPARE(spy.count(), 1);

    // Duplicate and non-positive reveals change nothing.
    model.reveal(5);
    model.reveal(0);
    QCOMPARE(spy.count(), 1);

    model.hide(5);
    QCOMPARE(spy.count(), 2);
    // Hiding again is a no-op.
    model.hide(5);
    QCOMPARE(spy.count(), 2);

    // Empty hideAll is silent.
    model.hideAll();
    QCOMPARE(spy.count(), 2);

    model.reveal(1);
    QCOMPARE(spy.count(), 3);

    model.hideAll();
    QCOMPARE(spy.count(), 4);
}

QTEST_GUILESS_MAIN(SpoilerRevealModelTest)
#include "tst_spoiler_reveal_model.moc"
