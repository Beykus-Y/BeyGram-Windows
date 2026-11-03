#include "telegram/TextFormatting.h"

#include <QJsonDocument>
#include <QStringList>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject parseFormattedText(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

// Verifies that every tag in the fragment is properly nested and closed.
bool isWellFormedHtml(const QString& html)
{
    QStringList stack;
    qsizetype index = 0;
    while (index < html.size()) {
        const qsizetype open = html.indexOf(QLatin1Char('<'), index);
        if (open < 0)
            break;
        const qsizetype close = html.indexOf(QLatin1Char('>'), open + 1);
        if (close < 0)
            return false;

        const QString tag = html.mid(open + 1, close - open - 1);
        index = close + 1;
        if (tag.isEmpty())
            return false;
        if (tag.startsWith(QLatin1Char('/'))) {
            if (stack.isEmpty() || stack.constLast() != tag.mid(1))
                return false;
            stack.removeLast();
        } else if (tag.endsWith(QLatin1Char('/'))) {
            continue;
        } else {
            const qsizetype space = tag.indexOf(QLatin1Char(' '));
            stack.append(space < 0 ? tag : tag.left(space));
        }
    }
    return stack.isEmpty();
}

// Removes markup so tests can compare the visible characters.
QString visibleText(const QString& html)
{
    QString result;
    qsizetype index = 0;
    while (index < html.size()) {
        const qsizetype open = html.indexOf(QLatin1Char('<'), index);
        if (open < 0) {
            result += html.mid(index);
            break;
        }
        result += html.mid(index, open - index);
        const qsizetype close = html.indexOf(QLatin1Char('>'), open + 1);
        if (close < 0)
            break;
        index = close + 1;
    }
    return result;
}

} // namespace

class TextFormattingTest final : public QObject {
    Q_OBJECT

private slots:
    void escapesPlainText();
    void appliesBoldAndItalic();
    void keepsLinksSafe();
    void rendersCodePreAndBlockquote();
    void keepsOverlappingEntitiesWellFormed();
    void usesUtf16Offsets();
    void ignoresInvalidEntities();
    void handlesEmptyInput();
};

void TextFormattingTest::escapesPlainText()
{
    const FormattedText result = formatTdlibText(parseFormattedText(R"({
        "text": "<script>alert(\"x\" & y)</script>",
        "entities": []
    })"));

    QCOMPARE(result.plainText, QStringLiteral("<script>alert(\"x\" & y)</script>"));
    QCOMPARE(result.html,
             QStringLiteral("&lt;script&gt;alert(&quot;x&quot; &amp; y)&lt;/script&gt;"));
    QVERIFY(!result.html.contains(QStringLiteral("<script>")));
    QVERIFY(isWellFormedHtml(result.html));

    const FormattedText lines = formatTdlibText(parseFormattedText(R"({
        "text": "line one\nline two",
        "entities": []
    })"));
    QCOMPARE(lines.plainText, QStringLiteral("line one\nline two"));
    QCOMPARE(lines.html, QStringLiteral("line one<br/>line two"));
}

void TextFormattingTest::appliesBoldAndItalic()
{
    const FormattedText separate = formatTdlibText(parseFormattedText(R"({
        "text": "Hello world",
        "entities": [
            {"offset": 0, "length": 5, "type": {"@type": "textEntityTypeBold"}},
            {"offset": 6, "length": 5, "type": {"@type": "textEntityTypeItalic"}}
        ]
    })"));
    QCOMPARE(separate.plainText, QStringLiteral("Hello world"));
    QCOMPARE(separate.html, QStringLiteral("<b>Hello</b> <i>world</i>"));

    const FormattedText nested = formatTdlibText(parseFormattedText(R"({
        "text": "bold italic",
        "entities": [
            {"offset": 0, "length": 11, "type": {"@type": "textEntityTypeBold"}},
            {"offset": 5, "length": 6, "type": {"@type": "textEntityTypeItalic"}}
        ]
    })"));
    QCOMPARE(nested.html, QStringLiteral("<b>bold <i>italic</i></b>"));
    QVERIFY(isWellFormedHtml(nested.html));
}

