#include "telegram/LazyChatLoader.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QVector<qint64> requestedIds(const QSignalSpy& spy)
{
    QVector<qint64> ids;
    ids.reserve(spy.count());
    for (const QList<QVariant>& arguments : spy)
        ids.append(arguments.constFirst().toLongLong());
    return ids;
}

} // namespace

class LazyChatLoaderTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmptyWithDefaultConcurrency();
    void setConcurrencyClampsToRange();
    void setConcurrencyEmitsOnlyOnChange();
    void raisingConcurrencyStartsWaitingChats();
    void loweringConcurrencyKeepsRunningChats();
    void enqueueStartsImmediately();
    void enqueueUsesDefaultConcurrency();
    void enqueueKeepsStartOrder();
    void enqueueIgnoresNonPositiveIds();
    void enqueueIgnoresDuplicates();
    void enqueueIgnoresLoadedChats();
    void enqueueAcceptsAFailedChatAgain();
    void pendingListsWaitingChatsOnly();
    void markLoadedFreesSlotAndStartsNext();
    void markLoadedResolvesAWaitingChat();
    void markLoadedIgnoresUnknownIds();
    void markLoadedCountsEachChatOnce();
    void markFailedFreesSlotWithoutLoading();
    void markFailedDropsAWaitingChat();
    void markFailedIgnoresUnknownIds();
    void markFailedAfterLoadedKeepsTheChatLoaded();
    void clearResetsEverything();
    void clearIsSilentWhenEmpty();
    void largeIdsArePreserved();
    void changedFiresOnlyOnRealChanges();
    void synchronousLoadHandlersAreSupported();
    void changedHandlerMayClearWithoutAnnouncing();
};

void LazyChatLoaderTest::startsEmptyWithDefaultConcurrency()
{
    const LazyChatLoader loader;

    QCOMPARE(loader.concurrency(), 6);
    QCOMPARE(loader.inFlight(), 0);
    QCOMPARE(loader.loadedCount(), 0);
    QVERIFY(loader.pending().isEmpty());
    QVERIFY(!loader.isLoaded(1));
    QVERIFY(!loader.isPending(1));
}

void LazyChatLoaderTest::setConcurrencyClampsToRange()
{
    LazyChatLoader loader;

    loader.setConcurrency(0);
    QCOMPARE(loader.concurrency(), 1);

    loader.setConcurrency(-4);
    QCOMPARE(loader.concurrency(), 1);

    loader.setConcurrency(17);
    QCOMPARE(loader.concurrency(), 16);

    loader.setConcurrency(100);
    QCOMPARE(loader.concurrency(), 16);

    loader.setConcurrency(9);
    QCOMPARE(loader.concurrency(), 9);

    loader.setConcurrency(1);
    QCOMPARE(loader.concurrency(), 1);
}

void LazyChatLoaderTest::setConcurrencyEmitsOnlyOnChange()
{
    LazyChatLoader loader;
    QSignalSpy changed(&loader, &LazyChatLoader::changed);

    // The default is already 6, so asking for it changes nothing.
    loader.setConcurrency(6);
    QCOMPARE(changed.count(), 0);

    loader.setConcurrency(7);
    QCOMPARE(changed.count(), 1);

    loader.setConcurrency(7);
    QCOMPARE(changed.count(), 1);

    // Clamping to 16 is the same cap, so the second call stays silent.
    loader.setConcurrency(100);
    QCOMPARE(changed.count(), 2);

    loader.setConcurrency(16);
    QCOMPARE(changed.count(), 2);

    // Clamping to 1 is a real change.
    loader.setConcurrency(0);
    QCOMPARE(changed.count(), 3);
}

void LazyChatLoaderTest::raisingConcurrencyStartsWaitingChats()
{
    LazyChatLoader loader;
    QSignalSpy requested(&loader, &LazyChatLoader::loadRequested);
    QSignalSpy changed(&loader, &LazyChatLoader::changed);

    loader.setConcurrency(1);
    loader.enqueue(1);
    loader.enqueue(2);
    loader.enqueue(3);

    QCOMPARE(loader.inFlight(), 1);
    QCOMPARE(loader.pending(), (QVector<qint64>{2, 3}));
    QCOMPARE(requestedIds(requested), (QVector<qint64>{1}));

    loader.setConcurrency(3);

    QCOMPARE(loader.inFlight(), 3);
    QVERIFY(loader.pending().isEmpty());
    QCOMPARE(requestedIds(requested), (QVector<qint64>{1, 2, 3}));

    // One changed() for the cap change plus one for each enqueue().
    QCOMPARE(changed.count(), 5);
}

