#include "ui/ShortcutsFacade.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::ui;

namespace {

/// The table the application ships, used as the baseline of most tests. It has
/// one row per interesting case: a plain binding, a second binding, and a
/// description that shares no word with its action.
void loadShippedTable(ShortcutsFacade& facade)
{
    facade.setShortcuts(
        {QStringLiteral("message.send"), QStringLiteral("message.reply"),
         QStringLiteral("chat.search")},
        {QStringLiteral("Ctrl+Enter"), QStringLiteral("Ctrl+R"), QStringLiteral("Ctrl+F")},
        {QStringLiteral("Send message"), QStringLiteral("Reply to message"),
         QStringLiteral("Search chats")});
}

/// A table that differs from loadShippedTable() in both actions and bindings,
/// i.e. what a saved user configuration would look like.
void loadUserTable(ShortcutsFacade& facade)
{
    facade.setShortcuts(
        {QStringLiteral("message.send"), QStringLiteral("message.forward")},
        {QStringLiteral("Ctrl+J"), QStringLiteral("Ctrl+Shift+F")},
        {QStringLiteral("Send message"), QStringLiteral("Forward message")});
}

} // namespace

class ShortcutsFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();

    void loadSetsParallelLists();
    void loadNormalisesRows();
    void loadPadsMissingEntries();
    void loadKeepsQuery();
    void reloadingSameTableIsSilent();
    void loadEmitsChangedOnce();

    void filterByAction();
    void filterByDescription();
    void filterBySequence();
    void filterIsCaseInsensitive();
    void filterWithoutMatchEmptiesLists();
    void clearingQueryRestoresAllRows();
    void queryRoundTripsAndNotifiesOnlyOnChange();

    void rebindChangesSequence();
    void rebindTrimsSequence();
    void rebindSameSequenceIsNoOp();
    void rebindDuplicateSequenceIsRefused();
    void rebindEmptySequenceIsRefused();
    void rebindOutOfRangeIndexIsIgnored();
    void rebindUsesVisibleIndexWhenFiltered();
    void rebindMapsFilteredIndexToItsAction();
    void rebindKeepsRowOrder();

    void resetRestoresShippedTable();
    void resetAfterRebindRestoresSequence();
    void resetWithoutDefaultsIsSilent();
    void resetWhenUnmodifiedIsSilent();
    void modifiedTracksRealDifferences();

    void triggerEmitsForKnownAction();
    void triggerEmitsForUnboundAction();
    void triggerIgnoresUnknownAndEmptyAction();
    void triggerEmitsOnEveryCall();
};

void ShortcutsFacadeTest::startsEmpty()
{
    const ShortcutsFacade facade;

    QVERIFY(facade.actions().isEmpty());
    QVERIFY(facade.sequences().isEmpty());
    QVERIFY(facade.query().isEmpty());
    QVERIFY(!facade.modified());
}

void ShortcutsFacadeTest::loadSetsParallelLists()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    QCOMPARE(facade.actions(),
             (QStringList{QStringLiteral("message.send"), QStringLiteral("message.reply"),
                          QStringLiteral("chat.search")}));
    QCOMPARE(facade.sequences(),
             (QStringList{QStringLiteral("Ctrl+Enter"), QStringLiteral("Ctrl+R"),
                          QStringLiteral("Ctrl+F")}));
    QCOMPARE(facade.sequences().size(), facade.actions().size());

    // The shipped table is the default, so loading it is not a modification.
    QVERIFY(!facade.modified());
}

void ShortcutsFacadeTest::loadNormalisesRows()
{
    ShortcutsFacade facade;
    facade.setShortcuts(
        {QString(), QStringLiteral("message.send"), QStringLiteral("message.send"),
         QStringLiteral("chat.search")},
        {QStringLiteral("Ctrl+0"), QStringLiteral("  Ctrl+Enter  "),
         QStringLiteral("Ctrl+R"), QStringLiteral("Ctrl+F")},
        {QStringLiteral("Broken"), QStringLiteral("Send message"),
         QStringLiteral("Duplicate"), QStringLiteral("Search chats")});

    // Empty actions are dropped, the first row of a repeated action wins and
    // sequences arrive trimmed; the facade does not repeat those rules.
    QCOMPARE(facade.actions(),
             (QStringList{QStringLiteral("message.send"), QStringLiteral("chat.search")}));
    QCOMPARE(facade.sequences(),
             (QStringList{QStringLiteral("Ctrl+Enter"), QStringLiteral("Ctrl+F")}));
}

