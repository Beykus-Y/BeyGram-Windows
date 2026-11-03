#include "telegram/ReplyInfo.h"

#include <QJsonDocument>
#include <QtTest>

using namespace beygram::telegram;

class ReplyInfoTest final : public QObject {
    Q_OBJECT

private slots:
    void ignoresMissingReply();
    void parsesMessageReply();
    void parsesExternalMessageReply();
    void parsesStoryReply();
    void readsIdsFromNumbersAndStrings();
    void rejectsInvalidIdentifiers();
    void buildsInputReplyToRequest();
};

void ReplyInfoTest::ignoresMissingReply()
{
    const QJsonObject withoutKey = QJsonDocument::fromJson(R"({
        "id": 10,
        "chat_id": 42
    })")
                                       .object();
    const ReplyTarget missing = replyTargetFromMessage(withoutKey);
    QCOMPARE(missing.kind, ReplyTarget::Kind::None);
    QVERIFY(!missing.isValid());
    QVERIFY(missing.quoteText.isEmpty());

    const QJsonObject nullReply = QJsonDocument::fromJson(R"({
        "id": 10,
        "chat_id": 42,
        "reply_to": null
    })")
                                      .object();
    const ReplyTarget nullTarget = replyTargetFromMessage(nullReply);
    QCOMPARE(nullTarget.kind, ReplyTarget::Kind::None);
    QVERIFY(!nullTarget.isValid());
}

void ReplyInfoTest::parsesMessageReply()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "id": 11,
        "chat_id": 42,
        "reply_to": {
            "@type": "messageReplyToMessage",
            "chat_id": 42,
            "message_id": 900,
            "quote": {
                "text": {
                    "@type": "formattedText",
                    "text": "quoted words"
                },
                "position": 3,
                "is_manual": true
            }
        }
    })")
                                    .object();

    const ReplyTarget target = replyTargetFromMessage(message);
    QCOMPARE(target.kind, ReplyTarget::Kind::Message);
    QCOMPARE(target.messageId, qint64(900));
    QCOMPARE(target.quoteText, QStringLiteral("quoted words"));
    QVERIFY(target.isValid());

    const QJsonObject noQuote = QJsonDocument::fromJson(R"({
        "id": 12,
        "chat_id": 42,
        "reply_to": {
            "@type": "messageReplyToMessage",
            "chat_id": 42,
            "message_id": 901,
            "quote": null
        }
    })")
                                    .object();
    const ReplyTarget plain = replyTargetFromMessage(noQuote);
    QCOMPARE(plain.kind, ReplyTarget::Kind::Message);
    QCOMPARE(plain.messageId, qint64(901));
    QVERIFY(plain.quoteText.isEmpty());
    QVERIFY(plain.isValid());
}

void ReplyInfoTest::parsesExternalMessageReply()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "id": 13,
        "chat_id": 42,
        "reply_to": {
            "@type": "messageReplyToExternalMessage",
            "chat_id": -1001234567890,
            "message_id": 555,
            "quote": {
                "text": {
                    "@type": "formattedText",
                    "text": "from another chat"
                },
                "position": 0,
                "is_manual": false
            }
        }
    })")
                                    .object();

    const ReplyTarget target = replyTargetFromMessage(message);
    QCOMPARE(target.kind, ReplyTarget::Kind::ExternalMessage);
    QCOMPARE(target.chatId, qint64(-1001234567890));
    QCOMPARE(target.messageId, qint64(555));
    QCOMPARE(target.quoteText, QStringLiteral("from another chat"));
    QVERIFY(target.isValid());
}

void ReplyInfoTest::parsesStoryReply()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "id": 14,
        "chat_id": 42,
        "reply_to": {
            "@type": "messageReplyToStory",
            "story_poster_chat_id": 42,
            "story_id": 7
        }
    })")
                                    .object();

    const ReplyTarget target = replyTargetFromMessage(message);
    QCOMPARE(target.kind, ReplyTarget::Kind::Story);
    QCOMPARE(target.storyPosterChatId, qint64(42));
    QCOMPARE(target.storyId, 7);
    QVERIFY(target.quoteText.isEmpty());
    QVERIFY(target.isValid());
}

