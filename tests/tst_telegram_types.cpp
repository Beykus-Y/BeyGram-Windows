#include "telegram/TelegramTypes.h"

#include <QJsonDocument>
#include <QtTest>

using namespace beygram::telegram;

class TelegramTypesTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesAChat();
    void appliesChatPositionRemoval();
    void buildsPreviews();
    void parsesMessages();
    void handlesTemporarySendingState();
    void computesInitialsAndAccent();
    void readsInt64FromNumbersAndStrings();
};

void TelegramTypesTest::parsesAChat()
{
    const QJsonObject chat = QJsonDocument::fromJson(R"({
        "id": 42,
        "title": "Alice",
        "type": {"@type": "chatTypePrivate"},
        "unread_count": 3,
        "notification_settings": {"mute_for": 3600},
        "last_message": {
            "id": 100,
            "date": 1700000000,
            "is_outgoing": false,
            "content": {"@type": "messageText", "text": {"text": "Hello"}}
        },
        "positions": [{
            "list": {"@type": "chatListMain"},
            "order": 900,
            "is_pinned": true
        }]
    })")
                                  .object();

    const ChatData data = chatFromJson(chat);
    QCOMPARE(data.id, qint64(42));
    QCOMPARE(data.title, QStringLiteral("Alice"));
    QCOMPARE(data.type, QStringLiteral("chatTypePrivate"));
    QCOMPARE(data.unreadCount, 3);
    QVERIFY(data.muted);
    QCOMPARE(data.preview, QStringLiteral("Hello"));
    QCOMPARE(data.lastMessageId, qint64(100));
    QCOMPARE(data.lastMessageDate, qint64(1700000000));
    QVERIFY(data.pinned);
    QVERIFY(data.inMainList);
    QCOMPARE(data.order, qint64(900));
}

void TelegramTypesTest::appliesChatPositionRemoval()
{
    ChatData chat;
    chat.inMainList = true;
    chat.order = 10;
    applyChatPosition(chat, QJsonObject{
                                {QStringLiteral("list"),
                                 QJsonObject{{QStringLiteral("@type"),
                                              QStringLiteral("chatListMain")}}},
                                {QStringLiteral("order"), 0},
                            });
    QVERIFY(!chat.inMainList);
    QCOMPARE(chat.order, qint64(0));
}

void TelegramTypesTest::buildsPreviews()
{
    const QJsonObject text = QJsonDocument::fromJson(R"({
        "is_outgoing": false,
        "content": {"@type": "messageText", "text": {"text": "line one\nline two"}}
    })")
                                 .object();
    QCOMPARE(chatPreview(text), QStringLiteral("line one line two"));

    const QJsonObject photo = QJsonDocument::fromJson(R"({
        "is_outgoing": false,
        "content": {"@type": "messagePhoto", "caption": {"text": "sunset"}}
    })")
                                  .object();
    QCOMPARE(chatPreview(photo), QStringLiteral("sunset"));

    const QJsonObject video = QJsonDocument::fromJson(R"({
        "is_outgoing": true,
        "content": {"@type": "messageVideo", "caption": {"text": ""}}
    })")
                                  .object();
    QCOMPARE(chatPreview(video), QStringLiteral("You: Video"));
}

void TelegramTypesTest::parsesMessages()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "id": 500,
        "chat_id": 42,
        "date": 1700000100,
        "is_outgoing": true,
        "edit_date": 1700000200,
        "sender_id": {"@type": "messageSenderUser", "user_id": 7},
        "content": {"@type": "messageText", "text": {"text": "Edited text"}}
    })")
                                    .object();

    const MessageData data = messageFromJson(message);
    QCOMPARE(data.id, qint64(500));
    QCOMPARE(data.chatId, qint64(42));
    QCOMPARE(data.senderUserId, qint64(7));
    QCOMPARE(data.text, QStringLiteral("Edited text"));
    QCOMPARE(data.contentType, QStringLiteral("text"));
    QVERIFY(data.outgoing);
    QVERIFY(data.edited);
    QVERIFY(!data.pending);
    QVERIFY(!data.failed);
    QCOMPARE(data.senderName, QStringLiteral("You"));
}

void TelegramTypesTest::handlesTemporarySendingState()
{
    const QJsonObject failed = QJsonDocument::fromJson(R"({
        "id": 600,
        "chat_id": 42,
        "date": 1700000300,
        "is_outgoing": true,
        "sending_state": {"@type": "messageSendingStateFailed"},
        "content": {"@type": "messageText", "text": {"text": "Pending"}}
    })")
                                   .object();
    const MessageData data = messageFromJson(failed);
    QVERIFY(data.failed);
    QVERIFY(!data.pending);

    const QJsonObject pending = QJsonDocument::fromJson(R"({
        "id": 601,
        "chat_id": 42,
        "date": 1700000301,
        "is_outgoing": true,
        "sending_state": {"@type": "messageSendingStatePending"},
        "content": {"@type": "messageText", "text": {"text": "Sending"}}
    })")
                                    .object();
    QVERIFY(messageFromJson(pending).pending);
}

void TelegramTypesTest::computesInitialsAndAccent()
{
    QCOMPARE(initialsForTitle(QStringLiteral("telegram")), QStringLiteral("T"));
    QCOMPARE(initialsForTitle(QStringLiteral("  ")), QStringLiteral("?"));
    QVERIFY(accentIndexForId(10) >= 0);
    QVERIFY(accentIndexForId(10) < 6);
    QCOMPARE(accentIndexForId(-10), accentIndexForId(10));
}

void TelegramTypesTest::readsInt64FromNumbersAndStrings()
{
    QCOMPARE(jsonInt64(QJsonValue(42.0)), qint64(42));
    QCOMPARE(jsonInt64(QJsonValue(QStringLiteral("9223372036854775807"))),
             qint64(9223372036854775807LL));
    QCOMPARE(jsonInt64(QJsonValue()), qint64(0));
}

QTEST_GUILESS_MAIN(TelegramTypesTest)
#include "tst_telegram_types.moc"
