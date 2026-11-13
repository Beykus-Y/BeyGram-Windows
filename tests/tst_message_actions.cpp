#include "telegram/MessageActions.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

constexpr int kMaxMessageTtl = 604800;

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

bool boolAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toBool();
}

} // namespace

class MessageActionsTest final : public QObject {
    Q_OBJECT

private slots:
    void pinsMessage();
    void unpinsMessage();
    void rejectsInvalidPinArguments();
    void unpinsAllMessages();
    void clearsHistoryButKeepsChat();
    void deletesChatAndHistory();
    void marksChatUnread();
    void setsMessageTtl();
    void clampsMessageTtl();
    void getsPinnedMessage();
    void preservesNegativeChatIds();
    void rejectsZeroChatId();
    void neverAddsExtra();
    void survivesJsonRoundTrip();
};

void MessageActionsTest::pinsMessage()
{
    const QJsonObject request = serialized(messageActionsPinRequest(42, 900, true));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("pinChatMessage"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(numberAt(request, QStringLiteral("message_id")), qint64(900));
    QVERIFY(request.contains(QStringLiteral("disable_notification")));
    QVERIFY(!boolAt(request, QStringLiteral("disable_notification")));
    QVERIFY(request.contains(QStringLiteral("only_for_self")));
    QVERIFY(!boolAt(request, QStringLiteral("only_for_self")));
    QCOMPARE(int(request.size()), 5);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void MessageActionsTest::unpinsMessage()
{
    const QJsonObject request = serialized(messageActionsPinRequest(42, 900, false));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("unpinChatMessage"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(numberAt(request, QStringLiteral("message_id")), qint64(900));
    QCOMPARE(int(request.size()), 3);
    QVERIFY(!request.contains(QStringLiteral("disable_notification")));
    QVERIFY(!request.contains(QStringLiteral("only_for_self")));
}

void MessageActionsTest::rejectsInvalidPinArguments()
{
    // Zero is not a chat, and a pin or unpin always names a real message.
    QVERIFY(messageActionsPinRequest(0, 900, true).isEmpty());
    QVERIFY(messageActionsPinRequest(0, 900, false).isEmpty());
    QVERIFY(messageActionsPinRequest(42, 0, true).isEmpty());
    QVERIFY(messageActionsPinRequest(42, -7, true).isEmpty());
    QVERIFY(messageActionsPinRequest(42, 0, false).isEmpty());
    QVERIFY(messageActionsPinRequest(42, -7, false).isEmpty());
}

void MessageActionsTest::unpinsAllMessages()
{
    const QJsonObject request = serialized(unpinAllChatMessagesRequest(42));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("unpinAllChatMessages"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(int(request.size()), 2);
    QVERIFY(!request.contains(QStringLiteral("message_id")));
}

void MessageActionsTest::clearsHistoryButKeepsChat()
{
    // TDLib has no clearChatHistory: clearing keeps the chat in the lists by
    // sending deleteChatHistory with remove_from_chat_list = false.
    const QJsonObject server = serialized(clearChatHistoryRequest(42, true));

    QCOMPARE(server.value(QStringLiteral("@type")).toString(),
             QStringLiteral("deleteChatHistory"));
    QCOMPARE(numberAt(server, QStringLiteral("chat_id")), qint64(42));
    QVERIFY(server.contains(QStringLiteral("remove_from_chat_list")));
    QVERIFY(!boolAt(server, QStringLiteral("remove_from_chat_list")));
    QVERIFY(boolAt(server, QStringLiteral("revoke")));
    QCOMPARE(int(server.size()), 4);

    const QJsonObject local = serialized(clearChatHistoryRequest(42, false));
    QCOMPARE(local.value(QStringLiteral("@type")).toString(),
             QStringLiteral("deleteChatHistory"));
    QVERIFY(!boolAt(local, QStringLiteral("remove_from_chat_list")));
    QVERIFY(!boolAt(local, QStringLiteral("revoke")));
}

void MessageActionsTest::deletesChatAndHistory()
{
    const QJsonObject all = serialized(deleteChatHistoryRequest(42, true));

    QCOMPARE(all.value(QStringLiteral("@type")).toString(),
             QStringLiteral("deleteChatHistory"));
    QCOMPARE(numberAt(all, QStringLiteral("chat_id")), qint64(42));
    QVERIFY(boolAt(all, QStringLiteral("remove_from_chat_list")));
    QVERIFY(boolAt(all, QStringLiteral("revoke")));
    QCOMPARE(int(all.size()), 4);

    const QJsonObject self = serialized(deleteChatHistoryRequest(42, false));
    QVERIFY(boolAt(self, QStringLiteral("remove_from_chat_list")));
    QVERIFY(!boolAt(self, QStringLiteral("revoke")));
}

void MessageActionsTest::marksChatUnread()
{
    const QJsonObject unread = serialized(messageActionsMarkUnreadRequest(42, true));

    QCOMPARE(unread.value(QStringLiteral("@type")).toString(),
             QStringLiteral("toggleChatIsMarkedAsUnread"));
    QCOMPARE(numberAt(unread, QStringLiteral("chat_id")), qint64(42));
    QVERIFY(boolAt(unread, QStringLiteral("is_marked_as_unread")));
    QCOMPARE(int(unread.size()), 3);

    const QJsonObject read = serialized(messageActionsMarkUnreadRequest(42, false));
    QCOMPARE(read.value(QStringLiteral("@type")).toString(),
             QStringLiteral("toggleChatIsMarkedAsUnread"));
    QVERIFY(read.contains(QStringLiteral("is_marked_as_unread")));
    QVERIFY(!boolAt(read, QStringLiteral("is_marked_as_unread")));
}

void MessageActionsTest::setsMessageTtl()
{
    const QJsonObject request = serialized(setChatMessageTtlRequest(42, 3600));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setChatMessageTtl"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(numberAt(request, QStringLiteral("ttl")), qint64(3600));
    QCOMPARE(int(request.size()), 3);
}

void MessageActionsTest::clampsMessageTtl()
{
    // Zero disables the TTL; anything outside the accepted range is clamped
    // rather than sent, because Telegram rejects out-of-range values.
    QCOMPARE(numberAt(serialized(setChatMessageTtlRequest(42, 0)),
                      QStringLiteral("ttl")),
             qint64(0));
    QCOMPARE(numberAt(serialized(setChatMessageTtlRequest(42, -1)),
                      QStringLiteral("ttl")),
             qint64(0));
    QCOMPARE(numberAt(serialized(setChatMessageTtlRequest(42, kMaxMessageTtl)),
                      QStringLiteral("ttl")),
             qint64(kMaxMessageTtl));
    QCOMPARE(numberAt(serialized(setChatMessageTtlRequest(42, kMaxMessageTtl + 1)),
                      QStringLiteral("ttl")),
             qint64(kMaxMessageTtl));
    QCOMPARE(numberAt(serialized(setChatMessageTtlRequest(
                         42, std::numeric_limits<int>::max())),
                      QStringLiteral("ttl")),
             qint64(kMaxMessageTtl));
}

void MessageActionsTest::getsPinnedMessage()
{
    const QJsonObject request = serialized(messageActionsPinnedMessageRequest(42));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getChatPinnedMessage"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(int(request.size()), 2);
    QVERIFY(!request.contains(QStringLiteral("message_id")));
}

void MessageActionsTest::preservesNegativeChatIds()
{
    // Telegram chat ids exceed 32 bits and groups/channels are negative; they
    // must not be truncated or rejected.
    const qint64 chatId = -1001234567890LL;

    QCOMPARE(numberAt(serialized(messageActionsPinRequest(chatId, 7, true)),
                      QStringLiteral("chat_id")),
             chatId);
    QCOMPARE(numberAt(serialized(messageActionsPinRequest(chatId, 7, false)),
                      QStringLiteral("chat_id")),
             chatId);
    QCOMPARE(numberAt(serialized(unpinAllChatMessagesRequest(chatId)),
                      QStringLiteral("chat_id")),
             chatId);
    QCOMPARE(numberAt(serialized(clearChatHistoryRequest(chatId, true)),
                      QStringLiteral("chat_id")),
             chatId);
    QCOMPARE(numberAt(serialized(deleteChatHistoryRequest(chatId, true)),
                      QStringLiteral("chat_id")),
             chatId);
    QCOMPARE(numberAt(serialized(messageActionsMarkUnreadRequest(chatId, true)),
                      QStringLiteral("chat_id")),
             chatId);
    QCOMPARE(numberAt(serialized(setChatMessageTtlRequest(chatId, 86400)),
                      QStringLiteral("chat_id")),
             chatId);
    QCOMPARE(numberAt(serialized(messageActionsPinnedMessageRequest(chatId)),
                      QStringLiteral("chat_id")),
             chatId);
}

void MessageActionsTest::rejectsZeroChatId()
{
    QVERIFY(messageActionsPinRequest(0, 7, true).isEmpty());
    QVERIFY(messageActionsPinRequest(0, 7, false).isEmpty());
    QVERIFY(unpinAllChatMessagesRequest(0).isEmpty());
    QVERIFY(clearChatHistoryRequest(0, true).isEmpty());
    QVERIFY(deleteChatHistoryRequest(0, true).isEmpty());
    QVERIFY(messageActionsMarkUnreadRequest(0, true).isEmpty());
    QVERIFY(setChatMessageTtlRequest(0, 60).isEmpty());
    QVERIFY(messageActionsPinnedMessageRequest(0).isEmpty());
}

void MessageActionsTest::neverAddsExtra()
{
    // The transport owns correlation; a builder that adds @extra would make
    // the response impossible to match.
    const QList<QJsonObject> requests{
        messageActionsPinRequest(1, 2, true),
        messageActionsPinRequest(1, 2, false),
        unpinAllChatMessagesRequest(1),
        clearChatHistoryRequest(1, true),
        deleteChatHistoryRequest(1, true),
        messageActionsMarkUnreadRequest(1, true),
        setChatMessageTtlRequest(1, 60),
        messageActionsPinnedMessageRequest(1),
    };

    for (const QJsonObject& request : requests) {
        QVERIFY(!request.isEmpty());
        QVERIFY(!request.contains(QStringLiteral("@extra")));
        QVERIFY(!request.value(QStringLiteral("@type")).toString().isEmpty());
    }
}

void MessageActionsTest::survivesJsonRoundTrip()
{
    // The builders must produce JSON that survives the compact encoding used by
    // the transport: booleans stay booleans and numbers stay numbers.
    const QJsonObject pin = serialized(messageActionsPinRequest(-1009876543210LL, 900, true));
    QCOMPARE(numberAt(pin, QStringLiteral("chat_id")), qint64(-1009876543210LL));
    QCOMPARE(numberAt(pin, QStringLiteral("message_id")), qint64(900));
    QVERIFY(pin.value(QStringLiteral("disable_notification")).isBool());
    QVERIFY(pin.value(QStringLiteral("only_for_self")).isBool());

    const QJsonObject clear = serialized(clearChatHistoryRequest(5, true));
    QVERIFY(clear.value(QStringLiteral("remove_from_chat_list")).isBool());
    QVERIFY(clear.value(QStringLiteral("revoke")).isBool());

    const QJsonObject ttl = serialized(setChatMessageTtlRequest(5, kMaxMessageTtl));
    QVERIFY(ttl.value(QStringLiteral("ttl")).isDouble());
    QCOMPARE(numberAt(ttl, QStringLiteral("ttl")), qint64(kMaxMessageTtl));
}

QTEST_GUILESS_MAIN(MessageActionsTest)
#include "tst_message_actions.moc"
