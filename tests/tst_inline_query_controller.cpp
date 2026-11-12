#include "telegram/InlineQueryController.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

InlineResultRow row(const QString& id, qint64 messageId = 0)
{
    return InlineResultRow{id, QStringLiteral("title-") + id,
        QStringLiteral("description-") + id, messageId};
}

} // namespace

class InlineQueryControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsInactiveAndEmpty();
    void openRejectsNonPositiveIds();
    void openActivatesSession();
    void openSameBotKeepsState();
    void openDifferentBotResetsSession();
    void closeResetsSession();
    void closeOnInactiveIsSilent();
    void setQueryTrims();
    void setQueryEmitsOnlyOnChange();
    void setQueryResetsAnswerSelectionAndLoading();
    void emptyQueryClearsResultsAndSelection();
    void setResultsRequiresOpenSession();
    void setResultsRequiresNonEmptyQuery();
    void setResultsDropsRowsWithoutId();
    void setResultsKeepsFirstDuplicate();
    void setResultsEmitsOnlyOnChange();
    void setResultsClearsStaleSelection();
    void setLoadingEmitsOnlyOnChange();
    void setSelectedIndexAcceptsInRangeIndex();
    void setSelectedIndexIgnoresOutOfRange();
    void setSelectedIndexMinusOneClears();
    void selectedReturnsRowOrDefault();
    void clearResetsSession();
    void clearIsSilentWhenAlreadyReset();
    void changedOncePerRealTransition();
};

void InlineQueryControllerTest::startsInactiveAndEmpty()
{
    const InlineQueryController controller;

    QVERIFY(!controller.active());
    QCOMPARE(controller.botUserId(), 0);
    QVERIFY(controller.query().isEmpty());
    QVERIFY(controller.results().isEmpty());
    QVERIFY(!controller.loading());
    QCOMPARE(controller.selectedIndex(), -1);
}

void InlineQueryControllerTest::openRejectsNonPositiveIds()
{
    InlineQueryController controller;
    QSignalSpy spy(&controller, &InlineQueryController::changed);

    // Zero and negative ids are not TDLib user ids, so they never open a
    // session and never report a change.
    controller.open(0);
    controller.open(-1);
    controller.open(-4242424242LL);

    QVERIFY(!controller.active());
    QCOMPARE(controller.botUserId(), 0);
    QCOMPARE(spy.count(), 0);
}

void InlineQueryControllerTest::openActivatesSession()
{
    InlineQueryController controller;
    QSignalSpy spy(&controller, &InlineQueryController::changed);

    controller.open(4242424242LL);

    QVERIFY(controller.active());
    QCOMPARE(controller.botUserId(), 4242424242LL);
    QCOMPARE(spy.count(), 1);
}

void InlineQueryControllerTest::openSameBotKeepsState()
{
    InlineQueryController controller;

    controller.open(7);
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({row(QStringLiteral("a")), row(QStringLiteral("b"))});
    controller.setSelectedIndex(1);

    QSignalSpy spy(&controller, &InlineQueryController::changed);
    controller.open(7);

    // Re-entering the same bot must not throw the answer away.
    QVERIFY(controller.active());
    QCOMPARE(controller.query(), QStringLiteral("cat"));
    QCOMPARE(controller.results().size(), 2);
    QCOMPARE(controller.selectedIndex(), 1);
    QCOMPARE(spy.count(), 0);
}

void InlineQueryControllerTest::openDifferentBotResetsSession()
{
    InlineQueryController controller;

    controller.open(7);
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({row(QStringLiteral("a"))});
    controller.setSelectedIndex(0);
    controller.setLoading(true);

    QSignalSpy spy(&controller, &InlineQueryController::changed);
    controller.open(8);

    // The old answer belonged to the old bot.
    QVERIFY(controller.active());
    QCOMPARE(controller.botUserId(), 8);
    QVERIFY(controller.query().isEmpty());
    QVERIFY(controller.results().isEmpty());
    QVERIFY(!controller.loading());
    QCOMPARE(controller.selectedIndex(), -1);
    QCOMPARE(spy.count(), 1);
}

