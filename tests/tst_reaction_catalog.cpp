#include "telegram/ReactionCatalog.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPair>
#include <QString>
#include <QVector>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

QJsonObject responseFromJson(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

QJsonObject emojiType(const char* emoji)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("reactionTypeEmoji")},
        {QStringLiteral("emoji"), QString::fromUtf8(emoji)},
    };
}

} // namespace

class ReactionCatalogTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsAvailableReactionsRequest();
    void buildsRecentReactionsRequest();
    void clampsRecentReactionsLimit();
    void buildsClearRecentReactionsRequest();
    void parsesAvailableReactions();
    void parsesLegacyAndTelegramShapes();
    void parsesRecentReactionsAnswer();
    void parsesTelegramApiReactions();
    void parsesOldCatalogReactionsList();
    void parsesChatAvailableReactions();
    void ignoresMalformedEntries();
    void readsStringEncodedNumbers();
    void clampsMaxReactionCount();
    void preservesOrderAndDuplicates();
    void ignoresPopularReactions();
    void prefersDirectCatalogOverNested();
};

void ReactionCatalogTest::buildsAvailableReactionsRequest()
{
    const QJsonObject request = getAvailableReactionsRequest();
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getAvailableReactions"));
    QCOMPARE(request.size(), 1);

    const QJsonObject encoded = serialized(request);
    QCOMPARE(encoded.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getAvailableReactions"));
    QCOMPARE(encoded.size(), 1);
}

void ReactionCatalogTest::buildsRecentReactionsRequest()
{
    const QJsonObject request = getRecentReactionsRequest();
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getRecentReactions"));
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 50);
    QCOMPARE(request.size(), 2);

    const QJsonObject custom = getRecentReactionsRequest(25);
    QCOMPARE(custom.value(QStringLiteral("limit")).toInt(), 25);

    const QJsonObject encoded = serialized(custom);
    QCOMPARE(encoded.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getRecentReactions"));
    QCOMPARE(encoded.value(QStringLiteral("limit")).toInt(), 25);
}

void ReactionCatalogTest::clampsRecentReactionsLimit()
{
    const QVector<QPair<int, int>> cases{
        {0, 1},
        {-7, 1},
        {1, 1},
        {25, 25},
        {50, 50},
        {100, 100},
        {101, 100},
        {5000, 100},
    };

    for (const auto& testCase : cases) {
        const QJsonObject request = getRecentReactionsRequest(testCase.first);
        QCOMPARE(request.value(QStringLiteral("limit")).toInt(),
                 testCase.second);
    }
}

void ReactionCatalogTest::buildsClearRecentReactionsRequest()
{
    const QJsonObject request = clearRecentReactionsRequest();
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("clearRecentReactions"));
    QCOMPARE(request.size(), 1);

    const QJsonObject encoded = serialized(request);
    QCOMPARE(encoded.value(QStringLiteral("@type")).toString(),
             QStringLiteral("clearRecentReactions"));
    QCOMPARE(encoded.size(), 1);
}

void ReactionCatalogTest::parsesAvailableReactions()
{
    const ReactionCatalogState state = reactionCatalogFromResponse(responseFromJson(R"({
        "@type": "availableReactions",
        "top_reactions": [
            {
                "@type": "availableReaction",
                "type": { "@type": "reactionTypeEmoji", "emoji": "👍" },
                "title": "Like",
                "is_active": true,
                "is_premium": false
            },
            {
                "@type": "availableReaction",
                "type": {
                    "@type": "reactionTypeCustomEmoji",
                    "custom_emoji_id": 5123456789012
                },
                "title": "Party",
                "is_active": false,
                "is_premium": true
            }
        ],
        "recent_reactions": [
            {
                "@type": "availableReaction",
                "type": { "@type": "reactionTypeEmoji", "emoji": "🔥" },
                "needs_premium": true
            }
        ],
        "popular_reactions": [
            {
                "@type": "availableReaction",
                "type": { "@type": "reactionTypeEmoji", "emoji": "🎉" }
            }
        ],
        "allow_custom_emoji": true,
        "are_tags": false,
        "unavailability_reason": null,
        "max_reaction_count": 5
    })"));

    QCOMPARE(state.topReactions.size(), 2);
    QCOMPARE(state.recentReactions.size(), 1);
    QCOMPARE(state.maxReactionCount, 5);

    const AvailableReaction& like = state.topReactions.at(0);
    QCOMPARE(like.type, QStringLiteral("emoji"));
    QCOMPARE(like.emoji, QStringLiteral("👍"));
    QCOMPARE(like.customEmojiId, qint64(0));
    QCOMPARE(like.title, QStringLiteral("Like"));
    QVERIFY(like.isActive);
    QVERIFY(!like.isPremium);

    const AvailableReaction& party = state.topReactions.at(1);
    QCOMPARE(party.type, QStringLiteral("custom_emoji"));
    QVERIFY(party.emoji.isEmpty());
    QCOMPARE(party.customEmojiId, qint64(5123456789012LL));
    QCOMPARE(party.title, QStringLiteral("Party"));
    QVERIFY(!party.isActive);
    QVERIFY(party.isPremium);

    const AvailableReaction& fire = state.recentReactions.at(0);
    QCOMPARE(fire.type, QStringLiteral("emoji"));
    QCOMPARE(fire.emoji, QStringLiteral("🔥"));
    QVERIFY(fire.title.isEmpty());
    QVERIFY(fire.isActive);
    QVERIFY(fire.isPremium);
}

