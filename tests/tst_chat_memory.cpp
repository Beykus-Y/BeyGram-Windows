#include "telegram/ChatMemory.h"

#include <QSignalSpy>
#include <QtTest>

using beygram::telegram::ChatMemory;
using beygram::telegram::ChatMemoryEntry;

namespace {

ChatMemoryEntry makeEntry(qint64 chatId, qint64 updatedAt = 0)
{
    ChatMemoryEntry entry;
    entry.chatId = chatId;
    entry.title = QStringLiteral("Chat %1").arg(chatId);
    entry.lastPreview = QStringLiteral("Preview %1").arg(chatId);
    entry.unreadCount = 0;
    entry.lastMessageDate = chatId * 60;
    entry.updatedAt = updatedAt;
    return entry;
}

QVector<qint64> idsOf(const QVector<ChatMemoryEntry>& entries)
{
    QVector<qint64> ids;
    ids.reserve(entries.size());
    for (const ChatMemoryEntry& entry : entries)
        ids.append(entry.chatId);
    return ids;
}

bool sameFields(const ChatMemoryEntry& left, const ChatMemoryEntry& right)
{
    return left.chatId == right.chatId
        && left.title == right.title
        && left.lastPreview == right.lastPreview
        && left.unreadCount == right.unreadCount
        && left.lastMessageDate == right.lastMessageDate
        && left.updatedAt == right.updatedAt;
}

} // namespace

class ChatMemoryTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void defaultCapacity();
    void setCapacityClampsToBounds();
    void setCapacityToCurrentValueDoesNotSignal();
    void setCapacityShrinkEvictsLeastRecentlyUpdated();
    void setCapacityIncreaseKeepsEntries();
    void rememberIgnoresNonPositiveChatId();
    void rememberStampsUpdatedAt();
    void rememberKeepsFields();
    void rememberReplacesEntryWithSameId();
    void rememberReplacementMovesToFront();
    void rememberReplacementAtCapacityDoesNotEvict();
    void rememberSameEntryAndNowIsNoOp();
    void rememberSameContentWithNewerNowSignals();
    void rememberAtCapacityEvictsLeastRecentlyUpdated();
    void rememberNewcomerOlderThanAllChangesNothing();
    void evictionTieBreaksOnChatId();
    void entryReturnsStoredCopy();
    void entryOfMissingChatIsDefault();
    void containsTracksRememberAndForget();
    void recentSortsByUpdatedAtDescending();
    void recentClampsLimit();
    void recentLimitClampsToCapacity();
    void recentOrdersEqualTimestampsByChatId();
    void recentReturnsIndependentCopy();
    void forgetRemovesOnlyThatChat();
    void forgetMissingIsNoOp();
    void forgetNonPositiveIsNoOp();
    void clearEmptiesStore();
    void clearOnEmptyDoesNotSignal();
    void rememberAfterForgetAddsAgain();
    void changedOnlyOnRealChanges();
};

void ChatMemoryTest::startsEmpty()
{
    ChatMemory store;

    QCOMPARE(store.count(), 0);
    QVERIFY(!store.contains(1));
    QCOMPARE(store.entry(1).chatId, qint64(0));
    QVERIFY(store.recent(10).isEmpty());
    QVERIFY(store.recent(0).isEmpty());
}

void ChatMemoryTest::defaultCapacity()
{
    ChatMemory store;

    QCOMPARE(store.capacity(), 200);
}

void ChatMemoryTest::setCapacityClampsToBounds()
{
    ChatMemory store;
    QSignalSpy spy(&store, &ChatMemory::changed);

    store.setCapacity(0);
    QCOMPARE(store.capacity(), 16);

    store.setCapacity(15);
    QCOMPARE(store.capacity(), 16);

    store.setCapacity(-100);
    QCOMPARE(store.capacity(), 16);

    store.setCapacity(5001);
    QCOMPARE(store.capacity(), 5000);

    store.setCapacity(1000000);
    QCOMPARE(store.capacity(), 5000);

    store.setCapacity(100);
    QCOMPARE(store.capacity(), 100);

    // 200 -> 16, 16 -> 5000, 5000 -> 100; the clamped no-ops did not signal.
    QCOMPARE(spy.count(), 3);
}

void ChatMemoryTest::setCapacityToCurrentValueDoesNotSignal()
{
    ChatMemory store;
    QSignalSpy spy(&store, &ChatMemory::changed);

    store.setCapacity(store.capacity());
    QCOMPARE(spy.count(), 0);

    store.setCapacity(10); // clamps to 16, a real change
    QCOMPARE(store.capacity(), 16);
    QCOMPARE(spy.count(), 1);

    store.setCapacity(16); // already there
    QCOMPARE(spy.count(), 1);
}