void InlineQueryControllerTest::closeResetsSession()
{
    InlineQueryController controller;

    controller.open(7);
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({row(QStringLiteral("a"))});
    controller.setSelectedIndex(0);
    controller.setLoading(true);

    QSignalSpy spy(&controller, &InlineQueryController::changed);
    controller.close();

    QVERIFY(!controller.active());
    QCOMPARE(controller.botUserId(), 0);
    QVERIFY(controller.query().isEmpty());
    QVERIFY(controller.results().isEmpty());
    QVERIFY(!controller.loading());
    QCOMPARE(controller.selectedIndex(), -1);
    QCOMPARE(spy.count(), 1);
}

void InlineQueryControllerTest::closeOnInactiveIsSilent()
{
    InlineQueryController controller;
    QSignalSpy spy(&controller, &InlineQueryController::changed);

    controller.close();
    controller.close();

    QVERIFY(!controller.active());
    QCOMPARE(spy.count(), 0);
}

void InlineQueryControllerTest::setQueryTrims()
{
    InlineQueryController controller;
    controller.open(7);

    controller.setQuery(QStringLiteral("  cat\n"));
    QCOMPARE(controller.query(), QStringLiteral("cat"));

    // Whitespace-only text is the empty query, not a query of spaces.
    controller.setQuery(QStringLiteral("   \t  "));
    QVERIFY(controller.query().isEmpty());
}

void InlineQueryControllerTest::setQueryEmitsOnlyOnChange()
{
    InlineQueryController controller;
    controller.open(7);
    QSignalSpy spy(&controller, &InlineQueryController::changed);

    controller.setQuery(QStringLiteral("cat"));
    QCOMPARE(spy.count(), 1);

    // The same trimmed text is not a new query.
    controller.setQuery(QStringLiteral("  cat  "));
    QCOMPARE(spy.count(), 1);

    controller.setQuery(QStringLiteral("cats"));
    QCOMPARE(spy.count(), 2);
}

void InlineQueryControllerTest::setQueryResetsAnswerSelectionAndLoading()
{
    InlineQueryController controller;

    controller.open(7);
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({row(QStringLiteral("a")), row(QStringLiteral("b"))});
    controller.setSelectedIndex(0);
    controller.setLoading(true);

    controller.setQuery(QStringLiteral("dog"));

    QCOMPARE(controller.query(), QStringLiteral("dog"));
    QVERIFY(controller.results().isEmpty());
    QVERIFY(!controller.loading());
    QCOMPARE(controller.selectedIndex(), -1);
    QVERIFY(controller.selected().id.isEmpty());
}

void InlineQueryControllerTest::emptyQueryClearsResultsAndSelection()
{
    InlineQueryController controller;

    controller.open(7);
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({row(QStringLiteral("a"))});
    controller.setSelectedIndex(0);

    controller.setQuery(QStringLiteral("   "));

    QVERIFY(controller.query().isEmpty());
    QVERIFY(controller.results().isEmpty());
    QCOMPARE(controller.selectedIndex(), -1);
    QVERIFY(controller.active());
}

void InlineQueryControllerTest::setResultsRequiresOpenSession()
{
    InlineQueryController controller;
    controller.setQuery(QStringLiteral("cat"));

    QSignalSpy spy(&controller, &InlineQueryController::changed);
    controller.setResults({row(QStringLiteral("a"))});

    QVERIFY(controller.results().isEmpty());
    QCOMPARE(spy.count(), 0);
}

void InlineQueryControllerTest::setResultsRequiresNonEmptyQuery()
{
    InlineQueryController controller;
    controller.open(7);
    QSignalSpy spy(&controller, &InlineQueryController::changed);

    controller.setResults({row(QStringLiteral("a"))});

    QVERIFY(controller.results().isEmpty());
    QCOMPARE(spy.count(), 0);

    // A query made only of whitespace is still empty.
    controller.setQuery(QStringLiteral("   "));
    QCOMPARE(spy.count(), 0);
    controller.setResults({row(QStringLiteral("a"))});
    QVERIFY(controller.results().isEmpty());
    QCOMPARE(spy.count(), 0);
}

void InlineQueryControllerTest::setResultsDropsRowsWithoutId()
{
    InlineQueryController controller;
    controller.open(7);
    controller.setQuery(QStringLiteral("cat"));

    controller.setResults({row(QString(), 11), row(QStringLiteral("a")), row(QString(), 12)});

    const QVector<InlineResultRow> stored = controller.results();
    QCOMPARE(stored.size(), 1);
    QCOMPARE(stored.at(0).id, QStringLiteral("a"));

    // A list made only of rejected rows leaves the answer empty.
    controller.setResults({row(QString()), row(QString())});
    QVERIFY(controller.results().isEmpty());
}