void ShortcutsFacadeTest::loadPadsMissingEntries()
{
    ShortcutsFacade facade;
    facade.setShortcuts({QStringLiteral("message.send"), QStringLiteral("chat.search")},
                        {QStringLiteral("Ctrl+Enter")}, {});

    QCOMPARE(facade.actions(),
             (QStringList{QStringLiteral("message.send"), QStringLiteral("chat.search")}));
    QCOMPARE(facade.sequences(),
             (QStringList{QStringLiteral("Ctrl+Enter"), QString()}));
}

void ShortcutsFacadeTest::loadKeepsQuery()
{
    ShortcutsFacade facade;
    facade.setQuery(QStringLiteral("reply"));

    loadShippedTable(facade);

    QCOMPARE(facade.query(), QStringLiteral("reply"));
    QCOMPARE(facade.actions(), QStringList{QStringLiteral("message.reply")});
    QCOMPARE(facade.sequences(), QStringList{QStringLiteral("Ctrl+R")});
}

void ShortcutsFacadeTest::reloadingSameTableIsSilent()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    QSignalSpy spy(&facade, &ShortcutsFacade::changed);
    loadShippedTable(facade);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.actions().size(), 3);
}

void ShortcutsFacadeTest::loadEmitsChangedOnce()
{
    ShortcutsFacade facade;
    QSignalSpy spy(&facade, &ShortcutsFacade::changed);

    loadShippedTable(facade);

    QCOMPARE(spy.count(), 1);
}

void ShortcutsFacadeTest::filterByAction()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    facade.setQuery(QStringLiteral("message."));

    QCOMPARE(facade.actions(),
             (QStringList{QStringLiteral("message.send"), QStringLiteral("message.reply")}));
    QCOMPARE(facade.sequences(),
             (QStringList{QStringLiteral("Ctrl+Enter"), QStringLiteral("Ctrl+R")}));
}

void ShortcutsFacadeTest::filterByDescription()
{
    ShortcutsFacade facade;
    facade.setShortcuts({QStringLiteral("message.forward")},
                        {QStringLiteral("Ctrl+Shift+F")},
                        {QStringLiteral("Share elsewhere")});

    // "elsewhere" appears only in the description, so this proves the facade
    // hands the descriptions to the model instead of dropping them.
    facade.setQuery(QStringLiteral("elsewhere"));

    QCOMPARE(facade.actions(), QStringList{QStringLiteral("message.forward")});
    QCOMPARE(facade.sequences(), QStringList{QStringLiteral("Ctrl+Shift+F")});
}

void ShortcutsFacadeTest::filterBySequence()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    facade.setQuery(QStringLiteral("ctrl+enter"));

    QCOMPARE(facade.actions(), QStringList{QStringLiteral("message.send")});
    QCOMPARE(facade.sequences(), QStringList{QStringLiteral("Ctrl+Enter")});
}

void ShortcutsFacadeTest::filterIsCaseInsensitive()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    facade.setQuery(QStringLiteral("CTRL+R"));

    QCOMPARE(facade.actions(), QStringList{QStringLiteral("message.reply")});
}

void ShortcutsFacadeTest::filterWithoutMatchEmptiesLists()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    facade.setQuery(QStringLiteral("no such shortcut"));

    QVERIFY(facade.actions().isEmpty());
    QVERIFY(facade.sequences().isEmpty());

    // Filtering hides rows, it does not modify them.
    QVERIFY(!facade.modified());
}

void ShortcutsFacadeTest::clearingQueryRestoresAllRows()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    facade.setQuery(QStringLiteral("reply"));
    QCOMPARE(facade.actions().size(), 1);

    facade.setQuery(QString());
    QCOMPARE(facade.actions().size(), 3);
    QCOMPARE(facade.sequences(),
             (QStringList{QStringLiteral("Ctrl+Enter"), QStringLiteral("Ctrl+R"),
                          QStringLiteral("Ctrl+F")}));
}

void ShortcutsFacadeTest::queryRoundTripsAndNotifiesOnlyOnChange()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    QSignalSpy spy(&facade, &ShortcutsFacade::changed);

    facade.setQuery(QStringLiteral("send"));
    QCOMPARE(facade.query(), QStringLiteral("send"));
    QCOMPARE(spy.count(), 1);

    // Re-sending the same text changes nothing, so bindings stay quiet.
    facade.setQuery(QStringLiteral("send"));
    QCOMPARE(spy.count(), 1);

    facade.setQuery(QString());
    QCOMPARE(facade.query(), QString());
    QCOMPARE(spy.count(), 2);
}

