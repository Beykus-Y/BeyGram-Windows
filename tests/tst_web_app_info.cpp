#include "telegram/WebAppInfo.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QtTest>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire, including null fields.
QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

QJsonObject messageFromJson(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

} // namespace

class WebAppInfoTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesSnakeCaseDescription();
    void parsesCamelCaseDescription();
    void prefersMessageLevelDescription();
    void fallsBackToViaBotId();
    void ignoresMessagesWithoutDescription();
    void flagsUnsafeDescriptionUrls();
    void acceptsSafeWebAppUrls();
    void rejectsUnsafeWebAppUrls();
    void buildsOpenWebAppRequest();
    void rejectsUnusableOpenWebAppRequests();
};

void WebAppInfoTest::parsesSnakeCaseDescription()
{
    const QJsonObject message = messageFromJson(R"({
        "id": 10,
        "chat_id": 42,
        "content": {
            "@type": "messageText",
            "text": { "@type": "formattedText", "text": "Open the shop" }
        },
        "web_app": {
            "@type": "webApp",
            "url": "https://mini.example/shop",
            "title": "Example Shop",
            "description": "Buy things from the bot.",
            "bot_user_id": 123456789
        }
    })");

    const WebAppInfo info = webAppFromMessage(message);
    QCOMPARE(info.url, QStringLiteral("https://mini.example/shop"));
    QCOMPARE(info.title, QStringLiteral("Example Shop"));
    QCOMPARE(info.description, QStringLiteral("Buy things from the bot."));
    QCOMPARE(info.botUserId, qint64(123456789));
    QVERIFY(info.isSecure);
}

void WebAppInfoTest::parsesCamelCaseDescription()
{
    const QJsonObject message = messageFromJson(R"({
        "id": 11,
        "chat_id": 42,
        "content": {
            "@type": "messageText",
            "text": { "@type": "formattedText", "text": "Open the shop" },
            "webApp": {
                "@type": "webApp",
                "url": "https://Mini.Example:8443/app?x=1",
                "title": "Camel Shop",
                "description": {
                    "@type": "formattedText",
                    "text": "A formatted description."
                },
                "bot_user_id": "777"
            }
        }
    })");

    const WebAppInfo info = webAppFromMessage(message);
    QCOMPARE(info.url, QStringLiteral("https://Mini.Example:8443/app?x=1"));
    QCOMPARE(info.title, QStringLiteral("Camel Shop"));
    QCOMPARE(info.description, QStringLiteral("A formatted description."));
    QCOMPARE(info.botUserId, qint64(777));
    QVERIFY(info.isSecure);
}

void WebAppInfoTest::prefersMessageLevelDescription()
{
    const QJsonObject both = messageFromJson(R"({
        "web_app": {
            "url": "https://outer.example/app",
            "title": "Outer"
        },
        "content": {
            "@type": "messageText",
            "webApp": {
                "url": "https://inner.example/app",
                "title": "Inner"
            }
        }
    })");

    const WebAppInfo outer = webAppFromMessage(both);
    QCOMPARE(outer.url, QStringLiteral("https://outer.example/app"));
    QCOMPARE(outer.title, QStringLiteral("Outer"));

    // An empty object carries nothing, so it must not shadow the content-level
    // description.
    const QJsonObject emptyOuter = messageFromJson(R"({
        "web_app": {},
        "content": {
            "@type": "messageText",
            "web_app": {
                "url": "https://inner.example/app",
                "title": "Inner"
            }
        }
    })");
    const WebAppInfo inner = webAppFromMessage(emptyOuter);
    QCOMPARE(inner.url, QStringLiteral("https://inner.example/app"));
    QCOMPARE(inner.title, QStringLiteral("Inner"));
}

