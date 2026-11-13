#include "ui/QuickSwitcherFacade.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::ui;

namespace {

/// A catalog with a title-only hit (Carol), preview-only hits (Alice and Dave)
/// and a row that matches in both fields (Bob).
void loadChats(QuickSwitcherFacade& facade)
{
    facade.setChats(
        {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
         QStringLiteral("4")},
        {QStringLiteral("Alice"), QStringLiteral("Bob"), QStringLiteral("Carol"),
         QStringLiteral("Dave")},
        {QStringLiteral("See you soon"), QStringLiteral("bob replied"),
         QStringLiteral("Lunch?"), QStringLiteral("Hello")});
}

/// Whether the facade keeps its selection invariant: ids and titles are
/// parallel, and the selection is -1 exactly when they are empty.
bool selectionIsValid(const QuickSwitcherFacade& facade)
{
    const QStringList titles = facade.titles();
    if (titles.isEmpty())
        return facade.selectedIndex() == -1;

    return titles.size() == facade.ids().size() && facade.selectedIndex() >= 0
           && facade.selectedIndex() < titles.size();
}

} // namespace

class QuickSwitcherFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsClosedAndEmpty();

    void loadSetsParallelListsAndSelectsFirst();
    void loadTrimsIdsAndDropsEmptyOnes();
    void loadKeepsFirstDuplicate();
    void loadPadsMissingStrings();
    void loadEmitsChangedOnce();
    void reloadingSameCatalogIsSilent();
    void reloadingEquivalentPreviewIsSilent();
    void changingTitleEmits();

    void emptyQueryShowsWholeCatalog();
    void whitespaceQueryShowsWholeCatalog();
    void queryMatchesTitleCaseInsensitively();
    void queryMatchesPreview();
    void rankingFollowsChatSearchIndexScores();
    void equalScoresKeepCatalogOrder();
    void titleWhitespaceIsPreservedButMatchedTrimmed();
    void noMatchEmptiesRowsAndClearsSelection();
    void queryChangeSelectsFirstResult();
    void clearingQueryRestoresCatalog();
    void queryRoundTripsAndNotifiesOnlyOnChange();

    void selectionMovesDownAndUp();
    void selectionWrapsAtBothEnds();
    void moveByWholeListIsSilent();
    void moveSelectionWithoutRowsIsSilent();
    void moveSelectionZeroIsSilent();
    void moveSelectionWrapsWithinFilteredRows();

    void showOpensFreshSession();
    void showWithEmptyCatalogStaysOpen();
    void showWhenOpenIsSilent();
    void hideClosesAndKeepsLastSearch();
    void hideWhenClosedIsSilent();
    void showAfterHideClearsQuery();

    void activateCurrentEmitsSelectionAndCloses();
    void activateCurrentClosesBeforeAnnouncing();
    void activateCurrentUsesFilteredSelection();
    void activateCurrentWithoutRowsIsSilent();
    void activateCurrentWhileClosedStillEmits();

    void setChatsKeepsSelectedChat();
    void setChatsFallsBackToFirstWhenSelectionDisappears();
    void setChatsKeepsSelectionUnderQuery();
    void setChatsFallsBackWhenSelectionStopsMatching();
    void setChatsWithoutMatchesClearsSelection();
    void setChatsEmptyInputClearsCatalog();
    void setChatsEmptyInputIsSilentOnEmptyFacade();

    void selectionInvariantHoldsAcrossOperations();
};

void QuickSwitcherFacadeTest::startsClosedAndEmpty()
{
    const QuickSwitcherFacade facade;

    QVERIFY(!facade.open());
    QVERIFY(facade.query().isEmpty());
    QVERIFY(facade.titles().isEmpty());
    QVERIFY(facade.ids().isEmpty());
    QCOMPARE(facade.selectedIndex(), -1);
}

void QuickSwitcherFacadeTest::loadSetsParallelListsAndSelectsFirst()
{
    QuickSwitcherFacade facade;
    loadChats(facade);

    QCOMPARE(facade.ids(),
             (QStringList{QStringLiteral("1"), QStringLiteral("2"),
                          QStringLiteral("3"), QStringLiteral("4")}));
    QCOMPARE(facade.titles(),
             (QStringList{QStringLiteral("Alice"), QStringLiteral("Bob"),
                          QStringLiteral("Carol"), QStringLiteral("Dave")}));
    QCOMPARE(facade.titles().size(), facade.ids().size());
    QCOMPARE(facade.selectedIndex(), 0);
    QVERIFY(!facade.open());
}

