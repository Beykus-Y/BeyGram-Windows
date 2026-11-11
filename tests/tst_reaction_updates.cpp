#include "telegram/ReactionUpdates.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace beygram::telegram;

class ReactionUpdatesTest final : public QObject {
    Q_OBJECT

private slots:
    void recognisesReactionUpdate();
    void parsesUpdateShape();
    void parsesChosenAndCounts();
    void acceptsNumericAndStringIds();
    void handlesEmptyAndUnknownUpdates();
    void toleratesMissingOrMalformedReactions();
};

void ReactionUpdatesTest::recognisesReactionUpdate()
{
    const QJsonObject update = QJsonDocument::fromJson(R"({
        "@type": "updateMessageReactions",
        "chat_id": 42,
        "message_id": 7,
        "reactions": []
    })")
                                   .object();
    QVERIFY(isReactionUpdate(update));

    const QJsonObject other = QJsonDocument::fromJson(R"({
        "@type": "updateMessageEdited",
        "chat_id": 42,
        "message_id": 7
    })")
                                  .object();
    QVERIFY(!isReactionUpdate(other));

    QVERIFY(!isReactionUpdate(QJsonObject{}));

    const QJsonObject numericType = QJsonDocument::fromJson(R"({ "@type": 5 })").object();
    QVERIFY(!isReactionUpdate(numericType));
}

void ReactionUpdatesTest::parsesUpdateShape()
{
    const QJsonObject update = QJsonDocument::fromJson(R"({
        "@type": "updateMessageReactions",
        "chat_id": 4242424242,
        "message_id": 900,
        "date": 1700000000,
        "reactions": [
            {
                "@type": "messageReaction",
                "type": { "@type": "reactionTypeEmoji", "emoji": "👍" },
                "total_count": 3,
                "is_chosen": true
            },
            {
                "@type": "messageReaction",
                "type": { "@type": "reactionTypeCustomEmoji", "custom_emoji_id": 5123456789012 },
                "total_count": 1,
                "is_chosen": false
            }
        ]
    })")
                                   .object();

    const ReactionUpdate parsed = reactionUpdateFromUpdate(update);
    QCOMPARE(parsed.chatId, qint64(4242424242LL));
    QCOMPARE(parsed.messageId, qint64(900));
    QCOMPARE(parsed.reactions.reactions.size(), 2);

    // TDLib's updateMessageReactions has no can_get_added_reactions field.
    QVERIFY(!parsed.reactions.canGetAddedReactions);

    QCOMPARE(parsed.reactions.reactions.at(0).type, QStringLiteral("emoji"));
    QCOMPARE(parsed.reactions.reactions.at(0).emoji, QStringLiteral("👍"));
    QCOMPARE(parsed.reactions.reactions.at(1).type, QStringLiteral("custom_emoji"));
    QCOMPARE(parsed.reactions.reactions.at(1).customEmojiId, qint64(5123456789012LL));
}

void ReactionUpdatesTest::parsesChosenAndCounts()
{
    const QJsonObject update = QJsonDocument::fromJson(R"({
        "@type": "updateMessageReactions",
        "chat_id": 1,
        "message_id": 2,
        "reactions": [
            {
                "@type": "messageReaction",
                "type": { "@type": "reactionTypeEmoji", "emoji": "🔥" },
                "count": 4,
                "is_chosen": false
            },
            {
                "@type": "messageReaction",
                "type": { "@type": "reactionTypeEmoji", "emoji": "🎉" },
                "total_count": 9,
                "is_chosen": true
            },
            {
                "@type": "messageReaction",
                "type": { "@type": "reactionTypePaid" },
                "total_count": 0,
                "is_chosen": false
            }
        ]
    })")
                                   .object();

    const ReactionUpdate parsed = reactionUpdateFromUpdate(update);
    QCOMPARE(parsed.reactions.reactions.size(), 3);

    const ReactionCount& legacyCount = parsed.reactions.reactions.at(0);
    QCOMPARE(legacyCount.emoji, QStringLiteral("🔥"));
    QCOMPARE(legacyCount.count, 4);
    QVERIFY(!legacyCount.chosen);

    const ReactionCount& chosen = parsed.reactions.reactions.at(1);
    QCOMPARE(chosen.emoji, QStringLiteral("🎉"));
    QCOMPARE(chosen.count, 9);
    QVERIFY(chosen.chosen);

    const ReactionCount& paid = parsed.reactions.reactions.at(2);
    QCOMPARE(paid.type, QStringLiteral("paid"));
    QCOMPARE(paid.count, 0);
    QVERIFY(!paid.chosen);
}

