#include "telegram/MessageReactions.h"

#include <QJsonDocument>
#include <QtTest>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& request)
{
    return QJsonDocument::fromJson(QJsonDocument(request).toJson(QJsonDocument::Compact)).object();
}

} // namespace

class MessageReactionsTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesEmojiCustomAndPaidReactions();
    void parsesNestedInteractionInfo();
    void parsesBareUpdateList();
    void handlesEmptyAndMalformedReactions();
    void buildsEmojiReactionRequest();
    void buildsCustomEmojiReactionRequest();
    void buildsClearReactionRequest();
};

void MessageReactionsTest::parsesEmojiCustomAndPaidReactions()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "@type": "message",
        "id": 10,
        "chat_id": 42,
        "reactions": {
            "@type": "messageReactions",
            "reactions": [
                {
                    "@type": "messageReaction",
                    "type": { "@type": "reactionTypeEmoji", "emoji": "👍" },
                    "count": 3,
                    "is_chosen": true
                },
                {
                    "@type": "messageReaction",
                    "type": { "@type": "reactionTypeCustomEmoji", "custom_emoji_id": "5123456789012" },
                    "count": 2,
                    "is_chosen": false
                },
                {
                    "@type": "messageReaction",
                    "type": { "@type": "reactionTypePaid" },
                    "count": 10,
                    "is_chosen": false
                }
            ],
            "can_get_added_reactions": true
        }
    })")
                                    .object();

    const MessageReactions parsed = reactionsFromMessage(message);
    QVERIFY(parsed.canGetAddedReactions);
    QCOMPARE(parsed.reactions.size(), 3);

    const ReactionCount& emoji = parsed.reactions.at(0);
    QCOMPARE(emoji.type, QStringLiteral("emoji"));
    QCOMPARE(emoji.emoji, QStringLiteral("👍"));
    QCOMPARE(emoji.customEmojiId, qint64(0));
    QCOMPARE(emoji.count, 3);
    QVERIFY(emoji.chosen);

    const ReactionCount& custom = parsed.reactions.at(1);
    QCOMPARE(custom.type, QStringLiteral("custom_emoji"));
    QVERIFY(custom.emoji.isEmpty());
    QCOMPARE(custom.customEmojiId, qint64(5123456789012LL));
    QCOMPARE(custom.count, 2);
    QVERIFY(!custom.chosen);

    const ReactionCount& paid = parsed.reactions.at(2);
    QCOMPARE(paid.type, QStringLiteral("paid"));
    QVERIFY(paid.emoji.isEmpty());
    QCOMPARE(paid.customEmojiId, qint64(0));
    QCOMPARE(paid.count, 10);
    QVERIFY(!paid.chosen);
}

void MessageReactionsTest::parsesNestedInteractionInfo()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "@type": "message",
        "id": 11,
        "chat_id": 42,
        "interaction_info": {
            "@type": "messageInteractionInfo",
            "view_count": 5,
            "forward_count": 0,
            "reply_info": null,
            "reactions": {
                "@type": "messageReactions",
                "reactions": [
                    {
                        "@type": "messageReaction",
                        "type": { "@type": "reactionTypeEmoji", "emoji": "🔥" },
                        "total_count": 7,
                        "is_chosen": true
                    }
                ],
                "can_get_added_reactions": false
            }
        }
    })")
                                    .object();

    const MessageReactions parsed = reactionsFromMessage(message);
    QVERIFY(!parsed.canGetAddedReactions);
    QCOMPARE(parsed.reactions.size(), 1);
    QCOMPARE(parsed.reactions.first().type, QStringLiteral("emoji"));
    QCOMPARE(parsed.reactions.first().emoji, QStringLiteral("🔥"));
    QCOMPARE(parsed.reactions.first().count, 7);
    QVERIFY(parsed.reactions.first().chosen);
}

void MessageReactionsTest::parsesBareUpdateList()
{
    const QJsonObject update = QJsonDocument::fromJson(R"({
        "@type": "updateMessageReactions",
        "chat_id": 42,
        "message_id": 12,
        "date": 1700000000,
        "reactions": [
            {
                "@type": "messageReaction",
                "type": { "@type": "reactionTypeCustomEmoji", "custom_emoji_id": 777 },
                "total_count": 1,
                "is_chosen": false
            }
        ]
    })")
                                   .object();

    const MessageReactions parsed = reactionsFromMessage(update);
    QVERIFY(!parsed.canGetAddedReactions);
    QCOMPARE(parsed.reactions.size(), 1);
    QCOMPARE(parsed.reactions.first().type, QStringLiteral("custom_emoji"));
    QCOMPARE(parsed.reactions.first().customEmojiId, qint64(777));
    QCOMPARE(parsed.reactions.first().count, 1);
}