void ShortcutsFacadeTest::rebindChangesSequence()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    QSignalSpy spy(&facade, &ShortcutsFacade::changed);
    QVERIFY(facade.rebind(0, QStringLiteral("Ctrl+Shift+Enter")));

    QCOMPARE(facade.sequences().at(0), QStringLiteral("Ctrl+Shift+Enter"));
    QCOMPARE(facade.actions().at(0), QStringLiteral("message.send"));
    QCOMPARE(spy.count(), 1);
    QVERIFY(facade.modified());
}

void ShortcutsFacadeTest::rebindTrimsSequence()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    QVERIFY(facade.rebind(1, QStringLiteral("  Ctrl+J  ")));
    QCOMPARE(facade.sequences().at(1), QStringLiteral("Ctrl+J"));
}

void ShortcutsFacadeTest::rebindSameSequenceIsNoOp()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    QSignalSpy spy(&facade, &ShortcutsFacade::changed);
    QVERIFY(!facade.rebind(0, QStringLiteral("Ctrl+Enter")));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.sequences().at(0), QStringLiteral("Ctrl+Enter"));
    QVERIFY(!facade.modified());
}

void ShortcutsFacadeTest::rebindDuplicateSequenceIsRefused()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    QSignalSpy spy(&facade, &ShortcutsFacade::changed);

    // Row 0 already holds Ctrl+Enter, so row 1 may not take it.
    QVERIFY(!facade.rebind(1, QStringLiteral("Ctrl+Enter")));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.sequences(),
             (QStringList{QStringLiteral("Ctrl+Enter"), QStringLiteral("Ctrl+R"),
                          QStringLiteral("Ctrl+F")}));
    QVERIFY(!facade.modified());
}

void ShortcutsFacadeTest::rebindEmptySequenceIsRefused()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    QSignalSpy spy(&facade, &ShortcutsFacade::changed);
    QVERIFY(!facade.rebind(0, QString()));
    QVERIFY(!facade.rebind(0, QStringLiteral("   ")));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.sequences().at(0), QStringLiteral("Ctrl+Enter"));
}

void ShortcutsFacadeTest::rebindOutOfRangeIndexIsIgnored()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    QSignalSpy spy(&facade, &ShortcutsFacade::changed);
    QVERIFY(!facade.rebind(-1, QStringLiteral("Ctrl+J")));
    QVERIFY(!facade.rebind(3, QStringLiteral("Ctrl+J")));
    QVERIFY(!facade.rebind(99, QStringLiteral("Ctrl+J")));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.sequences(),
             (QStringList{QStringLiteral("Ctrl+Enter"), QStringLiteral("Ctrl+R"),
                          QStringLiteral("Ctrl+F")}));
}

void ShortcutsFacadeTest::rebindUsesVisibleIndexWhenFiltered()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    facade.setQuery(QStringLiteral("reply"));
    QCOMPARE(facade.actions(), QStringList{QStringLiteral("message.reply")});

    // Index 0 is the first visible row, not the first stored row.
    QVERIFY(facade.rebind(0, QStringLiteral("Ctrl+Shift+R")));
    QCOMPARE(facade.sequences(), QStringList{QStringLiteral("Ctrl+Shift+R")});

    // The filtered list has one row, so the second index is out of range even
    // though the underlying table has three.
    QVERIFY(!facade.rebind(1, QStringLiteral("Ctrl+J")));

    facade.setQuery(QString());
    QCOMPARE(facade.sequences(),
             (QStringList{QStringLiteral("Ctrl+Enter"), QStringLiteral("Ctrl+Shift+R"),
                          QStringLiteral("Ctrl+F")}));
}

void ShortcutsFacadeTest::rebindMapsFilteredIndexToItsAction()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    // "message." keeps rows 0 and 1 of the table, so visible index 1 must map
    // to message.reply and leave message.send alone.
    facade.setQuery(QStringLiteral("message."));
    QVERIFY(facade.rebind(1, QStringLiteral("Ctrl+Shift+R")));

    QCOMPARE(facade.actions(),
             (QStringList{QStringLiteral("message.send"), QStringLiteral("message.reply")}));
    QCOMPARE(facade.sequences(),
             (QStringList{QStringLiteral("Ctrl+Enter"), QStringLiteral("Ctrl+Shift+R")}));

    facade.setQuery(QString());
    QCOMPARE(facade.sequences().at(0), QStringLiteral("Ctrl+Enter"));
    QCOMPARE(facade.sequences().at(1), QStringLiteral("Ctrl+Shift+R"));
    QCOMPARE(facade.sequences().at(2), QStringLiteral("Ctrl+F"));
}