void ReactionUpdatesTest::acceptsNumericAndStringIds()
{
    // TDLib emits int64 ids as numbers, but transports that stringify large
    // values must round-trip too.
    const QJsonObject numeric = QJsonDocument::fromJson(R"({
        "@type": "updateMessageReactions",
        "chat_id": 1000000000001,
        "message_id": 55
    })")
                                    .object();
    const ReactionUpdate numericParsed = reactionUpdateFromUpdate(numeric);
    QCOMPARE(numericParsed.chatId, qint64(1000000000001LL));
    QCOMPARE(numericParsed.messageId, qint64(55));

    const QJsonObject stringy = QJsonDocument::fromJson(R"({
        "@type": "updateMessageReactions",
        "chat_id": "1000000000001",
        "message_id": "55"
    })")
                                    .object();
    const ReactionUpdate stringyParsed = reactionUpdateFromUpdate(stringy);
    QCOMPARE(stringyParsed.chatId, qint64(1000000000001LL));
    QCOMPARE(stringyParsed.messageId, qint64(55));

    const QJsonObject mixed = QJsonDocument::fromJson(R"({
        "@type": "updateMessageReactions",
        "chat_id": "-1001234567890",
        "message_id": 77
    })")
                                  .object();
    const ReactionUpdate mixedParsed = reactionUpdateFromUpdate(mixed);
    QCOMPARE(mixedParsed.chatId, qint64(-1001234567890LL));
    QCOMPARE(mixedParsed.messageId, qint64(77));

    // Ids that are not numeric strings yield 0 rather than garbage.
    const QJsonObject garbage = QJsonDocument::fromJson(R"({
        "@type": "updateMessageReactions",
        "chat_id": "not-a-number",
        "message_id": true
    })")
                                    .object();
    const ReactionUpdate garbageParsed = reactionUpdateFromUpdate(garbage);
    QCOMPARE(garbageParsed.chatId, qint64(0));
    QCOMPARE(garbageParsed.messageId, qint64(0));
}

void ReactionUpdatesTest::handlesEmptyAndUnknownUpdates()
{
    const ReactionUpdate empty = reactionUpdateFromUpdate(QJsonObject{});
    QCOMPARE(empty.chatId, qint64(0));
    QCOMPARE(empty.messageId, qint64(0));
    QVERIFY(empty.reactions.reactions.isEmpty());
    QVERIFY(!empty.reactions.canGetAddedReactions);

    // Parsing is not gated on the type: callers decide with isReactionUpdate.
    const QJsonObject unknown = QJsonDocument::fromJson(R"({
        "@type": "updateSomethingElse",
        "chat_id": 3,
        "message_id": 4,
        "reactions": [
            {
                "@type": "messageReaction",
                "type": { "@type": "reactionTypeEmoji", "emoji": "❤" },
                "total_count": 2,
                "is_chosen": true
            }
        ]
    })")
                                    .object();
    QVERIFY(!isReactionUpdate(unknown));
    const ReactionUpdate unknownParsed = reactionUpdateFromUpdate(unknown);
    QCOMPARE(unknownParsed.chatId, qint64(3));
    QCOMPARE(unknownParsed.messageId, qint64(4));
    QCOMPARE(unknownParsed.reactions.reactions.size(), 1);
    QCOMPARE(unknownParsed.reactions.reactions.first().emoji, QStringLiteral("❤"));
}

void ReactionUpdatesTest::toleratesMissingOrMalformedReactions()
{
    // A reaction removal arrives as an empty list.
    const QJsonObject cleared = QJsonDocument::fromJson(R"({
        "@type": "updateMessageReactions",
        "chat_id": 42,
        "message_id": 7,
        "reactions": []
    })")
                                    .object();
    const ReactionUpdate clearedParsed = reactionUpdateFromUpdate(cleared);
    QCOMPARE(clearedParsed.chatId, qint64(42));
    QCOMPARE(clearedParsed.messageId, qint64(7));
    QVERIFY(clearedParsed.reactions.reactions.isEmpty());

    // Missing reactions entirely.
    const QJsonObject missing = QJsonDocument::fromJson(R"({
        "@type": "updateMessageReactions",
        "chat_id": 42,
        "message_id": 8
    })")
                                    .object();
    QVERIFY(reactionUpdateFromUpdate(missing).reactions.reactions.isEmpty());

    // Null and non-array lists.
    const QJsonObject nullList = QJsonDocument::fromJson(R"({
        "@type": "updateMessageReactions",
        "chat_id": 42,
        "message_id": 9,
        "reactions": null
    })")
                                     .object();
    QVERIFY(reactionUpdateFromUpdate(nullList).reactions.reactions.isEmpty());

    const QJsonObject wrongType = QJsonDocument::fromJson(R"({
        "@type": "updateMessageReactions",
        "chat_id": 42,
        "message_id": 10,
        "reactions": "not-a-list"
    })")
                                      .object();
    QVERIFY(reactionUpdateFromUpdate(wrongType).reactions.reactions.isEmpty());

    // Malformed entries are skipped, valid neighbours survive.
    const QJsonObject mixed = QJsonDocument::fromJson(R"({
        "@type": "updateMessageReactions",
        "chat_id": 42,
        "message_id": 11,
        "reactions": [
            null,
            5,
            { "type": 7, "count": 2 },
            { "type": { "@type": "reactionTypeFuture" }, "count": 2 },
            { "type": { "@type": "reactionTypeEmoji", "emoji": "❤" }, "count": "2" },
            { "type": { "@type": "reactionTypeEmoji", "emoji": "🔥" }, "count": -1 },
            {
                "@type": "messageReaction",
                "type": { "@type": "reactionTypeEmoji", "emoji": "🎉" },
                "total_count": 6,
                "is_chosen": true
            }
        ]
    })")
                                    .object();
    const ReactionUpdate mixedParsed = reactionUpdateFromUpdate(mixed);
    QCOMPARE(mixedParsed.reactions.reactions.size(), 1);
    QCOMPARE(mixedParsed.reactions.reactions.first().emoji, QStringLiteral("🎉"));
    QCOMPARE(mixedParsed.reactions.reactions.first().count, 6);
    QVERIFY(mixedParsed.reactions.reactions.first().chosen);
}

QTEST_GUILESS_MAIN(ReactionUpdatesTest)
#include "tst_reaction_updates.moc"