void QuickSwitcherFacadeTest::loadTrimsIdsAndDropsEmptyOnes()
{
    QuickSwitcherFacade facade;
    facade.setChats(
        {QStringLiteral(" 1 "), QString(), QStringLiteral("   "),
         QStringLiteral("2")},
        {QStringLiteral("Alice"), QStringLiteral("Empty"), QStringLiteral("Blank"),
         QStringLiteral("Bob")},
        {});

    QCOMPARE(facade.ids(), (QStringList{QStringLiteral("1"), QStringLiteral("2")}));
    QCOMPARE(facade.titles(),
             (QStringList{QStringLiteral("Alice"), QStringLiteral("Bob")}));
    QCOMPARE(facade.selectedIndex(), 0);
}

void QuickSwitcherFacadeTest::loadKeepsFirstDuplicate()
{
    QuickSwitcherFacade facade;
    facade.setChats(
        {QStringLiteral("1"), QStringLiteral("1"), QStringLiteral("2")},
        {QStringLiteral("First"), QStringLiteral("Second"), QStringLiteral("Third")},
        {QStringLiteral("needle"), QStringLiteral("other"),
         QStringLiteral("unrelated")});

    // Only the first row of a repeated id is stored, so the second title never
    // appears and the second preview never matches.
    QCOMPARE(facade.ids(), (QStringList{QStringLiteral("1"), QStringLiteral("2")}));
    QCOMPARE(facade.titles(),
             (QStringList{QStringLiteral("First"), QStringLiteral("Third")}));

    facade.setQuery(QStringLiteral("needle"));
    QCOMPARE(facade.ids(), QStringList{QStringLiteral("1")});
    QCOMPARE(facade.titles(), QStringList{QStringLiteral("First")});
}

void QuickSwitcherFacadeTest::loadPadsMissingStrings()
{
    QuickSwitcherFacade facade;
    facade.setChats(
        {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3")},
        {QStringLiteral("Only")}, {QStringLiteral("needle")});

    QCOMPARE(facade.titles(),
             (QStringList{QStringLiteral("Only"), QString(), QString()}));

    // The missing previews read as empty, so only the first row matches.
    facade.setQuery(QStringLiteral("needle"));
    QCOMPARE(facade.ids(), QStringList{QStringLiteral("1")});
}

void QuickSwitcherFacadeTest::loadEmitsChangedOnce()
{
    QuickSwitcherFacade facade;
    QSignalSpy spy(&facade, &QuickSwitcherFacade::changed);

    loadChats(facade);

    QCOMPARE(spy.count(), 1);
}

void QuickSwitcherFacadeTest::reloadingSameCatalogIsSilent()
{
    QuickSwitcherFacade facade;
    loadChats(facade);

    QSignalSpy spy(&facade, &QuickSwitcherFacade::changed);
    loadChats(facade);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.titles().size(), 4);
}

void QuickSwitcherFacadeTest::reloadingEquivalentPreviewIsSilent()
{
    QuickSwitcherFacade facade;
    facade.setChats({QStringLiteral("1")}, {QStringLiteral("Alice")},
                    {QStringLiteral("Hello")});

    QSignalSpy spy(&facade, &QuickSwitcherFacade::changed);
    facade.setChats({QStringLiteral("1")}, {QStringLiteral("Alice")},
                    {QStringLiteral("  hello  ")});

    // Previews are match-only, so a case/whitespace difference that normalizes
    // identically cannot change anything the palette shows.
    QCOMPARE(spy.count(), 0);
}

void QuickSwitcherFacadeTest::changingTitleEmits()
{
    QuickSwitcherFacade facade;
    facade.setChats({QStringLiteral("1")}, {QStringLiteral("Alice")},
                    {QStringLiteral("Hello")});

    QSignalSpy spy(&facade, &QuickSwitcherFacade::changed);
    facade.setChats({QStringLiteral("1")}, {QStringLiteral("Alicia")},
                    {QStringLiteral("Hello")});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.titles(), QStringList{QStringLiteral("Alicia")});
}