void ReplyInfoTest::readsIdsFromNumbersAndStrings()
{
    const QJsonObject numeric = QJsonDocument::fromJson(R"({
        "reply_to": {
            "@type": "messageReplyToExternalMessage",
            "chat_id": -1001234567890,
            "message_id": 1234567890123
        }
    })")
                                    .object();
    const QJsonObject textual = QJsonDocument::fromJson(R"({
        "reply_to": {
            "@type": "messageReplyToExternalMessage",
            "chat_id": "-1001234567890",
            "message_id": "1234567890123"
        }
    })")
                                    .object();

    const ReplyTarget fromNumbers = replyTargetFromMessage(numeric);
    const ReplyTarget fromStrings = replyTargetFromMessage(textual);
    QCOMPARE(fromNumbers.chatId, qint64(-1001234567890));
    QCOMPARE(fromNumbers.messageId, qint64(1234567890123));
    QCOMPARE(fromStrings.chatId, fromNumbers.chatId);
    QCOMPARE(fromStrings.messageId, fromNumbers.messageId);
    QVERIFY(fromStrings.isValid());

    const QJsonObject story = QJsonDocument::fromJson(R"({
        "reply_to": {
            "@type": "messageReplyToStory",
            "story_poster_chat_id": "42",
            "story_id": "9"
        }
    })")
                                  .object();
    const ReplyTarget storyTarget = replyTargetFromMessage(story);
    QCOMPARE(storyTarget.storyPosterChatId, qint64(42));
    QCOMPARE(storyTarget.storyId, 9);
    QVERIFY(storyTarget.isValid());
}

void ReplyInfoTest::rejectsInvalidIdentifiers()
{
    const QJsonObject zeroMessage = QJsonDocument::fromJson(R"({
        "reply_to": {
            "@type": "messageReplyToMessage",
            "chat_id": 42,
            "message_id": 0
        }
    })")
                                        .object();
    const ReplyTarget zeroTarget = replyTargetFromMessage(zeroMessage);
    QCOMPARE(zeroTarget.kind, ReplyTarget::Kind::Message);
    QVERIFY(!zeroTarget.isValid());

    const QJsonObject zeroExternal = QJsonDocument::fromJson(R"({
        "reply_to": {
            "@type": "messageReplyToExternalMessage",
            "chat_id": 42,
            "message_id": 0
        }
    })")
                                         .object();
    QVERIFY(!replyTargetFromMessage(zeroExternal).isValid());

    const QJsonObject zeroStory = QJsonDocument::fromJson(R"({
        "reply_to": {
            "@type": "messageReplyToStory",
            "story_poster_chat_id": 0,
            "story_id": 7
        }
    })")
                                      .object();
    const ReplyTarget zeroStoryTarget = replyTargetFromMessage(zeroStory);
    QCOMPARE(zeroStoryTarget.kind, ReplyTarget::Kind::Story);
    QVERIFY(!zeroStoryTarget.isValid());

    const QJsonObject zeroStoryId = QJsonDocument::fromJson(R"({
        "reply_to": {
            "@type": "messageReplyToStory",
            "story_poster_chat_id": 42,
            "story_id": 0
        }
    })")
                                        .object();
    QVERIFY(!replyTargetFromMessage(zeroStoryId).isValid());

    ReplyTarget empty;
    QVERIFY(!empty.isValid());
}

void ReplyInfoTest::buildsInputReplyToRequest()
{
    const QJsonObject request = inputReplyToMessageRequest(900);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputMessageReplyToMessage"));
    QCOMPARE(request.value(QStringLiteral("message_id")).toVariant().toLongLong(),
             qint64(900));
    QVERIFY(request.value(QStringLiteral("quote")).isNull());
    QCOMPARE(request.value(QStringLiteral("checklist_task_id")).toInt(), 0);
    QVERIFY(request.value(QStringLiteral("poll_option_id")).toString().isEmpty());

    const QJsonObject large = inputReplyToMessageRequest(1234567890123LL);
    QCOMPARE(large.value(QStringLiteral("message_id")).toVariant().toLongLong(),
             qint64(1234567890123LL));
}

QTEST_GUILESS_MAIN(ReplyInfoTest)
#include "tst_reply_info.moc"