void ChatMemoryTest::setCapacityShrinkEvictsLeastRecentlyUpdated()
{
    ChatMemory store;
    store.setCapacity(20);
    for (int i = 1; i <= 20; ++i)
        store.remember(makeEntry(i), i * 100);

    QSignalSpy spy(&store, &ChatMemory::changed);
    store.setCapacity(16);

    QCOMPARE(store.capacity(), 16);
    QCOMPARE(store.count(), 16);
    QVERIFY(!store.contains(1));
    QVERIFY(!store.contains(4));
    QVERIFY(store.contains(5));
    QVERIFY(store.contains(20));
    QCOMPARE(spy.count(), 1);
}

void ChatMemoryTest::setCapacityIncreaseKeepsEntries()
{
    ChatMemory store;
    for (int i = 1; i <= 16; ++i)
        store.remember(makeEntry(i), i);

    QSignalSpy spy(&store, &ChatMemory::changed);
    store.setCapacity(500);

    QCOMPARE(store.capacity(), 500);
    QCOMPARE(store.count(), 16);
    QVERIFY(store.contains(1));
    QVERIFY(store.contains(16));
    QCOMPARE(spy.count(), 1);
}

void ChatMemoryTest::rememberIgnoresNonPositiveChatId()
{
    ChatMemory store;
    QSignalSpy spy(&store, &ChatMemory::changed);

    store.remember(makeEntry(0), 100);
    store.remember(makeEntry(-5), 100);

    QCOMPARE(store.count(), 0);
    QCOMPARE(spy.count(), 0);
}

void ChatMemoryTest::rememberStampsUpdatedAt()
{
    ChatMemory store;

    store.remember(makeEntry(7, 123456), 9000);

    QCOMPARE(store.entry(7).updatedAt, qint64(9000));
}

void ChatMemoryTest::rememberKeepsFields()
{
    ChatMemory store;
    ChatMemoryEntry entry = makeEntry(42);
    entry.title = QStringLiteral("Alice");
    entry.lastPreview = QStringLiteral("see you at six");
    entry.unreadCount = 3;
    entry.lastMessageDate = 1700000000;

    store.remember(entry, 500);

    const ChatMemoryEntry stored = store.entry(42);
    QCOMPARE(stored.chatId, qint64(42));
    QCOMPARE(stored.title, QStringLiteral("Alice"));
    QCOMPARE(stored.lastPreview, QStringLiteral("see you at six"));
    QCOMPARE(stored.unreadCount, 3);
    QCOMPARE(stored.lastMessageDate, qint64(1700000000));
    QCOMPARE(stored.updatedAt, qint64(500));
}

void ChatMemoryTest::rememberReplacesEntryWithSameId()
{
    ChatMemory store;
    store.remember(makeEntry(1), 100);

    ChatMemoryEntry changed = makeEntry(1);
    changed.title = QStringLiteral("Renamed");
    changed.unreadCount = 4;
    store.remember(changed, 200);

    QCOMPARE(store.count(), 1);
    QCOMPARE(store.entry(1).title, QStringLiteral("Renamed"));
    QCOMPARE(store.entry(1).unreadCount, 4);
    QCOMPARE(store.entry(1).updatedAt, qint64(200));
}

void ChatMemoryTest::rememberReplacementMovesToFront()
{
    ChatMemory store;
    store.remember(makeEntry(1), 100);
    store.remember(makeEntry(2), 200);
    store.remember(makeEntry(3), 300);

    store.remember(makeEntry(1), 400);

    const QVector<qint64> expected{1, 3, 2};
    QCOMPARE(idsOf(store.recent(3)), expected);
}

void ChatMemoryTest::rememberReplacementAtCapacityDoesNotEvict()
{
    ChatMemory store;
    store.setCapacity(16);
    for (int i = 1; i <= 16; ++i)
        store.remember(makeEntry(i), i);

    store.remember(makeEntry(1), 999);

    QCOMPARE(store.count(), 16);
    QVERIFY(store.contains(1));
    QVERIFY(store.contains(2));
    QVERIFY(store.contains(16));
    QCOMPARE(store.entry(1).updatedAt, qint64(999));
}

