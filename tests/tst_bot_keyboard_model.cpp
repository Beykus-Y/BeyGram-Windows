#include "telegram/BotKeyboardModel.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject markup(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

QStringList textsOf(const QVector<QVector<BotKeyboardButton>>& rows)
{
    QStringList texts;
    for (const QVector<BotKeyboardButton>& row : rows) {
        for (const BotKeyboardButton& button : row)
            texts.append(button.text);
    }
    return texts;
}

} // namespace

class BotKeyboardModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void parsesReplyKeyboard();
    void parsesInlineKeyboard();
    void mapsCallbackAndLoginVariants();
    void dropsButtonsWithoutText();
    void skipsMalformedButtons();
    void preservesRowAndColumnOrder();
    void readsForceReply();
    void removesKeyboardOnEmptyMarkup();
    void emitsChangedOnlyOnRealChanges();
    void clearResetsEverything();
    void returnsIndependentRows();
};

void BotKeyboardModelTest::startsEmpty()
{
    BotKeyboardModel model;
    QVERIFY(!model.visible());
    QVERIFY(!model.forceReply());
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(model.rows().isEmpty());
}

void BotKeyboardModelTest::parsesReplyKeyboard()
{
    BotKeyboardModel model;
    model.setReplyMarkup(markup(R"({
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
                    "text": "Menu",
                    "type": {
                        "@type": "keyboardButtonTypeWebApp",
                        "url": "https://example.test/app"
                    }
                }
            ],
            [
                {
                    "@type": "keyboardButton",
                    "text": "No",
                    "type": { "@type": "keyboardButtonTypeText" }
                }
            ]
        ],
        "is_persistent": true,
        "resize_keyboard": false,
        "one_time": true,
        "input_field_placeholder": "Choose a reply"
    })"));

    QVERIFY(model.visible());
    QVERIFY(!model.forceReply());
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.rows().at(0).size(), 2);
    QCOMPARE(model.rows().at(1).size(), 1);

    const BotKeyboardButton yes = model.rows().at(0).at(0);
    QCOMPARE(yes.text, QStringLiteral("Yes"));
    QCOMPARE(yes.type, QStringLiteral("text"));
    QVERIFY(yes.value.isEmpty());

    const BotKeyboardButton menu = model.rows().at(0).at(1);
    QCOMPARE(menu.text, QStringLiteral("Menu"));
    QCOMPARE(menu.type, QStringLiteral("webapp"));
    QCOMPARE(menu.value, QStringLiteral("https://example.test/app"));

    const BotKeyboardButton no = model.rows().at(1).at(0);
    QCOMPARE(no.text, QStringLiteral("No"));
    QCOMPARE(no.type, QStringLiteral("text"));
}

void BotKeyboardModelTest::parsesInlineKeyboard()
{
    BotKeyboardModel model;
    model.setReplyMarkup(markup(R"({
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
                },
                {
                    "@type": "inlineKeyboardButton",
                    "text": "Open",
                    "type": {
                        "@type": "inlineKeyboardButtonTypeUrl",
                        "url": "https://example.test/open"
                    }
                }
            ],
            [
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
                    "text": "Mini app",
                    "type": {
                        "@type": "inlineKeyboardButtonTypeWebApp",
                        "url": "https://example.test/mini"
                    }
                }
            ]
        ]
    })"));

    QVERIFY(model.visible());
    QVERIFY(!model.forceReply());
    QCOMPARE(model.rowCount(), 2);

    const BotKeyboardButton details = model.rows().at(0).at(0);
    QCOMPARE(details.text, QStringLiteral("Details"));
    QCOMPARE(details.type, QStringLiteral("callback"));
    QCOMPARE(details.value, QStringLiteral("ZGV0YWlscw=="));

    const BotKeyboardButton open = model.rows().at(0).at(1);
    QCOMPARE(open.text, QStringLiteral("Open"));
    QCOMPARE(open.type, QStringLiteral("url"));
    QCOMPARE(open.value, QStringLiteral("https://example.test/open"));

    const BotKeyboardButton share = model.rows().at(1).at(0);
    QCOMPARE(share.text, QStringLiteral("Share"));
    QCOMPARE(share.type, QStringLiteral("switchInline"));
    QCOMPARE(share.value, QStringLiteral("share me"));

    const BotKeyboardButton mini = model.rows().at(1).at(1);
    QCOMPARE(mini.text, QStringLiteral("Mini app"));
    QCOMPARE(mini.type, QStringLiteral("webapp"));
    QCOMPARE(mini.value, QStringLiteral("https://example.test/mini"));
}

