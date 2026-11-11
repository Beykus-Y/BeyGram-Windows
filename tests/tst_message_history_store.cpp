#include "telegram/MessageHistoryStore.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

MessageData makeMessage(qint64 id, qint64 date, const QString& text = {})
{
    MessageData message;
    message.id = id;
    message.date = date;
    message.text = text.isEmpty() ? QStringLiteral("m%1").arg(id) : text;
    return message;
}

QVector<qint64> idsOf(const MessageHistoryStore& store)
{
    QVector<qint64> ids;
    const QVector<MessageData> messages = store.messages();
    ids.reserve(messages.size());
    for (const MessageData& message : messages)
        ids.append(message.id);
    return ids;
}

QString textOf(const MessageHistoryStore& store, qint64 id)
{
    const QVector<MessageData> messages = store.messages();
    for (const MessageData& message : messages) {
        if (message.id == id)
            return message.text;
    }
    return {};
}

} // namespace

class MessageHistoryStoreTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void sortsByDateThenId();
    void setMessagesDeduplicatesKeepingTheLastCopy();
    void addOrUpdateInsertsUpdatesAndResorts();
    void replaceIdSwapsTemporaryId();
    void removeIdsDeletesRows();
    void tombstoneBlocksLatePage();
    void tombstonesAreBounded();
    void mergeOlderPrependsWithoutDuplicates();
    void mergeOlderIgnoresTombstonedIds();
    void clearResetsRowsAndTombstones();
    void changedSignalFiresOnlyOnMutation();
};

void MessageHistoryStoreTest::startsEmpty()
{
    MessageHistoryStore store;

    QCOMPARE(store.count(), 0);
    QCOMPARE(store.firstMessageId(), qint64(0));
    QCOMPARE(store.lastMessageId(), qint64(0));
    QVERIFY(store.messages().isEmpty());
    QVERIFY(!store.contains(1));
}

void MessageHistoryStoreTest::sortsByDateThenId()
{
    MessageHistoryStore store;
    store.setMessages({
        makeMessage(3, 200),
        makeMessage(1, 100),
        makeMessage(2, 200),
        makeMessage(4, 100),
    });

    QCOMPARE(store.count(), 4);
    QCOMPARE(idsOf(store), (QVector<qint64>{1, 4, 2, 3}));
    QCOMPARE(store.firstMessageId(), qint64(1));
    QCOMPARE(store.lastMessageId(), qint64(3));
    QVERIFY(store.contains(4));
    QVERIFY(!store.contains(5));
}

void MessageHistoryStoreTest::setMessagesDeduplicatesKeepingTheLastCopy()
{
    MessageHistoryStore store;
    store.setMessages({
        makeMessage(5, 10, QStringLiteral("first")),
        makeMessage(6, 20),
        makeMessage(5, 10, QStringLiteral("second")),
    });

    QCOMPARE(store.count(), 2);
    QCOMPARE(idsOf(store), (QVector<qint64>{5, 6}));
    QCOMPARE(textOf(store, 5), QStringLiteral("second"));

    // Id 0 is not a message identity and is dropped.
    store.setMessages({makeMessage(0, 10)});
    QCOMPARE(store.count(), 0);
}

void MessageHistoryStoreTest::addOrUpdateInsertsUpdatesAndResorts()
{
    MessageHistoryStore store;
    store.setMessages({makeMessage(1, 10), makeMessage(2, 20)});

    store.addOrUpdate(makeMessage(1, 30, QStringLiteral("moved")));
    QCOMPARE(store.count(), 2);
    QCOMPARE(idsOf(store), (QVector<qint64>{2, 1}));
    QCOMPARE(textOf(store, 1), QStringLiteral("moved"));

    store.addOrUpdate(makeMessage(3, 40));
    QCOMPARE(store.count(), 3);
    QCOMPARE(idsOf(store), (QVector<qint64>{2, 1, 3}));

    store.addOrUpdate(makeMessage(0, 50));
    QCOMPARE(store.count(), 3);
}

void MessageHistoryStoreTest::replaceIdSwapsTemporaryId()
{
    MessageHistoryStore store;
    store.setMessages({makeMessage(10, 100), makeMessage(-1, 200, QStringLiteral("pending"))});

    store.replaceId(-1, makeMessage(42, 200, QStringLiteral("sent")));
    QCOMPARE(store.count(), 2);
    QCOMPARE(idsOf(store), (QVector<qint64>{10, 42}));
    QVERIFY(!store.contains(-1));
    QVERIFY(store.contains(42));
    QCOMPARE(textOf(store, 42), QStringLiteral("sent"));

    // A page that still carries the temporary id cannot bring it back.
    store.mergeOlder({makeMessage(-1, 200, QStringLiteral("pending"))});
    QCOMPARE(store.count(), 2);
    QVERIFY(!store.contains(-1));

    // Replacing an id that is not loaded still inserts the confirmed row.
    store.replaceId(999, makeMessage(43, 300));
    QVERIFY(store.contains(43));

    // Replacing an id with itself behaves like an update.
    store.replaceId(43, makeMessage(43, 300, QStringLiteral("updated")));
    QCOMPARE(textOf(store, 43), QStringLiteral("updated"));
    QCOMPARE(store.count(), 3);
}

void MessageHistoryStoreTest::removeIdsDeletesRows()
{
    MessageHistoryStore store;
    store.setMessages({makeMessage(1, 10), makeMessage(2, 20), makeMessage(3, 30)});

    store.removeIds({2});
    QCOMPARE(idsOf(store), (QVector<qint64>{1, 3}));
    QVERIFY(!store.contains(2));

    store.removeIds({});
    QCOMPARE(store.count(), 2);

    store.removeIds({1, 3, 99});
    QCOMPARE(store.count(), 0);
    QVERIFY(!store.contains(1));
    QVERIFY(!store.contains(3));
}

