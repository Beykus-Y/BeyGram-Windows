#include "telegram/StickerSearchController.h"

#include <QSignalSpy>
#include <QtTest>

using beygram::telegram::StickerSearchController;

class StickerSearchControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setQueryTrims();
    void setQueryKeepsInnerSpacing();
    void setQuerySameIsNoOp();
    void setQueryChangeDropsResultsAndLoading();
    void setQueryBlankClearsEverything();
    void setQueryBlankOnEmptyStateDoesNotSignal();
    void setResultsDropsInvalidAndDuplicateIds();
    void setResultsReplacesPreviousPage();
    void setResultsIgnoresEmptyQuery();
    void setResultsAnnouncesOnlyRealChanges();
    void setResultsAcceptsEmptyListToClear();
    void appendResultsAddsNewIdsInOrder();
    void appendResultsIgnoresEmptyQuery();
    void appendResultsAnnouncesOnlyWhenNewIdsExist();
    void setLoadingTogglesAndSignals();
    void setLoadingNoOpWhenUnchanged();
    void clearResetsEverything();
    void clearOnEmptyStateDoesNotSignal();
    void acceptsNewSearchAfterClear();
    void resultsIsASnapshot();
    void changedCountsAcrossOperations();
};

void StickerSearchControllerTest::startsEmpty()
{
    StickerSearchController controller;

    QVERIFY(controller.query().isEmpty());
    QVERIFY(controller.results().isEmpty());
    QVERIFY(!controller.loading());
}

void StickerSearchControllerTest::setQueryTrims()
{
    StickerSearchController controller;
    QSignalSpy spy(&controller, &StickerSearchController::changed);

    controller.setQuery(QStringLiteral("  cat  "));

    QCOMPARE(controller.query(), QStringLiteral("cat"));
    QCOMPARE(spy.count(), 1);

    // Edge whitespace only; the same query in other clothes stays a no-op.
    controller.setQuery(QStringLiteral("\tcat\n"));
    QCOMPARE(controller.query(), QStringLiteral("cat"));
    QCOMPARE(spy.count(), 1);
}

void StickerSearchControllerTest::setQueryKeepsInnerSpacing()
{
    StickerSearchController controller;

    controller.setQuery(QStringLiteral("  happy  dog  "));

    QCOMPARE(controller.query(), QStringLiteral("happy  dog"));
}

void StickerSearchControllerTest::setQuerySameIsNoOp()
{
    StickerSearchController controller;
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({1, 2});
    controller.setLoading(true);

    QSignalSpy spy(&controller, &StickerSearchController::changed);

    controller.setQuery(QStringLiteral("cat"));
    controller.setQuery(QStringLiteral("  cat  "));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.query(), QStringLiteral("cat"));
    QCOMPARE(controller.results(), (QVector<qint64>{1, 2}));
    QVERIFY(controller.loading());
}

void StickerSearchControllerTest::setQueryChangeDropsResultsAndLoading()
{
    StickerSearchController controller;
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({1, 2});
    controller.setLoading(true);

    QSignalSpy spy(&controller, &StickerSearchController::changed);

    controller.setQuery(QStringLiteral("dog"));

    QCOMPARE(controller.query(), QStringLiteral("dog"));
    QVERIFY(controller.results().isEmpty());
    QVERIFY(!controller.loading());
    QCOMPARE(spy.count(), 1);
}

void StickerSearchControllerTest::setQueryBlankClearsEverything()
{
    StickerSearchController controller;
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({1, 2});
    controller.setLoading(true);

    QSignalSpy spy(&controller, &StickerSearchController::changed);

    controller.setQuery(QStringLiteral("   "));

    QVERIFY(controller.query().isEmpty());
    QVERIFY(controller.results().isEmpty());
    QVERIFY(!controller.loading());
    QCOMPARE(spy.count(), 1);
}

