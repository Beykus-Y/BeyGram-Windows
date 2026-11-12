#include "telegram/GlobalSearchController.h"

#include <QSignalSpy>
#include <QtTest>

using beygram::telegram::GlobalSearchController;

class GlobalSearchControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void beginTrimsQuery();
    void beginWithBlankQueryClears();
    void beginKeepsStateForSameQuery();
    void beginRestartsPagination();
    void beginDropsStaleLoading();
    void setPageStoresCursor();
    void setPageIgnoresEmptyQuery();
    void setPageKeepsOffsetVerbatim();
    void setPageAnnouncesOnlyRealChanges();
    void setPageLeavesLoadingToCaller();
    void setLoadingTogglesAndSignals();
    void clearResetsEverything();
    void clearOnEmptyStateDoesNotSignal();
    void acceptsNewSearchAfterClear();
};

void GlobalSearchControllerTest::startsEmpty()
{
    GlobalSearchController controller;

    QVERIFY(controller.query().isEmpty());
    QVERIFY(controller.nextOffset().isEmpty());
    QVERIFY(!controller.hasMore());
    QVERIFY(!controller.loading());
}

void GlobalSearchControllerTest::beginTrimsQuery()
{
    GlobalSearchController controller;
    QSignalSpy spy(&controller, &GlobalSearchController::changed);

    controller.begin(QStringLiteral("  hello world  "));

    QCOMPARE(controller.query(), QStringLiteral("hello world"));
    QVERIFY(controller.nextOffset().isEmpty());
    QVERIFY(!controller.hasMore());
    QVERIFY(!controller.loading());
    QCOMPARE(spy.count(), 1);

    // Trimming only touches the edges, never the words themselves.
    controller.begin(QStringLiteral("\t two  words \n"));
    QCOMPARE(controller.query(), QStringLiteral("two  words"));
    QCOMPARE(spy.count(), 2);
}

void GlobalSearchControllerTest::beginWithBlankQueryClears()
{
    GlobalSearchController controller;
    controller.begin(QStringLiteral("cats"));
    controller.setPage(QStringLiteral("40"), true);
    controller.setLoading(true);

    QSignalSpy spy(&controller, &GlobalSearchController::changed);
    controller.begin(QStringLiteral("   "));

    QVERIFY(controller.query().isEmpty());
    QVERIFY(controller.nextOffset().isEmpty());
    QVERIFY(!controller.hasMore());
    QVERIFY(!controller.loading());
    QCOMPARE(spy.count(), 1);
}

void GlobalSearchControllerTest::beginKeepsStateForSameQuery()
{
    GlobalSearchController controller;
    controller.begin(QStringLiteral("cats"));

    QSignalSpy spy(&controller, &GlobalSearchController::changed);
    controller.begin(QStringLiteral("  cats "));

    QCOMPARE(controller.query(), QStringLiteral("cats"));
    QVERIFY(controller.nextOffset().isEmpty());
    QVERIFY(!controller.hasMore());
    QVERIFY(!controller.loading());
    // Nothing changed, so the panel does not need to repaint.
    QCOMPARE(spy.count(), 0);
}

void GlobalSearchControllerTest::beginRestartsPagination()
{
    GlobalSearchController controller;
    controller.begin(QStringLiteral("cats"));
    controller.setPage(QStringLiteral("next-50"), true);

    QSignalSpy spy(&controller, &GlobalSearchController::changed);
    controller.begin(QStringLiteral("cats"));

    QCOMPARE(controller.query(), QStringLiteral("cats"));
    QVERIFY(controller.nextOffset().isEmpty());
    QVERIFY(!controller.hasMore());
    QCOMPARE(spy.count(), 1);
}

void GlobalSearchControllerTest::beginDropsStaleLoading()
{
    GlobalSearchController controller;
    controller.begin(QStringLiteral("cats"));
    controller.setLoading(true);

    QSignalSpy spy(&controller, &GlobalSearchController::changed);
    controller.begin(QStringLiteral("dogs"));

    QCOMPARE(controller.query(), QStringLiteral("dogs"));
    QVERIFY(!controller.loading());
    QCOMPARE(spy.count(), 1);
}

void GlobalSearchControllerTest::setPageStoresCursor()
{
    GlobalSearchController controller;
    controller.begin(QStringLiteral("cats"));

    QSignalSpy spy(&controller, &GlobalSearchController::changed);
    controller.setPage(QStringLiteral("next-50"), true);

    QCOMPARE(controller.query(), QStringLiteral("cats"));
    QCOMPARE(controller.nextOffset(), QStringLiteral("next-50"));
    QVERIFY(controller.hasMore());
    QCOMPARE(spy.count(), 1);

    // The final page carries neither a cursor nor more results.
    controller.setPage(QString(), false);
    QVERIFY(controller.nextOffset().isEmpty());
    QVERIFY(!controller.hasMore());
    QCOMPARE(spy.count(), 2);
}

