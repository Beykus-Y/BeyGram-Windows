#include "telegram/BotMarkup.h"

#include <QJsonDocument>
#include <QtTest>

using namespace beygram::telegram;

class BotMarkupTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesReplyKeyboard();
    void parsesInlineKeyboard();
    void readsButtonUrls();
    void readsCallbackData();
    void readsUserAndCopyButtons();
    void ignoresMissingOrForeignMarkup();
    void dropsUnusableRows();
    void appliesKeyboardDefaults();
    void buildsCallbackQueryRequest();
};

void BotMarkupTest::parsesReplyKeyboard()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "id": 10,
        "chat_id": 42,
        "reply_markup": {
            "@type": "replyMarkupShowKeyboard",
            "rows": [
                [
                    {
                        "@type": "keyboardButton",
                        "text": "Yes",
                        "type": { "@type": "keyboardButtonTypeText" }
                    },
                    {
                        "@type": "keyboardButton",
                        "text": "Share phone",
                        "type": { "@type": "keyboardButtonTypeRequestPhoneNumber" }
                    }
                ],
                [
                    {
                        "@type": "keyboardButton",
                        "text": "Menu",
                        "type": {
                            "@type": "keyboardButtonTypeWebApp",
                            "url": "https://example.test/app"
                        }
                    }
                ]
            ],
            "is_persistent": true,
            "resize_keyboard": false,
            "one_time": true,
            "is_adaptive": false,
            "input_field_placeholder": "Choose a reply"
        }
    })")
                                    .object();

    const BotKeyboard keyboard = botMarkupFromMessage(message);
    QVERIFY(keyboard.replyKeyboard);
    QVERIFY(!keyboard.inlineKeyboard);
    QVERIFY(keyboard.oneTime);
    QVERIFY(!keyboard.resize);
    QCOMPARE(keyboard.placeholder, QStringLiteral("Choose a reply"));
    QVERIFY(!keyboard.isEmpty());

    QCOMPARE(keyboard.rows.size(), 2);
    QCOMPARE(keyboard.rows.at(0).size(), 2);

    const BotButton yes = keyboard.rows.at(0).at(0);
    QCOMPARE(yes.text, QStringLiteral("Yes"));
    QCOMPARE(yes.type, QStringLiteral("text"));
    QVERIFY(yes.data.isEmpty());
    QVERIFY(yes.url.isEmpty());
    QCOMPARE(yes.userId, qint64(0));

    const BotButton phone = keyboard.rows.at(0).at(1);
    QCOMPARE(phone.text, QStringLiteral("Share phone"));
    QCOMPARE(phone.type, QStringLiteral("requestPhoneNumber"));

    const BotButton menu = keyboard.rows.at(1).at(0);
    QCOMPARE(menu.text, QStringLiteral("Menu"));
    QCOMPARE(menu.type, QStringLiteral("webApp"));
    QCOMPARE(menu.url, QStringLiteral("https://example.test/app"));
}

void BotMarkupTest::parsesInlineKeyboard()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "id": 11,
        "chat_id": 42,
        "reply_markup": {
            "@type": "replyMarkupInlineKeyboard",
            "rows": [
                [
                    {
                        "@type": "inlineKeyboardButton",
                        "text": "Details",
                        "type": {
                            "@type": "inlineKeyboardButtonTypeCallback",
                            "data": "ZGV0YWlscw=="
                        }
                    }
                ],
                [
                    {
                        "@type": "inlineKeyboardButton",
                        "text": "Second row",
                        "type": { "@type": "inlineKeyboardButtonTypeCallbackGame" }
                    }
                ]
            ]
        }
    })")
                                    .object();

    const BotKeyboard keyboard = botMarkupFromMessage(message);
    QVERIFY(keyboard.inlineKeyboard);
    QVERIFY(!keyboard.replyKeyboard);
    QVERIFY(!keyboard.oneTime);
    QVERIFY(keyboard.resize);
    QVERIFY(keyboard.placeholder.isEmpty());
    QCOMPARE(keyboard.rows.size(), 2);

    const BotButton details = keyboard.rows.at(0).at(0);
    QCOMPARE(details.text, QStringLiteral("Details"));
    QCOMPARE(details.type, QStringLiteral("callback"));
    QCOMPARE(details.data, QStringLiteral("ZGV0YWlscw=="));
    QVERIFY(details.url.isEmpty());

    const BotButton game = keyboard.rows.at(1).at(0);
    QCOMPARE(game.type, QStringLiteral("callbackGame"));
    QVERIFY(game.data.isEmpty());
}

