#include "ui/SharedMediaFacade.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtTest>

#include <initializer_list>

using beygram::ui::SharedMediaFacade;

namespace {

QStringList list(std::initializer_list<const char*> values)
{
    QStringList out;
    out.reserve(static_cast<qsizetype>(values.size()));
    for (const char* value : values)
        out.append(QString::fromLatin1(value));
    return out;
}

} // namespace

class SharedMediaFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsOnPhotos();
    void selectTabSwitchesAndDropsItems();
    void selectTabOnEmptyPageStillNotifies();
    void selectTabIgnoresInvalidAndRepeated();
    void setItemsSortsNewestFirst();
    void setItemsBreaksDateTiesByDescendingId();
    void setItemsKeepsFirstDuplicate();
    void setItemsDropsInvalidIds();
    void setItemsTreatsUnreadableDatesAsZero();
    void setItemsRejectsMismatchedLists();
    void setItemsEmptyClearsTheList();
    void appendItemsAddsNewIdsNewestFirst();
    void appendItemsIgnoresLoadedIds();
    void changedOnlyForVisibleState();
    void activateEmitsTheRowId();
    void activateAfterTabChangeIsSilent();
};

void SharedMediaFacadeTest::startsOnPhotos()
{
    const SharedMediaFacade facade;

    QCOMPARE(facade.tabs(),
             list({ "Photos", "Videos", "Files", "Links", "Voice", "Music", "GIFs" }));
    QCOMPARE(facade.currentTab(), 0);
    QVERIFY(facade.ids().isEmpty());
    QVERIFY(facade.titles().isEmpty());
}

void SharedMediaFacadeTest::selectTabSwitchesAndDropsItems()
{
    SharedMediaFacade facade;

    facade.setItems(list({ "20", "10" }), list({ "B", "A" }), list({ "20", "10" }));
    QCOMPARE(facade.ids(), list({ "20", "10" }));

    QSignalSpy changed(&facade, &SharedMediaFacade::changed);
    facade.selectTab(2);

    // Every tab has its own filter, so the previous page cannot survive.
    QCOMPARE(facade.currentTab(), 2);
    QVERIFY(facade.ids().isEmpty());
    QVERIFY(facade.titles().isEmpty());
    QCOMPARE(changed.count(), 1);

    // A new page belongs to the tab that is open now.
    facade.setItems(list({ "30" }), list({ "C" }), list({ "30" }));
    QCOMPARE(facade.currentTab(), 2);
    QCOMPARE(facade.ids(), list({ "30" }));
}

void SharedMediaFacadeTest::selectTabOnEmptyPageStillNotifies()
{
    SharedMediaFacade facade;
    QSignalSpy changed(&facade, &SharedMediaFacade::changed);

    facade.selectTab(3);

    // The tab is visible state on its own, even when there is nothing to show.
    QCOMPARE(facade.currentTab(), 3);
    QVERIFY(facade.ids().isEmpty());
    QCOMPARE(changed.count(), 1);
}

void SharedMediaFacadeTest::selectTabIgnoresInvalidAndRepeated()
{
    SharedMediaFacade facade;

    facade.setItems(list({ "20", "10" }), list({ "B", "A" }), list({ "20", "10" }));

    QSignalSpy changed(&facade, &SharedMediaFacade::changed);

    facade.selectTab(-1);
    facade.selectTab(7);
    facade.selectTab(0);

    QCOMPARE(facade.currentTab(), 0);
    QCOMPARE(facade.ids(), list({ "20", "10" }));
    QCOMPARE(facade.titles(), list({ "B", "A" }));
    QCOMPARE(changed.count(), 0);
}

void SharedMediaFacadeTest::setItemsSortsNewestFirst()
{
    SharedMediaFacade facade;
    QSignalSpy changed(&facade, &SharedMediaFacade::changed);

    facade.setItems(list({ "100", "300", "200" }), list({ "old", "new", "mid" }),
                    list({ "10", "30", "20" }));

    // Ids and titles stay parallel after the sort.
    QCOMPARE(facade.ids(), list({ "300", "200", "100" }));
    QCOMPARE(facade.titles(), list({ "new", "mid", "old" }));
    QCOMPARE(changed.count(), 1);
}

void SharedMediaFacadeTest::setItemsBreaksDateTiesByDescendingId()
{
    SharedMediaFacade facade;

    facade.setItems(list({ "7", "9" }), list({ "seven", "nine" }), list({ "50", "50" }));

    QCOMPARE(facade.ids(), list({ "9", "7" }));
    QCOMPARE(facade.titles(), list({ "nine", "seven" }));
}

void SharedMediaFacadeTest::setItemsKeepsFirstDuplicate()
{
    SharedMediaFacade facade;

    facade.setItems(list({ "100", "200", "100" }),
                    list({ "first", "second", "later" }),
                    list({ "10", "20", "99" }));

    // The first copy of id 100 wins, including its date, so it sorts last.
    QCOMPARE(facade.ids(), list({ "200", "100" }));
    QCOMPARE(facade.titles(), list({ "second", "first" }));
}

void SharedMediaFacadeTest::setItemsDropsInvalidIds()
{
    SharedMediaFacade facade;

    facade.setItems(list({ "abc", "0", "-4", "300", " 200 " }),
                    list({ "bad1", "bad2", "bad3", "keep300", "keep200" }),
                    list({ "1", "2", "3", "30", "20" }));

    // An invalid id takes its title and date with it; surrounding spaces are
    // harmless.
    QCOMPARE(facade.ids(), list({ "300", "200" }));
    QCOMPARE(facade.titles(), list({ "keep300", "keep200" }));
}

