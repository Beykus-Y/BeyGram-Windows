#include "telegram/NotificationFormatter.h"

#include <QJsonDocument>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject objectFromJson(const char* json)
{
    return QJsonDocument::fromJson(json).object();
}

} // namespace

class NotificationFormatterTest final : public QObject {
    Q_OBJECT

private slots:
    void ignoresOutgoingMessages();
    void ignoresCurrentChat();
    void ignoresMutedChats();
    void ignoresServiceMessages();
    void ignoresUnsupportedContent();
    void notifiesForPrivateText();
    void usesPhotoCaptionOrLabel();
    void usesGroupChatTitle();
    void collapsesNewlinesAndTruncatesBody();
    void ignoresInvalidIds();
};

void NotificationFormatterTest::ignoresOutgoingMessages()
{
    const QJsonObject message = objectFromJson(R"({
        "id": 11,
        "chat_id": 42,
        "is_outgoing": true,
        "content": {"@type": "messageText", "text": {"text": "Sent by me"}}
    })");

    const NotificationPayload payload =
        notificationForMessage(message, QStringLiteral("Alice"), true, 0, false);
    QVERIFY(!payload.shouldNotify);
    QVERIFY(payload.title.isEmpty());
    QVERIFY(payload.body.isEmpty());
}

void NotificationFormatterTest::ignoresCurrentChat()
{
    const QJsonObject message = objectFromJson(R"({
        "id": 12,
        "chat_id": 42,
        "is_outgoing": false,
        "content": {"@type": "messageText", "text": {"text": "While reading"}}
    })");

    const NotificationPayload payload =
        notificationForMessage(message, QStringLiteral("Alice"), true, 42, false);
    QVERIFY(!payload.shouldNotify);
    QVERIFY(payload.body.isEmpty());
}

void NotificationFormatterTest::ignoresMutedChats()
{
    const QJsonObject message = objectFromJson(R"({
        "id": 13,
        "chat_id": 42,
        "is_outgoing": false,
        "content": {"@type": "messageText", "text": {"text": "Quiet please"}}
    })");

    const NotificationPayload payload =
        notificationForMessage(message, QStringLiteral("Alice"), true, 0, true);
    QVERIFY(!payload.shouldNotify);
    QVERIFY(payload.title.isEmpty());
    QVERIFY(payload.body.isEmpty());
}

void NotificationFormatterTest::ignoresServiceMessages()
{
    const QJsonObject message = objectFromJson(R"({
        "id": 14,
        "chat_id": 42,
        "is_outgoing": false,
        "content": {"@type": "messageBasicGroupChatCreate", "title": "Group"}
    })");

    const NotificationPayload payload =
        notificationForMessage(message, QStringLiteral("Alice"), false, 0, false);
    QVERIFY(!payload.shouldNotify);
    QVERIFY(payload.body.isEmpty());
}

void NotificationFormatterTest::ignoresUnsupportedContent()
{
    const QJsonObject message = objectFromJson(R"({
        "id": 15,
        "chat_id": 42,
        "is_outgoing": false,
        "content": {"@type": "messageUnsupported"}
    })");

    const NotificationPayload payload =
        notificationForMessage(message, QStringLiteral("Alice"), true, 0, false);
    QVERIFY(!payload.shouldNotify);
    QVERIFY(payload.title.isEmpty());
    QVERIFY(payload.body.isEmpty());
}

void NotificationFormatterTest::notifiesForPrivateText()
{
    const QJsonObject message = objectFromJson(R"({
        "id": 16,
        "chat_id": 42,
        "is_outgoing": false,
        "content": {"@type": "messageText", "text": {"text": "  Hello there  "}}
    })");

    const NotificationPayload payload =
        notificationForMessage(message, QStringLiteral("Alice"), true, 0, false);
    QVERIFY(payload.shouldNotify);
    QCOMPARE(payload.chatId, qint64(42));
    QCOMPARE(payload.messageId, qint64(16));
    QCOMPARE(payload.title, QStringLiteral("Alice"));
    QCOMPARE(payload.body, QStringLiteral("Hello there"));
}

