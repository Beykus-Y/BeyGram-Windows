#include "telegram/MessageThread.h"
#include "telegram/ThreadMessageModel.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSignalSpy>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& request)
{
    return QJsonDocument::fromJson(QJsonDocument(request).toJson(QJsonDocument::Compact)).object();
}

QJsonObject objectFromJson(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

qint64 idOf(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

MessageData makeMessage(qint64 id, const QString& text = {})
{
    MessageData message;
    message.id = id;
    // Deliberately the reverse of id order: the model must sort by id, not date.
    message.date = 1000000 - id;
    message.text = text.isEmpty() ? QStringLiteral("m%1").arg(id) : text;
    return message;
}

QVector<qint64> idsOf(const QVector<MessageData>& messages)
{
    QVector<qint64> ids;
    ids.reserve(messages.size());
    for (const MessageData& message : messages)
        ids.append(message.id);
    return ids;
}

QString textOf(const ThreadMessageModel& model, qint64 id)
{
    const QVector<MessageData> messages = model.messages();
    for (const MessageData& message : messages) {
        if (message.id == id)
            return message.text;
    }
    return {};
}

} // namespace

class MessageThreadTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsGetMessageThreadRequest();
    void rejectsNonPositiveThreadIds();
    void buildsHistoryRequest();
    void clampsHistoryPaging();
    void parsesThreadInfo();
    void parsesMessageIdFallback();
    void dedupesRecentIdsAndSkipsNonPositive();
    void handlesMissingReplyInfo();
    void clampsReplyCount();
    void startsEmpty();
    void setThreadActivates();
    void setThreadIgnoresInvalidIds();
    void setThreadUpdatesReplyCount();
    void setThreadSwitchClearsMessages();
    void setMessagesSortsDedupesAndDropsInvalidIds();
    void setMessagesReplacesPreviousPage();
    void appendOlderMergesOnlyNewIds();
    void appendOlderKeepsLoadedCopy();
    void appendOlderEmitsOnlyWhenNewIdsArrive();
    void clearResetsThreadAndMessages();
    void messagesReturnsSnapshot();
    void emitsChangedOnMutationsButNotOnEmptyNoOps();
};