void SharedMediaFacadeTest::setItemsTreatsUnreadableDatesAsZero()
{
    SharedMediaFacade facade;

    facade.setItems(list({ "1", "2" }), list({ "no-date", "dated" }),
                    list({ "oops", "50" }));

    QCOMPARE(facade.ids(), list({ "2", "1" }));
    QCOMPARE(facade.titles(), list({ "dated", "no-date" }));
}

void SharedMediaFacadeTest::setItemsRejectsMismatchedLists()
{
    SharedMediaFacade facade;

    facade.setItems(list({ "10" }), list({ "A" }), list({ "10" }));

    QSignalSpy changed(&facade, &SharedMediaFacade::changed);

    facade.setItems(list({ "20", "30" }), list({ "B" }), list({ "20", "30" }));
    facade.setItems(list({ "20", "30" }), list({ "B", "C" }), list({ "20" }));
    facade.appendItems(list({ "40", "50" }), list({ "D" }), list({ "40", "50" }));
    facade.appendItems(list({ "40", "50" }), list({ "D", "E" }), list({ "40" }));

    QCOMPARE(facade.ids(), list({ "10" }));
    QCOMPARE(facade.titles(), list({ "A" }));
    QCOMPARE(changed.count(), 0);
}

void SharedMediaFacadeTest::setItemsEmptyClearsTheList()
{
    SharedMediaFacade facade;

    facade.setItems(list({ "10" }), list({ "A" }), list({ "10" }));

    QSignalSpy changed(&facade, &SharedMediaFacade::changed);
    facade.setItems({}, {}, {});

    QVERIFY(facade.ids().isEmpty());
    QVERIFY(facade.titles().isEmpty());
    QCOMPARE(changed.count(), 1);

    // Clearing an empty page changes nothing.
    facade.setItems({}, {}, {});
    QCOMPARE(changed.count(), 1);
}

void SharedMediaFacadeTest::appendItemsAddsNewIdsNewestFirst()
{
    SharedMediaFacade facade;

    facade.setItems(list({ "100", "300" }), list({ "one", "three" }), list({ "10", "30" }));

    QSignalSpy changed(&facade, &SharedMediaFacade::changed);
    facade.appendItems(list({ "200", "400", "300" }),
                       list({ "two", "four", "duplicate" }),
                       list({ "20", "40", "35" }));

    // 400 is the newest; the loaded 300 keeps its own date, so it stays between
    // 400 and 200, and its title is not replaced by the duplicate.
    QCOMPARE(facade.ids(), list({ "400", "300", "200", "100" }));
    QCOMPARE(facade.titles(), list({ "four", "three", "two", "one" }));
    QCOMPARE(changed.count(), 1);
}

void SharedMediaFacadeTest::appendItemsIgnoresLoadedIds()
{
    SharedMediaFacade facade;

    facade.setItems(list({ "10", "20" }), list({ "A", "B" }), list({ "10", "20" }));

    QSignalSpy changed(&facade, &SharedMediaFacade::changed);
    facade.appendItems(list({ "20", "10" }), list({ "B2", "A2" }), list({ "99", "98" }));

    QCOMPARE(facade.ids(), list({ "20", "10" }));
    QCOMPARE(facade.titles(), list({ "B", "A" }));
    QCOMPARE(changed.count(), 0);
}

void SharedMediaFacadeTest::changedOnlyForVisibleState()
{
    SharedMediaFacade facade;
    QSignalSpy changed(&facade, &SharedMediaFacade::changed);

    facade.setItems(list({ "2", "1" }), list({ "B", "A" }), list({ "20", "10" }));
    QCOMPARE(changed.count(), 1);

    // The same rows in another input order normalize to the same page.
    facade.setItems(list({ "1", "2" }), list({ "A", "B" }), list({ "10", "20" }));
    QCOMPARE(changed.count(), 1);

    // Dates order the rows but are not exposed, so this edit is invisible.
    facade.setItems(list({ "2", "1" }), list({ "B", "A" }), list({ "21", "11" }));
    QCOMPARE(changed.count(), 1);

    // A title edit is visible.
    facade.setItems(list({ "2", "1" }), list({ "B!", "A" }), list({ "20", "10" }));
    QCOMPARE(changed.count(), 2);

    // Selecting the tab that is already open is silent.
    facade.selectTab(0);
    QCOMPARE(changed.count(), 2);
}

void SharedMediaFacadeTest::activateEmitsTheRowId()
{
    SharedMediaFacade facade;

    facade.setItems(list({ "100", "300", "200" }), list({ "old", "new", "mid" }),
                    list({ "10", "30", "20" }));

    QSignalSpy activated(&facade, &SharedMediaFacade::itemActivated);
    QSignalSpy changed(&facade, &SharedMediaFacade::changed);

    facade.activate(0);
    facade.activate(2);
    facade.activate(-1);
    facade.activate(3);

    QCOMPARE(activated.count(), 2);
    QCOMPARE(activated.at(0).at(0).toString(), QStringLiteral("300"));
    QCOMPARE(activated.at(1).at(0).toString(), QStringLiteral("100"));
    QCOMPARE(changed.count(), 0);
}

void SharedMediaFacadeTest::activateAfterTabChangeIsSilent()
{
    SharedMediaFacade facade;

    facade.setItems(list({ "10" }), list({ "A" }), list({ "10" }));
    facade.selectTab(1);

    QSignalSpy activated(&facade, &SharedMediaFacade::itemActivated);
    facade.activate(0);

    QCOMPARE(activated.count(), 0);
}

QTEST_GUILESS_MAIN(SharedMediaFacadeTest)
#include "tst_shared_media_facade.moc"
