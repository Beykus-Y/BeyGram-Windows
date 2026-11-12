#include "telegram/BotCommands.h"

#include <QJsonDocument>
#include <QStringList>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject botFullInfo()
{
    return QJsonDocument::fromJson(R"({
        "@type": "userFullInfo",
        "bot_info": {
            "@type": "botInfo",
            "short_description": "Helper bot",
            "commands": []
        }
    })")
        .object();
}

QJsonObject privateBotChat()
{
    return QJsonDocument::fromJson(R"({
        "@type": "chat",
        "id": 42,
        "type": { "@type": "chatTypePrivate", "user_id": 777 },
        "last_read_inbox_message_id": 0,
        "last_read_outbox_message_id": 0
    })")
        .object();
}

} // namespace

class BotCommandsTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesCommands();
    void skipsUnusableCommands();
    void handlesMissingBotInfo();
    void showsStartForFreshPrivateBotChat();
    void hidesStartOutsidePrivateChats();
    void hidesStartForNonBots();
    void hidesStartAfterOutgoingMessages();
    void buildsStartCommand();
    void keepsStartParameterPayload();
};

void BotCommandsTest::parsesCommands()
{
    const QJsonObject fullInfo = QJsonDocument::fromJson(R"({
        "@type": "userFullInfo",
        "bot_info": {
            "@type": "botInfo",
            "commands": [
                {
                    "@type": "botCommand",
                    "command": "start",
                    "description": "Start the bot"
                },
                {
                    "@type": "botCommand",
                    "command": " /help ",
                    "description": "  Show help  "
                },
                {
                    "@type": "botCommand",
                    "command": "settings",
                    "description": ""
                }
            ]
        }
    })")
                                    .object();

    const QVector<BotCommand> commands = botCommandsFromUserFullInfo(fullInfo);
    QCOMPARE(commands.size(), 3);

    QCOMPARE(commands.at(0).command, QStringLiteral("start"));
    QCOMPARE(commands.at(0).description, QStringLiteral("Start the bot"));

    // The menu stores the bare name, so the slash and the padding are gone.
    QCOMPARE(commands.at(1).command, QStringLiteral("help"));
    QCOMPARE(commands.at(1).description, QStringLiteral("Show help"));

    // A command without a description still belongs in the menu.
    QCOMPARE(commands.at(2).command, QStringLiteral("settings"));
    QVERIFY(commands.at(2).description.isEmpty());

    // The TDLib order is preserved, never sorted.
    const QJsonObject unsorted = QJsonDocument::fromJson(R"({
        "bot_info": {
            "commands": [
                { "command": "zeta", "description": "" },
                { "command": "alpha", "description": "" }
            ]
        }
    })")
                                     .object();
    const QVector<BotCommand> ordered = botCommandsFromUserFullInfo(unsorted);
    QCOMPARE(ordered.size(), 2);
    QCOMPARE(ordered.at(0).command, QStringLiteral("zeta"));
    QCOMPARE(ordered.at(1).command, QStringLiteral("alpha"));
}

void BotCommandsTest::skipsUnusableCommands()
{
    const QJsonObject fullInfo = QJsonDocument::fromJson(R"({
        "bot_info": {
            "commands": [
                { "@type": "botCommand", "command": "", "description": "no name" },
                { "@type": "botCommand", "command": "   ", "description": "blank" },
                { "@type": "botCommand", "command": " / ", "description": "only a slash" },
                { "@type": "botCommand", "description": "missing name" },
                { "@type": "botCommand", "command": 5, "description": "wrong type" },
                "not a command",
                null,
                { "@type": "botCommand", "command": "//odd", "description": "one slash kept" }
            ]
        }
    })")
                                    .object();

    const QVector<BotCommand> commands = botCommandsFromUserFullInfo(fullInfo);
    QCOMPARE(commands.size(), 1);
    // Exactly one leading slash is removed, the same as the Android parser.
    QCOMPARE(commands.at(0).command, QStringLiteral("/odd"));
    QCOMPARE(commands.at(0).description, QStringLiteral("one slash kept"));
}