void LazyChatLoaderTest::loweringConcurrencyKeepsRunningChats()
{
    LazyChatLoader loader;
    QSignalSpy requested(&loader, &LazyChatLoader::loadRequested);

    loader.setConcurrency(3);
    for (qint64 id = 1; id <= 5; ++id)
        loader.enqueue(id);

    QCOMPARE(loader.inFlight(), 3);
    QCOMPARE(loader.pending(), (QVector<qint64>{4, 5}));

    loader.setConcurrency(1);

    // The three chats already running are not cancelled.
    QCOMPARE(loader.inFlight(), 3);
    QCOMPARE(loader.pending(), (QVector<qint64>{4, 5}));

    loader.markLoaded(1);
    QCOMPARE(loader.inFlight(), 2);
    QCOMPARE(loader.pending(), (QVector<qint64>{4, 5}));

    loader.markLoaded(2);
    QCOMPARE(loader.inFlight(), 1);
    QCOMPARE(loader.pending(), (QVector<qint64>{4, 5}));

    // Only when the in-flight count drops below the new cap does the next
    // waiting chat start.
    loader.markLoaded(3);
    QCOMPARE(loader.inFlight(), 1);
    QCOMPARE(loader.pending(), (QVector<qint64>{5}));
    QCOMPARE(requestedIds(requested), (QVector<qint64>{1, 2, 3, 4}));
}

void LazyChatLoaderTest::enqueueStartsImmediately()
{
    LazyChatLoader loader;
    QSignalSpy requested(&loader, &LazyChatLoader::loadRequested);
    QSignalSpy changed(&loader, &LazyChatLoader::changed);

    loader.enqueue(42);

    QCOMPARE(loader.inFlight(), 1);
    QVERIFY(loader.pending().isEmpty());
    QVERIFY(!loader.isPending(42));
    QVERIFY(!loader.isLoaded(42));
    QCOMPARE(requestedIds(requested), (QVector<qint64>{42}));
    QCOMPARE(changed.count(), 1);
}

void LazyChatLoaderTest::enqueueUsesDefaultConcurrency()
{
    LazyChatLoader loader;
    QSignalSpy requested(&loader, &LazyChatLoader::loadRequested);

    for (qint64 id = 1; id <= 8; ++id)
        loader.enqueue(id);

    QCOMPARE(loader.inFlight(), 6);
    QCOMPARE(loader.pending(), (QVector<qint64>{7, 8}));
    QCOMPARE(requestedIds(requested), (QVector<qint64>{1, 2, 3, 4, 5, 6}));
}

void LazyChatLoaderTest::enqueueKeepsStartOrder()
{
    LazyChatLoader loader;
    QSignalSpy requested(&loader, &LazyChatLoader::loadRequested);

    loader.setConcurrency(2);
    loader.enqueue(40);
    loader.enqueue(10);
    loader.enqueue(30);
    loader.enqueue(20);

    QCOMPARE(requestedIds(requested), (QVector<qint64>{40, 10}));
    QCOMPARE(loader.pending(), (QVector<qint64>{30, 20}));

    loader.markLoaded(40);
    QCOMPARE(requestedIds(requested), (QVector<qint64>{40, 10, 30}));

    loader.markLoaded(10);
    QCOMPARE(requestedIds(requested), (QVector<qint64>{40, 10, 30, 20}));
    QVERIFY(loader.pending().isEmpty());
}

void LazyChatLoaderTest::enqueueIgnoresNonPositiveIds()
{
    LazyChatLoader loader;
    QSignalSpy requested(&loader, &LazyChatLoader::loadRequested);
    QSignalSpy changed(&loader, &LazyChatLoader::changed);

    loader.enqueue(0);
    loader.enqueue(-1);
    loader.enqueue(-5000000000LL);

    QCOMPARE(loader.inFlight(), 0);
    QVERIFY(loader.pending().isEmpty());
    QCOMPARE(loader.loadedCount(), 0);
    QCOMPARE(requested.count(), 0);
    QCOMPARE(changed.count(), 0);
}