void ReactionCatalogTest::parsesLegacyAndTelegramShapes()
{
    const ReactionCatalogState state = reactionCatalogFromResponse(responseFromJson(R"({
        "top_reactions": [
            {
                "reaction": "👍",
                "title": "Like",
                "is_active": false,
                "is_premium": true
            },
            {
                "reaction": {
                    "@type": "reactionTypeCustomEmoji",
                    "custom_emoji_id": "777"
                },
                "title": "Custom"
            },
            { "@type": "reactionTypePaid" },
            {
                "type": "reactionTypeEmoji",
                "emoji": "🎉",
                "inactive": true,
                "premium": true
            },
            { "emoji": "❤" },
            { "custom_emoji_id": 42 },
            {
                "reaction": "😀",
                "is_premium": false,
                "needs_premium": true
            }
        ],
        "recent_reactions": [
            "🔥",
            { "reaction": "❤" }
        ]
    })"));

    QCOMPARE(state.topReactions.size(), 7);
    QCOMPARE(state.recentReactions.size(), 2);

    const AvailableReaction& legacy = state.topReactions.at(0);
    QCOMPARE(legacy.type, QStringLiteral("emoji"));
    QCOMPARE(legacy.emoji, QStringLiteral("👍"));
    QCOMPARE(legacy.title, QStringLiteral("Like"));
    QVERIFY(!legacy.isActive);
    QVERIFY(legacy.isPremium);

    const AvailableReaction& custom = state.topReactions.at(1);
    QCOMPARE(custom.type, QStringLiteral("custom_emoji"));
    QCOMPARE(custom.customEmojiId, qint64(777));
    QCOMPARE(custom.title, QStringLiteral("Custom"));
    QVERIFY(custom.isActive);
    QVERIFY(!custom.isPremium);

    const AvailableReaction& paid = state.topReactions.at(2);
    QCOMPARE(paid.type, QStringLiteral("paid"));
    QVERIFY(paid.emoji.isEmpty());
    QCOMPARE(paid.customEmojiId, qint64(0));
    QVERIFY(paid.isActive);
    QVERIFY(!paid.isPremium);

    // A short type string and the Telegram API inactive/premium flags.
    const AvailableReaction& telegram = state.topReactions.at(3);
    QCOMPARE(telegram.type, QStringLiteral("emoji"));
    QCOMPARE(telegram.emoji, QStringLiteral("🎉"));
    QVERIFY(!telegram.isActive);
    QVERIFY(telegram.isPremium);

    // Flat objects fall back to emoji/custom_emoji_id.
    QCOMPARE(state.topReactions.at(4).type, QStringLiteral("emoji"));
    QCOMPARE(state.topReactions.at(4).emoji, QStringLiteral("❤"));
    QCOMPARE(state.topReactions.at(5).type, QStringLiteral("custom_emoji"));
    QCOMPARE(state.topReactions.at(5).customEmojiId, qint64(42));

    // An explicit is_premium wins over needs_premium.
    QVERIFY(!state.topReactions.at(6).isPremium);

    QCOMPARE(state.recentReactions.at(0).emoji, QStringLiteral("🔥"));
    QCOMPARE(state.recentReactions.at(1).emoji, QStringLiteral("❤"));
}

