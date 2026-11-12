#include "telegram/MessageSpecial.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject contentFromJson(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

// The source stays pure ASCII; the bytes are the UTF-8 form of U+1F3B2.
QString diceEmoji()
{
    return QString::fromUtf8("\xF0\x9F\x8E\xB2");
}

// U+1F600.
QString smileEmoji()
{
    return QString::fromUtf8("\xF0\x9F\x98\x80");
}

} // namespace

class MessageSpecialTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesDiceContent();
    void defaultsDiceEmoji();
    void clampsDiceValue();
    void readsDiceStateFromTypeSuffix();
    void parsesGameContent();
    void rejectsGameWithoutPositiveId();
    void parsesAnimatedEmojiContent();
    void readsAnimatedEmojiFallbackShapes();
    void rejectsAnimatedEmojiWithoutPositiveId();
    void recognizesSpecialContent();
    void rejectsNonSpecialContent();
    void acceptsMessageEnvelope();
    void toleratesMalformedFields();
};

void MessageSpecialTest::parsesDiceContent()
{
    const QJsonObject content = contentFromJson(R"({
        "@type": "messageDice",
        "emoji": "\uD83C\uDFB2",
        "value": 5,
        "state": { "@type": "diceStickersSlotMachine" }
    })");

    const DiceContent dice = diceFromContent(content);
    QCOMPARE(dice.emoji, diceEmoji());
    QCOMPARE(dice.value, 5);
    QCOMPARE(dice.state, QStringLiteral("SlotMachine"));
}

void MessageSpecialTest::defaultsDiceEmoji()
{
    const DiceContent missing = diceFromContent(contentFromJson(R"({
        "@type": "messageDice",
        "value": 2
    })"));
    QCOMPARE(missing.emoji, diceEmoji());

    const DiceContent blank = diceFromContent(contentFromJson(R"({
        "@type": "messageDice",
        "emoji": "   ",
        "value": 2
    })"));
    QCOMPARE(blank.emoji, diceEmoji());
}

void MessageSpecialTest::clampsDiceValue()
{
    struct Case {
        int input;
        int expected;
    };

    const Case cases[] = {
        {-5, 1},
        {0, 1},
        {1, 1},
        {4, 4},
        {6, 6},
        {7, 6},
        {99, 6},
    };

    for (const Case& item : cases) {
        QJsonObject content;
        content.insert(QStringLiteral("@type"), QStringLiteral("messageDice"));
        content.insert(QStringLiteral("value"), item.input);
        QCOMPARE(diceFromContent(content).value, item.expected);
    }

    // tdjson can hand integer values over as strings.
    QJsonObject stringValue;
    stringValue.insert(QStringLiteral("@type"), QStringLiteral("messageDice"));
    stringValue.insert(QStringLiteral("value"), QStringLiteral("3"));
    QCOMPARE(diceFromContent(stringValue).value, 3);
}

void MessageSpecialTest::readsDiceStateFromTypeSuffix()
{
    const DiceContent regular = diceFromContent(contentFromJson(R"({
        "@type": "messageDice",
        "state": { "@type": "diceStickersRegular" }
    })"));
    QCOMPARE(regular.state, QStringLiteral("Regular"));

    // A result normally arrives in final_state; initial_state is the fallback
    // before the server has answered.
    const DiceContent finalState = diceFromContent(contentFromJson(R"({
        "@type": "messageDice",
        "final_state": { "@type": "diceStickersSlotMachine" }
    })"));
    QCOMPARE(finalState.state, QStringLiteral("SlotMachine"));

    const DiceContent initialState = diceFromContent(contentFromJson(R"({
        "@type": "messageDice",
        "initial_state": { "@type": "diceStickersRegular" }
    })"));
    QCOMPARE(initialState.state, QStringLiteral("Regular"));

    // Stored wrappers may use a dotted class name or an already stripped
    // suffix.
    const DiceContent dotted = diceFromContent(contentFromJson(R"({
        "@type": "messageDice",
        "state": { "@type": "td.diceStickersRegular" }
    })"));
    QCOMPARE(dotted.state, QStringLiteral("Regular"));

    const DiceContent suffix = diceFromContent(contentFromJson(R"({
        "@type": "messageDice",
        "state": "Regular"
    })"));
    QCOMPARE(suffix.state, QStringLiteral("Regular"));
}