void LazyChatLoaderTest::enqueueIgnoresDuplicates()
{
    LazyChatLoader loader;
    QSignalSpy requested(&loader, &LazyChatLoader::loadRequested);
    QSignalSpy changed(&loader, &LazyChatLoader::changed);

    loader.setConcurrency(1);
    loader.enqueue(1);
    loader.enqueue(2);

    const auto changedBefore = changed.count();
    const auto requestedBefore = requested.count();

    // 1 is in flight and 2 is waiting: both are duplicates.
    loader.enqueue(1);
    loader.enqueue(2);

    QCOMPARE(changed.count(), changedBefore);
    QCOMPARE(requested.count(), requestedBefore);
    QCOMPARE(loader.inFlight(), 1);
    QCOMPARE(loader.pending(), (QVector<qint64>{2}));
    QCOMPARE(requestedIds(requested), (QVector<qint64>{1}));
}

void LazyChatLoaderTest::enqueueIgnoresLoadedChats()
{
    LazyChatLoader loader;
    QSignalSpy requested(&loader, &LazyChatLoader::loadRequested);
    QSignalSpy changed(&loader, &LazyChatLoader::changed);

    loader.enqueue(5);
    loader.markLoaded(5);

    const auto changedBefore = changed.count();
    const auto requestedBefore = requested.count();

    loader.enqueue(5);

    QVERIFY(loader.isLoaded(5));
    QCOMPARE(loader.inFlight(), 0);
    QVERIFY(loader.pending().isEmpty());
    QCOMPARE(changed.count(), changedBefore);
    QCOMPARE(requested.count(), requestedBefore);
}

void LazyChatLoaderTest::enqueueAcceptsAFailedChatAgain()
{
    LazyChatLoader loader;
    QSignalSpy requested(&loader, &LazyChatLoader::loadRequested);

    loader.setConcurrency(1);
    loader.enqueue(1);
    loader.enqueue(2);

    loader.markFailed(1);
    QVERIFY(!loader.isLoaded(1));
    QVERIFY(!loader.isPending(1));
    QCOMPARE(loader.loadedCount(), 0);
    QCOMPARE(requestedIds(requested), (QVector<qint64>{1, 2}));

    // 2 holds the only slot, so the failed chat waits again.
    loader.enqueue(1);
    QCOMPARE(loader.pending(), (QVector<qint64>{1}));
    QCOMPARE(requestedIds(requested), (QVector<qint64>{1, 2}));

    loader.markLoaded(2);
    QCOMPARE(loader.inFlight(), 1);
    QCOMPARE(requestedIds(requested), (QVector<qint64>{1, 2, 1}));

    loader.markLoaded(1);
    QVERIFY(loader.isLoaded(1));
    QCOMPARE(loader.loadedCount(), 2);
}

void LazyChatLoaderTest::pendingListsWaitingChatsOnly()
{
    LazyChatLoader loader;

    loader.setConcurrency(2);
    loader.enqueue(1);
    loader.enqueue(2);
    loader.enqueue(3);
    loader.enqueue(4);

    QCOMPARE(loader.pending(), (QVector<qint64>{3, 4}));
    QCOMPARE(loader.inFlight(), 2);

    QVERIFY(loader.isPending(3));
    QVERIFY(loader.isPending(4));

    // Started chats are in flight, not pending.
    QVERIFY(!loader.isPending(1));
    QVERIFY(!loader.isPending(2));
    QVERIFY(!loader.isPending(5));
    QVERIFY(!loader.isPending(0));
    QVERIFY(!loader.isPending(-3));
}

void LazyChatLoaderTest::markLoadedFreesSlotAndStartsNext()
{
    LazyChatLoader loader;
    QSignalSpy requested(&loader, &LazyChatLoader::loadRequested);

    loader.setConcurrency(1);
    loader.enqueue(1);
    loader.enqueue(2);
    loader.enqueue(3);

    QSignalSpy changed(&loader, &LazyChatLoader::changed);
    loader.markLoaded(1);

    QVERIFY(loader.isLoaded(1));
    QVERIFY(!loader.isPending(1));
    QCOMPARE(loader.loadedCount(), 1);
    QCOMPARE(loader.inFlight(), 1);
    QCOMPARE(loader.pending(), (QVector<qint64>{3}));
    QCOMPARE(requestedIds(requested), (QVector<qint64>{1, 2}));
    QCOMPARE(changed.count(), 1);
}

