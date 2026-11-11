#include "telegram/MessageSelectionModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class MessageSelectionTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void toggleAddsAndRemoves();
    void toggleIsOrderIndependent();
    void selectAddsOnce();
    void clearResetsSelection();
    void selectAllReplaces();
    void invalidIdsAreIgnored();
    void signalsOnlyOnChange();
};

void MessageSelectionTest::startsEmpty()
{
    MessageSelectionModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(!model.isActive());
    QVERIFY(!model.contains(1));
    QVERIFY(model.selectedIds().isEmpty());
}

void MessageSelectionTest::toggleAddsAndRemoves()
{
    MessageSelectionModel model;

    model.toggle(10);
    QCOMPARE(model.count(), 1);
    QVERIFY(model.isActive());
    QVERIFY(model.contains(10));
    QCOMPARE(model.selectedIds(), QVector<qint64>{10});

    model.toggle(20);
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.selectedIds(), (QVector<qint64>{10, 20}));

    model.toggle(10);
    QCOMPARE(model.count(), 1);
    QVERIFY(!model.contains(10));
    QCOMPARE(model.selectedIds(), QVector<qint64>{20});

    model.toggle(20);
    QCOMPARE(model.count(), 0);
    QVERIFY(!model.isActive());
}

void MessageSelectionTest::toggleIsOrderIndependent()
{
    MessageSelectionModel first;
    first.toggle(1);
    first.toggle(2);
    first.toggle(3);

    MessageSelectionModel second;
    second.toggle(3);
    second.toggle(1);
    second.toggle(2);

    QCOMPARE(first.selectedIds(), second.selectedIds());
    QCOMPARE(first.count(), second.count());
    QVERIFY(first.contains(1));
    QVERIFY(first.contains(2));
    QVERIFY(first.contains(3));
}

void MessageSelectionTest::selectAddsOnce()
{
    MessageSelectionModel model;

    model.select(5);
    QCOMPARE(model.count(), 1);
    QVERIFY(model.contains(5));

    model.select(5);
    QCOMPARE(model.count(), 1);

    model.select(6);
    QCOMPARE(model.selectedIds(), (QVector<qint64>{5, 6}));
}

void MessageSelectionTest::clearResetsSelection()
{
    MessageSelectionModel model;
    model.selectAll({1, 2, 3});

    model.clear();
    QCOMPARE(model.count(), 0);
    QVERIFY(!model.isActive());
    QVERIFY(!model.contains(1));
    QVERIFY(model.selectedIds().isEmpty());

    // Clearing an already empty selection stays empty.
    model.clear();
    QCOMPARE(model.count(), 0);
}

void MessageSelectionTest::selectAllReplaces()
{
    MessageSelectionModel model;
    model.select(1);

    model.selectAll({7, 8, 8, 9});
    QCOMPARE(model.count(), 3);
    QCOMPARE(model.selectedIds(), (QVector<qint64>{7, 8, 9}));
    QVERIFY(!model.contains(1));
    QVERIFY(model.isActive());

    model.selectAll({4});
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.selectedIds(), QVector<qint64>{4});
    QVERIFY(!model.contains(7));

    model.selectAll({});
    QCOMPARE(model.count(), 0);
    QVERIFY(!model.isActive());
}

void MessageSelectionTest::invalidIdsAreIgnored()
{
    MessageSelectionModel model;

    model.toggle(0);
    model.toggle(-1);
    model.select(0);
    model.select(-42);
    QCOMPARE(model.count(), 0);
    QVERIFY(!model.contains(0));
    QVERIFY(!model.contains(-1));

    model.selectAll({0, -1, -2});
    QCOMPARE(model.count(), 0);
    QVERIFY(!model.isActive());

    model.select(3);
    model.selectAll({0, -1, 3});
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.selectedIds(), QVector<qint64>{3});
    QVERIFY(!model.contains(0));
    QVERIFY(!model.contains(-1));
}

void MessageSelectionTest::signalsOnlyOnChange()
{
    MessageSelectionModel model;
    QSignalSpy spy(&model, &MessageSelectionModel::selectionChanged);

    model.select(1);
    QCOMPARE(spy.count(), 1);

    model.select(1);
    QCOMPARE(spy.count(), 1);

    model.toggle(2);
    QCOMPARE(spy.count(), 2);

    model.toggle(2);
    QCOMPARE(spy.count(), 3);

    model.selectAll({1, 3});
    QCOMPARE(spy.count(), 4);

    model.selectAll({1, 3});
    QCOMPARE(spy.count(), 4);

    model.clear();
    QCOMPARE(spy.count(), 5);

    model.clear();
    QCOMPARE(spy.count(), 5);

    model.toggle(0);
    model.select(-1);
    model.selectAll({0, -5});
    QCOMPARE(spy.count(), 5);
}

QTEST_GUILESS_MAIN(MessageSelectionTest)
#include "tst_message_selection.moc"