void BotKeyboardModelTest::mapsCallbackAndLoginVariants()
{
    BotKeyboardModel model;
    model.setReplyMarkup(markup(R"({
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
                    "text": "Sign in",
                    "type": {
                        "@type": "inlineKeyboardButtonTypeLoginUrl",
                        "url": "https://example.test/login",
                        "id": 5,
                        "forward_text": "Sign in"
                    }
                }
            ]
        ]
    })"));

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.rows().at(0).size(), 2);

    const BotKeyboardButton pay = model.rows().at(0).at(0);
    QCOMPARE(pay.text, QStringLiteral("Pay"));
    QCOMPARE(pay.type, QStringLiteral("callback"));
    QCOMPARE(pay.value, QStringLiteral("cGF5"));

    const BotKeyboardButton signIn = model.rows().at(0).at(1);
    QCOMPARE(signIn.text, QStringLiteral("Sign in"));
    QCOMPARE(signIn.type, QStringLiteral("url"));
    QCOMPARE(signIn.value, QStringLiteral("https://example.test/login"));
}

void BotKeyboardModelTest::dropsButtonsWithoutText()
{
    BotKeyboardModel model;
    model.setReplyMarkup(markup(R"({
        "@type": "replyMarkupShowKeyboard",
        "rows": [
            [
                {
                    "@type": "keyboardButton",
                    "text": "",
                    "type": { "@type": "keyboardButtonTypeText" }
                },
                {
                    "@type": "keyboardButton",
                    "type": { "@type": "keyboardButtonTypeText" }
                },
                {
                    "@type": "keyboardButton",
                    "text": "Kept",
                    "type": { "@type": "keyboardButtonTypeText" }
                }
            ],
            [
                {
                    "@type": "keyboardButton",
                    "text": "",
                    "type": { "@type": "keyboardButtonTypeText" }
                }
            ]
        ]
    })"));

    // The row that lost every button is not renderable and disappears too.
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.rows().at(0).size(), 1);
    QCOMPARE(model.rows().at(0).at(0).text, QStringLiteral("Kept"));
}

void BotKeyboardModelTest::skipsMalformedButtons()
{
    BotKeyboardModel model;
    model.setReplyMarkup(markup(R"({
        "@type": "replyMarkupShowKeyboard",
        "rows": [
            [],
            [
                "not a button",
                {
                    "@type": "keyboardButton",
                    "type": { "@type": "keyboardButtonTypeText" }
                },
                {
                    "@type": "keyboardButton",
                    "text": "No type"
                },
                {
                    "@type": "keyboardButton",
                    "text": "Bad type field",
                    "type": "keyboardButtonTypeText"
                },
                {
                    "@type": "keyboardButton",
                    "text": "Future",
                    "type": { "@type": "keyboardButtonTypeFromTheFuture" }
                },
                {
                    "@type": "keyboardButton",
                    "text": "Phone",
                    "type": { "@type": "keyboardButtonTypeRequestPhoneNumber" }
                },
                {
                    "@type": "keyboardButton",
                    "text": "Game",
                    "type": { "@type": "inlineKeyboardButtonTypeCallbackGame" }
                },
                {
                    "@type": "keyboardButton",
                    "text": "Good",
                    "type": { "@type": "keyboardButtonTypeText" }
                }
            ],
            "not a row"
        ]
    })"));

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.rows().at(0).size(), 1);
    QCOMPARE(model.rows().at(0).at(0).text, QStringLiteral("Good"));
    QCOMPARE(model.rows().at(0).at(0).type, QStringLiteral("text"));
}

