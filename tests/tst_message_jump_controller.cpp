#include "telegram/MessageJumpController.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class MessageJumpControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsInactive();
    void beginActivatesJump();
    void beginHighlightsTarget();
    void beginReplacesExistingJump();
    void ignoresInvalidIds();
    void setHighlightMovesHighlight();
    void setHighlightRequiresActiveJump();
    void setHighlightIgnoresInvalidIds();
    void finishClearsJump();
    void cancelClearsJump();
    void emitsChangedOnlyOnRealChanges();
};

void MessageJumpControllerTest::startsInactive()
{
    const MessageJumpController controller;

    QVERIFY(!controller.active());
    QCOMPARE(controller.chatId(), qint64(0));
    QCOMPARE(controller.targetId(), qint64(0));
    QCOMPARE(controller.highlightId(), qint64(0));
}

void MessageJumpControllerTest::beginActivatesJump()
{
    MessageJumpController controller;

    controller.begin(42, 700);

    QVERIFY(controller.active());
    QCOMPARE(controller.chatId(), qint64(42));
    QCOMPARE(controller.targetId(), qint64(700));
}

void MessageJumpControllerTest::beginHighlightsTarget()
{
    MessageJumpController controller;

    controller.begin(42, 700);
    QCOMPARE(controller.highlightId(), qint64(700));

    // A fresh jump starts with the new target highlighted, not the old one.
    controller.begin(43, 800);
    QCOMPARE(controller.highlightId(), qint64(800));
}

void MessageJumpControllerTest::beginReplacesExistingJump()
{
    MessageJumpController controller;

    controller.begin(1, 10);
    controller.setHighlight(11);
    controller.begin(2, 20);

    QVERIFY(controller.active());
    QCOMPARE(controller.chatId(), qint64(2));
    QCOMPARE(controller.targetId(), qint64(20));
    QCOMPARE(controller.highlightId(), qint64(20));
}

void MessageJumpControllerTest::ignoresInvalidIds()
{
    MessageJumpController controller;

    controller.begin(0, 10);
    controller.begin(-1, 10);
    controller.begin(1, 0);
    controller.begin(1, -10);
    controller.begin(0, 0);

    QVERIFY(!controller.active());
    QCOMPARE(controller.chatId(), qint64(0));
    QCOMPARE(controller.targetId(), qint64(0));
    QCOMPARE(controller.highlightId(), qint64(0));

    // An invalid begin must not disturb a jump that is already in flight.
    controller.begin(5, 50);
    controller.begin(0, 60);
    controller.begin(6, -1);
    QVERIFY(controller.active());
    QCOMPARE(controller.chatId(), qint64(5));
    QCOMPARE(controller.targetId(), qint64(50));
    QCOMPARE(controller.highlightId(), qint64(50));
}

void MessageJumpControllerTest::setHighlightMovesHighlight()
{
    MessageJumpController controller;

    controller.begin(3, 30);
    controller.setHighlight(31);
    QCOMPARE(controller.highlightId(), qint64(31));
    QCOMPARE(controller.targetId(), qint64(30));

    controller.setHighlight(32);
    QCOMPARE(controller.highlightId(), qint64(32));
    QCOMPARE(controller.chatId(), qint64(3));
}

void MessageJumpControllerTest::setHighlightRequiresActiveJump()
{
    MessageJumpController controller;

    controller.setHighlight(99);
    QCOMPARE(controller.highlightId(), qint64(0));

    controller.begin(3, 30);
    controller.finish();
    controller.setHighlight(99);
    QCOMPARE(controller.highlightId(), qint64(0));
    QVERIFY(!controller.active());
}

void MessageJumpControllerTest::setHighlightIgnoresInvalidIds()
{
    MessageJumpController controller;

    controller.begin(3, 30);
    controller.setHighlight(0);
    controller.setHighlight(-7);
    QCOMPARE(controller.highlightId(), qint64(30));
}

void MessageJumpControllerTest::finishClearsJump()
{
    MessageJumpController controller;

    controller.begin(3, 30);
    controller.setHighlight(31);
    controller.finish();

    QVERIFY(!controller.active());
    QCOMPARE(controller.chatId(), qint64(0));
    QCOMPARE(controller.targetId(), qint64(0));
    QCOMPARE(controller.highlightId(), qint64(0));

    // A new jump can start after finishing.
    controller.begin(4, 40);
    QVERIFY(controller.active());
    QCOMPARE(controller.targetId(), qint64(40));
    QCOMPARE(controller.highlightId(), qint64(40));
}

void MessageJumpControllerTest::cancelClearsJump()
{
    MessageJumpController controller;

    controller.begin(3, 30);
    controller.setHighlight(31);
    controller.cancel();

    QVERIFY(!controller.active());
    QCOMPARE(controller.chatId(), qint64(0));
    QCOMPARE(controller.targetId(), qint64(0));
    QCOMPARE(controller.highlightId(), qint64(0));
}

void MessageJumpControllerTest::emitsChangedOnlyOnRealChanges()
{
    MessageJumpController controller;
    QSignalSpy spy(&controller, &MessageJumpController::changed);

    controller.begin(3, 30);
    QCOMPARE(spy.count(), 1);

    // Repeating the same jump changes nothing.
    controller.begin(3, 30);
    QCOMPARE(spy.count(), 1);

    controller.setHighlight(31);
    QCOMPARE(spy.count(), 2);

    // Setting the same highlight again is a no-op.
    controller.setHighlight(31);
    QCOMPARE(spy.count(), 2);

    // Invalid input stays silent.
    controller.setHighlight(0);
    controller.begin(-1, 30);
    QCOMPARE(spy.count(), 2);

    controller.begin(4, 40);
    QCOMPARE(spy.count(), 3);

    controller.finish();
    QCOMPARE(spy.count(), 4);

    // Finishing or cancelling an inactive jump is a no-op.
    controller.finish();
    controller.cancel();
    QCOMPARE(spy.count(), 4);

    controller.begin(5, 50);
    QCOMPARE(spy.count(), 5);
    controller.cancel();
    QCOMPARE(spy.count(), 6);
}

QTEST_GUILESS_MAIN(MessageJumpControllerTest)
#include "tst_message_jump_controller.moc"