void QuickSwitcherFacadeTest::emptyQueryShowsWholeCatalog()
{
    QuickSwitcherFacade facade;
    loadChats(facade);

    QVERIFY(facade.query().isEmpty());
    QCOMPARE(facade.titles(),
             (QStringList{QStringLiteral("Alice"), QStringLiteral("Bob"),
                          QStringLiteral("Carol"), QStringLiteral("Dave")}));
}

void QuickSwitcherFacadeTest::whitespaceQueryShowsWholeCatalog()
{
    QuickSwitcherFacade facade;
    loadChats(facade);

    facade.setQuery(QStringLiteral("   "));

    QCOMPARE(facade.query(), QStringLiteral("   "));
    QCOMPARE(facade.titles().size(), 4);
    QCOMPARE(facade.selectedIndex(), 0);
}

void QuickSwitcherFacadeTest::queryMatchesTitleCaseInsensitively()
{
    QuickSwitcherFacade facade;
    loadChats(facade);

    facade.setQuery(QStringLiteral("ALI"));

    QCOMPARE(facade.ids(), QStringList{QStringLiteral("1")});
    QCOMPARE(facade.titles(), QStringList{QStringLiteral("Alice")});
}

void QuickSwitcherFacadeTest::queryMatchesPreview()
{
    QuickSwitcherFacade facade;
    loadChats(facade);

    // "reply" appears only in Bob's preview.
    facade.setQuery(QStringLiteral("replied"));

    QCOMPARE(facade.ids(), QStringList{QStringLiteral("2")});
    QCOMPARE(facade.titles(), QStringList{QStringLiteral("Bob")});
}

void QuickSwitcherFacadeTest::rankingFollowsChatSearchIndexScores()
{
    QuickSwitcherFacade facade;
    facade.setChats(
        {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
         QStringLiteral("4")},
        {QStringLiteral("xxNeedle"), QStringLiteral("Needle yy"),
         QStringLiteral("zzz"), QStringLiteral("zzz")},
        {QString(), QString(), QStringLiteral("Needle preview"),
         QStringLiteral("xx needle")});

    facade.setQuery(QStringLiteral("needle"));

    // Row 2 (title prefix) beats row 1 (title substring), which beats row 3
    // (preview prefix), which beats row 4 (preview substring).
    QCOMPARE(facade.ids(),
             (QStringList{QStringLiteral("2"), QStringLiteral("1"),
                          QStringLiteral("3"), QStringLiteral("4")}));
}

