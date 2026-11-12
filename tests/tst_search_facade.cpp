#include "ui/SearchFacade.h"

#include <QMetaObject>
#include <QMetaProperty>
#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

using beygram::ui::SearchFacade;

class SearchFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void beginTrimsAndRequests();
    void beginRecordsHistory();
    void beginBlankClearsWithoutRequest();
    void historyIsNewestFirstAndDeduped();
    void historyIsCapped();
    void rememberTrimsAndDedupes();
    void suggestionsFollowTheQueryPrefix();
    void suggestionsAreEmptyWithoutQuery();
    void suggestionsDropWhenHistoryClears();
    void paginationFlagsFollowPages();
    void setPageWithoutQueryIsIgnored();
    void loadingToggles();
    void loadingIsPassedThrough();
    void summaryCountsResults();
    void summaryClampsNegativeCounts();
    void summaryOnlySignalsOnChange();
    void beginResetsSummary();
    void beginKeepsSummaryForSameQuery();
    void clearResetsSearchButKeepsHistory();
    void clearReportsCountResetWithoutQuery();
    void clearOnEmptyStateDoesNotSignal();
    void removeHistoryForgetsOneEntry();
    void clearHistoryForgetsEverything();
    void changedSignalFiresOnlyOnRealChanges();
    void propertiesNotifyThroughChanged();
};

void SearchFacadeTest::startsEmpty()
{
    SearchFacade facade;

    QVERIFY(facade.query().isEmpty());
    QVERIFY(facade.history().isEmpty());
    QVERIFY(facade.suggestions().isEmpty());
    QVERIFY(!facade.loading());
    QVERIFY(!facade.hasMore());
    QCOMPARE(facade.summary(), QStringLiteral("0 chats, 0 messages, 0 files"));
}

void SearchFacadeTest::beginTrimsAndRequests()
{
    SearchFacade facade;
    QSignalSpy requests(&facade, &SearchFacade::searchRequested);

    facade.begin(QStringLiteral("  hello world  "));

    QCOMPARE(facade.query(), QStringLiteral("hello world"));
    QCOMPARE(requests.count(), 1);
    QCOMPARE(requests.at(0).at(0).toString(), QStringLiteral("hello world"));

    // Trimming only touches the edges, never the words themselves.
    facade.begin(QStringLiteral("\t two  words \n"));
    QCOMPARE(facade.query(), QStringLiteral("two  words"));
    QCOMPARE(requests.count(), 2);
    QCOMPARE(requests.at(1).at(0).toString(), QStringLiteral("two  words"));
}

void SearchFacadeTest::beginRecordsHistory()
{
    SearchFacade facade;
    QSignalSpy changed(&facade, &SearchFacade::changed);

    facade.begin(QStringLiteral("  cats  "));

    const QStringList expected{QStringLiteral("cats")};
    QCOMPARE(facade.history(), expected);
    QVERIFY(changed.count() > 0);
}

void SearchFacadeTest::beginBlankClearsWithoutRequest()
{
    SearchFacade facade;
    facade.begin(QStringLiteral("cats"));

    QSignalSpy requests(&facade, &SearchFacade::searchRequested);
    QSignalSpy changed(&facade, &SearchFacade::changed);

    facade.begin(QStringLiteral("   "));

    QVERIFY(facade.query().isEmpty());
    QCOMPARE(requests.count(), 0);
    QCOMPARE(changed.count(), 1);

    // A blank submit is never a history entry either.
    const QStringList expected{QStringLiteral("cats")};
    QCOMPARE(facade.history(), expected);
}

void SearchFacadeTest::historyIsNewestFirstAndDeduped()
{
    SearchFacade facade;

    facade.begin(QStringLiteral("cats"));
    facade.begin(QStringLiteral("dogs"));
    facade.begin(QStringLiteral("  CATS  "));

    // The repeat moves to the front and keeps the casing just typed.
    const QStringList expected{QStringLiteral("CATS"), QStringLiteral("dogs")};
    QCOMPARE(facade.history(), expected);
}