void WebAppInfoTest::fallsBackToViaBotId()
{
    const QJsonObject message = messageFromJson(R"({
        "via_bot_id": 4242,
        "web_app": {
            "url": "https://mini.example/app",
            "title": "No bot id"
        }
    })");

    QCOMPARE(webAppFromMessage(message).botUserId, qint64(4242));

    // An explicit id always wins over via_bot_id.
    const QJsonObject explicitId = messageFromJson(R"({
        "via_bot_id": 4242,
        "web_app": {
            "url": "https://mini.example/app",
            "bot_user_id": 99
        }
    })");
    QCOMPARE(webAppFromMessage(explicitId).botUserId, qint64(99));

    // A malformed id is not a bot id; via_bot_id still answers.
    const QJsonObject malformed = messageFromJson(R"({
        "via_bot_id": 4242,
        "web_app": {
            "url": "https://mini.example/app",
            "bot_user_id": "not-a-number"
        }
    })");
    QCOMPARE(webAppFromMessage(malformed).botUserId, qint64(4242));
}

void WebAppInfoTest::ignoresMessagesWithoutDescription()
{
    const WebAppInfo empty = webAppFromMessage({});
    QVERIFY(empty.url.isEmpty());
    QVERIFY(empty.title.isEmpty());
    QVERIFY(empty.description.isEmpty());
    QCOMPARE(empty.botUserId, qint64(0));

    const QJsonObject noDescription = messageFromJson(R"({
        "id": 12,
        "chat_id": 42,
        "content": {
            "@type": "messageText",
            "text": { "@type": "formattedText", "text": "plain text" },
            "web_app": null
        }
    })");
    const WebAppInfo plain = webAppFromMessage(noDescription);
    QVERIFY(plain.url.isEmpty());
    QVERIFY(plain.title.isEmpty());
    QVERIFY(plain.description.isEmpty());
    QCOMPARE(plain.botUserId, qint64(0));

    // A non-object description is ignored rather than read as text.
    const QJsonObject stringDescription = messageFromJson(R"({
        "web_app": "https://mini.example/app"
    })");
    QVERIFY(webAppFromMessage(stringDescription).url.isEmpty());

    // A description without a URL is still reported, so a caller can show the
    // title; it is simply not openable.
    const QJsonObject noUrl = messageFromJson(R"({
        "web_app": { "title": "No URL", "bot_user_id": 5 }
    })");
    const WebAppInfo titleOnly = webAppFromMessage(noUrl);
    QCOMPARE(titleOnly.title, QStringLiteral("No URL"));
    QCOMPARE(titleOnly.botUserId, qint64(5));
    QVERIFY(titleOnly.url.isEmpty());
    QVERIFY(!titleOnly.isSecure);
}

void WebAppInfoTest::flagsUnsafeDescriptionUrls()
{
    const QJsonObject insecure = messageFromJson(R"({
        "web_app": { "url": "http://mini.example/app" }
    })");
    const WebAppInfo plain = webAppFromMessage(insecure);
    QCOMPARE(plain.url, QStringLiteral("http://mini.example/app"));
    QVERIFY(!plain.isSecure);

    const QJsonObject credentials = messageFromJson(R"({
        "web_app": { "url": "https://user:secret@mini.example/app" }
    })");
    const WebAppInfo withCredentials = webAppFromMessage(credentials);
    QCOMPARE(withCredentials.url,
             QStringLiteral("https://user:secret@mini.example/app"));
    QVERIFY(!withCredentials.isSecure);
}

void WebAppInfoTest::acceptsSafeWebAppUrls()
{
    const QStringList safe{
        QStringLiteral("https://mini.example"),
        QStringLiteral("https://mini.example/"),
        QStringLiteral("https://mini.example/app?x=1#frag"),
        QStringLiteral("HTTPS://Mini.Example:8443/app"),
        QStringLiteral("https://sub-domain.example.test/a/b/c"),
        QStringLiteral("https://[2001:db8::1]:443/app"),
        QStringLiteral("  https://mini.example/app  "),
    };

    for (const QString& url : safe) {
        QVERIFY2(isSafeWebAppUrl(url), qPrintable(url));
    }
}

