#include "telegram/SearchHistoryStore.h"

#include <QSignalSpy>
#include <QtTest>

using beygram::telegram::SearchHistoryStore;

class SearchHistoryStoreTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void addKeepsNewestFirst();
    void addTrimsQuery();
    void addIgnoresEmptyQuery();
    void addIgnoresWhitespaceOnlyQuery();
    void addMovesExistingEntryToFront();
    void addComparesCaseInsensitively();
    void addStoresLatestCasing();
    void addOfFrontEntryDoesNotSignal();
    void addCapsAtMaxEntries();
    void addClampsMaxEntriesToOne();
    void addUpdatesCapForLaterCalls();
    void removeDeletesCaseInsensitively();
    void removeMissingEntryReturnsFalse();
    void removeLeavesOtherEntriesInOrder();
    void matchingIsCaseInsensitive();
    void matchingPreservesHistoryOrder();
    void matchingEmptyPrefixReturnsEverything();
    void matchingWithoutHitsIsEmpty();
    void clearEmptiesHistory();
    void clearOnEmptyStoreDoesNotSignal();
    void entriesReturnsIndependentCopy();
    void changedFiresOnlyOnRealChanges();
};

void SearchHistoryStoreTest::startsEmpty()
{
    SearchHistoryStore store;
    QCOMPARE(store.count(), 0);
    QVERIFY(store.entries().isEmpty());
    QVERIFY(store.matching(QString()).isEmpty());
}

void SearchHistoryStoreTest::addKeepsNewestFirst()
{
    SearchHistoryStore store;

    store.add(QStringLiteral("alpha"));
    store.add(QStringLiteral("beta"));

    QCOMPARE(store.count(), 2);
    QCOMPARE(store.entries(),
             QStringList({QStringLiteral("beta"), QStringLiteral("alpha")}));
}

void SearchHistoryStoreTest::addTrimsQuery()
{
    SearchHistoryStore store;

    store.add(QStringLiteral("  spaced out  "));

    QCOMPARE(store.entries(), QStringList({QStringLiteral("spaced out")}));
}

void SearchHistoryStoreTest::addIgnoresEmptyQuery()
{
    SearchHistoryStore store;
    QSignalSpy spy(&store, &SearchHistoryStore::changed);

    store.add(QString());

    QCOMPARE(store.count(), 0);
    QCOMPARE(spy.count(), 0);
}

void SearchHistoryStoreTest::addIgnoresWhitespaceOnlyQuery()
{
    SearchHistoryStore store;
    store.add(QStringLiteral("kept"));
    QSignalSpy spy(&store, &SearchHistoryStore::changed);

    store.add(QStringLiteral(" \t\n "));

    QCOMPARE(store.entries(), QStringList({QStringLiteral("kept")}));
    QCOMPARE(spy.count(), 0);
}

void SearchHistoryStoreTest::addMovesExistingEntryToFront()
{
    SearchHistoryStore store;
    store.add(QStringLiteral("one"));
    store.add(QStringLiteral("two"));
    store.add(QStringLiteral("three"));
    QCOMPARE(store.count(), 3);

    store.add(QStringLiteral("one"));

    QCOMPARE(store.count(), 3);
    QCOMPARE(store.entries(),
             QStringList({QStringLiteral("one"), QStringLiteral("three"),
                          QStringLiteral("two")}));
}

void SearchHistoryStoreTest::addComparesCaseInsensitively()
{
    SearchHistoryStore store;
    store.add(QStringLiteral("Alice"));
    store.add(QStringLiteral("bob"));

    store.add(QStringLiteral("ALICE"));

    QCOMPARE(store.count(), 2);
    QCOMPARE(store.entries(),
             QStringList({QStringLiteral("ALICE"), QStringLiteral("bob")}));
}

void SearchHistoryStoreTest::addStoresLatestCasing()
{
    SearchHistoryStore store;
    store.add(QStringLiteral("alice"));

    store.add(QStringLiteral("Alice"));

    QCOMPARE(store.entries(), QStringList({QStringLiteral("Alice")}));
}

void SearchHistoryStoreTest::addOfFrontEntryDoesNotSignal()
{
    SearchHistoryStore store;
    store.add(QStringLiteral("same"));
    QSignalSpy spy(&store, &SearchHistoryStore::changed);

    store.add(QStringLiteral("  same  "));

    QCOMPARE(store.entries(), QStringList({QStringLiteral("same")}));
    QCOMPARE(spy.count(), 0);
}

void SearchHistoryStoreTest::addCapsAtMaxEntries()
{
    SearchHistoryStore store;

    store.add(QStringLiteral("a"), 3);
    store.add(QStringLiteral("b"), 3);
    store.add(QStringLiteral("c"), 3);
    store.add(QStringLiteral("d"), 3);

    QCOMPARE(store.count(), 3);
    QCOMPARE(store.entries(),
             QStringList({QStringLiteral("d"), QStringLiteral("c"),
                          QStringLiteral("b")}));
}

void SearchHistoryStoreTest::addClampsMaxEntriesToOne()
{
    SearchHistoryStore store;
    store.add(QStringLiteral("old"));
    store.add(QStringLiteral("older"));

    store.add(QStringLiteral("newest"), 0);

    QCOMPARE(store.count(), 1);
    QCOMPARE(store.entries(), QStringList({QStringLiteral("newest")}));
}