void MessageSpecialTest::parsesGameContent()
{
    const QJsonObject content = contentFromJson(R"({
        "@type": "messageGame",
        "game": {
            "@type": "game",
            "id": "9007199254740993",
            "short_name": "chess",
            "title": "Chess",
            "description": "Play chess",
            "photo": {
                "@type": "photo",
                "sizes": [
                    {
                        "@type": "photoSize",
                        "width": 90,
                        "height": 90,
                        "photo": { "@type": "file", "id": 11 }
                    },
                    {
                        "@type": "photoSize",
                        "width": 640,
                        "height": 480,
                        "photo": { "@type": "file", "id": 22 }
                    }
                ]
            }
        }
    })");

    const GameContent game = gameFromContent(content);
    QCOMPARE(game.gameId, qint64(9007199254740993LL));
    QCOMPARE(game.title, QStringLiteral("Chess"));
    QCOMPARE(game.description, QStringLiteral("Play chess"));
    QCOMPARE(game.photoFileId, 22);

    // A minimal wrapper without size dimensions still yields a file id, and
    // the rules text stands in for an absent description.
    const GameContent minimal = gameFromContent(contentFromJson(R"({
        "@type": "messageGame",
        "game": {
            "id": 7,
            "title": "Tic-tac-toe",
            "text": { "@type": "formattedText", "text": "Three in a row" },
            "photo": {
                "sizes": [
                    { "photo": { "id": 9 } }
                ]
            }
        }
    })"));
    QCOMPARE(minimal.gameId, qint64(7));
    QCOMPARE(minimal.title, QStringLiteral("Tic-tac-toe"));
    QCOMPARE(minimal.description, QStringLiteral("Three in a row"));
    QCOMPARE(minimal.photoFileId, 9);

    const GameContent noPhoto = gameFromContent(contentFromJson(R"({
        "@type": "messageGame",
        "game": { "id": 8, "title": "No photo" }
    })"));
    QCOMPARE(noPhoto.gameId, qint64(8));
    QCOMPARE(noPhoto.photoFileId, 0);
}

void MessageSpecialTest::rejectsGameWithoutPositiveId()
{
    const char* invalid[] = {
        R"({"@type": "messageGame"})",
        R"({"@type": "messageGame", "game": {}})",
        R"({"@type": "messageGame", "game": { "id": 0, "title": "T" }})",
        R"({"@type": "messageGame", "game": { "id": -1, "title": "T" }})",
        R"({"@type": "messageGame", "game": { "id": "0", "title": "T" }})",
        R"({"@type": "messageText", "game": { "id": 5, "title": "T" }})",
    };

    for (const char* json : invalid) {
        const GameContent game = gameFromContent(contentFromJson(json));
        QVERIFY2(game.gameId == 0, json);
        QVERIFY2(game.title.isEmpty(), json);
        QVERIFY2(game.description.isEmpty(), json);
        QVERIFY2(game.photoFileId == 0, json);
    }
}

void MessageSpecialTest::parsesAnimatedEmojiContent()
{
    const QJsonObject content = contentFromJson(R"({
        "@type": "messageAnimatedEmoji",
        "emoji": " \uD83D\uDE00 ",
        "animated_emoji": {
            "@type": "animatedEmoji",
            "sticker": {
                "@type": "sticker",
                "full_type": {
                    "@type": "stickerFullTypeCustomEmoji",
                    "custom_emoji_id": "9007199254740993"
                },
                "sticker": { "@type": "file", "id": 88 }
            }
        }
    })");

    const AnimatedEmojiContent emoji = animatedEmojiFromContent(content);
    QCOMPARE(emoji.customEmojiId, qint64(9007199254740993LL));
    QCOMPARE(emoji.stickerFileId, 88);
    QCOMPARE(emoji.alt, smileEmoji());
}

void MessageSpecialTest::readsAnimatedEmojiFallbackShapes()
{
    // A flat wrapper may carry the id and file id directly.
    const AnimatedEmojiContent flat = animatedEmojiFromContent(contentFromJson(R"({
        "@type": "messageAnimatedEmoji",
        "alt": "  hello  ",
        "custom_emoji_id": 7,
        "sticker_file_id": 9
    })"));
    QCOMPARE(flat.customEmojiId, qint64(7));
    QCOMPARE(flat.stickerFileId, 9);
    QCOMPARE(flat.alt, QStringLiteral("hello"));

    // Older messageAnimatedEmoji revisions put a file object at
    // animated_emoji.sticker and the id on animatedEmoji itself.
    const AnimatedEmojiContent fileObject =
        animatedEmojiFromContent(contentFromJson(R"({
            "@type": "messageAnimatedEmoji",
            "emoji": "x",
            "animated_emoji": {
                "custom_emoji_id": 42,
                "sticker": { "@type": "file", "id": 5 }
            }
        })"));
    QCOMPARE(fileObject.customEmojiId, qint64(42));
    QCOMPARE(fileObject.stickerFileId, 5);
    QCOMPARE(fileObject.alt, QStringLiteral("x"));

    // A legacy wrapper carries the sticker at the content level.
    const AnimatedEmojiContent legacy = animatedEmojiFromContent(contentFromJson(R"({
        "@type": "messageAnimatedEmoji",
        "custom_emoji_id": 11,
        "sticker": { "sticker": { "id": 12 } }
    })"));
    QCOMPARE(legacy.customEmojiId, qint64(11));
    QCOMPARE(legacy.stickerFileId, 12);
}