void ReactionCatalogTest::parsesRecentReactionsAnswer()
{
    const ReactionCatalogState state = reactionCatalogFromResponse(responseFromJson(R"({
        "@type": "reactions",
        "reactions": [
            { "@type": "reactionTypeEmoji", "emoji": "🔥" },
            {
                "@type": "reactionTypeCustomEmoji",
                "custom_emoji_id": 42
            },
            { "@type": "reactionTypePaid" }
        ]
    })"));

    QVERIFY(state.topReactions.isEmpty());
    QCOMPARE(state.recentReactions.size(), 3);

    QCOMPARE(state.recentReactions.at(0).type, QStringLiteral("emoji"));
    QCOMPARE(state.recentReactions.at(0).emoji, QStringLiteral("🔥"));
    QCOMPARE(state.recentReactions.at(1).type,
             QStringLiteral("custom_emoji"));
    QCOMPARE(state.recentReactions.at(1).customEmojiId, qint64(42));
    QCOMPARE(state.recentReactions.at(2).type, QStringLiteral("paid"));
}

void ReactionCatalogTest::parsesTelegramApiReactions()
{
    const ReactionCatalogState state = reactionCatalogFromResponse(responseFromJson(R"({
        "@type": "messages.reactions",
        "reactions": [
            { "@type": "reactionEmoji", "emoticon": "👍" },
            { "@type": "reactionCustomEmoji", "document_id": 123 }
        ]
    })"));

    QVERIFY(state.topReactions.isEmpty());
    QCOMPARE(state.recentReactions.size(), 2);
    QCOMPARE(state.recentReactions.at(0).type, QStringLiteral("emoji"));
    QCOMPARE(state.recentReactions.at(0).emoji, QStringLiteral("👍"));
    QCOMPARE(state.recentReactions.at(1).type,
             QStringLiteral("custom_emoji"));
    QCOMPARE(state.recentReactions.at(1).customEmojiId, qint64(123));

    // The same reaction shape inside an availableReaction.reaction field.
    const ReactionCatalogState catalog = reactionCatalogFromResponse(responseFromJson(R"({
        "top_reactions": [
            { "reaction": { "@type": "reactionEmoji", "emoticon": "🔥" } }
        ]
    })"));
    QCOMPARE(catalog.topReactions.size(), 1);
    QCOMPARE(catalog.topReactions.at(0).emoji, QStringLiteral("🔥"));
}

void ReactionCatalogTest::parsesOldCatalogReactionsList()
{
    const ReactionCatalogState state = reactionCatalogFromResponse(responseFromJson(R"({
        "@type": "availableReactions",
        "reactions": [
            {
                "@type": "availableReaction",
                "reaction": "👍",
                "title": "Like"
            },
            "🔥",
            {
                "type": {
                    "@type": "reactionTypeCustomEmoji",
                    "custom_emoji_id": 9
                },
                "title": "Custom"
            }
        ]
    })"));

    QCOMPARE(state.topReactions.size(), 3);
    QVERIFY(state.recentReactions.isEmpty());

    QCOMPARE(state.topReactions.at(0).emoji, QStringLiteral("👍"));
    QCOMPARE(state.topReactions.at(0).title, QStringLiteral("Like"));
    QCOMPARE(state.topReactions.at(1).emoji, QStringLiteral("🔥"));
    QCOMPARE(state.topReactions.at(2).type,
             QStringLiteral("custom_emoji"));
    QCOMPARE(state.topReactions.at(2).customEmojiId, qint64(9));

    // A later availableReactions revision lists bare ReactionType objects;
    // they are still the catalog's top list, not recent reactions.
    const ReactionCatalogState types = reactionCatalogFromResponse(responseFromJson(R"({
        "@type": "availableReactions",
        "reactions": [
            { "@type": "reactionTypeEmoji", "emoji": "👍" },
            { "@type": "reactionTypeCustomEmoji", "custom_emoji_id": 8 }
        ]
    })"));
    QCOMPARE(types.topReactions.size(), 2);
    QVERIFY(types.recentReactions.isEmpty());
    QCOMPARE(types.topReactions.at(0).emoji, QStringLiteral("👍"));
    QCOMPARE(types.topReactions.at(1).customEmojiId, qint64(8));
}