void SearchFacadeTest::historyIsCapped()
{
    SearchFacade facade;

    for (int i = 0; i < 25; ++i)
        facade.begin(QStringLiteral("query-%1").arg(i));

    QCOMPARE(facade.history().size(), 20);
    QCOMPARE(facade.history().first(), QStringLiteral("query-24"));
    QVERIFY(!facade.history().contains(QStringLiteral("query-0")));
    QVERIFY(facade.history().contains(QStringLiteral("query-5")));
}

void SearchFacadeTest::rememberTrimsAndDedupes()
{
    SearchFacade facade;
    QSignalSpy requests(&facade, &SearchFacade::searchRequested);

    facade.remember(QStringLiteral("  alice  "));
    facade.remember(QStringLiteral("bob"));
    facade.remember(QStringLiteral("ALICE"));

    const QStringList expected{QStringLiteral("ALICE"), QStringLiteral("bob")};
    QCOMPARE(facade.history(), expected);

    // Remembering is not a search: no query is applied and nothing is emitted.
    QVERIFY(facade.query().isEmpty());
    QCOMPARE(requests.count(), 0);

    facade.remember(QStringLiteral("   "));
    QCOMPARE(facade.history(), expected);
}

void SearchFacadeTest::suggestionsFollowTheQueryPrefix()
{
    SearchFacade facade;
    facade.remember(QStringLiteral("dogs"));
    facade.remember(QStringLiteral("cats"));

    facade.begin(QStringLiteral("cat"));

    // begin() records its own query, so "cat" leads the recent list.
    const QStringList history{QStringLiteral("cat"), QStringLiteral("cats"),
                              QStringLiteral("dogs")};
    QCOMPARE(facade.history(), history);

    // Only the entries that start with the query, in history order.
    const QStringList suggestions{QStringLiteral("cat"), QStringLiteral("cats")};
    QCOMPARE(facade.suggestions(), suggestions);

    // The match ignores case.
    facade.begin(QStringLiteral("CAT"));
    const QStringList upperSuggestions{QStringLiteral("CAT"), QStringLiteral("cats")};
    QCOMPARE(facade.suggestions(), upperSuggestions);
}

void SearchFacadeTest::suggestionsAreEmptyWithoutQuery()
{
    SearchFacade facade;
    facade.remember(QStringLiteral("cats"));

    QVERIFY(facade.query().isEmpty());
    QVERIFY(facade.suggestions().isEmpty());

    const QStringList expected{QStringLiteral("cats")};
    QCOMPARE(facade.history(), expected);
}

void SearchFacadeTest::suggestionsDropWhenHistoryClears()
{
    SearchFacade facade;
    facade.begin(QStringLiteral("cats"));

    const QStringList suggestions{QStringLiteral("cats")};
    QCOMPARE(facade.suggestions(), suggestions);

    facade.clearHistory();

    QVERIFY(facade.history().isEmpty());
    QVERIFY(facade.suggestions().isEmpty());
}

void SearchFacadeTest::paginationFlagsFollowPages()
{
    SearchFacade facade;
    facade.begin(QStringLiteral("cats"));

    QVERIFY(!facade.hasMore());
    QVERIFY(!facade.loading());

    facade.setPage(QStringLiteral("next-50"), true);

    QVERIFY(facade.hasMore());
    // A page arriving does not end the request; the caller owns that.
    QVERIFY(!facade.loading());

    facade.setPage(QString(), false);

    QVERIFY(!facade.hasMore());
}

void SearchFacadeTest::setPageWithoutQueryIsIgnored()
{
    SearchFacade facade;
    facade.setPage(QStringLiteral("40"), true);
    QVERIFY(!facade.hasMore());

    // A late page for a cleared search must not revive it either.
    facade.begin(QStringLiteral("cats"));
    facade.clear();
    facade.setPage(QStringLiteral("80"), true);

    QVERIFY(facade.query().isEmpty());
    QVERIFY(!facade.hasMore());
}