void MessageHistoryStoreTest::tombstoneBlocksLatePage()
{
    MessageHistoryStore store;
    store.setMessages({makeMessage(1, 10), makeMessage(2, 20)});
    store.removeIds({2});

    store.mergeOlder({makeMessage(2, 20, QStringLiteral("deleted")), makeMessage(3, 30)});
    QCOMPARE(idsOf(store), (QVector<qint64>{1, 3}));
    QVERIFY(!store.contains(2));

    store.addOrUpdate(makeMessage(2, 20, QStringLiteral("deleted")));
    QVERIFY(!store.contains(2));

    // Replacing the whole window also filters the tombstoned id.
    store.setMessages({makeMessage(2, 20), makeMessage(4, 40)});
    QCOMPARE(idsOf(store), (QVector<qint64>{4}));
}

void MessageHistoryStoreTest::tombstonesAreBounded()
{
    MessageHistoryStore store;

    QVector<qint64> ids;
    for (qint64 id = 1; id <= 300; ++id)
        ids.append(id);
    store.removeIds(ids);
    QCOMPARE(store.count(), 0);

    // The oldest tombstones fall out of the 256-id window.
    store.addOrUpdate(makeMessage(1, 10));
    QVERIFY(store.contains(1));
    QCOMPARE(store.count(), 1);

    store.addOrUpdate(makeMessage(44, 20));
    QVERIFY(store.contains(44));
    QCOMPARE(store.count(), 2);

    // The newest tombstones are still enforced.
    store.addOrUpdate(makeMessage(45, 30));
    store.addOrUpdate(makeMessage(300, 40));
    QVERIFY(!store.contains(45));
    QVERIFY(!store.contains(300));
    QCOMPARE(store.count(), 2);
}

void MessageHistoryStoreTest::mergeOlderPrependsWithoutDuplicates()
{
    MessageHistoryStore store;
    store.setMessages({makeMessage(100, 200, QStringLiteral("live")), makeMessage(200, 300)});

    store.mergeOlder({
        makeMessage(50, 100),
        makeMessage(100, 200, QStringLiteral("stale")),
        makeMessage(200, 300, QStringLiteral("stale")),
    });

    QCOMPARE(store.count(), 3);
    QCOMPARE(idsOf(store), (QVector<qint64>{50, 100, 200}));
    QCOMPARE(store.firstMessageId(), qint64(50));
    QCOMPARE(store.lastMessageId(), qint64(200));

    // The live copy wins over the older page snapshot.
    QCOMPARE(textOf(store, 100), QStringLiteral("live"));
    QCOMPARE(textOf(store, 200), QStringLiteral("m200"));

    // Duplicates inside one page are collapsed; the later copy wins.
    store.mergeOlder({
        makeMessage(60, 120, QStringLiteral("a")),
        makeMessage(60, 120, QStringLiteral("b")),
    });
    QCOMPARE(store.count(), 4);
    QCOMPARE(idsOf(store), (QVector<qint64>{50, 60, 100, 200}));
    QCOMPARE(textOf(store, 60), QStringLiteral("b"));

    // A page with nothing new leaves the window untouched.
    store.mergeOlder({makeMessage(50, 100, QStringLiteral("stale"))});
    QCOMPARE(store.count(), 4);
    QCOMPARE(textOf(store, 50), QStringLiteral("m50"));
}

void MessageHistoryStoreTest::mergeOlderIgnoresTombstonedIds()
{
    MessageHistoryStore store;
    store.setMessages({makeMessage(10, 100)});
    store.removeIds({5, 10});

    store.mergeOlder({
        makeMessage(5, 50, QStringLiteral("gone")),
        makeMessage(10, 100, QStringLiteral("gone")),
        makeMessage(7, 70),
    });

    QCOMPARE(idsOf(store), (QVector<qint64>{7}));
    QVERIFY(!store.contains(5));
    QVERIFY(!store.contains(10));
}

void MessageHistoryStoreTest::clearResetsRowsAndTombstones()
{
    MessageHistoryStore store;
    store.setMessages({makeMessage(1, 10)});
    store.removeIds({2});

    store.clear();
    QCOMPARE(store.count(), 0);
    QVERIFY(store.messages().isEmpty());
    QVERIFY(!store.contains(1));

    // clear() forgets tombstones, so the store can be reused for a chat.
    store.addOrUpdate(makeMessage(2, 20));
    QVERIFY(store.contains(2));
    QCOMPARE(store.count(), 1);
}

void MessageHistoryStoreTest::changedSignalFiresOnlyOnMutation()
{
    MessageHistoryStore store;
    QSignalSpy spy(&store, &MessageHistoryStore::changed);

    store.addOrUpdate(makeMessage(1, 10));
    QCOMPARE(spy.count(), 1);

    store.addOrUpdate(makeMessage(1, 10, QStringLiteral("edit")));
    QCOMPARE(spy.count(), 2);

    // Only a duplicate id, so nothing changes.
    store.mergeOlder({makeMessage(1, 10, QStringLiteral("page"))});
    QCOMPARE(spy.count(), 2);

    store.mergeOlder({makeMessage(2, 20)});
    QCOMPARE(spy.count(), 3);

    store.removeIds({});
    QCOMPARE(spy.count(), 3);

    store.removeIds({1});
    QCOMPARE(spy.count(), 4);

    // Already removed and already tombstoned.
    store.removeIds({1});
    QCOMPARE(spy.count(), 4);

    store.clear();
    QCOMPARE(spy.count(), 5);

    store.clear();
    QCOMPARE(spy.count(), 5);
}

QTEST_GUILESS_MAIN(MessageHistoryStoreTest)
#include "tst_message_history_store.moc"