void TextFormattingTest::keepsLinksSafe()
{
    const FormattedText https = formatTdlibText(parseFormattedText(R"({
        "text": "site",
        "entities": [
            {"offset": 0, "length": 4, "type": {
                "@type": "textEntityTypeTextUrl",
                "url": "https://example.com/a?b=1&c=2"
            }}
        ]
    })"));
    QCOMPARE(https.plainText, QStringLiteral("site"));
    QVERIFY(https.html.startsWith(QStringLiteral("<a href=\"")));
    QVERIFY(https.html.endsWith(QStringLiteral("</a>")));
    QVERIFY(https.html.contains(
        QStringLiteral("href=\"https://example.com/a?b=1&amp;c=2\"")));
    QVERIFY(isWellFormedHtml(https.html));

    const FormattedText mail = formatTdlibText(parseFormattedText(R"({
        "text": "mail",
        "entities": [
            {"offset": 0, "length": 4, "type": {
                "@type": "textEntityTypeTextUrl",
                "url": "mailto:user@example.com"
            }}
        ]
    })"));
    QVERIFY(mail.html.contains(QStringLiteral("href=\"mailto:user@example.com\"")));

    const FormattedText script = formatTdlibText(parseFormattedText(R"({
        "text": "bad link",
        "entities": [
            {"offset": 0, "length": 8, "type": {
                "@type": "textEntityTypeTextUrl",
                "url": "javascript:alert(1);"
            }}
        ]
    })"));
    QCOMPARE(script.plainText, QStringLiteral("bad link"));
    QCOMPARE(script.html, QStringLiteral("bad link"));
    QVERIFY(!script.html.contains(QStringLiteral("<a")));

    const FormattedText injection = formatTdlibText(parseFormattedText(R"({
        "text": "danger",
        "entities": [
            {"offset": 0, "length": 6, "type": {
                "@type": "textEntityTypeTextUrl",
                "url": "https://example.com/\"><script>alert(1)</script>"
            }}
        ]
    })"));
    QVERIFY(!injection.html.contains(QStringLiteral("<script>")));
    QVERIFY(injection.html.contains(QStringLiteral("&quot;")));
    QVERIFY(isWellFormedHtml(injection.html));

    const FormattedText autoUrl = formatTdlibText(parseFormattedText(R"({
        "text": "https://example.org",
        "entities": [
            {"offset": 0, "length": 19, "type": {"@type": "textEntityTypeUrl"}}
        ]
    })"));
    QVERIFY(autoUrl.html.contains(QStringLiteral("href=\"https://example.org\"")));
    QVERIFY(autoUrl.html.endsWith(QStringLiteral("</a>")));
}

void TextFormattingTest::rendersCodePreAndBlockquote()
{
    const FormattedText result = formatTdlibText(parseFormattedText(R"({
        "text": "one two three four",
        "entities": [
            {"offset": 0, "length": 3, "type": {"@type": "textEntityTypeCode"}},
            {"offset": 4, "length": 3, "type": {"@type": "textEntityTypePre"}},
            {"offset": 8, "length": 5, "type": {"@type": "textEntityTypePreCode",
                                               "language": "cpp"}},
            {"offset": 14, "length": 4, "type": {"@type": "textEntityTypeBlockquote"}}
        ]
    })"));

    QVERIFY(result.html.contains(QStringLiteral("<code>one</code>")));
    QVERIFY(result.html.contains(QStringLiteral("<pre>two</pre>")));
    QVERIFY(result.html.contains(QStringLiteral("<pre>three</pre>")));
    QVERIFY(result.html.contains(QStringLiteral("<blockquote>four</blockquote>")));
    QCOMPARE(visibleText(result.html), QStringLiteral("one two three four"));
    QVERIFY(isWellFormedHtml(result.html));
}