void StickerSearchControllerTest::setQueryBlankOnEmptyStateDoesNotSignal()
{
    StickerSearchController controller;
    QSignalSpy spy(&controller, &StickerSearchController::changed);

    controller.setQuery(QString());
    controller.setQuery(QStringLiteral("  \t "));

    QCOMPARE(spy.count(), 0);
    QVERIFY(controller.query().isEmpty());
}

void StickerSearchControllerTest::setResultsDropsInvalidAndDuplicateIds()
{
    StickerSearchController controller;
    controller.setQuery(QStringLiteral("cat"));

    QSignalSpy spy(&controller, &StickerSearchController::changed);

    controller.setResults({0, -1, 3, 3, 5, 2, 5, -7});

    QCOMPARE(controller.results(), (QVector<qint64>{3, 5, 2}));
    QCOMPARE(spy.count(), 1);
}

void StickerSearchControllerTest::setResultsReplacesPreviousPage()
{
    StickerSearchController controller;
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({1, 2});

    QSignalSpy spy(&controller, &StickerSearchController::changed);

    controller.setResults({9});

    QCOMPARE(controller.results(), (QVector<qint64>{9}));
    QCOMPARE(spy.count(), 1);
}

void StickerSearchControllerTest::setResultsIgnoresEmptyQuery()
{
    StickerSearchController controller;
    QSignalSpy spy(&controller, &StickerSearchController::changed);

    controller.setResults({1, 2});

    QVERIFY(controller.results().isEmpty());
    QCOMPARE(spy.count(), 0);

    // A stale response after clear() must not revive the results either.
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({3});
    controller.clear();

    controller.setResults({4});

    QVERIFY(controller.results().isEmpty());
}

void StickerSearchControllerTest::setResultsAnnouncesOnlyRealChanges()
{
    StickerSearchController controller;
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({1, 2});

    QSignalSpy spy(&controller, &StickerSearchController::changed);

    controller.setResults({1, 2});   // no-op
    controller.setResults({2, 1});   // order is part of the value

    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.results(), (QVector<qint64>{2, 1}));
}

void StickerSearchControllerTest::setResultsAcceptsEmptyListToClear()
{
    StickerSearchController controller;
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({1, 2});

    QSignalSpy spy(&controller, &StickerSearchController::changed);

    controller.setResults({});
    QCOMPARE(spy.count(), 1);
    QVERIFY(controller.results().isEmpty());

    controller.setResults({});
    QCOMPARE(spy.count(), 1);
}

void StickerSearchControllerTest::appendResultsAddsNewIdsInOrder()
{
    StickerSearchController controller;
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({1, 2});

    QSignalSpy spy(&controller, &StickerSearchController::changed);

    controller.appendResults({2, 3, 0, -4, 3, 4});

    QCOMPARE(controller.results(), (QVector<qint64>{1, 2, 3, 4}));
    QCOMPARE(spy.count(), 1);
}

void StickerSearchControllerTest::appendResultsIgnoresEmptyQuery()
{
    StickerSearchController controller;
    QSignalSpy spy(&controller, &StickerSearchController::changed);

    controller.appendResults({1, 2});

    QVERIFY(controller.results().isEmpty());
    QCOMPARE(spy.count(), 0);

    controller.setQuery(QStringLiteral("cat"));
    controller.appendResults({5});

    QCOMPARE(controller.results(), (QVector<qint64>{5}));
}

void StickerSearchControllerTest::appendResultsAnnouncesOnlyWhenNewIdsExist()
{
    StickerSearchController controller;
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({1, 2});

    QSignalSpy spy(&controller, &StickerSearchController::changed);

    controller.appendResults({});
    controller.appendResults({0, -1});
    controller.appendResults({2, 1});   // already known

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.results(), (QVector<qint64>{1, 2}));

    controller.appendResults({3});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.results(), (QVector<qint64>{1, 2, 3}));
}