void InlineQueryControllerTest::setResultsKeepsFirstDuplicate()
{
    InlineQueryController controller;
    controller.open(7);
    controller.setQuery(QStringLiteral("cat"));

    controller.setResults({row(QStringLiteral("a"), 1), row(QStringLiteral("b"), 2),
        row(QStringLiteral("a"), 3), row(QStringLiteral("c"), 4),
        row(QStringLiteral("b"), 5)});

    const QVector<InlineResultRow> stored = controller.results();
    QCOMPARE(stored.size(), 3);
    QCOMPARE(stored.at(0).messageId, 1);
    QCOMPARE(stored.at(1).messageId, 2);
    QCOMPARE(stored.at(2).id, QStringLiteral("c"));

    // The order is the TDLib order; the controller must not sort the rows.
    QCOMPARE(stored.at(0).id, QStringLiteral("a"));
    QCOMPARE(stored.at(1).id, QStringLiteral("b"));
}

void InlineQueryControllerTest::setResultsEmitsOnlyOnChange()
{
    InlineQueryController controller;
    controller.open(7);
    controller.setQuery(QStringLiteral("cat"));
    QSignalSpy spy(&controller, &InlineQueryController::changed);

    controller.setResults({row(QStringLiteral("a"))});
    QCOMPARE(spy.count(), 1);

    controller.setResults({row(QStringLiteral("a"))});
    QCOMPARE(spy.count(), 1);

    // A different id with the same payload is still a different answer.
    controller.setResults({row(QStringLiteral("b"))});
    QCOMPARE(spy.count(), 2);

    // Normalisation to the same rows must stay silent.
    controller.setResults({row(QStringLiteral("b")), row(QStringLiteral("b"))});
    QCOMPARE(spy.count(), 2);

    controller.setResults({});
    QCOMPARE(spy.count(), 3);
}

void InlineQueryControllerTest::setResultsClearsStaleSelection()
{
    InlineQueryController controller;
    controller.open(7);
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({row(QStringLiteral("a")), row(QStringLiteral("b")),
        row(QStringLiteral("c"))});

    controller.setSelectedIndex(2);

    // The selection survives an answer that still contains the index.
    controller.setResults({row(QStringLiteral("d")), row(QStringLiteral("e")),
        row(QStringLiteral("f"))});
    QCOMPARE(controller.selectedIndex(), 2);
    QCOMPARE(controller.selected().id, QStringLiteral("f"));

    // A shorter answer cannot keep it.
    controller.setResults({row(QStringLiteral("d"))});
    QCOMPARE(controller.selectedIndex(), -1);
    QVERIFY(controller.selected().id.isEmpty());
}

void InlineQueryControllerTest::setLoadingEmitsOnlyOnChange()
{
    InlineQueryController controller;
    controller.open(7);
    QSignalSpy spy(&controller, &InlineQueryController::changed);

    controller.setLoading(false);
    QCOMPARE(spy.count(), 0);

    controller.setLoading(true);
    QVERIFY(controller.loading());
    QCOMPARE(spy.count(), 1);

    controller.setLoading(true);
    QCOMPARE(spy.count(), 1);

    controller.setLoading(false);
    QVERIFY(!controller.loading());
    QCOMPARE(spy.count(), 2);
}

void InlineQueryControllerTest::setSelectedIndexAcceptsInRangeIndex()
{
    InlineQueryController controller;
    controller.open(7);
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({row(QStringLiteral("a")), row(QStringLiteral("b"))});

    controller.setSelectedIndex(0);
    QCOMPARE(controller.selectedIndex(), 0);
    QCOMPARE(controller.selected().id, QStringLiteral("a"));

    controller.setSelectedIndex(1);
    QCOMPARE(controller.selectedIndex(), 1);
    QCOMPARE(controller.selected().id, QStringLiteral("b"));
}