void ChatMemoryTest::rememberSameEntryAndNowIsNoOp()
{
    ChatMemory store;
    const ChatMemoryEntry entry = makeEntry(5);
    store.remember(entry, 100);

    QSignalSpy spy(&store, &ChatMemory::changed);
    store.remember(entry, 100);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(store.count(), 1);
}

void ChatMemoryTest::rememberSameContentWithNewerNowSignals()
{
    ChatMemory store;
    store.remember(makeEntry(1), 100);
    store.remember(makeEntry(2), 200);

    QSignalSpy spy(&store, &ChatMemory::changed);
    store.remember(makeEntry(1), 300);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(store.entry(1).updatedAt, qint64(300));
    const QVector<qint64> expected{1, 2};
    QCOMPARE(idsOf(store.recent(2)), expected);
}

void ChatMemoryTest::rememberAtCapacityEvictsLeastRecentlyUpdated()
{
    ChatMemory store;
    store.setCapacity(16);
    for (int i = 1; i <= 16; ++i)
        store.remember(makeEntry(i), i * 10);

    QSignalSpy spy(&store, &ChatMemory::changed);
    store.remember(makeEntry(17), 170);

    QCOMPARE(store.count(), 16);
    QVERIFY(!store.contains(1));
    QVERIFY(store.contains(2));
    QVERIFY(store.contains(17));
    QCOMPARE(store.entry(17).updatedAt, qint64(170));
    QCOMPARE(spy.count(), 1);
}

void ChatMemoryTest::rememberNewcomerOlderThanAllChangesNothing()
{
    ChatMemory store;
    store.setCapacity(16);
    for (int i = 2; i <= 17; ++i)
        store.remember(makeEntry(i), 1000);

    QSignalSpy spy(&store, &ChatMemory::changed);
    store.remember(makeEntry(1), 500);

    QCOMPARE(store.count(), 16);
    QVERIFY(!store.contains(1));
    QVERIFY(store.contains(2));
    QVERIFY(store.contains(17));
    QCOMPARE(spy.count(), 0);
}

void ChatMemoryTest::evictionTieBreaksOnChatId()
{
    ChatMemory store;
    store.setCapacity(16);
    for (int i = 10; i <= 25; ++i)
        store.remember(makeEntry(i), 1000);

    QSignalSpy spy(&store, &ChatMemory::changed);
    store.remember(makeEntry(26), 1000);

    QCOMPARE(store.count(), 16);
    QVERIFY(!store.contains(10));
    QVERIFY(store.contains(11));
    QVERIFY(store.contains(26));
    QCOMPARE(spy.count(), 1);
}

void ChatMemoryTest::entryReturnsStoredCopy()
{
    ChatMemory store;
    ChatMemoryEntry entry = makeEntry(8);
    store.remember(entry, 400);

    ChatMemoryEntry expected = entry;
    expected.updatedAt = 400;
    QVERIFY(sameFields(store.entry(8), expected));

    ChatMemoryEntry copy = store.entry(8);
    copy.title = QStringLiteral("mutated");
    QCOMPARE(store.entry(8).title, entry.title);
}

void ChatMemoryTest::entryOfMissingChatIsDefault()
{
    ChatMemory store;
    store.remember(makeEntry(1), 100);

    const ChatMemoryEntry missing = store.entry(999);
    QCOMPARE(missing.chatId, qint64(0));
    QVERIFY(missing.title.isEmpty());
    QVERIFY(missing.lastPreview.isEmpty());
    QCOMPARE(missing.unreadCount, 0);
    QCOMPARE(missing.lastMessageDate, qint64(0));
    QCOMPARE(missing.updatedAt, qint64(0));
}

void ChatMemoryTest::containsTracksRememberAndForget()
{
    ChatMemory store;

    QVERIFY(!store.contains(3));
    store.remember(makeEntry(3), 100);
    QVERIFY(store.contains(3));
    store.forget(3);
    QVERIFY(!store.contains(3));
}

void ChatMemoryTest::recentSortsByUpdatedAtDescending()
{
    ChatMemory store;
    store.remember(makeEntry(1), 100);
    store.remember(makeEntry(2), 300);
    store.remember(makeEntry(3), 200);

    const QVector<qint64> expected{2, 3, 1};
    QCOMPARE(idsOf(store.recent(3)), expected);
}