void ShortcutsFacadeTest::rebindKeepsRowOrder()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    QVERIFY(facade.rebind(2, QStringLiteral("Ctrl+K")));

    QCOMPARE(facade.actions(),
             (QStringList{QStringLiteral("message.send"), QStringLiteral("message.reply"),
                          QStringLiteral("chat.search")}));
}

void ShortcutsFacadeTest::resetRestoresShippedTable()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);
    loadUserTable(facade);

    QCOMPARE(facade.actions().size(), 2);
    QVERIFY(facade.modified());

    QSignalSpy spy(&facade, &ShortcutsFacade::changed);
    facade.resetToDefaults();

    QCOMPARE(facade.actions(),
             (QStringList{QStringLiteral("message.send"), QStringLiteral("message.reply"),
                          QStringLiteral("chat.search")}));
    QCOMPARE(facade.sequences(),
             (QStringList{QStringLiteral("Ctrl+Enter"), QStringLiteral("Ctrl+R"),
                          QStringLiteral("Ctrl+F")}));
    QVERIFY(!facade.modified());
    QCOMPARE(spy.count(), 1);
}

void ShortcutsFacadeTest::resetAfterRebindRestoresSequence()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    QVERIFY(facade.rebind(0, QStringLiteral("Ctrl+J")));
    QVERIFY(facade.modified());

    facade.resetToDefaults();

    QCOMPARE(facade.sequences().at(0), QStringLiteral("Ctrl+Enter"));
    QVERIFY(!facade.modified());
}

void ShortcutsFacadeTest::resetWithoutDefaultsIsSilent()
{
    ShortcutsFacade facade;

    QSignalSpy spy(&facade, &ShortcutsFacade::changed);
    facade.resetToDefaults();

    QCOMPARE(spy.count(), 0);
    QVERIFY(facade.actions().isEmpty());
    QVERIFY(!facade.modified());
}

void ShortcutsFacadeTest::resetWhenUnmodifiedIsSilent()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    QSignalSpy spy(&facade, &ShortcutsFacade::changed);
    facade.resetToDefaults();

    QCOMPARE(spy.count(), 0);
}

void ShortcutsFacadeTest::modifiedTracksRealDifferences()
{
    ShortcutsFacade facade;
    QVERIFY(!facade.modified());

    loadShippedTable(facade);
    QVERIFY(!facade.modified());

    QVERIFY(facade.rebind(0, QStringLiteral("Ctrl+J")));
    QVERIFY(facade.modified());

    facade.resetToDefaults();
    QVERIFY(!facade.modified());

    // A changed description counts too: the row no longer matches what shipped.
    facade.setShortcuts(
        {QStringLiteral("message.send"), QStringLiteral("message.reply"),
         QStringLiteral("chat.search")},
        {QStringLiteral("Ctrl+Enter"), QStringLiteral("Ctrl+R"), QStringLiteral("Ctrl+F")},
        {QStringLiteral("Send message"), QStringLiteral("Reply to message"),
         QStringLiteral("Find chats")});
    QVERIFY(facade.modified());

    facade.resetToDefaults();
    QVERIFY(!facade.modified());
}

void ShortcutsFacadeTest::triggerEmitsForKnownAction()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    QSignalSpy spy(&facade, &ShortcutsFacade::shortcutTriggered);
    facade.trigger(QStringLiteral("message.send"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("message.send"));
}

void ShortcutsFacadeTest::triggerEmitsForUnboundAction()
{
    ShortcutsFacade facade;
    facade.setShortcuts({QStringLiteral("message.send")}, {QString()}, {});

    QSignalSpy spy(&facade, &ShortcutsFacade::shortcutTriggered);
    facade.trigger(QStringLiteral("message.send"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("message.send"));
}

void ShortcutsFacadeTest::triggerIgnoresUnknownAndEmptyAction()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    QSignalSpy spy(&facade, &ShortcutsFacade::shortcutTriggered);
    facade.trigger(QStringLiteral("message.nope"));
    facade.trigger(QString());
    // Actions are matched exactly, as the model documents.
    facade.trigger(QStringLiteral("Message.Send"));

    QCOMPARE(spy.count(), 0);
}

void ShortcutsFacadeTest::triggerEmitsOnEveryCall()
{
    ShortcutsFacade facade;
    loadShippedTable(facade);

    QSignalSpy spy(&facade, &ShortcutsFacade::shortcutTriggered);
    facade.trigger(QStringLiteral("message.send"));
    facade.trigger(QStringLiteral("message.send"));

    QCOMPARE(spy.count(), 2);
}

QTEST_GUILESS_MAIN(ShortcutsFacadeTest)

#include "tst_shortcuts_facade.moc"
