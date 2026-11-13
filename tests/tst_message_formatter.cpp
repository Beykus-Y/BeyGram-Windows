#include "telegram/MessageFormatter.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject parseObject(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

QJsonArray parseArray(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).array();
}

QJsonObject entity(qsizetype offset, qsizetype length, const QString& typeName)
{
    QJsonObject type;
    type.insert(QStringLiteral("@type"), typeName);

    QJsonObject value;
    value.insert(QStringLiteral("offset"), double(offset));
    value.insert(QStringLiteral("length"), double(length));
    value.insert(QStringLiteral("type"), type);
    return value;
}

} // namespace

class MessageFormatterTest final : public QObject {
    Q_OBJECT

private slots:
    void escapesHtml();
    void rendersInlineEntities();
    void rendersBlockEntities();
    void rendersLinks();
    void rendersMentionLikeEntities();
    void acceptsBareEntityNames();
    void escapesEntityContent();
    void usesUtf16Offsets();
    void skipsMalformedEntities();
    void skipsOverlappingEntities();
    void keepsUnknownEntityText();
    void rendersMessageTextObject();
    void readsPlainText();
    void handlesEmptyInput();
};

void MessageFormatterTest::escapesHtml()
{
    QCOMPARE(escapeHtml(QStringLiteral("<&>\"'")),
             QStringLiteral("&lt;&amp;&gt;&quot;&#39;"));
    QCOMPARE(escapeHtml(QStringLiteral("2 < 3 & 4 > 1")),
             QStringLiteral("2 &lt; 3 &amp; 4 &gt; 1"));
    QCOMPARE(escapeHtml(QStringLiteral("it's")), QStringLiteral("it&#39;s"));
    QCOMPARE(escapeHtml(QStringLiteral("&amp;")), QStringLiteral("&amp;amp;"));
    QCOMPARE(escapeHtml(QStringLiteral("line\nnext")), QStringLiteral("line\nnext"));
    QCOMPARE(escapeHtml(QStringLiteral("plain")), QStringLiteral("plain"));
    QVERIFY(escapeHtml(QString()).isEmpty());
}

void MessageFormatterTest::rendersInlineEntities()
{
    const QString text = QStringLiteral("bold italic under strike spoiler");
    const QJsonArray entities = parseArray(R"([
        {"offset": 0, "length": 4, "type": {"@type": "textEntityTypeBold"}},
        {"offset": 5, "length": 6, "type": {"@type": "textEntityTypeItalic"}},
        {"offset": 12, "length": 5, "type": {"@type": "textEntityTypeUnderline"}},
        {"offset": 18, "length": 6, "type": {"@type": "textEntityTypeStrikethrough"}},
        {"offset": 25, "length": 7, "type": {"@type": "textEntityTypeSpoiler"}}
    ])");

    QCOMPARE(renderEntities(text, entities),
             QStringLiteral("<b>bold</b> <i>italic</i> <u>under</u> "
                            "<s>strike</s> <span class=\"spoiler\">spoiler</span>"));
}

void MessageFormatterTest::rendersBlockEntities()
{
    const QString text = QStringLiteral("code pre block lang");
    const QJsonArray entities = parseArray(R"([
        {"offset": 0, "length": 4, "type": {"@type": "textEntityTypeCode"}},
        {"offset": 5, "length": 3, "type": {"@type": "textEntityTypePre"}},
        {"offset": 9, "length": 5, "type": {"@type": "textEntityTypeBlockquote"}},
        {"offset": 15, "length": 4, "type": {"@type": "textEntityTypePreCode",
                                             "language": "cpp"}}
    ])");

    QCOMPARE(renderEntities(text, entities),
             QStringLiteral("<code>code</code> <pre>pre</pre> "
                            "<blockquote>block</blockquote> <pre>lang</pre>"));
}