void BotCommandsTest::handlesMissingBotInfo()
{
    QVERIFY(botCommandsFromUserFullInfo({}).isEmpty());

    const QJsonObject nullBotInfo = QJsonDocument::fromJson(R"({
        "@type": "userFullInfo",
        "bot_info": null
    })")
                                        .object();
    QVERIFY(botCommandsFromUserFullInfo(nullBotInfo).isEmpty());

    const QJsonObject withoutCommands = QJsonDocument::fromJson(R"({
        "@type": "userFullInfo",
        "bot_info": { "@type": "botInfo" }
    })")
                                           .object();
    QVERIFY(botCommandsFromUserFullInfo(withoutCommands).isEmpty());

    const QJsonObject wrongCommands = QJsonDocument::fromJson(R"({
        "@type": "userFullInfo",
        "bot_info": { "@type": "botInfo", "commands": "start" }
    })")
                                          .object();
    QVERIFY(botCommandsFromUserFullInfo(wrongCommands).isEmpty());

    // A plain user full info is not a bot and must not yield a menu.
    const QJsonObject plainUser = QJsonDocument::fromJson(R"({
        "@type": "userFullInfo",
        "bio": "Hello"
    })")
                                      .object();
    QVERIFY(botCommandsFromUserFullInfo(plainUser).isEmpty());
}

void BotCommandsTest::showsStartForFreshPrivateBotChat()
{
    const QJsonObject fullInfo = botFullInfo();
    QVERIFY(shouldShowStartButton(privateBotChat(), fullInfo));

    // TDLib may serialise an absent last message as null or omit it entirely;
    // both mean an empty history.
    const QJsonObject nullLastMessage = QJsonDocument::fromJson(R"({
        "@type": "chat",
        "id": 42,
        "type": { "@type": "chatTypePrivate", "user_id": 777 },
        "last_message": null
    })")
                                             .object();
    QVERIFY(shouldShowStartButton(nullLastMessage, fullInfo));

    // A chat where only the bot has posted has no outgoing message either, so
    // the action still shows, exactly like the Android BotStartBar.
    const QJsonObject botPostedFirst = QJsonDocument::fromJson(R"({
        "@type": "chat",
        "id": 42,
        "type": { "@type": "chatTypePrivate", "user_id": 777 },
        "last_message": {
            "@type": "message",
            "id": 9,
            "is_outgoing": false
        },
        "last_read_outbox_message_id": 0
    })")
                                           .object();
    QVERIFY(shouldShowStartButton(botPostedFirst, fullInfo));
}

void BotCommandsTest::hidesStartOutsidePrivateChats()
{
    const QJsonObject fullInfo = botFullInfo();
    const QStringList foreignTypes{
        QStringLiteral("chatTypeBasicGroup"),
        QStringLiteral("chatTypeSupergroup"),
        QStringLiteral("chatTypeSecret"),
    };

    for (const QString& type : foreignTypes) {
        const QJsonObject chat = QJsonDocument::fromJson(
                                     QStringLiteral(R"({
            "@type": "chat",
            "id": 42,
            "type": { "@type": "%1" }
        })")
                                         .arg(type)
                                         .toUtf8())
                                     .object();
        QVERIFY2(!shouldShowStartButton(chat, fullInfo), qPrintable(type));
    }

    // A chat without a type is not known to be a private chat.
    QVERIFY(!shouldShowStartButton(QJsonObject{{QStringLiteral("id"), 42}}, fullInfo));
}