void MessageReactionsTest::handlesEmptyAndMalformedReactions()
{
    const MessageReactions missing = reactionsFromMessage(QJsonObject{});
    QVERIFY(missing.reactions.isEmpty());
    QVERIFY(!missing.canGetAddedReactions);

    const QJsonObject nullReactions = QJsonDocument::fromJson(R"({
        "id": 13,
        "reactions": null
    })")
                                          .object();
    const MessageReactions nullParsed = reactionsFromMessage(nullReactions);
    QVERIFY(nullParsed.reactions.isEmpty());
    QVERIFY(!nullParsed.canGetAddedReactions);

    const QJsonObject malformed = QJsonDocument::fromJson(R"({
        "id": 14,
        "reactions": {
            "reactions": [
                null,
                5,
                { "type": 7, "count": 2 },
                { "type": { "@type": "reactionTypeFuture" }, "count": 2 },
                { "type": { "@type": "reactionTypeEmoji", "emoji": "❤" }, "count": "2" },
                { "type": { "@type": "reactionTypeEmoji", "emoji": "🔥" }, "count": -1 },
                { "type": { "@type": "reactionTypeEmoji", "emoji": "🎉" }, "count": 0, "is_chosen": true }
            ],
            "can_get_added_reactions": true
        }
    })")
                                       .object();

    const MessageReactions parsed = reactionsFromMessage(malformed);
    QVERIFY(parsed.canGetAddedReactions);
    QCOMPARE(parsed.reactions.size(), 1);
    QCOMPARE(parsed.reactions.first().emoji, QStringLiteral("🎉"));
    QCOMPARE(parsed.reactions.first().count, 0);
    QVERIFY(parsed.reactions.first().chosen);

    const QJsonObject wrongList = QJsonDocument::fromJson(R"({
        "id": 15,
        "reactions": { "reactions": "not-an-array" }
    })")
                                      .object();
    QVERIFY(reactionsFromMessage(wrongList).reactions.isEmpty());
}

void MessageReactionsTest::buildsEmojiReactionRequest()
{
    const QJsonObject request = serialized(
        sendReactionRequest(qint64(42), qint64(900), QStringLiteral("👍")));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("addMessageReaction"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(42));
    QCOMPARE(request.value(QStringLiteral("message_id")).toVariant().toLongLong(), qint64(900));
    QVERIFY(!request.value(QStringLiteral("is_big")).toBool());
    QVERIFY(request.value(QStringLiteral("update_recent_reactions")).toBool());

    const QJsonObject reactionType = request.value(QStringLiteral("reaction_type")).toObject();
    QCOMPARE(reactionType.value(QStringLiteral("@type")).toString(),
             QStringLiteral("reactionTypeEmoji"));
    QCOMPARE(reactionType.value(QStringLiteral("emoji")).toString(), QStringLiteral("👍"));
    QVERIFY(!reactionType.contains(QStringLiteral("custom_emoji_id")));
}

void MessageReactionsTest::buildsCustomEmojiReactionRequest()
{
    const QJsonObject request = serialized(
        sendReactionRequest(qint64(42), qint64(901), qint64(5123456789012LL), true));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("addMessageReaction"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(42));
    QCOMPARE(request.value(QStringLiteral("message_id")).toVariant().toLongLong(), qint64(901));
    QVERIFY(request.value(QStringLiteral("is_big")).toBool());
    QVERIFY(request.value(QStringLiteral("update_recent_reactions")).toBool());

    const QJsonObject reactionType = request.value(QStringLiteral("reaction_type")).toObject();
    QCOMPARE(reactionType.value(QStringLiteral("@type")).toString(),
             QStringLiteral("reactionTypeCustomEmoji"));
    QCOMPARE(reactionType.value(QStringLiteral("custom_emoji_id")).toVariant().toLongLong(),
             qint64(5123456789012LL));
    QVERIFY(!reactionType.contains(QStringLiteral("emoji")));
}

void MessageReactionsTest::buildsClearReactionRequest()
{
    const QJsonObject request = serialized(
        clearReactionRequest(qint64(42), qint64(902)));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("removeMessageReaction"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(42));
    QCOMPARE(request.value(QStringLiteral("message_id")).toVariant().toLongLong(), qint64(902));
    QVERIFY(request.value(QStringLiteral("reaction_type")).isNull());
}

QTEST_GUILESS_MAIN(MessageReactionsTest)
#include "tst_message_reactions.moc"