void ReactionCatalogTest::parsesChatAvailableReactions()
{
    const ReactionCatalogState some = reactionCatalogFromResponse(responseFromJson(R"({
        "@type": "chatAvailableReactionsSome",
        "reactions": [
            { "@type": "reactionTypeEmoji", "emoji": "👍" }
        ],
        "max_reaction_count": 3
    })"));
    QCOMPARE(some.topReactions.size(), 1);
    QVERIFY(some.recentReactions.isEmpty());
    QCOMPARE(some.maxReactionCount, 3);

    const ReactionCatalogState all = reactionCatalogFromResponse(responseFromJson(R"({
        "@type": "chatAvailableReactionsAll",
        "max_reaction_count": 11
    })"));
    QVERIFY(all.topReactions.isEmpty());
    QVERIFY(all.recentReactions.isEmpty());
    QCOMPARE(all.maxReactionCount, 11);

    // A chat nests the object under available_reactions.
    const ReactionCatalogState nested = reactionCatalogFromResponse(responseFromJson(R"({
        "@type": "chat",
        "available_reactions": {
            "@type": "chatAvailableReactionsSome",
            "reactions": [ "👍", "🔥" ],
            "max_reaction_count": "2"
        }
    })"));
    QCOMPARE(nested.topReactions.size(), 2);
    QVERIFY(nested.recentReactions.isEmpty());
    QCOMPARE(nested.maxReactionCount, 2);
    QCOMPARE(nested.topReactions.at(0).emoji, QStringLiteral("👍"));
    QCOMPARE(nested.topReactions.at(1).emoji, QStringLiteral("🔥"));
}

void ReactionCatalogTest::ignoresMalformedEntries()
{
    const ReactionCatalogState empty =
        reactionCatalogFromResponse(QJsonObject{});
    QVERIFY(empty.topReactions.isEmpty());
    QVERIFY(empty.recentReactions.isEmpty());
    QCOMPARE(empty.maxReactionCount, 0);

    QVERIFY(reactionCatalogFromResponse(
                responseFromJson(R"({ "top_reactions": null })"))
                .topReactions.isEmpty());
    QVERIFY(reactionCatalogFromResponse(
                responseFromJson(R"({ "top_reactions": "emoji" })"))
                .topReactions.isEmpty());
    QVERIFY(reactionCatalogFromResponse(
                responseFromJson(R"({ "recent_reactions": {} })"))
                .recentReactions.isEmpty());
    QVERIFY(reactionCatalogFromResponse(
                responseFromJson(R"({ "reactions": [] })"))
                .recentReactions.isEmpty());

    const ReactionCatalogState malformed = reactionCatalogFromResponse(responseFromJson(R"({
        "top_reactions": [
            null,
            5,
            "",
            {},
            { "@type": "reactionTypeFuture" },
            { "type": { "@type": "reactionTypeEmoji", "emoji": "" } },
            {
                "type": {
                    "@type": "reactionTypeCustomEmoji",
                    "custom_emoji_id": 0
                }
            },
            { "type": { "@type": "reactionTypeEmoji", "emoji": "👍" } }
        ],
        "recent_reactions": null
    })"));

    QCOMPARE(malformed.topReactions.size(), 1);
    QCOMPARE(malformed.topReactions.at(0).emoji, QStringLiteral("👍"));
    QVERIFY(malformed.recentReactions.isEmpty());

    // An unknown type does not hide a valid legacy reaction.
    const ReactionCatalogState fallback = reactionCatalogFromResponse(responseFromJson(R"({
        "top_reactions": [
            {
                "type": { "@type": "reactionTypeFuture" },
                "reaction": "🔥"
            }
        ]
    })"));
    QCOMPARE(fallback.topReactions.size(), 1);
    QCOMPARE(fallback.topReactions.at(0).emoji, QStringLiteral("🔥"));
}

void ReactionCatalogTest::readsStringEncodedNumbers()
{
    const ReactionCatalogState state = reactionCatalogFromResponse(responseFromJson(R"({
        "top_reactions": [
            {
                "type": {
                    "@type": "reactionTypeCustomEmoji",
                    "custom_emoji_id": "9007199254740993"
                }
            }
        ],
        "max_reaction_count": "7"
    })"));

    QCOMPARE(state.topReactions.size(), 1);
    QCOMPARE(state.topReactions.at(0).customEmojiId,
             qint64(9007199254740993LL));
    QCOMPARE(state.maxReactionCount, 7);

    const QJsonObject encoded = serialized(responseFromJson(R"({
        "top_reactions": [
            {
                "type": {
                    "@type": "reactionTypeCustomEmoji",
                    "custom_emoji_id": 9007199254740994
                }
            }
        ]
    })"));
    QCOMPARE(reactionCatalogFromResponse(encoded)
                 .topReactions.at(0)
                 .customEmojiId,
             qint64(9007199254740994LL));
}