void QuickSwitcherFacadeTest::equalScoresKeepCatalogOrder()
{
    QuickSwitcherFacade facade;
    facade.setChats(
        {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3")},
        {QStringLiteral("Alpha"), QStringLiteral("Beta"), QStringLiteral("Gamma")},
        {QStringLiteral("needle one"), QStringLiteral("needle two"),
         QStringLiteral("needle three")});

    facade.setQuery(QStringLiteral("needle"));

    QCOMPARE(facade.ids(),
             (QStringList{QStringLiteral("1"), QStringLiteral("2"),
                          QStringLiteral("3")}));
}

void QuickSwitcherFacadeTest::titleWhitespaceIsPreservedButMatchedTrimmed()
{
    QuickSwitcherFacade facade;
    facade.setChats({QStringLiteral("1")}, {QStringLiteral("  Alice  ")}, {});

    QCOMPARE(facade.titles(), QStringList{QStringLiteral("  Alice  ")});

    facade.setQuery(QStringLiteral("alice"));
    QCOMPARE(facade.titles(), QStringList{QStringLiteral("  Alice  ")});
}

void QuickSwitcherFacadeTest::noMatchEmptiesRowsAndClearsSelection()
{
    QuickSwitcherFacade facade;
    loadChats(facade);

    facade.setQuery(QStringLiteral("no such chat"));

    QVERIFY(facade.ids().isEmpty());
    QVERIFY(facade.titles().isEmpty());
    QCOMPARE(facade.selectedIndex(), -1);
}

void QuickSwitcherFacadeTest::queryChangeSelectsFirstResult()
{
    QuickSwitcherFacade facade;
    loadChats(facade);

    // Alice and Dave match only through their previews, so Bob and Carol lead
    // on title hits and keep their catalog order among equal scores.
    facade.setQuery(QStringLiteral("o"));

    QCOMPARE(facade.ids(),
             (QStringList{QStringLiteral("2"), QStringLiteral("3"),
                          QStringLiteral("1"), QStringLiteral("4")}));
    QCOMPARE(facade.selectedIndex(), 0);
    QCOMPARE(facade.ids().at(facade.selectedIndex()), QStringLiteral("2"));
}

void QuickSwitcherFacadeTest::clearingQueryRestoresCatalog()
{
    QuickSwitcherFacade facade;
    loadChats(facade);

    facade.setQuery(QStringLiteral("carol"));
    QCOMPARE(facade.titles(), QStringList{QStringLiteral("Carol")});

    facade.setQuery(QString());

    QCOMPARE(facade.titles().size(), 4);
    QCOMPARE(facade.selectedIndex(), 0);
}

void QuickSwitcherFacadeTest::queryRoundTripsAndNotifiesOnlyOnChange()
{
    QuickSwitcherFacade facade;
    loadChats(facade);

    QSignalSpy spy(&facade, &QuickSwitcherFacade::changed);

    facade.setQuery(QStringLiteral("bob"));
    QCOMPARE(facade.query(), QStringLiteral("bob"));
    QCOMPARE(spy.count(), 1);

    // Re-sending the same text changes nothing, so bindings stay quiet.
    facade.setQuery(QStringLiteral("bob"));
    QCOMPARE(spy.count(), 1);

    // Different text that leaves the same row visible still changed the query.
    facade.setQuery(QStringLiteral("BOB"));
    QCOMPARE(facade.query(), QStringLiteral("BOB"));
    QCOMPARE(facade.titles(), QStringList{QStringLiteral("Bob")});
    QCOMPARE(spy.count(), 2);
}

void QuickSwitcherFacadeTest::selectionMovesDownAndUp()
{
    QuickSwitcherFacade facade;
    loadChats(facade);

    QSignalSpy spy(&facade, &QuickSwitcherFacade::changed);

    facade.moveSelection(1);
    QCOMPARE(facade.selectedIndex(), 1);
    QCOMPARE(spy.count(), 1);

    facade.moveSelection(2);
    QCOMPARE(facade.selectedIndex(), 3);
    QCOMPARE(spy.count(), 2);

    facade.moveSelection(-2);
    QCOMPARE(facade.selectedIndex(), 1);
    QCOMPARE(spy.count(), 3);
}

void QuickSwitcherFacadeTest::selectionWrapsAtBothEnds()
{
    QuickSwitcherFacade facade;
    loadChats(facade);

    facade.moveSelection(-1);
    QCOMPARE(facade.selectedIndex(), 3);

    facade.moveSelection(1);
    QCOMPARE(facade.selectedIndex(), 0);
}

void QuickSwitcherFacadeTest::moveByWholeListIsSilent()
{
    QuickSwitcherFacade facade;
    loadChats(facade);

    QSignalSpy spy(&facade, &QuickSwitcherFacade::changed);

    facade.moveSelection(4);
    QCOMPARE(facade.selectedIndex(), 0);
    QCOMPARE(spy.count(), 0);

    facade.moveSelection(-4);
    QCOMPARE(facade.selectedIndex(), 0);
    QCOMPARE(spy.count(), 0);
}

void QuickSwitcherFacadeTest::moveSelectionWithoutRowsIsSilent()
{
    QuickSwitcherFacade facade;

    QSignalSpy spy(&facade, &QuickSwitcherFacade::changed);
    facade.moveSelection(1);

    QCOMPARE(facade.selectedIndex(), -1);
    QCOMPARE(spy.count(), 0);
}

void QuickSwitcherFacadeTest::moveSelectionZeroIsSilent()
{
    QuickSwitcherFacade facade;
    loadChats(facade);

    QSignalSpy spy(&facade, &QuickSwitcherFacade::changed);
    facade.moveSelection(0);

    QCOMPARE(facade.selectedIndex(), 0);
    QCOMPARE(spy.count(), 0);
}

void QuickSwitcherFacadeTest::moveSelectionWrapsWithinFilteredRows()
{
    QuickSwitcherFacade facade;
    loadChats(facade);

    facade.setQuery(QStringLiteral("o"));
    QCOMPARE(facade.ids().size(), 4);

    // Up from the first visible row wraps to the last *visible* row, not the
    // last catalog row.
    facade.moveSelection(-1);

    QCOMPARE(facade.selectedIndex(), 3);
    QCOMPARE(facade.ids().at(facade.selectedIndex()), QStringLiteral("4"));
}

void QuickSwitcherFacadeTest::showOpensFreshSession()
{
    QuickSwitcherFacade facade;
    loadChats(facade);

    facade.setQuery(QStringLiteral("carol"));
    QCOMPARE(facade.titles(), QStringList{QStringLiteral("Carol")});

    QSignalSpy spy(&facade, &QuickSwitcherFacade::changed);
    facade.show();

    QVERIFY(facade.open());
    QVERIFY(facade.query().isEmpty());
    QCOMPARE(facade.titles().size(), 4);
    QCOMPARE(facade.selectedIndex(), 0);
    QCOMPARE(spy.count(), 1);
}

void QuickSwitcherFacadeTest::showWithEmptyCatalogStaysOpen()
{
    QuickSwitcherFacade facade;

    QSignalSpy spy(&facade, &QuickSwitcherFacade::changed);
    facade.show();

    QVERIFY(facade.open());
    QVERIFY(facade.titles().isEmpty());
    QCOMPARE(facade.selectedIndex(), -1);
    QCOMPARE(spy.count(), 1);
}

void QuickSwitcherFacadeTest::showWhenOpenIsSilent()
{
    QuickSwitcherFacade facade;
    loadChats(facade);
    facade.show();
    facade.setQuery(QStringLiteral("bob"));

    QSignalSpy spy(&facade, &QuickSwitcherFacade::changed);
    facade.show();

    QCOMPARE(spy.count(), 0);
    QVERIFY(facade.open());
    QCOMPARE(facade.query(), QStringLiteral("bob"));
    QCOMPARE(facade.titles(), QStringList{QStringLiteral("Bob")});
}

void QuickSwitcherFacadeTest::hideClosesAndKeepsLastSearch()
{
    QuickSwitcherFacade facade;
    loadChats(facade);
    facade.show();
    facade.setQuery(QStringLiteral("bob"));

    QSignalSpy spy(&facade, &QuickSwitcherFacade::changed);
    facade.hide();

    QVERIFY(!facade.open());
    QCOMPARE(facade.query(), QStringLiteral("bob"));
    QCOMPARE(facade.titles(), QStringList{QStringLiteral("Bob")});
    QCOMPARE(facade.selectedIndex(), 0);
    QCOMPARE(spy.count(), 1);
}

void QuickSwitcherFacadeTest::hideWhenClosedIsSilent()
{
    QuickSwitcherFacade facade;
    loadChats(facade);

    QSignalSpy spy(&facade, &QuickSwitcherFacade::changed);
    facade.hide();

    QVERIFY(!facade.open());
    QCOMPARE(spy.count(), 0);
}

void QuickSwitcherFacadeTest::showAfterHideClearsQuery()
{
    QuickSwitcherFacade facade;
    loadChats(facade);
    facade.show();
    facade.setQuery(QStringLiteral("bob"));
    facade.hide();

    QSignalSpy spy(&facade, &QuickSwitcherFacade::changed);
    facade.show();

    QVERIFY(facade.open());
    QVERIFY(facade.query().isEmpty());
    QCOMPARE(facade.titles().size(), 4);
    QCOMPARE(facade.selectedIndex(), 0);
    QCOMPARE(spy.count(), 1);
}

void QuickSwitcherFacadeTest::activateCurrentEmitsSelectionAndCloses()
{
    QuickSwitcherFacade facade;
    loadChats(facade);
    facade.show();
    facade.moveSelection(2);

    QSignalSpy activated(&facade, &QuickSwitcherFacade::chatActivated);
    QSignalSpy changed(&facade, &QuickSwitcherFacade::changed);

    facade.activateCurrent();

    QCOMPARE(activated.count(), 1);
    QCOMPARE(activated.at(0).at(0).toString(), QStringLiteral("3"));
    QVERIFY(!facade.open());
    QCOMPARE(changed.count(), 1);
}

void QuickSwitcherFacadeTest::activateCurrentClosesBeforeAnnouncing()
{
    QuickSwitcherFacade facade;
    loadChats(facade);
    facade.show();

    bool closedWhenActivated = false;
    connect(&facade, &QuickSwitcherFacade::chatActivated, &facade,
            [&facade, &closedWhenActivated](const QString&) {
                closedWhenActivated = !facade.open();
            });

    facade.activateCurrent();

    // A handler that opens the chosen chat must not see the palette still open.
    QVERIFY(closedWhenActivated);
}

void QuickSwitcherFacadeTest::activateCurrentUsesFilteredSelection()
{
    QuickSwitcherFacade facade;
    loadChats(facade);
    facade.show();
    facade.setQuery(QStringLiteral("o"));

    // The filtered order is Bob, Carol, Alice, Dave.
    facade.moveSelection(1);
    QCOMPARE(facade.ids().at(facade.selectedIndex()), QStringLiteral("3"));

    QSignalSpy activated(&facade, &QuickSwitcherFacade::chatActivated);
    facade.activateCurrent();

    QCOMPARE(activated.count(), 1);
    QCOMPARE(activated.at(0).at(0).toString(), QStringLiteral("3"));
}

void QuickSwitcherFacadeTest::activateCurrentWithoutRowsIsSilent()
{
    QuickSwitcherFacade facade;
    loadChats(facade);
    facade.setQuery(QStringLiteral("no such chat"));

    QSignalSpy activated(&facade, &QuickSwitcherFacade::chatActivated);
    QSignalSpy changed(&facade, &QuickSwitcherFacade::changed);

    facade.activateCurrent();

    QCOMPARE(activated.count(), 0);
    QCOMPARE(changed.count(), 0);
}

void QuickSwitcherFacadeTest::activateCurrentWhileClosedStillEmits()
{
    QuickSwitcherFacade facade;
    loadChats(facade);

    QVERIFY(!facade.open());

    QSignalSpy activated(&facade, &QuickSwitcherFacade::chatActivated);
    QSignalSpy changed(&facade, &QuickSwitcherFacade::changed);

    facade.activateCurrent();

    QCOMPARE(activated.count(), 1);
    QCOMPARE(activated.at(0).at(0).toString(), QStringLiteral("1"));
    QVERIFY(!facade.open());
    QCOMPARE(changed.count(), 0);
}

void QuickSwitcherFacadeTest::setChatsKeepsSelectedChat()
{
    QuickSwitcherFacade facade;
    loadChats(facade);
    facade.moveSelection(2);
    QCOMPARE(facade.ids().at(facade.selectedIndex()), QStringLiteral("3"));

    facade.setChats(
        {QStringLiteral("4"), QStringLiteral("3"), QStringLiteral("1"),
         QStringLiteral("2")},
        {QStringLiteral("Dave"), QStringLiteral("Carol"), QStringLiteral("Alice"),
         QStringLiteral("Bob")},
        {});

    QCOMPARE(facade.ids(),
             (QStringList{QStringLiteral("4"), QStringLiteral("3"),
                          QStringLiteral("1"), QStringLiteral("2")}));
    QCOMPARE(facade.selectedIndex(), 1);
    QCOMPARE(facade.ids().at(facade.selectedIndex()), QStringLiteral("3"));
}

void QuickSwitcherFacadeTest::setChatsFallsBackToFirstWhenSelectionDisappears()
{
    QuickSwitcherFacade facade;
    loadChats(facade);
    facade.moveSelection(2);
    QCOMPARE(facade.ids().at(facade.selectedIndex()), QStringLiteral("3"));

    facade.setChats(
        {QStringLiteral("4"), QStringLiteral("1"), QStringLiteral("2")},
        {QStringLiteral("Dave"), QStringLiteral("Alice"), QStringLiteral("Bob")},
        {});

    QCOMPARE(facade.ids().at(0), QStringLiteral("4"));
    QCOMPARE(facade.selectedIndex(), 0);
}

void QuickSwitcherFacadeTest::setChatsKeepsSelectionUnderQuery()
{
    QuickSwitcherFacade facade;
    loadChats(facade);
    facade.setQuery(QStringLiteral("o"));

    // Filtered order: Bob, Carol, Alice, Dave. Move to Carol.
    facade.moveSelection(1);
    QCOMPARE(facade.ids().at(facade.selectedIndex()), QStringLiteral("3"));

    // Reorder the catalog and drop every preview: only Bob and Carol still
    // match "o", and Carol must stay selected at her new visible index.
    facade.setChats(
        {QStringLiteral("4"), QStringLiteral("1"), QStringLiteral("2"),
         QStringLiteral("3")},
        {QStringLiteral("Dave"), QStringLiteral("Alice"), QStringLiteral("Bob"),
         QStringLiteral("Carol")},
        {});

    QCOMPARE(facade.ids(),
             (QStringList{QStringLiteral("2"), QStringLiteral("3")}));
    QCOMPARE(facade.selectedIndex(), 1);
    QCOMPARE(facade.ids().at(facade.selectedIndex()), QStringLiteral("3"));
}

void QuickSwitcherFacadeTest::setChatsFallsBackWhenSelectionStopsMatching()
{
    QuickSwitcherFacade facade;
    facade.setChats(
        {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3")},
        {QStringLiteral("Alice"), QStringLiteral("Bob"), QStringLiteral("Bobby")},
        {});

    facade.setQuery(QStringLiteral("bob"));
    QCOMPARE(facade.ids(),
             (QStringList{QStringLiteral("2"), QStringLiteral("3")}));
    QCOMPARE(facade.selectedIndex(), 0);

    // Rename Bob so the selected chat no longer matches; Bobby is the only
    // remaining result and becomes selected.
    facade.setChats(
        {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3")},
        {QStringLiteral("Alice"), QStringLiteral("Robert"), QStringLiteral("Bobby")},
        {});

    QCOMPARE(facade.ids(), QStringList{QStringLiteral("3")});
    QCOMPARE(facade.selectedIndex(), 0);
}

void QuickSwitcherFacadeTest::setChatsWithoutMatchesClearsSelection()
{
    QuickSwitcherFacade facade;
    loadChats(facade);
    facade.setQuery(QStringLiteral("carol"));
    QCOMPARE(facade.ids(), QStringList{QStringLiteral("3")});

    facade.setChats(
        {QStringLiteral("1"), QStringLiteral("2")},
        {QStringLiteral("Alice"), QStringLiteral("Bob")}, {});

    QVERIFY(facade.ids().isEmpty());
    QVERIFY(facade.titles().isEmpty());
    QCOMPARE(facade.selectedIndex(), -1);
}

void QuickSwitcherFacadeTest::setChatsEmptyInputClearsCatalog()
{
    QuickSwitcherFacade facade;
    loadChats(facade);

    QSignalSpy spy(&facade, &QuickSwitcherFacade::changed);
    facade.setChats({}, {}, {});

    QVERIFY(facade.ids().isEmpty());
    QVERIFY(facade.titles().isEmpty());
    QCOMPARE(facade.selectedIndex(), -1);
    QCOMPARE(spy.count(), 1);
}

void QuickSwitcherFacadeTest::setChatsEmptyInputIsSilentOnEmptyFacade()
{
    QuickSwitcherFacade facade;

    QSignalSpy spy(&facade, &QuickSwitcherFacade::changed);
    facade.setChats({}, {}, {});

    QCOMPARE(spy.count(), 0);
    QVERIFY(facade.ids().isEmpty());
    QCOMPARE(facade.selectedIndex(), -1);
}

void QuickSwitcherFacadeTest::selectionInvariantHoldsAcrossOperations()
{
    QuickSwitcherFacade facade;
    QVERIFY(selectionIsValid(facade));

    loadChats(facade);
    QVERIFY(selectionIsValid(facade));

    facade.setQuery(QStringLiteral("o"));
    QVERIFY(selectionIsValid(facade));

    facade.moveSelection(3);
    QVERIFY(selectionIsValid(facade));

    facade.setChats(
        {QStringLiteral("4"), QStringLiteral("3"), QStringLiteral("1"),
         QStringLiteral("2")},
        {QStringLiteral("Dave"), QStringLiteral("Carol"), QStringLiteral("Alice"),
         QStringLiteral("Bob")},
        {});
    QVERIFY(selectionIsValid(facade));

    facade.setQuery(QStringLiteral("no such chat"));
    QVERIFY(selectionIsValid(facade));

    facade.show();
    QVERIFY(selectionIsValid(facade));

    facade.hide();
    QVERIFY(selectionIsValid(facade));
}

QTEST_GUILESS_MAIN(QuickSwitcherFacadeTest)

#include "tst_quick_switcher_facade.moc"