void MessageFormatterTest::rendersLinks()
{
    const QJsonArray textUrl = parseArray(R"([
        {"offset": 0, "length": 7, "type": {
            "@type": "textEntityTypeTextUrl",
            "url": "https://example.com/a?b=1&c=2"
        }}
    ])");
    QCOMPARE(renderEntities(QStringLiteral("example"), textUrl),
             QStringLiteral("<a href=\"https://example.com/a?b=1&amp;c=2\" "
                            "rel=\"noopener\">example</a>"));

    const QJsonArray autoUrl = parseArray(R"([
        {"offset": 0, "length": 21, "type": {"@type": "textEntityTypeUrl"}}
    ])");
    QCOMPARE(renderEntities(QStringLiteral("https://example.org/x"), autoUrl),
             QStringLiteral("<a href=\"https://example.org/x\" "
                            "rel=\"noopener\">https://example.org/x</a>"));

    const QJsonArray mailto = parseArray(R"([
        {"offset": 0, "length": 4, "type": {
            "@type": "textEntityTypeTextUrl",
            "url": "mailto:user@example.com"
        }}
    ])");
    QCOMPARE(renderEntities(QStringLiteral("mail"), mailto),
             QStringLiteral("<a href=\"mailto:user@example.com\" "
                            "rel=\"noopener\">mail</a>"));

    const QJsonArray tg = parseArray(R"([
        {"offset": 0, "length": 4, "type": {
            "@type": "textEntityTypeTextUrl",
            "url": "tg://user?id=1"
        }}
    ])");
    QCOMPARE(renderEntities(QStringLiteral("chat"), tg),
             QStringLiteral("<a href=\"tg://user?id=1\" "
                            "rel=\"noopener\">chat</a>"));

    // Unsafe schemes are rendered as plain text, without an anchor.
    const QJsonArray script = parseArray(R"json([
        {"offset": 0, "length": 5, "type": {
            "@type": "textEntityTypeTextUrl",
            "url": "javascript:alert(1)"
        }}
    ])json");
    QCOMPARE(renderEntities(QStringLiteral("click"), script), QStringLiteral("click"));
    QVERIFY(!renderEntities(QStringLiteral("click"), script).contains(QStringLiteral("<a")));

    // Quotes and angle brackets in the target are escaped inside the attribute.
    const QJsonArray injection = parseArray(R"json([
        {"offset": 0, "length": 6, "type": {
            "@type": "textEntityTypeTextUrl",
            "url": "https://example.com/\"><script>alert(1)</script>"
        }}
    ])json");
    const QString injected = renderEntities(QStringLiteral("danger"), injection);
    QCOMPARE(injected,
             QStringLiteral("<a href=\"https://example.com/&quot;&gt;&lt;script&gt;"
                            "alert(1)&lt;/script&gt;\" rel=\"noopener\">danger</a>"));
    QVERIFY(!injected.contains(QStringLiteral("<script>")));
}

void MessageFormatterTest::rendersMentionLikeEntities()
{
    struct Case {
        const char* text;
        const char* type;
        const char* className;
    };

    const Case cases[] = {
        {"@alice", "textEntityTypeMention", "mention"},
        {"@bob", "textEntityTypeTextMention", "mention"},
        {"#tag", "textEntityTypeHashtag", "hashtag"},
        {"$USD", "textEntityTypeCashtag", "cashtag"},
        {"/start", "textEntityTypeBotCommand", "bot-command"},
        {"mail@example.com", "textEntityTypeEmailAddress", "email"},
        {"+1234567890", "textEntityTypePhoneNumber", "phone"},
    };

    for (const Case& item : cases) {
        const QString text = QString::fromUtf8(item.text);
        const QJsonArray entities{entity(0, text.size(), QString::fromLatin1(item.type))};
        QCOMPARE(renderEntities(text, entities),
                 QStringLiteral("<span class=\"") + QString::fromLatin1(item.className)
                     + QStringLiteral("\">") + text + QStringLiteral("</span>"));
    }
}

void MessageFormatterTest::acceptsBareEntityNames()
{
    const QJsonArray entities = parseArray(R"([
        {"offset": 0, "length": 4, "type": "bold"},
        {"offset": 5, "length": 4, "type": "textUrl",
         "url": "https://example.com"}
    ])");

    QCOMPARE(renderEntities(QStringLiteral("bold link"), entities),
             QStringLiteral("<b>bold</b> <a href=\"https://example.com\" "
                            "rel=\"noopener\">link</a>"));
}