void SearchFacadeTest::loadingToggles()
{
    SearchFacade facade;
    facade.begin(QStringLiteral("cats"));

    QSignalSpy changed(&facade, &SearchFacade::changed);

    facade.setLoading(true);
    QVERIFY(facade.loading());
    QCOMPARE(changed.count(), 1);

    facade.setLoading(true);
    QVERIFY(facade.loading());
    QCOMPARE(changed.count(), 1);

    facade.setLoading(false);
    QVERIFY(!facade.loading());
    QCOMPARE(changed.count(), 2);
}

void SearchFacadeTest::loadingIsPassedThrough()
{
    SearchFacade facade;

    // The facade does not invent a query requirement: the request layer owns
    // when a request is in flight, and begin()/clear() reset the flag.
    facade.setLoading(true);
    QVERIFY(facade.loading());

    facade.setLoading(false);
    QVERIFY(!facade.loading());
}

void SearchFacadeTest::summaryCountsResults()
{
    SearchFacade facade;

    facade.setResultCounts(3, 5, 2);

    QCOMPARE(facade.summary(), QStringLiteral("3 chats, 5 messages, 2 files"));
}

void SearchFacadeTest::summaryClampsNegativeCounts()
{
    SearchFacade facade;

    facade.setResultCounts(-4, -2, -9);

    QCOMPARE(facade.summary(), QStringLiteral("0 chats, 0 messages, 0 files"));
}

void SearchFacadeTest::summaryOnlySignalsOnChange()
{
    SearchFacade facade;
    QSignalSpy changed(&facade, &SearchFacade::changed);

    facade.setResultCounts(1, 1, 1);
    QCOMPARE(changed.count(), 1);

    facade.setResultCounts(1, 1, 1);
    QCOMPARE(changed.count(), 1);

    facade.setResultCounts(1, 1, 2);
    QCOMPARE(changed.count(), 2);
    QCOMPARE(facade.summary(), QStringLiteral("1 chats, 1 messages, 2 files"));
}

void SearchFacadeTest::beginResetsSummary()
{
    SearchFacade facade;
    facade.begin(QStringLiteral("cats"));
    facade.setResultCounts(2, 7, 3);

    facade.begin(QStringLiteral("dogs"));

    QCOMPARE(facade.query(), QStringLiteral("dogs"));
    QCOMPARE(facade.summary(), QStringLiteral("0 chats, 0 messages, 0 files"));
}

void SearchFacadeTest::beginKeepsSummaryForSameQuery()
{
    SearchFacade facade;
    facade.begin(QStringLiteral("cats"));
    facade.setResultCounts(2, 7, 3);

    QSignalSpy changed(&facade, &SearchFacade::changed);
    facade.begin(QStringLiteral("  cats "));

    QCOMPARE(facade.query(), QStringLiteral("cats"));
    // Re-submitting the same query keeps the page already on screen.
    QCOMPARE(facade.summary(), QStringLiteral("2 chats, 7 messages, 3 files"));
    QCOMPARE(changed.count(), 0);
}

void SearchFacadeTest::clearResetsSearchButKeepsHistory()
{
    SearchFacade facade;
    facade.begin(QStringLiteral("cats"));
    facade.setPage(QStringLiteral("40"), true);
    facade.setLoading(true);
    facade.setResultCounts(2, 7, 3);

    QSignalSpy changed(&facade, &SearchFacade::changed);
    facade.clear();

    QVERIFY(facade.query().isEmpty());
    QVERIFY(!facade.hasMore());
    QVERIFY(!facade.loading());
    QCOMPARE(facade.summary(), QStringLiteral("0 chats, 0 messages, 0 files"));
    QCOMPARE(changed.count(), 1);

    // Clearing the search is not clearing the history.
    const QStringList expected{QStringLiteral("cats")};
    QCOMPARE(facade.history(), expected);
}

void SearchFacadeTest::clearReportsCountResetWithoutQuery()
{
    SearchFacade facade;
    facade.setResultCounts(1, 2, 3);

    QSignalSpy changed(&facade, &SearchFacade::changed);
    facade.clear();

    QCOMPARE(changed.count(), 1);
    QCOMPARE(facade.summary(), QStringLiteral("0 chats, 0 messages, 0 files"));
}