void BotMarkupTest::readsButtonUrls()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "reply_markup": {
            "@type": "replyMarkupInlineKeyboard",
            "rows": [
                [
                    {
                        "@type": "inlineKeyboardButton",
                        "text": "Open",
                        "type": {
                            "@type": "inlineKeyboardButtonTypeUrl",
                            "url": "https://example.test/open"
                        }
                    },
                    {
                        "@type": "inlineKeyboardButton",
                        "text": "Sign in",
                        "type": {
                            "@type": "inlineKeyboardButtonTypeLoginUrl",
                            "url": "https://example.test/login",
                            "id": 5,
                            "forward_text": "Sign in"
                        }
                    },
                    {
                        "@type": "inlineKeyboardButton",
                        "text": "Mini app",
                        "type": {
                            "@type": "inlineKeyboardButtonTypeWebApp",
                            "url": "https://example.test/mini"
                        }
                    }
                ]
            ]
        }
    })")
                                    .object();

    const BotKeyboard keyboard = botMarkupFromMessage(message);
    QCOMPARE(keyboard.rows.size(), 1);
    QCOMPARE(keyboard.rows.at(0).size(), 3);

    QCOMPARE(keyboard.rows.at(0).at(0).type, QStringLiteral("url"));
    QCOMPARE(keyboard.rows.at(0).at(0).url, QStringLiteral("https://example.test/open"));
    QCOMPARE(keyboard.rows.at(0).at(1).type, QStringLiteral("loginUrl"));
    QCOMPARE(keyboard.rows.at(0).at(1).url, QStringLiteral("https://example.test/login"));
    QCOMPARE(keyboard.rows.at(0).at(2).type, QStringLiteral("webApp"));
    QCOMPARE(keyboard.rows.at(0).at(2).url, QStringLiteral("https://example.test/mini"));

    for (const BotButton& button : keyboard.rows.at(0))
        QVERIFY(button.data.isEmpty());
}

void BotMarkupTest::readsCallbackData()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "reply_markup": {
            "@type": "replyMarkupInlineKeyboard",
            "rows": [
                [
                    {
                        "@type": "inlineKeyboardButton",
                        "text": "Pay",
                        "type": {
                            "@type": "inlineKeyboardButtonTypeCallbackWithPassword",
                            "data": "cGF5"
                        }
                    },
                    {
                        "@type": "inlineKeyboardButton",
                        "text": "Share",
                        "type": {
                            "@type": "inlineKeyboardButtonTypeSwitchInline",
                            "query": "share me",
                            "target_chat": { "@type": "targetChatCurrent" }
                        }
                    },
                    {
                        "@type": "inlineKeyboardButton",
                        "text": "Buy",
                        "type": { "@type": "inlineKeyboardButtonTypeBuy" }
                    }
                ]
            ]
        }
    })")
                                    .object();

    const BotKeyboard keyboard = botMarkupFromMessage(message);
    QCOMPARE(keyboard.rows.size(), 1);
    QCOMPARE(keyboard.rows.at(0).size(), 3);

    const BotButton pay = keyboard.rows.at(0).at(0);
    QCOMPARE(pay.type, QStringLiteral("callbackWithPassword"));
    QCOMPARE(pay.data, QStringLiteral("cGF5"));

    const BotButton share = keyboard.rows.at(0).at(1);
    QCOMPARE(share.type, QStringLiteral("switchInline"));
    QCOMPARE(share.data, QStringLiteral("share me"));

    const BotButton buy = keyboard.rows.at(0).at(2);
    QCOMPARE(buy.type, QStringLiteral("buy"));
    QVERIFY(buy.data.isEmpty());
    QVERIFY(buy.url.isEmpty());
}