void BotKeyboardModelTest::preservesRowAndColumnOrder()
{
    BotKeyboardModel model;
    model.setReplyMarkup(markup(R"({
        "@type": "replyMarkupInlineKeyboard",
        "rows": [
            [
                {
                    "@type": "inlineKeyboardButton",
                    "text": "a",
                    "type": { "@type": "inlineKeyboardButtonTypeCallback", "data": "YQ==" }
                },
                {
                    "@type": "inlineKeyboardButton",
                    "text": "b",
                    "type": { "@type": "inlineKeyboardButtonTypeCallback", "data": "Yg==" }
                }
            ],
            [
                {
                    "@type": "inlineKeyboardButton",
                    "text": "c",
                    "type": { "@type": "inlineKeyboardButtonTypeCallback", "data": "Yw==" }
                }
            ],
            [
                {
                    "@type": "inlineKeyboardButton",
                    "text": "d",
                    "type": { "@type": "inlineKeyboardButtonTypeCallback", "data": "ZA==" }
                },
                {
                    "@type": "inlineKeyboardButton",
                    "text": "e",
                    "type": { "@type": "inlineKeyboardButtonTypeCallback", "data": "ZQ==" }
                },
                {
                    "@type": "inlineKeyboardButton",
                    "text": "f",
                    "type": { "@type": "inlineKeyboardButtonTypeCallback", "data": "Zg==" }
                }
            ]
        ]
    })"));

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.rows().at(0).size(), 2);
    QCOMPARE(model.rows().at(1).size(), 1);
    QCOMPARE(model.rows().at(2).size(), 3);
    QCOMPARE(textsOf(model.rows()),
             QStringList({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"),
                          QStringLiteral("d"), QStringLiteral("e"), QStringLiteral("f")}));
}

void BotKeyboardModelTest::readsForceReply()
{
    BotKeyboardModel model;
    model.setReplyMarkup(markup(R"({
        "@type": "replyMarkupForceReply",
        "is_personal": true,
        "input_field_placeholder": "Reply"
    })"));

    QVERIFY(model.forceReply());
    QVERIFY(!model.visible());
    QCOMPARE(model.rowCount(), 0);

    // Force reply never carries rows; stray rows in the object are ignored.
    model.setReplyMarkup(markup(R"({
        "@type": "replyMarkupForceReply",
        "rows": [
            [
                {
                    "@type": "keyboardButton",
                    "text": "Stray",
                    "type": { "@type": "keyboardButtonTypeText" }
                }
            ]
        ]
    })"));
    QVERIFY(model.forceReply());
    QCOMPARE(model.rowCount(), 0);

    // A real keyboard drops the force-reply flag.
    model.setReplyMarkup(markup(R"({
        "@type": "replyMarkupShowKeyboard",
        "rows": [
            [
                {
                    "@type": "keyboardButton",
                    "text": "Back",
                    "type": { "@type": "keyboardButtonTypeText" }
                }
            ]
        ]
    })"));
    QVERIFY(!model.forceReply());
    QVERIFY(model.visible());
    QCOMPARE(model.rowCount(), 1);
}

void BotKeyboardModelTest::removesKeyboardOnEmptyMarkup()
{
    const QJsonObject keyboard = markup(R"({
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
                }
            ]
        ]
    })");

    BotKeyboardModel model;
    model.setReplyMarkup(keyboard);
    QVERIFY(model.visible());

    model.setReplyMarkup(markup(R"({
        "@type": "replyMarkupRemoveKeyboard",
        "is_personal": false
    })"));
    QVERIFY(!model.visible());
    QVERIFY(!model.forceReply());
    QCOMPARE(model.rowCount(), 0);

    model.setReplyMarkup(keyboard);
    model.setReplyMarkup(QJsonObject{});
    QVERIFY(!model.visible());
    QCOMPARE(model.rowCount(), 0);

    model.setReplyMarkup(keyboard);
    model.setReplyMarkup(markup(R"({
        "@type": "replyMarkupFromTheFuture",
        "rows": [
            [
                {
                    "@type": "keyboardButton",
                    "text": "Ignored",
                    "type": { "@type": "keyboardButtonTypeText" }
                }
            ]
        ]
    })"));
    QVERIFY(!model.visible());
    QVERIFY(!model.forceReply());
    QCOMPARE(model.rowCount(), 0);
}