void SearchFacadeTest::clearOnEmptyStateDoesNotSignal()
{
    SearchFacade facade;
    QSignalSpy changed(&facade, &SearchFacade::changed);

    facade.clear();

    QVERIFY(facade.query().isEmpty());
    QVERIFY(!facade.hasMore());
    QVERIFY(!facade.loading());
    QCOMPARE(changed.count(), 0);
}

void SearchFacadeTest::removeHistoryForgetsOneEntry()
{
    SearchFacade facade;
    facade.begin(QStringLiteral("cats"));
    facade.begin(QStringLiteral("dogs"));

    // Matching ignores case and surrounding whitespace.
    facade.removeHistory(QStringLiteral("  CATS "));

    const QStringList expected{QStringLiteral("dogs")};
    QCOMPARE(facade.history(), expected);

    // Removing something that is not there changes nothing.
    facade.removeHistory(QStringLiteral("missing"));
    QCOMPARE(facade.history(), expected);
}

void SearchFacadeTest::clearHistoryForgetsEverything()
{
    SearchFacade facade;
    facade.begin(QStringLiteral("cats"));
    facade.begin(QStringLiteral("dogs"));

    QSignalSpy changed(&facade, &SearchFacade::changed);
    facade.clearHistory();

    QVERIFY(facade.history().isEmpty());
    QCOMPARE(changed.count(), 1);
    // The active search survives a history wipe.
    QCOMPARE(facade.query(), QStringLiteral("dogs"));
}

void SearchFacadeTest::changedSignalFiresOnlyOnRealChanges()
{
    SearchFacade facade;
    QSignalSpy spy(&facade, &SearchFacade::changed);
    QVERIFY(spy.isValid());

    facade.begin(QStringLiteral("cats"));
    QCOMPARE(spy.count(), 1);

    // The same query with clean state is a no-op for the panel.
    facade.begin(QStringLiteral("  cats "));
    QCOMPARE(spy.count(), 1);

    // A different query is one repaint, not one per child.
    facade.begin(QStringLiteral("dogs"));
    QCOMPARE(spy.count(), 2);

    facade.setPage(QStringLiteral("40"), true);
    QCOMPARE(spy.count(), 3);
    facade.setPage(QStringLiteral("40"), true);
    QCOMPARE(spy.count(), 3);

    facade.setLoading(true);
    QCOMPARE(spy.count(), 4);

    // A page's counts and the end of the request are separate changes.
    facade.setResultCounts(2, 5, 1);
    QCOMPARE(spy.count(), 5);
    facade.setLoading(false);
    QCOMPARE(spy.count(), 6);

    // clear() reports once even though it drops the query, the cursor, the
    // flag and the counts together.
    facade.clear();
    QCOMPARE(spy.count(), 7);
    facade.clear();
    QCOMPARE(spy.count(), 7);
}

void SearchFacadeTest::propertiesNotifyThroughChanged()
{
    SearchFacade facade;
    const QMetaObject* meta = facade.metaObject();

    const QStringList names{QStringLiteral("query"), QStringLiteral("history"),
                            QStringLiteral("suggestions"), QStringLiteral("loading"),
                            QStringLiteral("hasMore"), QStringLiteral("summary")};
    for (const QString& name : names) {
        const int index = meta->indexOfProperty(name.toLatin1().constData());
        QVERIFY2(index >= 0, qPrintable(name));

        const QMetaProperty property = meta->property(index);
        QVERIFY2(property.hasNotifySignal(), qPrintable(name));
        QCOMPARE(QByteArray(property.notifySignal().name()), QByteArrayLiteral("changed"));
    }

    QVERIFY(meta->indexOfSignal("changed()") >= 0);
    QVERIFY(meta->indexOfSignal("searchRequested(QString)") >= 0);
    QVERIFY(meta->indexOfMethod("begin(QString)") >= 0);
    QVERIFY(meta->indexOfMethod("setResultCounts(int,int,int)") >= 0);
}

QTEST_GUILESS_MAIN(SearchFacadeTest)
#include "tst_search_facade.moc"