void BotMarkupTest::readsUserAndCopyButtons()
{
    const QJsonObject numeric = QJsonDocument::fromJson(R"({
        "reply_markup": {
            "@type": "replyMarkupInlineKeyboard",
            "rows": [
                [
                    {
                        "@type": "inlineKeyboardButton",
                        "text": "Copy code",
                        "type": {
                            "@type": "inlineKeyboardButtonTypeCopyText",
                            "text": "ABC-123"
                        }
                    },
                    {
                        "@type": "inlineKeyboardButton",
                        "text": "Profile",
                        "type": {
                            "@type": "inlineKeyboardButtonTypeUser",
                            "user_id": 777
                        }
                    }
                ]
            ]
        }
    })")
                                     .object();

    const BotKeyboard keyboard = botMarkupFromMessage(numeric);
    const BotButton copy = keyboard.rows.at(0).at(0);
    QCOMPARE(copy.type, QStringLiteral("copyText"));
    QCOMPARE(copy.data, QStringLiteral("ABC-123"));
    QCOMPARE(copy.text, QStringLiteral("Copy code"));

    const BotButton profile = keyboard.rows.at(0).at(1);
    QCOMPARE(profile.type, QStringLiteral("user"));
    QCOMPARE(profile.userId, qint64(777));
    QVERIFY(profile.data.isEmpty());

    // TDLib serialises int64 as a number, but a textual id must read the same.
    const QJsonObject textual = QJsonDocument::fromJson(R"({
        "reply_markup": {
            "@type": "replyMarkupInlineKeyboard",
            "rows": [
                [
                    {
                        "@type": "inlineKeyboardButton",
                        "text": "Profile",
                        "type": {
                            "@type": "inlineKeyboardButtonTypeUser",
                            "user_id": "1234567890123"
                        }
                    }
                ]
            ]
        }
    })")
                                     .object();

    const BotKeyboard textualKeyboard = botMarkupFromMessage(textual);
    QCOMPARE(textualKeyboard.rows.at(0).at(0).userId, qint64(1234567890123LL));
}

void BotMarkupTest::ignoresMissingOrForeignMarkup()
{
    const BotKeyboard withoutKey = botMarkupFromMessage(QJsonDocument::fromJson(R"({
        "id": 12,
        "chat_id": 42
    })")
                                                            .object());
    QVERIFY(!withoutKey.replyKeyboard);
    QVERIFY(!withoutKey.inlineKeyboard);
    QVERIFY(withoutKey.isEmpty());
    QVERIFY(withoutKey.resize);
    QVERIFY(!withoutKey.oneTime);
    QVERIFY(withoutKey.placeholder.isEmpty());

    const BotKeyboard nullMarkup = botMarkupFromMessage(QJsonDocument::fromJson(R"({
        "id": 12,
        "chat_id": 42,
        "reply_markup": null
    })")
                                                            .object());
    QVERIFY(nullMarkup.isEmpty());
    QVERIFY(!nullMarkup.replyKeyboard);
    QVERIFY(!nullMarkup.inlineKeyboard);

    const BotKeyboard forceReply = botMarkupFromMessage(QJsonDocument::fromJson(R"({
        "reply_markup": {
            "@type": "replyMarkupForceReply",
            "is_personal": true,
            "input_field_placeholder": "Reply"
        }
    })")
                                                              .object());
    QVERIFY(forceReply.isEmpty());
    QVERIFY(!forceReply.replyKeyboard);
    QVERIFY(!forceReply.inlineKeyboard);

    const BotKeyboard removed = botMarkupFromMessage(QJsonDocument::fromJson(R"({
        "reply_markup": { "@type": "replyMarkupRemoveKeyboard", "is_personal": false }
    })")
                                                            .object());
    QVERIFY(removed.isEmpty());

    const BotKeyboard unknown = botMarkupFromMessage(QJsonDocument::fromJson(R"({
        "reply_markup": { "@type": "replyMarkupFromTheFuture", "rows": [] }
    })")
                                                            .object());
    QVERIFY(unknown.isEmpty());
    QVERIFY(!unknown.replyKeyboard);
    QVERIFY(!unknown.inlineKeyboard);

    const BotKeyboard markupWithoutRows =
        botMarkupFromMessage(QJsonDocument::fromJson(R"({
        "reply_markup": { "@type": "replyMarkupInlineKeyboard" }
    })")
                                 .object());
    QVERIFY(markupWithoutRows.isEmpty());
    QVERIFY(markupWithoutRows.inlineKeyboard);
    QVERIFY(!markupWithoutRows.replyKeyboard);
}