void ReactionCatalogTest::clampsMaxReactionCount()
{
    QCOMPARE(reactionCatalogFromResponse(
                 responseFromJson(R"({ "max_reaction_count": 11 })"))
                 .maxReactionCount,
             11);
    QCOMPARE(reactionCatalogFromResponse(
                 responseFromJson(R"({ "max_reaction_count": -5 })"))
                 .maxReactionCount,
             0);
    QCOMPARE(reactionCatalogFromResponse(
                 responseFromJson(R"({ "max_reaction_count": 0 })"))
                 .maxReactionCount,
             0);
    QCOMPARE(reactionCatalogFromResponse(
                 responseFromJson(R"({ "max_reaction_count": "not-a-number" })"))
                 .maxReactionCount,
             0);
    QCOMPARE(reactionCatalogFromResponse(
                 responseFromJson(R"({ "max_reaction_count": 2147483647 })"))
                 .maxReactionCount,
             std::numeric_limits<int>::max());
    QCOMPARE(reactionCatalogFromResponse(
                 responseFromJson(R"({ "max_reaction_count": 2147483648 })"))
                 .maxReactionCount,
             std::numeric_limits<int>::max());
}

void ReactionCatalogTest::preservesOrderAndDuplicates()
{
    const QJsonObject topA{
        {QStringLiteral("@type"), QStringLiteral("availableReaction")},
        {QStringLiteral("type"), emojiType("👍")},
    };
    const QJsonObject topB{
        {QStringLiteral("@type"), QStringLiteral("availableReaction")},
        {QStringLiteral("type"), emojiType("👍")},
    };
    const QJsonObject topC{
        {QStringLiteral("@type"), QStringLiteral("availableReaction")},
        {QStringLiteral("type"), emojiType("🔥")},
    };
    const QJsonObject recentA{
        {QStringLiteral("@type"), QStringLiteral("availableReaction")},
        {QStringLiteral("type"), emojiType("🔥")},
    };
    const QJsonObject recentB{
        {QStringLiteral("@type"), QStringLiteral("availableReaction")},
        {QStringLiteral("type"), emojiType("👍")},
    };

    const QJsonObject response{
        {QStringLiteral("top_reactions"), QJsonArray{topA, topB, topC}},
        {QStringLiteral("recent_reactions"), QJsonArray{recentA, recentB}},
    };

    const ReactionCatalogState state = reactionCatalogFromResponse(response);
    QCOMPARE(state.topReactions.size(), 3);
    QCOMPARE(state.topReactions.at(0).emoji, QStringLiteral("👍"));
    QCOMPARE(state.topReactions.at(1).emoji, QStringLiteral("👍"));
    QCOMPARE(state.topReactions.at(2).emoji, QStringLiteral("🔥"));

    QCOMPARE(state.recentReactions.size(), 2);
    QCOMPARE(state.recentReactions.at(0).emoji, QStringLiteral("🔥"));
    QCOMPARE(state.recentReactions.at(1).emoji, QStringLiteral("👍"));
}

void ReactionCatalogTest::ignoresPopularReactions()
{
    const ReactionCatalogState state = reactionCatalogFromResponse(responseFromJson(R"({
        "top_reactions": [
            { "type": { "@type": "reactionTypeEmoji", "emoji": "👍" } }
        ],
        "recent_reactions": [
            { "type": { "@type": "reactionTypeEmoji", "emoji": "🔥" } }
        ],
        "popular_reactions": [
            { "type": { "@type": "reactionTypeEmoji", "emoji": "🎉" } }
        ]
    })"));

    QCOMPARE(state.topReactions.size(), 1);
    QCOMPARE(state.recentReactions.size(), 1);
    QCOMPARE(state.topReactions.at(0).emoji, QStringLiteral("👍"));
    QCOMPARE(state.recentReactions.at(0).emoji, QStringLiteral("🔥"));
}

void ReactionCatalogTest::prefersDirectCatalogOverNested()
{
    const ReactionCatalogState state = reactionCatalogFromResponse(responseFromJson(R"({
        "top_reactions": [
            { "type": { "@type": "reactionTypeEmoji", "emoji": "👍" } }
        ],
        "available_reactions": {
            "@type": "chatAvailableReactionsSome",
            "reactions": [ "🔥" ],
            "max_reaction_count": 3
        }
    })"));

    QCOMPARE(state.topReactions.size(), 1);
    QCOMPARE(state.topReactions.at(0).emoji, QStringLiteral("👍"));
    QVERIFY(state.recentReactions.isEmpty());
    QCOMPARE(state.maxReactionCount, 0);
}

QTEST_GUILESS_MAIN(ReactionCatalogTest)
#include "tst_reaction_catalog.moc"