void MessageSpecialTest::rejectsAnimatedEmojiWithoutPositiveId()
{
    const char* invalid[] = {
        R"({"@type": "messageAnimatedEmoji"})",
        R"({"@type": "messageAnimatedEmoji", "custom_emoji_id": 0, "sticker_file_id": 9})",
        R"({"@type": "messageAnimatedEmoji", "custom_emoji_id": -4, "sticker_file_id": 9})",
        R"({"@type": "messageAnimatedEmoji", "animated_emoji": { "custom_emoji_id": 0 }})",
        R"({"@type": "messageText", "custom_emoji_id": 5})",
    };

    for (const char* json : invalid) {
        const AnimatedEmojiContent emoji = animatedEmojiFromContent(contentFromJson(json));
        QVERIFY2(emoji.customEmojiId == 0, json);
        QVERIFY2(emoji.stickerFileId == 0, json);
        QVERIFY2(emoji.alt.isEmpty(), json);
    }

    // A positive custom emoji id is enough for the content to be usable even
    // when the sticker file has not been registered yet.
    const AnimatedEmojiContent bare = animatedEmojiFromContent(contentFromJson(R"({
        "@type": "messageAnimatedEmoji",
        "custom_emoji_id": 5
    })"));
    QCOMPARE(bare.customEmojiId, qint64(5));
    QCOMPARE(bare.stickerFileId, 0);
    QVERIFY(bare.alt.isEmpty());
}

void MessageSpecialTest::recognizesSpecialContent()
{
    QVERIFY(isSpecialContent(contentFromJson(R"({"@type": "messageDice"})")));
    QVERIFY(isSpecialContent(contentFromJson(R"({"@type": "messageGame"})")));
    QVERIFY(isSpecialContent(contentFromJson(R"({"@type": "messageAnimatedEmoji"})")));
}

void MessageSpecialTest::rejectsNonSpecialContent()
{
    QVERIFY(!isSpecialContent(QJsonObject{}));
    QVERIFY(!isSpecialContent(contentFromJson(R"({"@type": "messageText"})")));
    QVERIFY(!isSpecialContent(contentFromJson(R"({"@type": "messagePhoto"})")));
    QVERIFY(!isSpecialContent(contentFromJson(R"({"@type": "messageSticker"})")));
    QVERIFY(!isSpecialContent(contentFromJson(R"({"@type": "messageFutureThing"})")));
}

void MessageSpecialTest::acceptsMessageEnvelope()
{
    const QJsonObject message = contentFromJson(R"({
        "@type": "message",
        "id": 77,
        "content": {
            "@type": "messageDice",
            "value": 4,
            "emoji": "x"
        }
    })");

    QCOMPARE(diceFromContent(message).value, 4);
    QCOMPARE(diceFromContent(message).emoji, QStringLiteral("x"));
    QVERIFY(isSpecialContent(message));

    QVERIFY(isSpecialContent(contentFromJson(R"({
        "@type": "message",
        "content": { "@type": "messageGame" }
    })")));
    QVERIFY(isSpecialContent(contentFromJson(R"({
        "@type": "message",
        "content": { "@type": "messageAnimatedEmoji" }
    })")));
}

void MessageSpecialTest::toleratesMalformedFields()
{
    const DiceContent dice = diceFromContent(contentFromJson(R"({
        "@type": "messageDice",
        "value": "nope",
        "state": 42
    })"));
    QCOMPARE(dice.value, 1);
    QCOMPARE(dice.emoji, diceEmoji());
    QVERIFY(dice.state.isEmpty());

    const GameContent game = gameFromContent(contentFromJson(R"({
        "@type": "messageGame",
        "game": []
    })"));
    QCOMPARE(game.gameId, qint64(0));
    QCOMPARE(game.photoFileId, 0);

    const AnimatedEmojiContent emoji = animatedEmojiFromContent(contentFromJson(R"({
        "@type": "messageAnimatedEmoji",
        "animated_emoji": "nope"
    })"));
    QCOMPARE(emoji.customEmojiId, qint64(0));
    QCOMPARE(emoji.stickerFileId, 0);
    QVERIFY(emoji.alt.isEmpty());
}

QTEST_GUILESS_MAIN(MessageSpecialTest)
#include "tst_message_special.moc"