void BotMarkupTest::dropsUnusableRows()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "reply_markup": {
            "@type": "replyMarkupShowKeyboard",
            "rows": [
                [],
                [
                    {
                        "@type": "keyboardButton",
                        "text": "Known",
                        "type": { "@type": "keyboardButtonTypeText" }
                    },
                    {
                        "@type": "keyboardButton",
                        "text": "Mystery",
                        "type": { "@type": "keyboardButtonTypeFromTheFuture" }
                    },
                    "not a button"
                ],
                "not a row"
            ]
        }
    })")
                                    .object();

    const BotKeyboard keyboard = botMarkupFromMessage(message);
    QVERIFY(keyboard.replyKeyboard);
    QCOMPARE(keyboard.rows.size(), 1);
    QCOMPARE(keyboard.rows.at(0).size(), 2);
    QCOMPARE(keyboard.rows.at(0).at(0).type, QStringLiteral("text"));
    QCOMPARE(keyboard.rows.at(0).at(1).text, QStringLiteral("Mystery"));
    QCOMPARE(keyboard.rows.at(0).at(1).type, QStringLiteral("unsupported"));
}

void BotMarkupTest::appliesKeyboardDefaults()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "reply_markup": {
            "@type": "replyMarkupShowKeyboard",
            "rows": [
                [
                    {
                        "@type": "keyboardButton",
                        "text": "Only",
                        "type": { "@type": "keyboardButtonTypeText" }
                    }
                ]
            ]
        }
    })")
                                    .object();

    const BotKeyboard keyboard = botMarkupFromMessage(message);
    QVERIFY(keyboard.replyKeyboard);
    // TDLib defaults both flags: not one-time, resized.
    QVERIFY(!keyboard.oneTime);
    QVERIFY(keyboard.resize);
    QVERIFY(keyboard.placeholder.isEmpty());
    QCOMPARE(keyboard.rows.size(), 1);
    QCOMPARE(keyboard.rows.at(0).size(), 1);
    QCOMPARE(keyboard.rows.at(0).at(0).type, QStringLiteral("text"));
}

void BotMarkupTest::buildsCallbackQueryRequest()
{
    const QJsonObject request =
        callbackQueryRequest(42, 900, QStringLiteral("ZGV0YWlscw=="));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getCallbackQueryAnswer"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(42));
    QCOMPARE(request.value(QStringLiteral("message_id")).toVariant().toLongLong(),
             qint64(900));

    const QJsonObject payload = request.value(QStringLiteral("payload")).toObject();
    QCOMPARE(payload.value(QStringLiteral("@type")).toString(),
             QStringLiteral("callbackQueryPayloadData"));
    QCOMPARE(payload.value(QStringLiteral("data")).toString(), QStringLiteral("ZGV0YWlscw=="));

    const QJsonObject large =
        callbackQueryRequest(-1001234567890LL, 1234567890123LL, QString());
    QCOMPARE(large.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(-1001234567890LL));
    QCOMPARE(large.value(QStringLiteral("message_id")).toVariant().toLongLong(),
             qint64(1234567890123LL));
    QVERIFY(large.value(QStringLiteral("payload"))
                .toObject()
                .value(QStringLiteral("data"))
                .toString()
                .isEmpty());
}

QTEST_GUILESS_MAIN(BotMarkupTest)
#include "tst_bot_markup.moc"