void BotKeyboardModelTest::emitsChangedOnlyOnRealChanges()
{
    const QJsonObject keyboard = markup(R"({
        "@type": "replyMarkupInlineKeyboard",
        "rows": [
            [
                {
                    "@type": "inlineKeyboardButton",
                    "text": "Open",
                    "type": {
                        "@type": "inlineKeyboardButtonTypeUrl",
                        "url": "https://example.test/one"
                    }
                }
            ]
        ]
    })");
    const QJsonObject sameKeyboard = markup(R"({
        "@type": "replyMarkupInlineKeyboard",
        "rows": [
            [
                {
                    "@type": "inlineKeyboardButton",
                    "text": "Open",
                    "type": {
                        "@type": "inlineKeyboardButtonTypeUrl",
                        "url": "https://example.test/one"
                    }
                }
            ]
        ]
    })");
    const QJsonObject changedTarget = markup(R"({
        "@type": "replyMarkupInlineKeyboard",
        "rows": [
            [
                {
                    "@type": "inlineKeyboardButton",
                    "text": "Open",
                    "type": {
                        "@type": "inlineKeyboardButtonTypeUrl",
                        "url": "https://example.test/two"
                    }
                }
            ]
        ]
    })");
    const QJsonObject forceReply = markup(R"({
        "@type": "replyMarkupForceReply",
        "is_personal": false
    })");
    const QJsonObject noVisibleButtons = markup(R"({
        "@type": "replyMarkupInlineKeyboard",
        "rows": [
            [
                {
                    "@type": "inlineKeyboardButton",
                    "text": "",
                    "type": {
                        "@type": "inlineKeyboardButtonTypeUrl",
                        "url": "https://example.test/dropped"
                    }
                }
            ]
        ]
    })");

    BotKeyboardModel model;
    QSignalSpy spy(&model, &BotKeyboardModel::changed);

    // An empty markup does not change an empty model.
    model.setReplyMarkup(QJsonObject{});
    QCOMPARE(spy.count(), 0);

    model.setReplyMarkup(keyboard);
    QCOMPARE(spy.count(), 1);

    // Re-parsing the same content is not a change.
    model.setReplyMarkup(sameKeyboard);
    QCOMPARE(spy.count(), 1);

    model.setReplyMarkup(changedTarget);
    QCOMPARE(spy.count(), 2);

    // A keyboard whose buttons are all dropped leaves nothing visible.
    model.setReplyMarkup(noVisibleButtons);
    QCOMPARE(spy.count(), 3);
    QVERIFY(!model.visible());

    model.setReplyMarkup(forceReply);
    QCOMPARE(spy.count(), 4);
    QVERIFY(model.forceReply());

    model.setReplyMarkup(forceReply);
    QCOMPARE(spy.count(), 4);

    model.setReplyMarkup(QJsonObject{});
    QCOMPARE(spy.count(), 5);
    QVERIFY(!model.forceReply());

    model.setReplyMarkup(QJsonObject{});
    QCOMPARE(spy.count(), 5);
}

void BotKeyboardModelTest::clearResetsEverything()
{
    BotKeyboardModel model;
    QSignalSpy spy(&model, &BotKeyboardModel::changed);

    // Clearing an empty model is not a change.
    model.clear();
    QCOMPARE(spy.count(), 0);

    model.setReplyMarkup(markup(R"({
        "@type": "replyMarkupShowKeyboard",
        "rows": [
            [
                {
                    "@type": "keyboardButton",
                    "text": "Yes",
                    "type": { "@type": "keyboardButtonTypeText" }
                }
            ]
        ]
    })"));
    QCOMPARE(spy.count(), 1);

    model.clear();
    QCOMPARE(spy.count(), 2);
    QVERIFY(!model.visible());
    QVERIFY(!model.forceReply());
    QCOMPARE(model.rowCount(), 0);

    model.clear();
    QCOMPARE(spy.count(), 2);

    model.setReplyMarkup(markup(R"({
        "@type": "replyMarkupForceReply",
        "is_personal": true
    })"));
    QCOMPARE(spy.count(), 3);
    QVERIFY(model.forceReply());

    model.clear();
    QCOMPARE(spy.count(), 4);
    QVERIFY(!model.forceReply());
    QVERIFY(!model.visible());
}

void BotKeyboardModelTest::returnsIndependentRows()
{
    BotKeyboardModel model;
    model.setReplyMarkup(markup(R"({
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
                    "text": "Close",
                    "type": {
                        "@type": "inlineKeyboardButtonTypeCallback",
                        "data": "Y2xvc2U="
                    }
                }
            ]
        ]
    })"));

    QVector<QVector<BotKeyboardButton>> snapshot = model.rows();
    snapshot[0][0].text = QStringLiteral("mutated");
    snapshot[0][0].value = QStringLiteral("https://example.test/mutated");
    snapshot[0].append(BotKeyboardButton{QStringLiteral("extra"), QStringLiteral("text"), {}});
    snapshot.append(QVector<BotKeyboardButton>{});

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.rows().at(0).size(), 2);
    QCOMPARE(model.rows().at(0).at(0).text, QStringLiteral("Open"));
    QCOMPARE(model.rows().at(0).at(0).value, QStringLiteral("https://example.test/open"));
    QCOMPARE(model.rows().at(0).at(1).text, QStringLiteral("Close"));
}

QTEST_GUILESS_MAIN(BotKeyboardModelTest)
#include "tst_bot_keyboard_model.moc"
