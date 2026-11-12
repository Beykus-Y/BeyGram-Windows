#include "telegram/ChatArchiveController.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class ChatArchiveControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsClosedAndEmpty();
    void setArchiveOpenTogglesFlag();
    void setArchiveOpenEmitsOnlyOnChange();
    void setArchivedIdsKeepsOrder();
    void setArchivedIdsDropsNonPositiveIds();
    void setArchivedIdsDropsDuplicates();
    void setArchivedIdsEmitsOnlyOnChange();
    void addArchivedAppendsToTheEnd();
    void addArchivedIgnoresDuplicatesAndNonPositiveIds();
    void removeArchivedKeepsTheRestInOrder();
    void removeArchivedIgnoresUnknownIds();
    void clearEmptiesTheList();
    void clearKeepsArchiveOpen();
    void archivedIdsAreIndependentOfTheOpenFlag();
    void emitsChangedOncePerRealTransition();
};

void ChatArchiveControllerTest::startsClosedAndEmpty()
{
    const ChatArchiveController controller;

    QVERIFY(!controller.isArchiveOpen());
    QVERIFY(controller.archivedIds().isEmpty());
}

void ChatArchiveControllerTest::setArchiveOpenTogglesFlag()
{
    ChatArchiveController controller;

    controller.setArchiveOpen(true);
    QVERIFY(controller.isArchiveOpen());

    controller.setArchiveOpen(false);
    QVERIFY(!controller.isArchiveOpen());

    controller.setArchiveOpen(true);
    QVERIFY(controller.isArchiveOpen());
}

void ChatArchiveControllerTest::setArchiveOpenEmitsOnlyOnChange()
{
    ChatArchiveController controller;
    QSignalSpy spy(&controller, &ChatArchiveController::changed);

    // The initial state is closed, so asking for closed changes nothing.
    controller.setArchiveOpen(false);
    QCOMPARE(spy.count(), 0);

    controller.setArchiveOpen(true);
    QCOMPARE(spy.count(), 1);

    controller.setArchiveOpen(true);
    QCOMPARE(spy.count(), 1);

    controller.setArchiveOpen(false);
    QCOMPARE(spy.count(), 2);
}

void ChatArchiveControllerTest::setArchivedIdsKeepsOrder()
{
    ChatArchiveController controller;

    controller.setArchivedIds({90, 12, 500, 7});

    // Order is the TDLib order; the controller must not sort the list.
    QCOMPARE(controller.archivedIds(), (QVector<qint64>{90, 12, 500, 7}));
}

void ChatArchiveControllerTest::setArchivedIdsDropsNonPositiveIds()
{
    ChatArchiveController controller;

    controller.setArchivedIds({0, -1, 5, -42, 6});

    QCOMPARE(controller.archivedIds(), (QVector<qint64>{5, 6}));

    // A list made only of rejected ids leaves the archive empty.
    controller.setArchivedIds({0, -3});
    QVERIFY(controller.archivedIds().isEmpty());
}

void ChatArchiveControllerTest::setArchivedIdsDropsDuplicates()
{
    ChatArchiveController controller;

    controller.setArchivedIds({4, 9, 4, 11, 9, 4});

    // The first occurrence wins, so the surviving order is the input order.
    QCOMPARE(controller.archivedIds(), (QVector<qint64>{4, 9, 11}));
}

void ChatArchiveControllerTest::setArchivedIdsEmitsOnlyOnChange()
{
    ChatArchiveController controller;
    QSignalSpy spy(&controller, &ChatArchiveController::changed);

    controller.setArchivedIds({3, 8});
    QCOMPARE(spy.count(), 1);

    controller.setArchivedIds({3, 8});
    QCOMPARE(spy.count(), 1);

    // Duplicates and rejected ids normalise to the same list, so still silent.
    controller.setArchivedIds({3, 8, 3, 0, -1});
    QCOMPARE(spy.count(), 1);

    // A different order is a different list and must be reported.
    controller.setArchivedIds({8, 3});
    QCOMPARE(spy.count(), 2);

    // Emptying through the setter is a real change too.
    controller.setArchivedIds({});
    QCOMPARE(spy.count(), 3);

    controller.setArchivedIds({});
    QCOMPARE(spy.count(), 3);
}