void LazyChatLoaderTest::markLoadedResolvesAWaitingChat()
{
    LazyChatLoader loader;
    QSignalSpy requested(&loader, &LazyChatLoader::loadRequested);

    loader.setConcurrency(1);
    loader.enqueue(1);
    loader.enqueue(2);

    // The waiting chat never occupied a slot, so nothing new starts.
    loader.markLoaded(2);

    QVERIFY(loader.isLoaded(2));
    QVERIFY(!loader.isPending(2));
    QCOMPARE(loader.loadedCount(), 1);
    QCOMPARE(loader.inFlight(), 1);
    QVERIFY(loader.pending().isEmpty());
    QCOMPARE(requestedIds(requested), (QVector<qint64>{1}));
}

void LazyChatLoaderTest::markLoadedIgnoresUnknownIds()
{
    LazyChatLoader loader;
    QSignalSpy requested(&loader, &LazyChatLoader::loadRequested);
    QSignalSpy changed(&loader, &LazyChatLoader::changed);

    loader.markLoaded(0);
    loader.markLoaded(-3);
    loader.markLoaded(9);

    QCOMPARE(loader.loadedCount(), 0);
    QCOMPARE(loader.inFlight(), 0);
    QCOMPARE(requested.count(), 0);
    QCOMPARE(changed.count(), 0);
}

void LazyChatLoaderTest::markLoadedCountsEachChatOnce()
{
    LazyChatLoader loader;

    loader.enqueue(1);
    loader.markLoaded(1);
    QCOMPARE(loader.loadedCount(), 1);

    QSignalSpy changed(&loader, &LazyChatLoader::changed);
    loader.markLoaded(1);

    QCOMPARE(loader.loadedCount(), 1);
    QCOMPARE(changed.count(), 0);
}

void LazyChatLoaderTest::markFailedFreesSlotWithoutLoading()
{
    LazyChatLoader loader;
    QSignalSpy requested(&loader, &LazyChatLoader::loadRequested);

    loader.setConcurrency(1);
    loader.enqueue(1);
    loader.enqueue(2);

    QSignalSpy changed(&loader, &LazyChatLoader::changed);
    loader.markFailed(1);

    QVERIFY(!loader.isLoaded(1));
    QVERIFY(!loader.isPending(1));
    QCOMPARE(loader.loadedCount(), 0);
    QCOMPARE(loader.inFlight(), 1);
    QVERIFY(loader.pending().isEmpty());
    QCOMPARE(requestedIds(requested), (QVector<qint64>{1, 2}));
    QCOMPARE(changed.count(), 1);
}

void LazyChatLoaderTest::markFailedDropsAWaitingChat()
{
    LazyChatLoader loader;
    QSignalSpy requested(&loader, &LazyChatLoader::loadRequested);

    loader.setConcurrency(1);
    loader.enqueue(1);
    loader.enqueue(2);

    loader.markFailed(2);

    QVERIFY(!loader.isPending(2));
    QVERIFY(!loader.isLoaded(2));
    QVERIFY(loader.pending().isEmpty());
    QCOMPARE(loader.inFlight(), 1);

    loader.markLoaded(1);
    QCOMPARE(loader.inFlight(), 0);
    QCOMPARE(requestedIds(requested), (QVector<qint64>{1}));
}

void LazyChatLoaderTest::markFailedIgnoresUnknownIds()
{
    LazyChatLoader loader;
    QSignalSpy requested(&loader, &LazyChatLoader::loadRequested);
    QSignalSpy changed(&loader, &LazyChatLoader::changed);

    loader.markFailed(0);
    loader.markFailed(-1);
    loader.markFailed(77);

    QCOMPARE(loader.inFlight(), 0);
    QCOMPARE(loader.loadedCount(), 0);
    QCOMPARE(requested.count(), 0);
    QCOMPARE(changed.count(), 0);
}

void LazyChatLoaderTest::markFailedAfterLoadedKeepsTheChatLoaded()
{
    LazyChatLoader loader;

    loader.enqueue(1);
    loader.markLoaded(1);

    QSignalSpy changed(&loader, &LazyChatLoader::changed);
    loader.markFailed(1);

    QVERIFY(loader.isLoaded(1));
    QCOMPARE(loader.loadedCount(), 1);
    QCOMPARE(changed.count(), 0);
}