void ChatMemoryTest::recentClampsLimit()
{
    ChatMemory store;
    store.remember(makeEntry(1), 100);
    store.remember(makeEntry(2), 200);
    store.remember(makeEntry(3), 300);

    QCOMPARE(int(store.recent(0).size()), 1);
    QCOMPARE(store.recent(0).at(0).chatId, qint64(3));
    QCOMPARE(int(store.recent(-7).size()), 1);
    QCOMPARE(int(store.recent(1).size()), 1);
    QCOMPARE(int(store.recent(2).size()), 2);
    QCOMPARE(int(store.recent(99).size()), 3);
}

void ChatMemoryTest::recentLimitClampsToCapacity()
{
    ChatMemory store;
    store.setCapacity(16);
    for (int i = 1; i <= 16; ++i)
        store.remember(makeEntry(i), i);

    QCOMPARE(int(store.recent(1000).size()), 16);
    QCOMPARE(int(store.recent(16).size()), 16);
}

void ChatMemoryTest::recentOrdersEqualTimestampsByChatId()
{
    ChatMemory store;
    store.remember(makeEntry(30), 100);
    store.remember(makeEntry(10), 100);
    store.remember(makeEntry(20), 100);

    const QVector<qint64> expected{10, 20, 30};
    QCOMPARE(idsOf(store.recent(3)), expected);
}

void ChatMemoryTest::recentReturnsIndependentCopy()
{
    ChatMemory store;
    store.remember(makeEntry(1), 100);

    QVector<ChatMemoryEntry> copy = store.recent(1);
    QCOMPARE(int(copy.size()), 1);
    copy[0].title = QStringLiteral("mutated");
    copy.clear();

    QCOMPARE(store.count(), 1);
    QCOMPARE(store.entry(1).title, makeEntry(1).title);
}

void ChatMemoryTest::forgetRemovesOnlyThatChat()
{
    ChatMemory store;
    store.remember(makeEntry(1), 100);
    store.remember(makeEntry(2), 200);
    store.remember(makeEntry(3), 300);

    store.forget(2);

    QCOMPARE(store.count(), 2);
    QVERIFY(store.contains(1));
    QVERIFY(!store.contains(2));
    QVERIFY(store.contains(3));
    const QVector<qint64> expected{3, 1};
    QCOMPARE(idsOf(store.recent(2)), expected);
}

void ChatMemoryTest::forgetMissingIsNoOp()
{
    ChatMemory store;
    store.remember(makeEntry(1), 100);

    QSignalSpy spy(&store, &ChatMemory::changed);
    store.forget(2);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(store.count(), 1);
}

void ChatMemoryTest::forgetNonPositiveIsNoOp()
{
    ChatMemory store;
    store.remember(makeEntry(1), 100);

    QSignalSpy spy(&store, &ChatMemory::changed);
    store.forget(0);
    store.forget(-4);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(store.count(), 1);
}

void ChatMemoryTest::clearEmptiesStore()
{
    ChatMemory store;
    store.remember(makeEntry(1), 100);
    store.remember(makeEntry(2), 200);

    QSignalSpy spy(&store, &ChatMemory::changed);
    store.clear();

    QCOMPARE(store.count(), 0);
    QVERIFY(store.recent(5).isEmpty());
    QVERIFY(!store.contains(1));
    QCOMPARE(spy.count(), 1);
}

void ChatMemoryTest::clearOnEmptyDoesNotSignal()
{
    ChatMemory store;
    QSignalSpy spy(&store, &ChatMemory::changed);

    store.clear();
    store.clear();

    QCOMPARE(spy.count(), 0);
}

void ChatMemoryTest::rememberAfterForgetAddsAgain()
{
    ChatMemory store;
    store.remember(makeEntry(1), 100);
    store.forget(1);

    QSignalSpy spy(&store, &ChatMemory::changed);
    store.remember(makeEntry(1), 200);

    QCOMPARE(store.count(), 1);
    QCOMPARE(store.entry(1).updatedAt, qint64(200));
    QCOMPARE(spy.count(), 1);
}

void ChatMemoryTest::changedOnlyOnRealChanges()
{
    ChatMemory store;
    QSignalSpy spy(&store, &ChatMemory::changed);

    store.remember(makeEntry(1), 100);  // new entry
    store.remember(makeEntry(1), 100);  // identical
    store.remember(makeEntry(0), 100);  // invalid id
    store.remember(makeEntry(2), 200);  // new entry
    store.remember(makeEntry(1), 300);  // recency moved
    store.forget(99);                    // absent
    store.forget(2);                     // present
    store.clear();                       // one left
    store.clear();                       // already empty

    QCOMPARE(spy.count(), 5);
}

QTEST_GUILESS_MAIN(ChatMemoryTest)
#include "tst_chat_memory.moc"
