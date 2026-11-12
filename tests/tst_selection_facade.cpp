#include "ui/SelectionFacade.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::ui;

class SelectionFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsInactiveAndEmpty();

    void toggleAddsAndRemoves();
    void selectIsIdempotent();
    void ignoresInvalidIds();
    void clearDropsSelection();
    void idsAreSortedAscending();
    void changedOnlyOnRealSelectionChanges();

    void forwardStartsClosed();
    void beginForwardWithoutSelectionDoesNothing();
    void beginForwardOpensPicker();
    void beginForwardStartsUnfiltered();
    void forwardFilterRoundTrips();
    void setForwardChatsDoesNotEnableForward();
    void chooseForwardChatEmitsRequest();
    void chooseForwardChatIgnoresUnknownChat();
    void chooseForwardChatWithoutSessionDoesNothing();
    void canForwardRequiresOpenPickerAndTarget();
    void cancelForwardClosesPicker();
    void clearClosesForwardPicker();

    void jumpStartsInactive();
    void beginJumpSetsHighlightAndEmits();
    void beginJumpReplacesPreviousJump();
    void beginJumpIgnoresInvalidIds();
    void finishJumpClearsHighlight();
};

void SelectionFacadeTest::startsInactiveAndEmpty()
{
    const SelectionFacade facade;

    QVERIFY(!facade.active());
    QCOMPARE(facade.count(), 0);
    QVERIFY(facade.ids().isEmpty());
    QVERIFY(!facade.forwardOpen());
    QVERIFY(facade.forwardFilter().isEmpty());
    QVERIFY(!facade.canForward());
    QVERIFY(facade.highlightId().isEmpty());
}

void SelectionFacadeTest::toggleAddsAndRemoves()
{
    SelectionFacade facade;

    facade.toggle(QStringLiteral("3"));
    QVERIFY(facade.active());
    QCOMPARE(facade.count(), 1);
    QCOMPARE(facade.ids(), QStringList{QStringLiteral("3")});

    facade.toggle(QStringLiteral("3"));
    QVERIFY(!facade.active());
    QCOMPARE(facade.count(), 0);
    QVERIFY(facade.ids().isEmpty());
}

void SelectionFacadeTest::selectIsIdempotent()
{
    SelectionFacade facade;

    facade.select(QStringLiteral("4"));
    facade.select(QStringLiteral("4"));

    QCOMPARE(facade.count(), 1);
    QCOMPARE(facade.ids(), QStringList{QStringLiteral("4")});
}

void SelectionFacadeTest::ignoresInvalidIds()
{
    SelectionFacade facade;

    facade.toggle(QString());
    facade.toggle(QStringLiteral("0"));
    facade.toggle(QStringLiteral("-8"));
    facade.toggle(QStringLiteral("abc"));
    facade.select(QStringLiteral("1.5"));
    facade.select(QStringLiteral("-1"));

    QVERIFY(!facade.active());
    QCOMPARE(facade.count(), 0);
    QVERIFY(facade.ids().isEmpty());
}

void SelectionFacadeTest::clearDropsSelection()
{
    SelectionFacade facade;

    facade.select(QStringLiteral("1"));
    facade.select(QStringLiteral("2"));
    QCOMPARE(facade.count(), 2);

    facade.clear();

    QVERIFY(!facade.active());
    QCOMPARE(facade.count(), 0);
    QVERIFY(facade.ids().isEmpty());
}

void SelectionFacadeTest::idsAreSortedAscending()
{
    SelectionFacade facade;

    facade.select(QStringLiteral("10"));
    facade.select(QStringLiteral("2"));
    facade.select(QStringLiteral("33"));

    QCOMPARE(facade.ids(), (QStringList{QStringLiteral("2"),
                                        QStringLiteral("10"),
                                        QStringLiteral("33")}));
}

void SelectionFacadeTest::changedOnlyOnRealSelectionChanges()
{
    SelectionFacade facade;
    QSignalSpy spy(&facade, &SelectionFacade::changed);

    facade.select(QStringLiteral("5"));
    QCOMPARE(spy.count(), 1);

    // Selecting an already selected message is a no-op.
    facade.select(QStringLiteral("5"));
    QCOMPARE(spy.count(), 1);

    facade.toggle(QStringLiteral("5"));
    QCOMPARE(spy.count(), 2);

    // Clearing an empty selection is a no-op.
    facade.clear();
    QCOMPARE(spy.count(), 2);
}