void WebAppInfoTest::rejectsUnsafeWebAppUrls()
{
    const QStringList unsafe{
        QString(),
        QStringLiteral("   "),
        QStringLiteral("mini.example/app"),
        QStringLiteral("//mini.example/app"),
        QStringLiteral("http://mini.example/app"),
        QStringLiteral("javascript:alert(1)"),
        QStringLiteral("tg://resolve?domain=bot"),
        QStringLiteral("ftp://mini.example/app"),
        QStringLiteral("file:///C:/app.html"),
        QStringLiteral("https:mini.example"),
        QStringLiteral("https:/mini.example"),
        QStringLiteral("https://"),
        QStringLiteral("https:///app"),
        QStringLiteral("https://user@mini.example"),
        QStringLiteral("https://user:pass@mini.example"),
        QStringLiteral("https://mini.example@evil.example"),
        QStringLiteral("https://mini.example:"),
        QStringLiteral("https://mini.example:0"),
        QStringLiteral("https://mini.example:65536"),
        QStringLiteral("https://mini.example:8o"),
        QStringLiteral("https://mini.example:443:80"),
        QStringLiteral("https://mini..example"),
        QStringLiteral("https://.mini.example"),
        QStringLiteral("https://mini.example."),
        QStringLiteral("https://-mini.example"),
        QStringLiteral("https://mini-.example"),
        QStringLiteral("https://mini_example.test"),
        QStringLiteral("https://mini exa mple.test"),
        QStringLiteral("https://mini.exa\tmple.test"),
        QStringLiteral("https://mini.example\\app"),
        QStringLiteral("https://[2001:db8::1"),
        QStringLiteral("https://[zzzz::1]"),
    };

    for (const QString& url : unsafe) {
        QVERIFY2(!isSafeWebAppUrl(url), qPrintable(url));
    }

    // The 2048-character cap is inclusive; one character more is rejected.
    const QString prefix = QStringLiteral("https://mini.example/");
    QVERIFY(isSafeWebAppUrl(prefix + QString(2048 - prefix.size(), QLatin1Char('a'))));
    QVERIFY(!isSafeWebAppUrl(prefix
                             + QString(2049 - prefix.size(), QLatin1Char('a'))));
}

void WebAppInfoTest::buildsOpenWebAppRequest()
{
    const QJsonObject request = serialized(
        openWebAppRequest(42, 777, QStringLiteral("  https://mini.example/app  ")));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("openWebApp"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(42));
    QCOMPARE(request.value(QStringLiteral("bot_user_id")).toVariant().toLongLong(),
             qint64(777));
    QCOMPARE(request.value(QStringLiteral("url")).toString(),
             QStringLiteral("https://mini.example/app"));

    // No forum topic and no reply target: both are explicit nulls, not absent.
    QVERIFY(request.contains(QStringLiteral("topic_id")));
    QVERIFY(request.value(QStringLiteral("topic_id")).isNull());
    QVERIFY(request.contains(QStringLiteral("reply_to")));
    QVERIFY(request.value(QStringLiteral("reply_to")).isNull());

    const QJsonObject parameters =
        request.value(QStringLiteral("parameters")).toObject();
    QCOMPARE(parameters.value(QStringLiteral("@type")).toString(),
             QStringLiteral("webAppOpenParameters"));
    QCOMPARE(parameters.value(QStringLiteral("application_name")).toString(),
             QStringLiteral("BeyGram"));
    QVERIFY(parameters.value(QStringLiteral("theme")).isNull());
    QCOMPARE(parameters.value(QStringLiteral("mode"))
                 .toObject()
                 .value(QStringLiteral("@type"))
                 .toString(),
             QStringLiteral("webAppOpenModeFullSize"));
}

void WebAppInfoTest::rejectsUnusableOpenWebAppRequests()
{
    const QString url = QStringLiteral("https://mini.example/app");

    QVERIFY(openWebAppRequest(0, 777, url).isEmpty());
    QVERIFY(openWebAppRequest(-5, 777, url).isEmpty());
    QVERIFY(openWebAppRequest(42, 0, url).isEmpty());
    QVERIFY(openWebAppRequest(42, -777, url).isEmpty());
    QVERIFY(openWebAppRequest(42, 777, QString()).isEmpty());
    QVERIFY(openWebAppRequest(42, 777, QStringLiteral("http://mini.example/app"))
                .isEmpty());
    QVERIFY(openWebAppRequest(42, 777,
                              QStringLiteral("https://user:pass@mini.example/app"))
                .isEmpty());
}

QTEST_GUILESS_MAIN(WebAppInfoTest)
#include "tst_web_app_info.moc"