void TextFormattingTest::keepsOverlappingEntitiesWellFormed()
{
    const FormattedText result = formatTdlibText(parseFormattedText(R"({
        "text": "abcdefgh",
        "entities": [
            {"offset": 0, "length": 5, "type": {"@type": "textEntityTypeBold"}},
            {"offset": 3, "length": 5, "type": {"@type": "textEntityTypeItalic"}},
            {"offset": 2, "length": 5, "type": {"@type": "textEntityTypeUnderline"}}
        ]
    })"));

    QCOMPARE(result.plainText, QStringLiteral("abcdefgh"));
    QVERIFY(isWellFormedHtml(result.html));
    QCOMPARE(result.html.count(QStringLiteral("<b>")),
             result.html.count(QStringLiteral("</b>")));
    QCOMPARE(result.html.count(QStringLiteral("<i>")),
             result.html.count(QStringLiteral("</i>")));
    QCOMPARE(result.html.count(QStringLiteral("<u>")),
             result.html.count(QStringLiteral("</u>")));
    QVERIFY(result.html.contains(QStringLiteral("<b>")));
    QVERIFY(result.html.contains(QStringLiteral("<i>")));
    QVERIFY(result.html.contains(QStringLiteral("<u>")));
    QCOMPARE(visibleText(result.html), QStringLiteral("abcdefgh"));
}

void TextFormattingTest::usesUtf16Offsets()
{
    const QString emoji = QString::fromUtf8("\xF0\x9F\x98\x80");

    const FormattedText boldEmoji = formatTdlibText(parseFormattedText(R"({
        "text": "\ud83d\ude00 hi",
        "entities": [
            {"offset": 0, "length": 2, "type": {"@type": "textEntityTypeBold"}}
        ]
    })"));
    QCOMPARE(boldEmoji.plainText.size(), qsizetype(5));
    QCOMPARE(boldEmoji.plainText, emoji + QStringLiteral(" hi"));
    QCOMPARE(boldEmoji.html, QStringLiteral("<b>") + emoji + QStringLiteral("</b> hi"));

    // The surrogate pair occupies two UTF-16 units, so "hi" starts at offset 3.
    const FormattedText tail = formatTdlibText(parseFormattedText(R"({
        "text": "\ud83d\ude00 hi",
        "entities": [
            {"offset": 3, "length": 2, "type": {"@type": "textEntityTypeItalic"}}
        ]
    })"));
    QCOMPARE(tail.html, emoji + QStringLiteral(" <i>hi</i>"));
    QVERIFY(isWellFormedHtml(tail.html));
}

void TextFormattingTest::ignoresInvalidEntities()
{
    const FormattedText result = formatTdlibText(parseFormattedText(R"({
        "text": "hello",
        "entities": [
            {"offset": -1, "length": 3, "type": {"@type": "textEntityTypeBold"}},
            {"offset": 10, "length": 3, "type": {"@type": "textEntityTypeBold"}},
            {"offset": 2, "length": 0, "type": {"@type": "textEntityTypeBold"}},
            {"offset": 2, "length": -4, "type": {"@type": "textEntityTypeBold"}},
            {"offset": 0, "length": 5, "type": {"@type": "textEntityTypeUnknown"}},
            {"offset": 0, "length": 1, "type": "not an object"},
            {"length": 2, "type": {"@type": "textEntityTypeBold"}},
            {"offset": 0, "length": 2, "type": {"@type": "textEntityTypeBold"}}
        ]
    })"));

    QCOMPARE(result.plainText, QStringLiteral("hello"));
    QCOMPARE(result.html, QStringLiteral("<b>he</b>llo"));
    QVERIFY(isWellFormedHtml(result.html));
}

void TextFormattingTest::handlesEmptyInput()
{
    const FormattedText missing = formatTdlibText(QJsonObject{});
    QVERIFY(missing.plainText.isEmpty());
    QVERIFY(missing.html.isEmpty());

    const FormattedText empty = formatTdlibText(parseFormattedText(R"({
        "text": "",
        "entities": [
            {"offset": 0, "length": 5, "type": {"@type": "textEntityTypeBold"}}
        ]
    })"));
    QVERIFY(empty.plainText.isEmpty());
    QVERIFY(empty.html.isEmpty());
}

QTEST_GUILESS_MAIN(TextFormattingTest)
#include "tst_text_formatting.moc"