void SelectionFacadeTest::forwardStartsClosed()
{
    SelectionFacade facade;

    facade.select(QStringLiteral("1"));

    QVERIFY(!facade.forwardOpen());
    QVERIFY(!facade.canForward());
    QVERIFY(facade.forwardFilter().isEmpty());
}

void SelectionFacadeTest::beginForwardWithoutSelectionDoesNothing()
{
    SelectionFacade facade;
    QSignalSpy spy(&facade, &SelectionFacade::changed);

    facade.beginForward();

    QVERIFY(!facade.forwardOpen());
    QVERIFY(!facade.canForward());
    QCOMPARE(spy.count(), 0);
}

void SelectionFacadeTest::beginForwardOpensPicker()
{
    SelectionFacade facade;

    facade.select(QStringLiteral("7"));
    facade.select(QStringLiteral("8"));
    facade.beginForward();

    QVERIFY(facade.forwardOpen());
    QVERIFY(!facade.canForward());
    QCOMPARE(facade.count(), 2);
}

void SelectionFacadeTest::beginForwardStartsUnfiltered()
{
    SelectionFacade facade;

    facade.select(QStringLiteral("7"));
    facade.beginForward();
    facade.setForwardFilter(QStringLiteral("chat"));
    QCOMPARE(facade.forwardFilter(), QStringLiteral("chat"));

    facade.cancelForward();
    facade.beginForward();

    QVERIFY(facade.forwardFilter().isEmpty());
}

void SelectionFacadeTest::forwardFilterRoundTrips()
{
    SelectionFacade facade;

    facade.select(QStringLiteral("7"));
    facade.beginForward();

    QSignalSpy spy(&facade, &SelectionFacade::changed);
    facade.setForwardFilter(QStringLiteral("ann"));

    QCOMPARE(facade.forwardFilter(), QStringLiteral("ann"));
    QCOMPARE(spy.count(), 1);

    // Re-applying the same text is not a change.
    facade.setForwardFilter(QStringLiteral("ann"));
    QCOMPARE(spy.count(), 1);
}

void SelectionFacadeTest::setForwardChatsDoesNotEnableForward()
{
    SelectionFacade facade;

    facade.select(QStringLiteral("1"));
    facade.beginForward();
    facade.setForwardChats({QStringLiteral("5"), QStringLiteral("6")});

    QVERIFY(facade.forwardOpen());
    QVERIFY(!facade.canForward());
}

void SelectionFacadeTest::chooseForwardChatEmitsRequest()
{
    SelectionFacade facade;

    facade.select(QStringLiteral("11"));
    facade.select(QStringLiteral("12"));
    facade.beginForward();
    facade.setForwardChats({QStringLiteral("5"), QStringLiteral("6")});

    QSignalSpy spy(&facade, &SelectionFacade::forwardRequested);
    facade.chooseForwardChat(QStringLiteral("6"));

    QCOMPARE(spy.count(), 1);
    const QList<QVariant> args = spy.at(0);
    QCOMPARE(args.at(0).toString(), QStringLiteral("6"));
    QCOMPARE(args.at(1).toStringList(),
             (QStringList{QStringLiteral("11"), QStringLiteral("12")}));
    QVERIFY(facade.canForward());
}

void SelectionFacadeTest::chooseForwardChatIgnoresUnknownChat()
{
    SelectionFacade facade;

    facade.select(QStringLiteral("1"));
    facade.beginForward();
    facade.setForwardChats({QStringLiteral("5")});

    QSignalSpy spy(&facade, &SelectionFacade::forwardRequested);
    facade.chooseForwardChat(QStringLiteral("99"));
    facade.chooseForwardChat(QStringLiteral("0"));
    facade.chooseForwardChat(QString());

    QCOMPARE(spy.count(), 0);
    QVERIFY(!facade.canForward());
}