void ChatArchiveControllerTest::addArchivedAppendsToTheEnd()
{
    ChatArchiveController controller;

    controller.addArchived(100);
    controller.addArchived(200);
    controller.addArchived(300);

    QCOMPARE(controller.archivedIds(), (QVector<qint64>{100, 200, 300}));
}

void ChatArchiveControllerTest::addArchivedIgnoresDuplicatesAndNonPositiveIds()
{
    ChatArchiveController controller;
    QSignalSpy spy(&controller, &ChatArchiveController::changed);

    controller.addArchived(5);
    QCOMPARE(spy.count(), 1);

    // Already archived, non-positive, or both: no change and no signal.
    controller.addArchived(5);
    controller.addArchived(0);
    controller.addArchived(-5);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.archivedIds(), (QVector<qint64>{5}));

    // A large chat id must not be truncated.
    controller.addArchived(4242424242LL);
    QCOMPARE(controller.archivedIds(), (QVector<qint64>{5, 4242424242LL}));
}

void ChatArchiveControllerTest::removeArchivedKeepsTheRestInOrder()
{
    ChatArchiveController controller;

    controller.setArchivedIds({1, 2, 3, 4});
    controller.removeArchived(2);

    QCOMPARE(controller.archivedIds(), (QVector<qint64>{1, 3, 4}));

    // Removing from the middle twice and then the last element.
    controller.removeArchived(3);
    QCOMPARE(controller.archivedIds(), (QVector<qint64>{1, 4}));
    controller.removeArchived(4);
    QCOMPARE(controller.archivedIds(), (QVector<qint64>{1}));
}

void ChatArchiveControllerTest::removeArchivedIgnoresUnknownIds()
{
    ChatArchiveController controller;
    QSignalSpy spy(&controller, &ChatArchiveController::changed);

    controller.setArchivedIds({1, 2});
    QCOMPARE(spy.count(), 1);

    controller.removeArchived(3);
    controller.removeArchived(0);
    controller.removeArchived(-2);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.archivedIds(), (QVector<qint64>{1, 2}));

    controller.removeArchived(2);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(controller.archivedIds(), (QVector<qint64>{1}));
}

void ChatArchiveControllerTest::clearEmptiesTheList()
{
    ChatArchiveController controller;

    controller.setArchivedIds({10, 20, 30});
    controller.clear();

    QVERIFY(controller.archivedIds().isEmpty());

    // Clearing an empty list is a no-op and must stay silent.
    QSignalSpy spy(&controller, &ChatArchiveController::changed);
    controller.clear();
    QCOMPARE(spy.count(), 0);
}

void ChatArchiveControllerTest::clearKeepsArchiveOpen()
{
    ChatArchiveController controller;

    controller.setArchiveOpen(true);
    controller.setArchivedIds({7, 8});
    controller.clear();

    QVERIFY(controller.isArchiveOpen());
    QVERIFY(controller.archivedIds().isEmpty());
}

void ChatArchiveControllerTest::archivedIdsAreIndependentOfTheOpenFlag()
{
    ChatArchiveController controller;

    controller.setArchivedIds({40, 50});
    controller.setArchiveOpen(true);
    QCOMPARE(controller.archivedIds(), (QVector<qint64>{40, 50}));

    // Closing the archive must not forget what it contained.
    controller.setArchiveOpen(false);
    QCOMPARE(controller.archivedIds(), (QVector<qint64>{40, 50}));
}

void ChatArchiveControllerTest::emitsChangedOncePerRealTransition()
{
    ChatArchiveController controller;
    QSignalSpy spy(&controller, &ChatArchiveController::changed);

    controller.setArchiveOpen(true);      // 1
    controller.setArchiveOpen(true);      // no-op
    controller.setArchivedIds({1, 2});    // 2
    controller.addArchived(3);            // 3
    controller.addArchived(3);            // no-op
    controller.removeArchived(1);         // 4
    controller.removeArchived(1);         // no-op
    controller.clear();                   // 5
    controller.clear();                   // no-op
    controller.setArchiveOpen(false);     // 6

    QCOMPARE(spy.count(), 6);
}

QTEST_GUILESS_MAIN(ChatArchiveControllerTest)
#include "tst_chat_archive_controller.moc"
