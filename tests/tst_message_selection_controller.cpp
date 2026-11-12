#include "telegram/MessageSelectionController.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class MessageSelectionControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void toggleAddsAndRemoves();
    void keepsIdsSorted();
    void toggleIsOrderIndependent();
    void selectAddsOnce();
    void clearResetsSelection();
    void ignoresInvalidIds();
    void idsForForwardIsSnapshot();
    void signalsOnlyOnChange();
};

void MessageSelectionControllerTest::startsEmpty()
{
    const MessageSelectionController controller;

    QCOMPARE(controller.count(), 0);
    QVERIFY(!controller.active());
    QVERIFY(controller.ids().isEmpty());
    QVERIFY(controller.idsForForward().isEmpty());
}

void MessageSelectionControllerTest::toggleAddsAndRemoves()
{
    MessageSelectionController controller;

    controller.toggle(10);
    QCOMPARE(controller.count(), 1);
    QVERIFY(controller.active());
    QCOMPARE(controller.ids(), QVector<qint64>{10});

    controller.toggle(20);
    QCOMPARE(controller.count(), 2);
    QCOMPARE(controller.ids(), (QVector<qint64>{10, 20}));

    controller.toggle(10);
    QCOMPARE(controller.count(), 1);
    QCOMPARE(controller.ids(), QVector<qint64>{20});

    controller.toggle(20);
    QCOMPARE(controller.count(), 0);
    QVERIFY(!controller.active());
    QVERIFY(controller.ids().isEmpty());
}

void MessageSelectionControllerTest::keepsIdsSorted()
{
    MessageSelectionController controller;

    controller.toggle(30);
    controller.toggle(5);
    controller.toggle(17);
    controller.toggle(1);

    QCOMPARE(controller.ids(), (QVector<qint64>{1, 5, 17, 30}));
    QCOMPARE(controller.idsForForward(), (QVector<qint64>{1, 5, 17, 30}));

    // Removing from the middle keeps the remaining ids ordered.
    controller.toggle(17);
    QCOMPARE(controller.ids(), (QVector<qint64>{1, 5, 30}));
}

void MessageSelectionControllerTest::toggleIsOrderIndependent()
{
    MessageSelectionController first;
    first.toggle(1);
    first.toggle(2);
    first.toggle(3);

    MessageSelectionController second;
    second.toggle(3);
    second.toggle(1);
    second.toggle(2);

    QCOMPARE(first.ids(), second.ids());
    QCOMPARE(first.count(), second.count());
    QVERIFY(first.active());
    QVERIFY(second.active());
}

void MessageSelectionControllerTest::selectAddsOnce()
{
    MessageSelectionController controller;

    controller.select(5);
    QCOMPARE(controller.count(), 1);
    QCOMPARE(controller.ids(), QVector<qint64>{5});

    // Selecting an already selected id neither duplicates nor reorders.
    controller.select(5);
    QCOMPARE(controller.count(), 1);
    QCOMPARE(controller.ids(), QVector<qint64>{5});

    controller.select(2);
    QCOMPARE(controller.ids(), (QVector<qint64>{2, 5}));
}

void MessageSelectionControllerTest::clearResetsSelection()
{
    MessageSelectionController controller;
    controller.toggle(4);
    controller.toggle(8);

    controller.clear();
    QCOMPARE(controller.count(), 0);
    QVERIFY(!controller.active());
    QVERIFY(controller.ids().isEmpty());
    QVERIFY(controller.idsForForward().isEmpty());

    // Clearing an already empty selection stays empty.
    controller.clear();
    QCOMPARE(controller.count(), 0);
    QVERIFY(!controller.active());
}

void MessageSelectionControllerTest::ignoresInvalidIds()
{
    MessageSelectionController controller;

    controller.toggle(0);
    controller.toggle(-1);
    controller.select(0);
    controller.select(-42);
    QCOMPARE(controller.count(), 0);
    QVERIFY(!controller.active());

    controller.select(3);
    controller.toggle(0);
    controller.toggle(-7);
    controller.select(-7);
    QCOMPARE(controller.count(), 1);
    QCOMPARE(controller.ids(), QVector<qint64>{3});
}

void MessageSelectionControllerTest::idsForForwardIsSnapshot()
{
    MessageSelectionController controller;
    controller.toggle(9);
    controller.toggle(2);

    const QVector<qint64> snapshot = controller.idsForForward();
    QCOMPARE(snapshot, (QVector<qint64>{2, 9}));

    controller.toggle(9);
    controller.select(11);

    // The snapshot taken earlier must not follow later selection changes.
    QCOMPARE(snapshot, (QVector<qint64>{2, 9}));
    QCOMPARE(controller.idsForForward(), (QVector<qint64>{2, 11}));
}

void MessageSelectionControllerTest::signalsOnlyOnChange()
{
    MessageSelectionController controller;
    QSignalSpy spy(&controller, &MessageSelectionController::changed);

    controller.select(1);
    QCOMPARE(spy.count(), 1);

    controller.select(1);
    QCOMPARE(spy.count(), 1);

    controller.toggle(2);
    QCOMPARE(spy.count(), 2);

    controller.toggle(2);
    QCOMPARE(spy.count(), 3);

    controller.toggle(0);
    controller.select(-1);
    QCOMPARE(spy.count(), 3);

    controller.clear();
    QCOMPARE(spy.count(), 4);

    controller.clear();
    QCOMPARE(spy.count(), 4);
}

QTEST_GUILESS_MAIN(MessageSelectionControllerTest)
#include "tst_message_selection_controller.moc"
