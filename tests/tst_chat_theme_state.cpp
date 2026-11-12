#include "telegram/ChatThemeState.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

QJsonObject objectFromJson(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

} // namespace

class ChatThemeStateTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesChatWithThemeName();
    void parsesChatThemeObject();
    void parsesSnakeCaseThemeMember();
    void parsesThemeFieldsInline();
    void prefersChatThemeNameOverThemeObjectName();
    void treatsExplicitEmptyThemeNameAsDefault();
    void acceptsCamelCaseSpellings();
    void readsStringEncodedNumbers();
    void preservesLargeThemeIds();
    void keepsDefaultsForMissingOrWronglyTypedFields();
    void parsesUpdateWithThemeName();
    void parsesUpdateWithThemeObject();
    void buildsSetRequestShape();
    void writesEmptyNameForDefaultTheme();
    void trimsThemeName();
    void preservesLargeChatIds();
    void roundTripsNameThroughRequestJson();
};

void ChatThemeStateTest::parsesChatWithThemeName()
{
    const QJsonObject chat = objectFromJson(R"({
        "@type": "chat",
        "id": 42,
        "title": "Test",
        "theme_name": "night"
    })");

    const ChatThemeState state = chatThemeFromChat(chat);

    QCOMPARE(state.name, QStringLiteral("night"));
    QCOMPARE(state.lightThemeId, qint64(0));
    QCOMPARE(state.darkThemeId, qint64(0));
    QCOMPARE(state.accentColor, 0);
    QCOMPARE(state.outgoingBubbleColor, 0);
    QCOMPARE(state.messageTextColor, 0);
}

void ChatThemeStateTest::parsesChatThemeObject()
{
    const QJsonObject chat = objectFromJson(R"({
        "@type": "chat",
        "id": 42,
        "theme_name": "tinted",
        "chatTheme": {
            "name": "tinted",
            "light_theme": 1234,
            "dark_theme": 5678,
            "accent_color": 2796526,
            "outgoing_bubble_color": 1048117,
            "message_text_color": 16777215
        }
    })");

    const ChatThemeState state = chatThemeFromChat(chat);

    QCOMPARE(state.name, QStringLiteral("tinted"));
    QCOMPARE(state.lightThemeId, qint64(1234));
    QCOMPARE(state.darkThemeId, qint64(5678));
    QCOMPARE(state.accentColor, 2796526);
    QCOMPARE(state.outgoingBubbleColor, 1048117);
    QCOMPARE(state.messageTextColor, 16777215);
}

void ChatThemeStateTest::parsesSnakeCaseThemeMember()
{
    // TDLib's JSON uses snake_case members; both spellings describe the same
    // theme object.
    const QJsonObject chat = objectFromJson(R"({
        "@type": "chat",
        "id": 42,
        "theme_name": "day",
        "chat_theme": {
            "name": "day",
            "light_theme": 11,
            "dark_theme": 22,
            "accent_color": 3050327,
            "outgoing_bubble_color": 14079702,
            "message_text_color": 1
        }
    })");

    const ChatThemeState state = chatThemeFromChat(chat);

    QCOMPARE(state.name, QStringLiteral("day"));
    QCOMPARE(state.lightThemeId, qint64(11));
    QCOMPARE(state.darkThemeId, qint64(22));
    QCOMPARE(state.accentColor, 3050327);
    QCOMPARE(state.outgoingBubbleColor, 14079702);
    QCOMPARE(state.messageTextColor, 1);
}

void ChatThemeStateTest::parsesThemeFieldsInline()
{
    // A bare chatTheme object, or an update that carries the fields directly,
    // has no wrapper to unwrap.
    const QJsonObject theme = objectFromJson(R"({
        "name": "custom",
        "light_theme": 7,
        "dark_theme": 8,
        "accent_color": 255,
        "outgoing_bubble_color": 65535,
        "message_text_color": 16777215
    })");

    const ChatThemeState state = chatThemeFromChat(theme);

    QCOMPARE(state.name, QStringLiteral("custom"));
    QCOMPARE(state.lightThemeId, qint64(7));
    QCOMPARE(state.darkThemeId, qint64(8));
    QCOMPARE(state.accentColor, 255);
    QCOMPARE(state.outgoingBubbleColor, 65535);
    QCOMPARE(state.messageTextColor, 16777215);
}