void NotificationFormatterTest::usesPhotoCaptionOrLabel()
{
    const QJsonObject withCaption = objectFromJson(R"({
        "id": 17,
        "chat_id": 42,
        "is_outgoing": false,
        "content": {
            "@type": "messagePhoto",
            "photo": {"sizes": []},
            "caption": {"text": "  Sunset over the sea  "}
        }
    })");
    const NotificationPayload captioned =
        notificationForMessage(withCaption, QStringLiteral("Alice"), true, 0, false);
    QVERIFY(captioned.shouldNotify);
    QCOMPARE(captioned.body, QStringLiteral("Sunset over the sea"));

    const QJsonObject withoutCaption = objectFromJson(R"({
        "id": 18,
        "chat_id": 42,
        "is_outgoing": false,
        "content": {"@type": "messagePhoto", "photo": {"sizes": []}}
    })");
    const NotificationPayload labelled =
        notificationForMessage(withoutCaption, QStringLiteral("Alice"), true, 0, false);
    QVERIFY(labelled.shouldNotify);
    QCOMPARE(labelled.body, QStringLiteral("Photo"));
}

void NotificationFormatterTest::usesGroupChatTitle()
{
    const QJsonObject message = objectFromJson(R"({
        "id": 19,
        "chat_id": 7,
        "is_outgoing": false,
        "content": {"@type": "messageVoiceNote", "voice_note": {"duration": 4}}
    })");

    const NotificationPayload payload =
        notificationForMessage(message, QStringLiteral("Team"), false, 0, false);
    QVERIFY(payload.shouldNotify);
    QCOMPARE(payload.chatId, qint64(7));
    QCOMPARE(payload.title, QStringLiteral("Team"));
    QCOMPARE(payload.body, QStringLiteral("Voice message"));
}

void NotificationFormatterTest::collapsesNewlinesAndTruncatesBody()
{
    const QJsonObject multiline = objectFromJson(R"({
        "id": 20,
        "chat_id": 42,
        "is_outgoing": false,
        "content": {"@type": "messageText", "text": {"text": "Hello\n\nworld\r\n  again"}}
    })");
    const NotificationPayload collapsed =
        notificationForMessage(multiline, QStringLiteral("Alice"), true, 0, false);
    QVERIFY(collapsed.shouldNotify);
    QCOMPARE(collapsed.body, QStringLiteral("Hello world again"));

    const QString longText(400, QLatin1Char('a'));
    QJsonObject message = objectFromJson(R"({
        "id": 21,
        "chat_id": 42,
        "is_outgoing": false,
        "content": {"@type": "messageText"}
    })");
    message[QStringLiteral("content")] = QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageText")},
        {QStringLiteral("text"), QJsonObject{{QStringLiteral("text"), longText}}},
    };

    const NotificationPayload truncated =
        notificationForMessage(message, QStringLiteral("Alice"), true, 0, false);
    QVERIFY(truncated.shouldNotify);
    QVERIFY(truncated.body.size() <= 181);
    QVERIFY(truncated.body.endsWith(QChar(0x2026)));
    QVERIFY(truncated.body.startsWith(QString(180, QLatin1Char('a'))));
}

void NotificationFormatterTest::ignoresInvalidIds()
{
    const QJsonObject missingIds = objectFromJson(R"({
        "is_outgoing": false,
        "content": {"@type": "messageText", "text": {"text": "No ids"}}
    })");
    const NotificationPayload missing =
        notificationForMessage(missingIds, QStringLiteral("Alice"), true, 0, false);
    QVERIFY(!missing.shouldNotify);
    QVERIFY(missing.body.isEmpty());

    const QJsonObject zeroId = objectFromJson(R"({
        "id": 0,
        "chat_id": 42,
        "is_outgoing": false,
        "content": {"@type": "messageText", "text": {"text": "Zero id"}}
    })");
    const NotificationPayload zero =
        notificationForMessage(zeroId, QStringLiteral("Alice"), true, 0, false);
    QVERIFY(!zero.shouldNotify);
    QVERIFY(zero.body.isEmpty());

    const QJsonObject zeroChat = objectFromJson(R"({
        "id": 22,
        "chat_id": 0,
        "is_outgoing": false,
        "content": {"@type": "messageText", "text": {"text": "Zero chat"}}
    })");
    const NotificationPayload noChat =
        notificationForMessage(zeroChat, QStringLiteral("Alice"), true, 0, false);
    QVERIFY(!noChat.shouldNotify);
    QVERIFY(noChat.body.isEmpty());
}

QTEST_GUILESS_MAIN(NotificationFormatterTest)
#include "tst_notification_formatter.moc"