void BotCommandsTest::hidesStartForNonBots()
{
    QVERIFY(!shouldShowStartButton(privateBotChat(), {}));

    const QJsonObject nullBotInfo = QJsonDocument::fromJson(R"({
        "@type": "userFullInfo",
        "bot_info": null
    })")
                                        .object();
    QVERIFY(!shouldShowStartButton(privateBotChat(), nullBotInfo));

    // bot_info carries the command list, not the bot flag; an object is what
    // marks a bot, so an empty bot_info still qualifies.
    const QJsonObject emptyBotInfo = QJsonDocument::fromJson(R"({
        "@type": "userFullInfo",
        "bot_info": {}
    })")
                                         .object();
    QVERIFY(shouldShowStartButton(privateBotChat(), emptyBotInfo));

    const QJsonObject plainUser = QJsonDocument::fromJson(R"({
        "@type": "userFullInfo",
        "bio": "Hello"
    })")
                                      .object();
    QVERIFY(!shouldShowStartButton(privateBotChat(), plainUser));
}

void BotCommandsTest::hidesStartAfterOutgoingMessages()
{
    const QJsonObject fullInfo = botFullInfo();

    const QJsonObject outgoingLast = QJsonDocument::fromJson(R"({
        "@type": "chat",
        "id": 42,
        "type": { "@type": "chatTypePrivate", "user_id": 777 },
        "last_message": {
            "@type": "message",
            "id": 12,
            "is_outgoing": true
        },
        "last_read_outbox_message_id": 12
    })")
                                         .object();
    QVERIFY(!shouldShowStartButton(outgoingLast, fullInfo));

    // The newest message is incoming, but an older outgoing message was read,
    // so the user has already written to this bot.
    const QJsonObject olderOutgoing = QJsonDocument::fromJson(R"({
        "@type": "chat",
        "id": 42,
        "type": { "@type": "chatTypePrivate", "user_id": 777 },
        "last_message": {
            "@type": "message",
            "id": 14,
            "is_outgoing": false
        },
        "last_read_outbox_message_id": 11
    })")
                                          .object();
    QVERIFY(!shouldShowStartButton(olderOutgoing, fullInfo));

    // TDLib serialises int64 as a number, but a textual id must read the same.
    const QJsonObject textualOutbox = QJsonDocument::fromJson(R"({
        "@type": "chat",
        "id": 42,
        "type": { "@type": "chatTypePrivate", "user_id": 777 },
        "last_message": {
            "@type": "message",
            "id": 14,
            "is_outgoing": false
        },
        "last_read_outbox_message_id": "11"
    })")
                                           .object();
    QVERIFY(!shouldShowStartButton(textualOutbox, fullInfo));
}

void BotCommandsTest::buildsStartCommand()
{
    QCOMPARE(botStartCommand(), QStringLiteral("/start"));
    QCOMPARE(botStartCommand(QString()), QStringLiteral("/start"));
    QCOMPARE(botStartCommand(QStringLiteral("   \t\n ")), QStringLiteral("/start"));
    QCOMPARE(botStartCommand(QStringLiteral("hello")), QStringLiteral("/start hello"));
    QCOMPARE(botStartCommand(QStringLiteral("  hello  ")), QStringLiteral("/start hello"));
    QCOMPARE(botStartCommand(QStringLiteral("hello   world")),
             QStringLiteral("/start hello world"));
    QCOMPARE(botStartCommand(QStringLiteral("hello\nworld")), QStringLiteral("/start hello world"));
    QCOMPARE(botStartCommand(QStringLiteral("hello\t\tworld")),
             QStringLiteral("/start hello world"));

    // Control characters never reach the message, so the payload cannot open a
    // second line.
    QCOMPARE(botStartCommand(QStringLiteral("a") + QChar(u'\x01') + QStringLiteral("b")),
             QStringLiteral("/start ab"));
    QCOMPARE(botStartCommand(QString(QChar(u'\x01')) + QStringLiteral(" hi ")),
             QStringLiteral("/start hi"));
}

void BotCommandsTest::keepsStartParameterPayload()
{
    // Non-ASCII payloads are passed through unchanged.
    QCOMPARE(botStartCommand(QStringLiteral("привет")), QStringLiteral("/start привет"));

    // A slash inside the parameter stays an argument, not a new command.
    QCOMPARE(botStartCommand(QStringLiteral("/help")), QStringLiteral("/start /help"));
}

QTEST_GUILESS_MAIN(BotCommandsTest)
#include "tst_bot_commands.moc"