void ChatThemeStateTest::prefersChatThemeNameOverThemeObjectName()
{
    const QJsonObject chat = objectFromJson(R"({
        "@type": "chat",
        "id": 42,
        "theme_name": "chosen",
        "chatTheme": {
            "name": "resolved",
            "light_theme": 3,
            "dark_theme": 4
        }
    })");

    const ChatThemeState state = chatThemeFromChat(chat);

    QCOMPARE(state.name, QStringLiteral("chosen"));
    QCOMPARE(state.lightThemeId, qint64(3));
    QCOMPARE(state.darkThemeId, qint64(4));
}

void ChatThemeStateTest::treatsExplicitEmptyThemeNameAsDefault()
{
    // chat.theme_name "" is TDLib's "no per-chat theme"; the resolved theme
    // object must not put its own name back.
    const QJsonObject chat = objectFromJson(R"({
        "@type": "chat",
        "id": 42,
        "theme_name": "",
        "chatTheme": {
            "name": "resolved",
            "light_theme": 3,
            "dark_theme": 4
        }
    })");

    const ChatThemeState state = chatThemeFromChat(chat);

    QVERIFY(state.name.isEmpty());
    QCOMPARE(state.lightThemeId, qint64(3));
    QCOMPARE(state.darkThemeId, qint64(4));
}

void ChatThemeStateTest::acceptsCamelCaseSpellings()
{
    const QJsonObject chat = objectFromJson(R"({
        "@type": "chat",
        "id": 42,
        "chatTheme": {
            "name": "snow",
            "lightTheme": 100,
            "darkTheme": 200,
            "accentColor": 111,
            "outgoingBubbleColor": 222,
            "messageTextColor": 333
        }
    })");

    const ChatThemeState state = chatThemeFromChat(chat);

    QCOMPARE(state.name, QStringLiteral("snow"));
    QCOMPARE(state.lightThemeId, qint64(100));
    QCOMPARE(state.darkThemeId, qint64(200));
    QCOMPARE(state.accentColor, 111);
    QCOMPARE(state.outgoingBubbleColor, 222);
    QCOMPARE(state.messageTextColor, 333);
}

void ChatThemeStateTest::readsStringEncodedNumbers()
{
    const QJsonObject theme = objectFromJson(R"({
        "name": "quoted",
        "light_theme": "9007199254740993",
        "dark_theme": "42",
        "accent_color": "2796526",
        "outgoing_bubble_color": "1048117",
        "message_text_color": "16777215"
    })");

    const ChatThemeState state = chatThemeFromChat(theme);

    QCOMPARE(state.lightThemeId, qint64(9007199254740993LL));
    QCOMPARE(state.darkThemeId, qint64(42));
    QCOMPARE(state.accentColor, 2796526);
    QCOMPARE(state.outgoingBubbleColor, 1048117);
    QCOMPARE(state.messageTextColor, 16777215);
}

void ChatThemeStateTest::preservesLargeThemeIds()
{
    // Theme ids are int64 and exceed 32 bits; they must not be truncated.
    // 2^53 keeps the round-trip through the double-backed JSON value exact.
    const qint64 light = -9007199254740992LL;
    const qint64 dark = 9007199254740992LL;

    const QJsonObject chat = QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("chat")},
        {QStringLiteral("id"), qint64(42)},
        {QStringLiteral("theme_name"), QStringLiteral("huge")},
        {QStringLiteral("chatTheme"),
         QJsonObject{
             {QStringLiteral("light_theme"), light},
             {QStringLiteral("dark_theme"), dark},
         }},
    };

    const ChatThemeState state = chatThemeFromChat(serialized(chat));

    QCOMPARE(state.lightThemeId, light);
    QCOMPARE(state.darkThemeId, dark);
}