void SearchHistoryStoreTest::addUpdatesCapForLaterCalls()
{
    SearchHistoryStore store;

    store.add(QStringLiteral("a"), 2);
    store.add(QStringLiteral("b"), 2);
    store.add(QStringLiteral("c"), 2);
    QCOMPARE(store.count(), 2);

    // The default argument is the usual 20, so the history may grow again.
    store.add(QStringLiteral("d"));
    store.add(QStringLiteral("e"));

    QCOMPARE(store.count(), 4);
    QCOMPARE(store.entries(),
             QStringList({QStringLiteral("e"), QStringLiteral("d"),
                          QStringLiteral("c"), QStringLiteral("b")}));
}

void SearchHistoryStoreTest::removeDeletesCaseInsensitively()
{
    SearchHistoryStore store;
    store.add(QStringLiteral("Alice"));
    store.add(QStringLiteral("bob"));

    QVERIFY(store.remove(QStringLiteral("aLiCe")));

    QCOMPARE(store.entries(), QStringList({QStringLiteral("bob")}));
}

void SearchHistoryStoreTest::removeMissingEntryReturnsFalse()
{
    SearchHistoryStore store;
    store.add(QStringLiteral("present"));
    QSignalSpy spy(&store, &SearchHistoryStore::changed);

    QVERIFY(!store.remove(QStringLiteral("absent")));

    QCOMPARE(store.count(), 1);
    QCOMPARE(spy.count(), 0);
}

void SearchHistoryStoreTest::removeLeavesOtherEntriesInOrder()
{
    SearchHistoryStore store;
    store.add(QStringLiteral("first"));
    store.add(QStringLiteral("second"));
    store.add(QStringLiteral("third"));

    QVERIFY(store.remove(QStringLiteral("second")));

    QCOMPARE(store.entries(),
             QStringList({QStringLiteral("third"), QStringLiteral("first")}));
}

void SearchHistoryStoreTest::matchingIsCaseInsensitive()
{
    SearchHistoryStore store;
    store.add(QStringLiteral("Photos"));
    store.add(QStringLiteral("music"));

    QCOMPARE(store.matching(QStringLiteral("ph")),
             QStringList({QStringLiteral("Photos")}));
    QCOMPARE(store.matching(QStringLiteral("PHO")),
             QStringList({QStringLiteral("Photos")}));
}

void SearchHistoryStoreTest::matchingPreservesHistoryOrder()
{
    SearchHistoryStore store;
    store.add(QStringLiteral("alpha"));
    store.add(QStringLiteral("alpine"));
    store.add(QStringLiteral("beta"));

    QCOMPARE(store.matching(QStringLiteral("al")),
             QStringList({QStringLiteral("alpine"), QStringLiteral("alpha")}));
}

void SearchHistoryStoreTest::matchingEmptyPrefixReturnsEverything()
{
    SearchHistoryStore store;
    store.add(QStringLiteral("one"));
    store.add(QStringLiteral("two"));

    QCOMPARE(store.matching(QString()),
             QStringList({QStringLiteral("two"), QStringLiteral("one")}));
}

void SearchHistoryStoreTest::matchingWithoutHitsIsEmpty()
{
    SearchHistoryStore store;
    store.add(QStringLiteral("alpha"));

    QVERIFY(store.matching(QStringLiteral("zzz")).isEmpty());
}

void SearchHistoryStoreTest::clearEmptiesHistory()
{
    SearchHistoryStore store;
    store.add(QStringLiteral("alpha"));
    store.add(QStringLiteral("beta"));
    QSignalSpy spy(&store, &SearchHistoryStore::changed);

    store.clear();

    QCOMPARE(store.count(), 0);
    QVERIFY(store.entries().isEmpty());
    QCOMPARE(spy.count(), 1);
}

void SearchHistoryStoreTest::clearOnEmptyStoreDoesNotSignal()
{
    SearchHistoryStore store;
    QSignalSpy spy(&store, &SearchHistoryStore::changed);

    store.clear();

    QCOMPARE(store.count(), 0);
    QCOMPARE(spy.count(), 0);
}

void SearchHistoryStoreTest::entriesReturnsIndependentCopy()
{
    SearchHistoryStore store;
    store.add(QStringLiteral("alpha"));

    QStringList copy = store.entries();
    copy.append(QStringLiteral("injected"));

    QCOMPARE(store.count(), 1);
    QCOMPARE(store.entries(), QStringList({QStringLiteral("alpha")}));
}

void SearchHistoryStoreTest::changedFiresOnlyOnRealChanges()
{
    SearchHistoryStore store;
    QSignalSpy spy(&store, &SearchHistoryStore::changed);

    store.add(QStringLiteral("alpha"));   // new entry
    store.add(QStringLiteral("alpha"));   // already first
    store.add(QStringLiteral("beta"));    // new entry
    store.add(QStringLiteral("alpha"));   // moved, not duplicated
    store.remove(QStringLiteral("nope")); // absent
    store.remove(QStringLiteral("beta")); // present

    QCOMPARE(spy.count(), 4);
}

QTEST_GUILESS_MAIN(SearchHistoryStoreTest)
#include "tst_search_history_store.moc"