void StickerSearchControllerTest::setLoadingTogglesAndSignals()
{
    StickerSearchController controller;
    QSignalSpy spy(&controller, &StickerSearchController::changed);

    controller.setLoading(true);
    QVERIFY(controller.loading());
    QCOMPARE(spy.count(), 1);

    controller.setLoading(false);
    QVERIFY(!controller.loading());
    QCOMPARE(spy.count(), 2);
}

void StickerSearchControllerTest::setLoadingNoOpWhenUnchanged()
{
    StickerSearchController controller;
    QSignalSpy spy(&controller, &StickerSearchController::changed);

    controller.setLoading(false);
    QCOMPARE(spy.count(), 0);

    controller.setLoading(true);
    controller.setLoading(true);
    QCOMPARE(spy.count(), 1);
}

void StickerSearchControllerTest::clearResetsEverything()
{
    StickerSearchController controller;
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({1, 2});
    controller.setLoading(true);

    QSignalSpy spy(&controller, &StickerSearchController::changed);

    controller.clear();

    QVERIFY(controller.query().isEmpty());
    QVERIFY(controller.results().isEmpty());
    QVERIFY(!controller.loading());
    QCOMPARE(spy.count(), 1);
}

void StickerSearchControllerTest::clearOnEmptyStateDoesNotSignal()
{
    StickerSearchController controller;
    QSignalSpy spy(&controller, &StickerSearchController::changed);

    controller.clear();
    controller.clear();

    QCOMPARE(spy.count(), 0);
}

void StickerSearchControllerTest::acceptsNewSearchAfterClear()
{
    StickerSearchController controller;
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({1});
    controller.setLoading(true);
    controller.clear();

    QSignalSpy spy(&controller, &StickerSearchController::changed);

    controller.setQuery(QStringLiteral("dog"));
    QCOMPARE(controller.query(), QStringLiteral("dog"));
    QCOMPARE(spy.count(), 1);

    controller.setResults({7, 8});
    QCOMPARE(controller.results(), (QVector<qint64>{7, 8}));
    QCOMPARE(spy.count(), 2);

    controller.setLoading(true);
    QVERIFY(controller.loading());
    QCOMPARE(spy.count(), 3);
}

void StickerSearchControllerTest::resultsIsASnapshot()
{
    StickerSearchController controller;
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({10, 20});

    QVector<qint64> snapshot = controller.results();
    QCOMPARE(snapshot, (QVector<qint64>{10, 20}));

    controller.appendResults({30});
    QCOMPARE(snapshot, (QVector<qint64>{10, 20}));
    QCOMPARE(controller.results(), (QVector<qint64>{10, 20, 30}));

    // Mutating the snapshot must not touch the stored page.
    snapshot.append(99);
    QCOMPARE(controller.results(), (QVector<qint64>{10, 20, 30}));
}

void StickerSearchControllerTest::changedCountsAcrossOperations()
{
    StickerSearchController controller;
    QSignalSpy spy(&controller, &StickerSearchController::changed);

    controller.setQuery(QStringLiteral("cat"));      // 1
    controller.setQuery(QStringLiteral("cat"));      // no-op
    controller.setResults({1, 2});                   // 2
    controller.setLoading(true);                     // 3
    controller.appendResults({2, 3});                // 4
    controller.appendResults({3});                   // no-op
    controller.setResults({1, 2, 3});                // no-op
    controller.setQuery(QStringLiteral("  cat "));   // no-op
    controller.setLoading(false);                    // 5
    controller.setQuery(QStringLiteral("dog"));      // 6
    controller.setResults({7});                      // 7
    controller.setQuery(QStringLiteral("   "));      // 8
    controller.clear();                              // no-op

    QCOMPARE(spy.count(), 8);
    QVERIFY(controller.query().isEmpty());
    QVERIFY(controller.results().isEmpty());
    QVERIFY(!controller.loading());
}

QTEST_GUILESS_MAIN(StickerSearchControllerTest)
#include "tst_sticker_search_controller.moc"