void ChatThemeStateTest::keepsDefaultsForMissingOrWronglyTypedFields()
{
    const ChatThemeState empty = chatThemeFromChat(QJsonObject{});
    QVERIFY(empty.name.isEmpty());
    QCOMPARE(empty.lightThemeId, qint64(0));
    QCOMPARE(empty.darkThemeId, qint64(0));
    QCOMPARE(empty.accentColor, 0);
    QCOMPARE(empty.outgoingBubbleColor, 0);
    QCOMPARE(empty.messageTextColor, 0);

    const QJsonObject malformed = objectFromJson(R"({
        "@type": "chat",
        "id": 42,
        "theme_name": 5,
        "chatTheme": {
            "name": 6,
            "light_theme": true,
            "dark_theme": null,
            "accent_color": "not a colour",
            "outgoing_bubble_color": {},
            "message_text_color": []
        }
    })");

    const ChatThemeState state = chatThemeFromChat(malformed);
    QVERIFY(state.name.isEmpty());
    QCOMPARE(state.lightThemeId, qint64(0));
    QCOMPARE(state.darkThemeId, qint64(0));
    QCOMPARE(state.accentColor, 0);
    QCOMPARE(state.outgoingBubbleColor, 0);
    QCOMPARE(state.messageTextColor, 0);
}

void ChatThemeStateTest::parsesUpdateWithThemeName()
{
    const QJsonObject update = objectFromJson(R"({
        "@type": "updateChatTheme",
        "chat_id": 42,
        "theme_name": "night"
    })");

    const ChatThemeState state = chatThemeFromUpdate(update);

    QCOMPARE(state.name, QStringLiteral("night"));
    QCOMPARE(state.lightThemeId, qint64(0));
    QCOMPARE(state.darkThemeId, qint64(0));
}

void ChatThemeStateTest::parsesUpdateWithThemeObject()
{
    // Newer updates carry the resolved theme in a "theme" member.
    const QJsonObject update = objectFromJson(R"({
        "@type": "updateChatTheme",
        "chat_id": 42,
        "theme_name": "tinted",
        "theme": {
            "name": "tinted",
            "light_theme": 11,
            "dark_theme": 22,
            "accent_color": 3050327,
            "outgoing_bubble_color": 14079702,
            "message_text_color": 16
        }
    })");

    const ChatThemeState state = chatThemeFromUpdate(update);

    QCOMPARE(state.name, QStringLiteral("tinted"));
    QCOMPARE(state.lightThemeId, qint64(11));
    QCOMPARE(state.darkThemeId, qint64(22));
    QCOMPARE(state.accentColor, 3050327);
    QCOMPARE(state.outgoingBubbleColor, 14079702);
    QCOMPARE(state.messageTextColor, 16);
}

void ChatThemeStateTest::buildsSetRequestShape()
{
    const QJsonObject request =
        serialized(setChatThemeRequest(42, QStringLiteral("night")));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setChatTheme"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(request.value(QStringLiteral("theme_name")).toString(),
             QStringLiteral("night"));
    QCOMPARE(int(request.size()), 3);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void ChatThemeStateTest::writesEmptyNameForDefaultTheme()
{
    const QJsonObject request = serialized(setChatThemeRequest(42, QString()));

    QVERIFY(request.value(QStringLiteral("theme_name")).isString());
    QVERIFY(request.value(QStringLiteral("theme_name")).toString().isEmpty());
}

void ChatThemeStateTest::trimsThemeName()
{
    // A stray space must not become a theme name that does not exist.
    const QJsonObject request =
        serialized(setChatThemeRequest(42, QStringLiteral("  night \t")));

    QCOMPARE(request.value(QStringLiteral("theme_name")).toString(),
             QStringLiteral("night"));

    // Whitespace alone means the default theme, not a blank-named theme.
    const QJsonObject blank =
        serialized(setChatThemeRequest(42, QStringLiteral("   ")));
    QVERIFY(blank.value(QStringLiteral("theme_name")).toString().isEmpty());
}

void ChatThemeStateTest::preservesLargeChatIds()
{
    const qint64 chatId = -1001234567890LL;
    const QJsonObject request =
        serialized(setChatThemeRequest(chatId, QStringLiteral("day")));

    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), chatId);
}

void ChatThemeStateTest::roundTripsNameThroughRequestJson()
{
    const QJsonObject request =
        serialized(setChatThemeRequest(42, QStringLiteral("tinted")));

    // The request body is the shape an update carries, so the name survives a
    // parse of what was sent.
    const ChatThemeState parsed = chatThemeFromUpdate(request);
    QCOMPARE(parsed.name, QStringLiteral("tinted"));
}

QTEST_GUILESS_MAIN(ChatThemeStateTest)
#include "tst_chat_theme_state.moc"