void SelectionFacadeTest::chooseForwardChatWithoutSessionDoesNothing()
{
    SelectionFacade facade;

    facade.select(QStringLiteral("1"));
    facade.setForwardChats({QStringLiteral("5")});

    QSignalSpy spy(&facade, &SelectionFacade::forwardRequested);
    facade.chooseForwardChat(QStringLiteral("5"));

    QCOMPARE(spy.count(), 0);
    QVERIFY(!facade.forwardOpen());
}

void SelectionFacadeTest::canForwardRequiresOpenPickerAndTarget()
{
    SelectionFacade facade;

    facade.select(QStringLiteral("1"));
    QVERIFY(!facade.canForward());

    facade.beginForward();
    QVERIFY(facade.forwardOpen());
    QVERIFY(!facade.canForward());

    facade.setForwardChats({QStringLiteral("5")});
    QVERIFY(!facade.canForward());

    facade.chooseForwardChat(QStringLiteral("5"));
    QVERIFY(facade.canForward());

    facade.cancelForward();
    QVERIFY(!facade.canForward());
}

void SelectionFacadeTest::cancelForwardClosesPicker()
{
    SelectionFacade facade;

    facade.select(QStringLiteral("1"));
    facade.beginForward();
    facade.setForwardChats({QStringLiteral("5")});
    facade.chooseForwardChat(QStringLiteral("5"));
    QVERIFY(facade.forwardOpen());

    facade.cancelForward();

    QVERIFY(!facade.forwardOpen());
    QVERIFY(!facade.canForward());
    QVERIFY(facade.forwardFilter().isEmpty());
    // The message selection itself survives closing the picker.
    QCOMPARE(facade.count(), 1);
}

void SelectionFacadeTest::clearClosesForwardPicker()
{
    SelectionFacade facade;

    facade.select(QStringLiteral("1"));
    facade.beginForward();
    facade.setForwardChats({QStringLiteral("5")});
    facade.chooseForwardChat(QStringLiteral("5"));
    QVERIFY(facade.forwardOpen());

    facade.clear();

    QVERIFY(!facade.forwardOpen());
    QVERIFY(!facade.canForward());
    QCOMPARE(facade.count(), 0);
}

void SelectionFacadeTest::jumpStartsInactive()
{
    const SelectionFacade facade;

    QVERIFY(facade.highlightId().isEmpty());
}

void SelectionFacadeTest::beginJumpSetsHighlightAndEmits()
{
    SelectionFacade facade;

    QSignalSpy spy(&facade, &SelectionFacade::jumpRequested);
    facade.beginJump(QStringLiteral("42"), QStringLiteral("700"));

    QCOMPARE(spy.count(), 1);
    const QList<QVariant> args = spy.at(0);
    QCOMPARE(args.at(0).toString(), QStringLiteral("42"));
    QCOMPARE(args.at(1).toString(), QStringLiteral("700"));
    QCOMPARE(facade.highlightId(), QStringLiteral("700"));
}

void SelectionFacadeTest::beginJumpReplacesPreviousJump()
{
    SelectionFacade facade;

    facade.beginJump(QStringLiteral("42"), QStringLiteral("700"));
    QCOMPARE(facade.highlightId(), QStringLiteral("700"));

    facade.beginJump(QStringLiteral("42"), QStringLiteral("800"));

    QCOMPARE(facade.highlightId(), QStringLiteral("800"));
}

void SelectionFacadeTest::beginJumpIgnoresInvalidIds()
{
    SelectionFacade facade;

    QSignalSpy spy(&facade, &SelectionFacade::jumpRequested);

    facade.beginJump(QStringLiteral("0"), QStringLiteral("700"));
    facade.beginJump(QStringLiteral("42"), QStringLiteral("-1"));
    facade.beginJump(QString(), QStringLiteral("700"));
    facade.beginJump(QStringLiteral("42"), QStringLiteral("abc"));

    QCOMPARE(spy.count(), 0);
    QVERIFY(facade.highlightId().isEmpty());
}

void SelectionFacadeTest::finishJumpClearsHighlight()
{
    SelectionFacade facade;

    facade.beginJump(QStringLiteral("42"), QStringLiteral("700"));
    QSignalSpy spy(&facade, &SelectionFacade::changed);

    facade.finishJump();

    QVERIFY(facade.highlightId().isEmpty());
    QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(SelectionFacadeTest)

#include "tst_selection_facade.moc"