void MessageThreadTest::buildsGetMessageThreadRequest()
{
    const QJsonObject request = serialized(getMessageThreadRequest(42, 900));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getMessageThread"));
    QCOMPARE(idOf(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(idOf(request, QStringLiteral("message_id")), qint64(900));
    QCOMPARE(int(request.size()), 3);
}

void MessageThreadTest::rejectsNonPositiveThreadIds()
{
    QVERIFY(getMessageThreadRequest(0, 900).isEmpty());
    QVERIFY(getMessageThreadRequest(-42, 900).isEmpty());
    QVERIFY(getMessageThreadRequest(42, 0).isEmpty());
    QVERIFY(getMessageThreadRequest(42, -5).isEmpty());

    QVERIFY(getMessageThreadHistoryRequest(0, 900, 0, 0, 20).isEmpty());
    QVERIFY(getMessageThreadHistoryRequest(-42, 900, 0, 0, 20).isEmpty());
    QVERIFY(getMessageThreadHistoryRequest(42, 0, 0, 0, 20).isEmpty());
    QVERIFY(getMessageThreadHistoryRequest(42, -9, 0, 0, 20).isEmpty());
}

void MessageThreadTest::buildsHistoryRequest()
{
    const QJsonObject request =
        serialized(getMessageThreadHistoryRequest(42, 900, 0, 0, 20));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getMessageThreadHistory"));
    QCOMPARE(idOf(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(idOf(request, QStringLiteral("message_id")), qint64(900));
    // 0 is the newest page, so the first request is valid.
    QCOMPARE(idOf(request, QStringLiteral("from_message_id")), qint64(0));
    QCOMPARE(idOf(request, QStringLiteral("offset")), qint64(0));
    QCOMPARE(idOf(request, QStringLiteral("limit")), qint64(20));
    QCOMPARE(int(request.size()), 6);
}

void MessageThreadTest::clampsHistoryPaging()
{
    const QJsonObject low =
        serialized(getMessageThreadHistoryRequest(42, 900, -7, -3, 0));
    QCOMPARE(idOf(low, QStringLiteral("from_message_id")), qint64(0));
    QCOMPARE(idOf(low, QStringLiteral("offset")), qint64(0));
    QCOMPARE(idOf(low, QStringLiteral("limit")), qint64(1));

    const QJsonObject high =
        serialized(getMessageThreadHistoryRequest(42, 900, 500, 7, 500));
    QCOMPARE(idOf(high, QStringLiteral("from_message_id")), qint64(500));
    QCOMPARE(idOf(high, QStringLiteral("offset")), qint64(7));
    QCOMPARE(idOf(high, QStringLiteral("limit")), qint64(100));
}

void MessageThreadTest::parsesThreadInfo()
{
    const QJsonObject response = objectFromJson(R"({
        "@type": "messageThreadInfo",
        "chat_id": -1001234567890,
        "message_thread_id": 900,
        "reply_info": {
            "@type": "messageReplyInfo",
            "reply_count": 7,
            "recent_replier_ids": [],
            "last_read_inbox_message_id": 0,
            "last_read_outbox_message_id": 0,
            "last_message_id": 905
        },
        "recent_message_ids": [903, 901, 905]
    })");

    const MessageThreadInfo info = messageThreadFromResponse(response);

    QCOMPARE(info.chatId, qint64(-1001234567890LL));
    QCOMPARE(info.messageId, qint64(900));
    QCOMPARE(info.replyCount, 7);
    // Server order is preserved; the parser does not sort the preview ids.
    QCOMPARE(info.recentMessageIds, (QVector<qint64>{903, 901, 905}));
}

void MessageThreadTest::parsesMessageIdFallback()
{
    const QJsonObject response = objectFromJson(R"({
        "chat_id": 42,
        "message_id": 77,
        "reply_info": { "reply_count": 2 }
    })");

    const MessageThreadInfo info = messageThreadFromResponse(response);
    QCOMPARE(info.chatId, qint64(42));
    QCOMPARE(info.messageId, qint64(77));
    QCOMPARE(info.replyCount, 2);
    QVERIFY(info.recentMessageIds.isEmpty());
}

void MessageThreadTest::dedupesRecentIdsAndSkipsNonPositive()
{
    const QJsonObject response = objectFromJson(R"({
        "recent_message_ids": [5, "7", -1, 0, 5, 7, 2, 2, "-3", true, null, {}]
    })");

    const MessageThreadInfo info = messageThreadFromResponse(response);
    QCOMPARE(info.recentMessageIds, (QVector<qint64>{5, 7, 2}));
}

void MessageThreadTest::handlesMissingReplyInfo()
{
    const MessageThreadInfo empty = messageThreadFromResponse(QJsonObject{});
    QCOMPARE(empty.chatId, qint64(0));
    QCOMPARE(empty.messageId, qint64(0));
    QCOMPARE(empty.replyCount, 0);
    QVERIFY(empty.recentMessageIds.isEmpty());

    const MessageThreadInfo malformed = messageThreadFromResponse(objectFromJson(R"({
        "chat_id": 42,
        "reply_info": "not an object",
        "recent_message_ids": "not an array"
    })"));
    QCOMPARE(malformed.chatId, qint64(42));
    QCOMPARE(malformed.replyCount, 0);
    QVERIFY(malformed.recentMessageIds.isEmpty());
}

void MessageThreadTest::clampsReplyCount()
{
    const QJsonObject negative =
        objectFromJson(R"({ "reply_info": { "reply_count": -4 } })");
    QCOMPARE(messageThreadFromResponse(negative).replyCount, 0);

    const QJsonObject huge = objectFromJson(
        R"({ "reply_info": { "reply_count": 5000000000 } })");
    QCOMPARE(messageThreadFromResponse(huge).replyCount,
             std::numeric_limits<int>::max());
}

void MessageThreadTest::startsEmpty()
{
    ThreadMessageModel model;

    QVERIFY(!model.hasThread());
    QCOMPARE(model.chatId(), qint64(0));
    QCOMPARE(model.messageId(), qint64(0));
    QCOMPARE(model.replyCount(), 0);
    QCOMPARE(model.count(), 0);
    QVERIFY(model.messages().isEmpty());
}

void MessageThreadTest::setThreadActivates()
{
    ThreadMessageModel model;
    QSignalSpy spy(&model, &ThreadMessageModel::changed);

    model.setThread(42, 900, 5);

    QVERIFY(model.hasThread());
    QCOMPARE(model.chatId(), qint64(42));
    QCOMPARE(model.messageId(), qint64(900));
    QCOMPARE(model.replyCount(), 5);
    QCOMPARE(model.count(), 0);
    QCOMPARE(spy.count(), 1);
}

void MessageThreadTest::setThreadIgnoresInvalidIds()
{
    ThreadMessageModel model;
    QSignalSpy spy(&model, &ThreadMessageModel::changed);

    model.setThread(0, 900, 5);
    model.setThread(-42, 900, 5);
    model.setThread(42, 0, 5);
    model.setThread(42, -9, 5);

    QVERIFY(!model.hasThread());
    QCOMPARE(model.chatId(), qint64(0));
    QCOMPARE(model.messageId(), qint64(0));
    QCOMPARE(model.replyCount(), 0);
    QCOMPARE(spy.count(), 0);
}

void MessageThreadTest::setThreadUpdatesReplyCount()
{
    ThreadMessageModel model;
    model.setThread(42, 900, 5);
    model.setMessages({makeMessage(3), makeMessage(1)});

    QSignalSpy spy(&model, &ThreadMessageModel::changed);

    model.setThread(42, 900, 9);
    QCOMPARE(model.replyCount(), 9);
    QCOMPARE(model.count(), 2);
    QCOMPARE(spy.count(), 1);

    // The same pair and count is a no-op.
    model.setThread(42, 900, 9);
    QCOMPARE(spy.count(), 1);

    // A negative count is stored as 0 but the loaded comments stay.
    model.setThread(42, 900, -4);
    QCOMPARE(model.replyCount(), 0);
    QCOMPARE(model.count(), 2);
    QCOMPARE(spy.count(), 2);
}

void MessageThreadTest::setThreadSwitchClearsMessages()
{
    ThreadMessageModel model;
    model.setThread(42, 900, 5);
    model.setMessages({makeMessage(1), makeMessage(2)});

    QSignalSpy spy(&model, &ThreadMessageModel::changed);

    model.setThread(43, 901, 2);

    QCOMPARE(model.chatId(), qint64(43));
    QCOMPARE(model.messageId(), qint64(901));
    QCOMPARE(model.replyCount(), 2);
    QCOMPARE(model.count(), 0);
    QVERIFY(model.messages().isEmpty());
    QCOMPARE(spy.count(), 1);
}

void MessageThreadTest::setMessagesSortsDedupesAndDropsInvalidIds()
{
    ThreadMessageModel model;
    model.setMessages({
        makeMessage(5, QStringLiteral("first")),
        makeMessage(1),
        makeMessage(5, QStringLiteral("second")),
        makeMessage(0),
        makeMessage(-2),
        makeMessage(3),
    });

    QCOMPARE(model.count(), 3);
    QCOMPARE(idsOf(model.messages()), (QVector<qint64>{1, 3, 5}));
    // The later duplicate is the live copy and wins.
    QCOMPARE(textOf(model, 5), QStringLiteral("second"));
}

void MessageThreadTest::setMessagesReplacesPreviousPage()
{
    ThreadMessageModel model;

    model.setMessages({makeMessage(1), makeMessage(2), makeMessage(3)});
    model.setMessages({makeMessage(9), makeMessage(8)});

    QCOMPARE(model.count(), 2);
    QCOMPARE(idsOf(model.messages()), (QVector<qint64>{8, 9}));

    model.setMessages({});
    QCOMPARE(model.count(), 0);
    QVERIFY(model.messages().isEmpty());
}

void MessageThreadTest::appendOlderMergesOnlyNewIds()
{
    ThreadMessageModel model;
    model.setMessages({makeMessage(3), makeMessage(5)});

    model.appendOlder({
        makeMessage(1),
        makeMessage(5),
        makeMessage(3),
        makeMessage(7),
        makeMessage(0),
        makeMessage(-1),
        makeMessage(7),
    });

    QCOMPARE(model.count(), 4);
    QCOMPARE(idsOf(model.messages()), (QVector<qint64>{1, 3, 5, 7}));
}

void MessageThreadTest::appendOlderKeepsLoadedCopy()
{
    ThreadMessageModel model;
    model.setMessages({makeMessage(5, QStringLiteral("live"))});

    model.appendOlder({
        makeMessage(5, QStringLiteral("page")),
        makeMessage(4, QStringLiteral("older")),
    });

    QCOMPARE(model.count(), 2);
    QCOMPARE(idsOf(model.messages()), (QVector<qint64>{4, 5}));
    QCOMPARE(textOf(model, 5), QStringLiteral("live"));
}

void MessageThreadTest::appendOlderEmitsOnlyWhenNewIdsArrive()
{
    ThreadMessageModel model;
    model.setMessages({makeMessage(3)});

    QSignalSpy spy(&model, &ThreadMessageModel::changed);

    model.appendOlder({});
    QCOMPARE(spy.count(), 0);

    model.appendOlder({makeMessage(3, QStringLiteral("page copy"))});
    QCOMPARE(spy.count(), 0);

    model.appendOlder({makeMessage(2)});
    QCOMPARE(spy.count(), 1);
    QCOMPARE(idsOf(model.messages()), (QVector<qint64>{2, 3}));

    model.appendOlder({makeMessage(2)});
    QCOMPARE(spy.count(), 1);
}

void MessageThreadTest::clearResetsThreadAndMessages()
{
    ThreadMessageModel model;
    QSignalSpy spy(&model, &ThreadMessageModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);

    model.setThread(42, 900, 5);
    model.setMessages({makeMessage(1), makeMessage(2)});
    QCOMPARE(spy.count(), 2);

    model.clear();
    QCOMPARE(spy.count(), 3);
    QVERIFY(!model.hasThread());
    QCOMPARE(model.chatId(), qint64(0));
    QCOMPARE(model.messageId(), qint64(0));
    QCOMPARE(model.replyCount(), 0);
    QCOMPARE(model.count(), 0);
    QVERIFY(model.messages().isEmpty());

    model.clear();
    QCOMPARE(spy.count(), 3);
}

void MessageThreadTest::messagesReturnsSnapshot()
{
    ThreadMessageModel model;
    model.setMessages({makeMessage(2, QStringLiteral("original"))});

    QVector<MessageData> snapshot = model.messages();
    QCOMPARE(snapshot.size(), 1);
    snapshot[0].text = QStringLiteral("mutated");
    snapshot.append(makeMessage(3));

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.messages().at(0).text, QStringLiteral("original"));
}

void MessageThreadTest::emitsChangedOnMutationsButNotOnEmptyNoOps()
{
    ThreadMessageModel model;
    QSignalSpy spy(&model, &ThreadMessageModel::changed);

    // An empty page does not change an empty model.
    model.setMessages({});
    QCOMPARE(spy.count(), 0);

    model.setMessages({makeMessage(1)});
    QCOMPARE(spy.count(), 1);

    // Re-parsing the identical page is not a change.
    model.setMessages({makeMessage(1)});
    QCOMPARE(spy.count(), 1);

    // A live edit of an already loaded row is.
    model.setMessages({makeMessage(1, QStringLiteral("edited"))});
    QCOMPARE(spy.count(), 2);

    model.setMessages({});
    QCOMPARE(spy.count(), 3);

    model.setMessages({});
    QCOMPARE(spy.count(), 3);

    model.setThread(42, 900, 0);
    QCOMPARE(spy.count(), 4);

    model.setThread(42, 900, 0);
    QCOMPARE(spy.count(), 4);

    model.appendOlder({});
    QCOMPARE(spy.count(), 4);

    model.clear();
    QCOMPARE(spy.count(), 5);
}

QTEST_GUILESS_MAIN(MessageThreadTest)
#include "tst_message_thread.moc"