void InlineQueryControllerTest::setSelectedIndexIgnoresOutOfRange()
{
    InlineQueryController controller;
    controller.open(7);
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({row(QStringLiteral("a")), row(QStringLiteral("b"))});
    controller.setSelectedIndex(1);

    QSignalSpy spy(&controller, &InlineQueryController::changed);

    controller.setSelectedIndex(2);
    controller.setSelectedIndex(-2);
    controller.setSelectedIndex(1000);

    QCOMPARE(controller.selectedIndex(), 1);
    QCOMPARE(spy.count(), 0);

    // With no answer at all, only -1 is accepted.
    controller.setResults({});
    QCOMPARE(controller.selectedIndex(), -1);
    controller.setSelectedIndex(0);
    QCOMPARE(controller.selectedIndex(), -1);
}

void InlineQueryControllerTest::setSelectedIndexMinusOneClears()
{
    InlineQueryController controller;
    controller.open(7);
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({row(QStringLiteral("a"))});
    controller.setSelectedIndex(0);

    QSignalSpy spy(&controller, &InlineQueryController::changed);
    controller.setSelectedIndex(-1);

    QCOMPARE(controller.selectedIndex(), -1);
    QCOMPARE(spy.count(), 1);

    // Clearing an already clear selection is silent.
    controller.setSelectedIndex(-1);
    QCOMPARE(spy.count(), 1);
}

void InlineQueryControllerTest::selectedReturnsRowOrDefault()
{
    InlineQueryController controller;

    // Nothing is selected before a session exists.
    const InlineResultRow fallback = controller.selected();
    QVERIFY(fallback.id.isEmpty());
    QVERIFY(fallback.title.isEmpty());
    QVERIFY(fallback.description.isEmpty());
    QCOMPARE(fallback.messageId, 0);

    controller.open(7);
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({row(QStringLiteral("a"), 42)});

    // Results without a selection still yield the default row.
    const InlineResultRow unselected = controller.selected();
    QVERIFY(unselected.id.isEmpty());
    QVERIFY(unselected.title.isEmpty());
    QCOMPARE(unselected.messageId, 0);

    controller.setSelectedIndex(0);
    const InlineResultRow chosen = controller.selected();
    QCOMPARE(chosen.id, QStringLiteral("a"));
    QCOMPARE(chosen.title, QStringLiteral("title-a"));
    QCOMPARE(chosen.description, QStringLiteral("description-a"));
    QCOMPARE(chosen.messageId, 42);
}

void InlineQueryControllerTest::clearResetsSession()
{
    InlineQueryController controller;

    controller.open(7);
    controller.setQuery(QStringLiteral("cat"));
    controller.setResults({row(QStringLiteral("a"))});
    controller.setSelectedIndex(0);
    controller.setLoading(true);

    QSignalSpy spy(&controller, &InlineQueryController::changed);
    controller.clear();

    QVERIFY(!controller.active());
    QCOMPARE(controller.botUserId(), 0);
    QVERIFY(controller.query().isEmpty());
    QVERIFY(controller.results().isEmpty());
    QVERIFY(!controller.loading());
    QCOMPARE(controller.selectedIndex(), -1);
    QCOMPARE(spy.count(), 1);
}

void InlineQueryControllerTest::clearIsSilentWhenAlreadyReset()
{
    InlineQueryController controller;
    QSignalSpy spy(&controller, &InlineQueryController::changed);

    controller.clear();

    QCOMPARE(spy.count(), 0);
    QVERIFY(!controller.active());
}

void InlineQueryControllerTest::changedOncePerRealTransition()
{
    InlineQueryController controller;
    QSignalSpy spy(&controller, &InlineQueryController::changed);

    controller.open(7);                              // 1
    controller.open(7);                              // no-op
    controller.setQuery(QStringLiteral("cat"));      // 2
    controller.setQuery(QStringLiteral("  cat "));   // no-op
    controller.setLoading(true);                     // 3
    controller.setLoading(true);                     // no-op
    controller.setResults({row(QStringLiteral("a")), row(QStringLiteral("b"))}); // 4
    controller.setSelectedIndex(1);                  // 5
    controller.setSelectedIndex(1);                  // no-op
    controller.setSelectedIndex(9);                  // no-op
    controller.setResults({row(QStringLiteral("a"))}); // 6, drops the selection
    controller.setSelectedIndex(-1);                 // no-op, already cleared
    controller.close();                              // 7
    controller.close();                              // no-op

    QCOMPARE(spy.count(), 7);
}

QTEST_GUILESS_MAIN(InlineQueryControllerTest)
#include "tst_inline_query_controller.moc"