void MessageFormatterTest::escapesEntityContent()
{
    const QJsonArray bold = parseArray(R"([
        {"offset": 0, "length": 5, "type": {"@type": "textEntityTypeBold"}}
    ])");
    QCOMPARE(renderEntities(QStringLiteral("<tag>"), bold),
             QStringLiteral("<b>&lt;tag&gt;</b>"));

    const QJsonArray link = parseArray(R"([
        {"offset": 0, "length": 3, "type": {
            "@type": "textEntityTypeTextUrl",
            "url": "https://example.com/?a=1&b=2"
        }}
    ])");
    QCOMPARE(renderEntities(QStringLiteral("a&b"), link),
             QStringLiteral("<a href=\"https://example.com/?a=1&amp;b=2\" "
                            "rel=\"noopener\">a&amp;b</a>"));
}

void MessageFormatterTest::usesUtf16Offsets()
{
    const QString emoji = QString::fromUtf8("\xF0\x9F\x98\x80");
    const QString text = emoji + QStringLiteral(" hi");
    QCOMPARE(text.size(), qsizetype(5));

    const QJsonArray bold = parseArray(R"([
        {"offset": 0, "length": 2, "type": {"@type": "textEntityTypeBold"}}
    ])");
    QCOMPARE(renderEntities(text, bold),
             QStringLiteral("<b>") + emoji + QStringLiteral("</b> hi"));

    // The surrogate pair occupies two UTF-16 units, so "hi" starts at offset 3.
    const QJsonArray italic = parseArray(R"([
        {"offset": 3, "length": 2, "type": {"@type": "textEntityTypeItalic"}}
    ])");
    QCOMPARE(renderEntities(text, italic),
             emoji + QStringLiteral(" <i>hi</i>"));
}

void MessageFormatterTest::skipsMalformedEntities()
{
    const QJsonArray entities = parseArray(R"([
        {"offset": -1, "length": 3, "type": {"@type": "textEntityTypeBold"}},
        {"offset": 10, "length": 3, "type": {"@type": "textEntityTypeBold"}},
        {"offset": 2, "length": 0, "type": {"@type": "textEntityTypeBold"}},
        {"offset": 2, "length": -4, "type": {"@type": "textEntityTypeBold"}},
        {"offset": 0, "length": 99, "type": {"@type": "textEntityTypeBold"}},
        {"offset": 0.5, "length": 2, "type": {"@type": "textEntityTypeBold"}},
        {"offset": 1, "length": 2},
        {"offset": 1, "length": 2, "type": "not an object"},
        {"length": 2, "type": {"@type": "textEntityTypeBold"}},
        "not an entity",
        {"offset": 0, "length": 2, "type": {"@type": "textEntityTypeBold"}}
    ])");

    QCOMPARE(renderEntities(QStringLiteral("hello"), entities),
             QStringLiteral("<b>he</b>llo"));

    // Ranges may not start or end inside a surrogate pair.
    const QString emoji = QString::fromUtf8("\xF0\x9F\x98\x80");
    QCOMPARE(renderEntities(emoji, parseArray(R"([
        {"offset": 1, "length": 1, "type": {"@type": "textEntityTypeBold"}}
    ])")),
             emoji);
    QCOMPARE(renderEntities(emoji, parseArray(R"([
        {"offset": 0, "length": 1, "type": {"@type": "textEntityTypeBold"}}
    ])")),
             emoji);
}