void LazyChatLoaderTest::clearResetsEverything()
{
    LazyChatLoader loader;
    QSignalSpy requested(&loader, &LazyChatLoader::loadRequested);

    loader.setConcurrency(2);
    loader.enqueue(1);
    loader.enqueue(2);
    loader.enqueue(3);
    loader.enqueue(4);
    loader.markLoaded(1);

    QCOMPARE(loader.loadedCount(), 1);
    QCOMPARE(loader.inFlight(), 2);
    QCOMPARE(loader.pending(), (QVector<qint64>{4}));

    QSignalSpy changed(&loader, &LazyChatLoader::changed);
    loader.clear();

    QCOMPARE(changed.count(), 1);
    QCOMPARE(loader.inFlight(), 0);
    QVERIFY(loader.pending().isEmpty());
    QCOMPARE(loader.loadedCount(), 0);
    QVERIFY(!loader.isLoaded(1));
    QVERIFY(!loader.isPending(4));

    // The cap survives and the loader is usable again; even a chat that was
    // loaded before the clear can be enqueued now.
    QCOMPARE(loader.concurrency(), 2);
    loader.enqueue(9);
    loader.enqueue(10);
    loader.enqueue(11);
    loader.enqueue(1);

    QCOMPARE(loader.inFlight(), 2);
    QCOMPARE(loader.pending(), (QVector<qint64>{11, 1}));
    QCOMPARE(requestedIds(requested), (QVector<qint64>{1, 2, 3, 9, 10}));
}

void LazyChatLoaderTest::clearIsSilentWhenEmpty()
{
    LazyChatLoader loader;
    QSignalSpy changed(&loader, &LazyChatLoader::changed);

    loader.clear();
    loader.clear();

    QCOMPARE(changed.count(), 0);
    QCOMPARE(loader.inFlight(), 0);
    QCOMPARE(loader.loadedCount(), 0);
}

void LazyChatLoaderTest::largeIdsArePreserved()
{
    LazyChatLoader loader;
    QSignalSpy requested(&loader, &LazyChatLoader::loadRequested);
    const qint64 big = 5000000000LL;

    loader.enqueue(big);

    QCOMPARE(loader.inFlight(), 1);
    QCOMPARE(requestedIds(requested), (QVector<qint64>{big}));

    loader.markLoaded(big);
    QVERIFY(loader.isLoaded(big));
    QCOMPARE(loader.loadedCount(), 1);

    loader.enqueue(big);
    QCOMPARE(requested.count(), 1);
}

void LazyChatLoaderTest::changedFiresOnlyOnRealChanges()
{
    LazyChatLoader loader;
    QSignalSpy changed(&loader, &LazyChatLoader::changed);

    loader.enqueue(1);            // 1: starts, changes
    loader.enqueue(1);            // duplicate in flight
    loader.enqueue(0);            // non-positive
    loader.enqueue(-9);           // non-positive
    loader.markLoaded(99);        // unknown
    loader.markFailed(99);        // unknown
    loader.markLoaded(1);         // 2: loaded
    loader.markLoaded(1);         // already loaded
    loader.enqueue(1);            // already loaded
    loader.clear();               // 3: forgets the loaded chat
    loader.clear();               // already empty
    loader.setConcurrency(6);     // already the default
    loader.setConcurrency(7);     // 4: real cap change
    loader.setConcurrency(7);     // same cap

    QCOMPARE(changed.count(), 4);
}

void LazyChatLoaderTest::synchronousLoadHandlersAreSupported()
{
    LazyChatLoader loader;
    loader.setConcurrency(2);

    QVector<qint64> started;
    connect(&loader, &LazyChatLoader::loadRequested, &loader,
            [&loader, &started](qint64 chatId) {
                started.append(chatId);
                // A chat history that is already cached can be settled from
                // inside the handler without breaking the loader.
                loader.markLoaded(chatId);
            });

    loader.enqueue(1);
    loader.enqueue(2);
    loader.enqueue(3);

    QCOMPARE(started, (QVector<qint64>{1, 2, 3}));
    QCOMPARE(loader.inFlight(), 0);
    QVERIFY(loader.pending().isEmpty());
    QCOMPARE(loader.loadedCount(), 3);
}

void LazyChatLoaderTest::changedHandlerMayClearWithoutAnnouncing()
{
    LazyChatLoader loader;
    QSignalSpy requested(&loader, &LazyChatLoader::loadRequested);

    // changed() is delivered before loadRequested(), so a listener can drop the
    // whole queue and the started chat must not be announced afterwards.
    connect(&loader, &LazyChatLoader::changed, &loader,
            [&loader] { loader.clear(); });

    loader.enqueue(1);

    QCOMPARE(loader.inFlight(), 0);
    QVERIFY(loader.pending().isEmpty());
    QCOMPARE(requested.count(), 0);
}

QTEST_GUILESS_MAIN(LazyChatLoaderTest)
#include "tst_lazy_chat_loader.moc"