void GlobalSearchControllerTest::setPageIgnoresEmptyQuery()
{
    GlobalSearchController controller;
    QSignalSpy spy(&controller, &GlobalSearchController::changed);

    controller.setPage(QStringLiteral("40"), true);

    QVERIFY(controller.query().isEmpty());
    QVERIFY(controller.nextOffset().isEmpty());
    QVERIFY(!controller.hasMore());
    QCOMPARE(spy.count(), 0);

    // A response for a cleared search must not revive it either.
    controller.begin(QStringLiteral("cats"));
    controller.clear();
    spy.clear();

    controller.setPage(QStringLiteral("80"), true);

    QVERIFY(controller.query().isEmpty());
    QVERIFY(controller.nextOffset().isEmpty());
    QVERIFY(!controller.hasMore());
    QCOMPARE(spy.count(), 0);
}

void GlobalSearchControllerTest::setPageKeepsOffsetVerbatim()
{
    GlobalSearchController controller;
    controller.begin(QStringLiteral("cats"));

    // The token is opaque: padding inside it may be significant.
    controller.setPage(QStringLiteral(" 50 / 2 "), true);

    QCOMPARE(controller.nextOffset(), QStringLiteral(" 50 / 2 "));
    QVERIFY(controller.hasMore());
}

void GlobalSearchControllerTest::setPageAnnouncesOnlyRealChanges()
{
    GlobalSearchController controller;
    controller.begin(QStringLiteral("cats"));

    QSignalSpy spy(&controller, &GlobalSearchController::changed);
    controller.setPage(QStringLiteral("40"), true);
    QCOMPARE(spy.count(), 1);

    controller.setPage(QStringLiteral("40"), true);
    QCOMPARE(spy.count(), 1);

    // Only the flag differs, which is still a visible change.
    controller.setPage(QStringLiteral("40"), false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!controller.hasMore());
}

void GlobalSearchControllerTest::setPageLeavesLoadingToCaller()
{
    GlobalSearchController controller;
    controller.begin(QStringLiteral("cats"));
    controller.setLoading(true);

    controller.setPage(QStringLiteral("40"), true);

    // A page arrives, but the caller owns the end of the request.
    QVERIFY(controller.loading());
    controller.setLoading(false);
    QVERIFY(!controller.loading());
}

void GlobalSearchControllerTest::setLoadingTogglesAndSignals()
{
    GlobalSearchController controller;
    controller.begin(QStringLiteral("cats"));

    QSignalSpy spy(&controller, &GlobalSearchController::changed);

    controller.setLoading(true);
    QVERIFY(controller.loading());
    QCOMPARE(spy.count(), 1);

    controller.setLoading(true);
    QVERIFY(controller.loading());
    QCOMPARE(spy.count(), 1);

    controller.setLoading(false);
    QVERIFY(!controller.loading());
    QCOMPARE(spy.count(), 2);
}

void GlobalSearchControllerTest::clearResetsEverything()
{
    GlobalSearchController controller;
    controller.begin(QStringLiteral("cats"));
    controller.setPage(QStringLiteral("40"), true);
    controller.setLoading(true);

    QSignalSpy spy(&controller, &GlobalSearchController::changed);
    controller.clear();

    QVERIFY(controller.query().isEmpty());
    QVERIFY(controller.nextOffset().isEmpty());
    QVERIFY(!controller.hasMore());
    QVERIFY(!controller.loading());
    QCOMPARE(spy.count(), 1);
}

void GlobalSearchControllerTest::clearOnEmptyStateDoesNotSignal()
{
    GlobalSearchController controller;

    QSignalSpy spy(&controller, &GlobalSearchController::changed);
    controller.clear();

    QVERIFY(controller.query().isEmpty());
    QVERIFY(!controller.hasMore());
    QVERIFY(!controller.loading());
    QCOMPARE(spy.count(), 0);
}

void GlobalSearchControllerTest::acceptsNewSearchAfterClear()
{
    GlobalSearchController controller;
    controller.begin(QStringLiteral("cats"));
    controller.setPage(QStringLiteral("40"), true);
    controller.clear();

    QSignalSpy spy(&controller, &GlobalSearchController::changed);
    controller.begin(QStringLiteral("dogs"));

    QCOMPARE(controller.query(), QStringLiteral("dogs"));
    QVERIFY(controller.nextOffset().isEmpty());
    QVERIFY(!controller.hasMore());
    QVERIFY(!controller.loading());
    QCOMPARE(spy.count(), 1);

    controller.setPage(QStringLiteral("20"), true);
    QCOMPARE(controller.nextOffset(), QStringLiteral("20"));
    QVERIFY(controller.hasMore());
}

QTEST_GUILESS_MAIN(GlobalSearchControllerTest)
#include "tst_global_search_controller.moc"