void MessageFormatterTest::skipsOverlappingEntities()
{
    const QJsonArray overlapping = parseArray(R"([
        {"offset": 0, "length": 5, "type": {"@type": "textEntityTypeBold"}},
        {"offset": 3, "length": 5, "type": {"@type": "textEntityTypeItalic"}},
        {"offset": 2, "length": 2, "type": {"@type": "textEntityTypeUnderline"}},
        {"offset": 0, "length": 1, "type": {"@type": "textEntityTypeCode"}}
    ])");
    QCOMPARE(renderEntities(QStringLiteral("abcdefgh"), overlapping),
             QStringLiteral("<b>abcde</b>fgh"));

    // Entities are sorted by offset before rendering.
    const QJsonArray unsorted = parseArray(R"([
        {"offset": 3, "length": 3, "type": {"@type": "textEntityTypeItalic"}},
        {"offset": 0, "length": 3, "type": {"@type": "textEntityTypeBold"}}
    ])");
    QCOMPARE(renderEntities(QStringLiteral("abcdef"), unsorted),
             QStringLiteral("<b>abc</b><i>def</i>"));

    // Adjacent ranges are allowed; containment counts as overlap here.
    const QJsonArray adjacent = parseArray(R"([
        {"offset": 0, "length": 3, "type": {"@type": "textEntityTypeBold"}},
        {"offset": 3, "length": 3, "type": {"@type": "textEntityTypeItalic"}}
    ])");
    QCOMPARE(renderEntities(QStringLiteral("abcdef"), adjacent),
             QStringLiteral("<b>abc</b><i>def</i>"));

    const QJsonArray nested = parseArray(R"([
        {"offset": 0, "length": 6, "type": {"@type": "textEntityTypeBold"}},
        {"offset": 2, "length": 2, "type": {"@type": "textEntityTypeItalic"}}
    ])");
    QCOMPARE(renderEntities(QStringLiteral("abcdef"), nested),
             QStringLiteral("<b>abcdef</b>"));
}

void MessageFormatterTest::keepsUnknownEntityText()
{
    const QJsonArray entities = parseArray(R"([
        {"offset": 0, "length": 5, "type": {"@type": "textEntityTypeUnknown"}},
        {"offset": 6, "length": 5, "type": {"@type": "textEntityTypeCustomEmoji",
                                             "custom_emoji_id": "123"}},
        {"offset": 0, "length": 1, "type": {"@type": "textEntityTypeFromTheFuture"}}
    ])");
    QCOMPARE(renderEntities(QStringLiteral("hello world"), entities),
             QStringLiteral("hello world"));

    const QJsonArray unknown = parseArray(R"([
        {"offset": 0, "length": 5, "type": {"@type": "textEntityTypeUnknown"}}
    ])");
    QCOMPARE(renderEntities(QStringLiteral("a < b"), unknown),
             QStringLiteral("a &lt; b"));
}

void MessageFormatterTest::rendersMessageTextObject()
{
    const QJsonObject formatted = parseObject(R"({
        "text": "Hello <world>",
        "entities": [
            {"offset": 0, "length": 5, "type": {"@type": "textEntityTypeBold"}}
        ]
    })");
    QCOMPARE(renderMessageText(formatted),
             QStringLiteral("<b>Hello</b> &lt;world&gt;"));

    const QJsonObject plain = parseObject(R"({"text": "it's a <test>"})");
    QCOMPARE(renderMessageText(plain),
             QStringLiteral("it&#39;s a &lt;test&gt;"));

    const QJsonObject badEntities = parseObject(R"({
        "text": "text",
        "entities": "not an array"
    })");
    QCOMPARE(renderMessageText(badEntities), QStringLiteral("text"));

    QVERIFY(renderMessageText(QJsonObject{}).isEmpty());
}

void MessageFormatterTest::readsPlainText()
{
    const QJsonObject formatted = parseObject(R"({
        "text": "Hello <world>",
        "entities": [
            {"offset": 0, "length": 5, "type": {"@type": "textEntityTypeBold"}}
        ]
    })");
    QCOMPARE(plainText(formatted), QStringLiteral("Hello <world>"));
    QVERIFY(plainText(QJsonObject{}).isEmpty());
    QVERIFY(plainText(parseObject(R"({"text": ""})")).isEmpty());
}

void MessageFormatterTest::handlesEmptyInput()
{
    QVERIFY(renderEntities(QString(), parseArray(R"([
        {"offset": 0, "length": 1, "type": {"@type": "textEntityTypeBold"}}
    ])")).isEmpty());

    QCOMPARE(renderEntities(QStringLiteral("plain"), QJsonArray{}),
             QStringLiteral("plain"));
    QCOMPARE(renderEntities(QStringLiteral("plain"), parseArray("[]")),
             QStringLiteral("plain"));
}

QTEST_GUILESS_MAIN(MessageFormatterTest)
#include "tst_message_formatter.moc"
