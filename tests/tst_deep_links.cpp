#include "telegram/DeepLinks.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QtTest>

using namespace beygram::telegram;

namespace {

// Every rejected link must look exactly like a default-constructed DeepLink, so
// an unparsed link can never be mistaken for a parsed one by a caller that only
// checks one field.
void verifyUnknown(const QString& url)
{
    const DeepLink link = parseDeepLink(url);
    const QByteArray context = url.toUtf8();
    QVERIFY2(link.kind == DeepLink::Kind::Unknown, context.constData());
    QVERIFY2(link.value.isEmpty(), context.constData());
    QVERIFY2(link.botStartParameter.isEmpty(), context.constData());
    QVERIFY2(link.messageId == 0, context.constData());
}

} // namespace

class DeepLinksTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesPublicChatLinks();
    void parsesMessageLinks();
    void parsesBotStartLinks();
    void parsesInviteLinks();
    void parsesPhoneLinks();
    void parsesUserAndSettings();
    void ignoresCaseInSchemeAndHost();
    void rejectsUnsafeSchemes();
    void rejectsForeignHosts();
    void rejectsMalformedPaths();
    void rejectsMalformedQueries();
    void rejectsUnsupportedRoutes();
    void decodesPercentEncoding();
    void rejectsBrokenPercentEncoding();
    void buildsResolvePublicChatRequest();
    void buildsJoinChatRequest();
};

void DeepLinksTest::parsesPublicChatLinks()
{
    const DeepLink https = parseDeepLink(QStringLiteral("https://t.me/durov"));
    QCOMPARE(https.kind, DeepLink::Kind::PublicChat);
    QCOMPARE(https.value, QStringLiteral("durov"));
    QVERIFY(https.botStartParameter.isEmpty());
    QCOMPARE(https.messageId, qint64(0));

    // A trailing slash, the telegram.me alias and the www host are one route.
    QCOMPARE(parseDeepLink(QStringLiteral("https://t.me/durov/")).kind,
             DeepLink::Kind::PublicChat);
    QCOMPARE(parseDeepLink(QStringLiteral("https://telegram.me/durov")).value,
             QStringLiteral("durov"));
    QCOMPARE(parseDeepLink(QStringLiteral("https://www.telegram.me/durov")).value,
             QStringLiteral("durov"));

    // tg://resolve is the app-scheme form of the same link.
    const DeepLink tg = parseDeepLink(QStringLiteral("tg://resolve?domain=durov"));
    QCOMPARE(tg.kind, DeepLink::Kind::PublicChat);
    QCOMPARE(tg.value, QStringLiteral("durov"));
    QCOMPARE(tg.messageId, qint64(0));

    // Case is preserved: usernames are case-insensitive, display is not.
    QCOMPARE(parseDeepLink(QStringLiteral("https://t.me/Durov")).value,
             QStringLiteral("Durov"));

    // Legacy short usernames still resolve.
    QCOMPARE(parseDeepLink(QStringLiteral("https://t.me/gif")).kind,
             DeepLink::Kind::PublicChat);
}

void DeepLinksTest::parsesMessageLinks()
{
    const DeepLink https = parseDeepLink(QStringLiteral("https://t.me/durov/123"));
    QCOMPARE(https.kind, DeepLink::Kind::Message);
    QCOMPARE(https.value, QStringLiteral("durov"));
    QCOMPARE(https.messageId, qint64(123));
    QVERIFY(https.botStartParameter.isEmpty());

    const DeepLink tg =
        parseDeepLink(QStringLiteral("tg://resolve?domain=durov&post=123"));
    QCOMPARE(tg.kind, DeepLink::Kind::Message);
    QCOMPARE(tg.value, QStringLiteral("durov"));
    QCOMPARE(tg.messageId, qint64(123));

    // The three-segment form is a forum topic; the topic is validated but the
    // message id alone identifies the target.
    const DeepLink topic = parseDeepLink(QStringLiteral("https://t.me/durov/12/456"));
    QCOMPARE(topic.kind, DeepLink::Kind::Message);
    QCOMPARE(topic.value, QStringLiteral("durov"));
    QCOMPARE(topic.messageId, qint64(456));

    QCOMPARE(parseDeepLink(QStringLiteral("tg://resolve?domain=durov&post=77&thread=12"))
                 .messageId,
             qint64(77));

    // The web "single message" flag and the comment fragment change nothing.
    QCOMPARE(parseDeepLink(QStringLiteral("https://t.me/durov/123?single=1")).messageId,
             qint64(123));
    QCOMPARE(parseDeepLink(QStringLiteral("https://t.me/durov/123#comment-5")).messageId,
             qint64(123));
}

void DeepLinksTest::parsesBotStartLinks()
{
    const DeepLink https =
        parseDeepLink(QStringLiteral("https://t.me/somebot?start=hello"));
    QCOMPARE(https.kind, DeepLink::Kind::BotStart);
    QCOMPARE(https.value, QStringLiteral("somebot"));
    QCOMPARE(https.botStartParameter, QStringLiteral("hello"));
    QCOMPARE(https.messageId, qint64(0));

    const DeepLink tg =
        parseDeepLink(QStringLiteral("tg://resolve?domain=somebot&start=hello"));
    QCOMPARE(tg.kind, DeepLink::Kind::BotStart);
    QCOMPARE(tg.value, QStringLiteral("somebot"));
    QCOMPARE(tg.botStartParameter, QStringLiteral("hello"));

    // startgroup keeps its payload as well; the caller still only opens the bot.
    const DeepLink group =
        parseDeepLink(QStringLiteral("https://t.me/somebot?startgroup=abc"));
    QCOMPARE(group.kind, DeepLink::Kind::BotStart);
    QCOMPARE(group.botStartParameter, QStringLiteral("abc"));

    // An empty payload is a plain chat link, not an empty bot command.
    const DeepLink empty = parseDeepLink(QStringLiteral("https://t.me/somebot?start="));
    QCOMPARE(empty.kind, DeepLink::Kind::PublicChat);
    QCOMPARE(empty.value, QStringLiteral("somebot"));
    QVERIFY(empty.botStartParameter.isEmpty());
}

void DeepLinksTest::parsesInviteLinks()
{
    const DeepLink plus = parseDeepLink(QStringLiteral("https://t.me/+AbCdEf123"));
    QCOMPARE(plus.kind, DeepLink::Kind::Invite);
    QCOMPARE(plus.value, QStringLiteral("AbCdEf123"));
    QCOMPARE(plus.messageId, qint64(0));

    // The legacy /joinchat/ form carries the same hash.
    QCOMPARE(parseDeepLink(QStringLiteral("https://t.me/joinchat/AAAAAEabc_123-x")).kind,
             DeepLink::Kind::Invite);
    QCOMPARE(parseDeepLink(QStringLiteral("https://t.me/joinchat/AAAAAEabc_123-x")).value,
             QStringLiteral("AAAAAEabc_123-x"));

    const DeepLink tg = parseDeepLink(QStringLiteral("tg://join?invite=AAAAAEabc"));
    QCOMPARE(tg.kind, DeepLink::Kind::Invite);
    QCOMPARE(tg.value, QStringLiteral("AAAAAEabc"));

    // A '+' sent as an escape is stripped like a literal one.
    QCOMPARE(parseDeepLink(QStringLiteral("tg://join?invite=%2BAAAAAEabc")).value,
             QStringLiteral("AAAAAEabc"));
}

void DeepLinksTest::parsesPhoneLinks()
{
    const DeepLink https = parseDeepLink(QStringLiteral("https://t.me/+15551234567"));
    QCOMPARE(https.kind, DeepLink::Kind::Phone);
    QCOMPARE(https.value, QStringLiteral("+15551234567"));

    QCOMPARE(parseDeepLink(QStringLiteral("tg://resolve?phone=%2B15551234567")).kind,
             DeepLink::Kind::Phone);
    QCOMPARE(parseDeepLink(QStringLiteral("tg://resolve?phone=%2B15551234567")).value,
             QStringLiteral("+15551234567"));

    // The '+' is normalised in, so both spellings agree.
    QCOMPARE(parseDeepLink(QStringLiteral("tg://resolve?phone=15551234567")).value,
             QStringLiteral("+15551234567"));
}

void DeepLinksTest::parsesUserAndSettings()
{
    const DeepLink user = parseDeepLink(QStringLiteral("tg://user?id=123456789"));
    QCOMPARE(user.kind, DeepLink::Kind::PrivateChat);
    QCOMPARE(user.value, QStringLiteral("123456789"));
    QCOMPARE(user.messageId, qint64(0));

    // Leading zeros are normalised away; the value is a decimal id.
    QCOMPARE(parseDeepLink(QStringLiteral("tg://user?id=00042")).value,
             QStringLiteral("42"));

    const DeepLink settings = parseDeepLink(QStringLiteral("tg://settings"));
    QCOMPARE(settings.kind, DeepLink::Kind::Settings);
    QVERIFY(settings.value.isEmpty());
    QCOMPARE(parseDeepLink(QStringLiteral("tg://settings/")).kind,
             DeepLink::Kind::Settings);
}

void DeepLinksTest::ignoresCaseInSchemeAndHost()
{
    QCOMPARE(parseDeepLink(QStringLiteral("HTTPS://T.ME/durov")).kind,
             DeepLink::Kind::PublicChat);
    QCOMPARE(parseDeepLink(QStringLiteral("https://T.ME/durov")).value,
             QStringLiteral("durov"));
    QCOMPARE(parseDeepLink(QStringLiteral("TG://RESOLVE?domain=durov")).kind,
             DeepLink::Kind::PublicChat);
}

void DeepLinksTest::rejectsUnsafeSchemes()
{
    const QStringList urls{
        QString(),
        QStringLiteral("   "),
        QStringLiteral("durov"),
        QStringLiteral("t.me/durov"),
        QStringLiteral("javascript:alert(1)"),
        QStringLiteral("JaVaScRiPt:alert(1)"),
        QStringLiteral("http://t.me/durov"),
        QStringLiteral("http://telegram.me/durov"),
        QStringLiteral("ftp://t.me/durov"),
        QStringLiteral("file:///C:/Windows/System32"),
        QStringLiteral("data:text/html,<script>alert(1)</script>"),
        QStringLiteral("tg:resolve?domain=durov"),
        QStringLiteral("https:t.me/durov"),
        QStringLiteral("https://"),
    };
    for (const QString& url : urls)
        verifyUnknown(url);
}

void DeepLinksTest::rejectsForeignHosts()
{
    const QStringList urls{
        QStringLiteral("https://evil.com/durov"),
        QStringLiteral("https://evil.com/t.me/durov"),
        QStringLiteral("https://t.me.evil.com/durov"),
        QStringLiteral("https://t.me@evil.com/durov"),
        QStringLiteral("https://evil.com@t.me/durov"),
        QStringLiteral("https://t.me:443/durov"),
        QStringLiteral("https://telegram.org/durov"),
        QStringLiteral("https://not-t.me/durov"),
        QStringLiteral("https://t.me."),
    };
    for (const QString& url : urls)
        verifyUnknown(url);
}

void DeepLinksTest::rejectsMalformedPaths()
{
    const QStringList urls{
        QStringLiteral("https://t.me"),
        QStringLiteral("https://t.me/"),
        QStringLiteral("https://t.me/durov//123"),
        QStringLiteral("https://t.me/durov/123/456/789"),
        QStringLiteral("https://t.me/durov/abc"),
        QStringLiteral("https://t.me/durov/0"),
        QStringLiteral("https://t.me/durov/-5"),
        QStringLiteral("https://t.me/durov/1234567890123456789"),
        QStringLiteral("https://t.me/durov/9007199254740992"),
        QStringLiteral("https://t.me/durov/0/5"),
        QStringLiteral("https://t.me/durov/12/0"),
        QStringLiteral("https://t.me/12345"),
        QStringLiteral("https://t.me/_durov"),
        QStringLiteral("https://t.me/du rov"),
        QStringLiteral("https://t.me/du\nrov"),
        QStringLiteral("https://t.me/joinchat"),
        QStringLiteral("https://t.me/joinchat/"),
        QStringLiteral("https://t.me/+"),
        QStringLiteral("https://t.me/+AbCdEf123/123"),
        QStringLiteral("https://t.me/durov%2F123"),
    };
    for (const QString& url : urls)
        verifyUnknown(url);
}

void DeepLinksTest::rejectsMalformedQueries()
{
    const QStringList urls{
        // resolve needs exactly one of domain and phone.
        QStringLiteral("tg://resolve"),
        QStringLiteral("tg://resolve?domain="),
        QStringLiteral("tg://resolve?domain=durov&phone=15551234567"),
        // Duplicate keys are ambiguous.
        QStringLiteral("tg://resolve?domain=durov&domain=x"),
        QStringLiteral("https://t.me/durov?start=a&start=b"),
        // A part without '=' has no value to read.
        QStringLiteral("tg://resolve?domain"),
        QStringLiteral("https://t.me/durov?single"),
        // Empty parts and empty keys are malformed.
        QStringLiteral("https://t.me/durov?&start=a"),
        QStringLiteral("https://t.me/durov?=x"),
        // A start payload never mixes with a post or a topic.
        QStringLiteral("tg://resolve?domain=durov&post=1&start=x"),
        QStringLiteral("https://t.me/durov/123?start=x"),
        QStringLiteral("tg://resolve?domain=durov&thread=12"),
        QStringLiteral("tg://resolve?domain=durov&post=0"),
        QStringLiteral("tg://resolve?domain=durov&post=abc"),
        // Unknown parameters are not silently dropped.
        QStringLiteral("https://t.me/durov?unknown=1"),
        QStringLiteral("https://t.me/durov?comment=5"),
        QStringLiteral("tg://resolve?domain=durov&foo=1"),
        // Invite and user links accept only their own parameter.
        QStringLiteral("tg://join"),
        QStringLiteral("tg://join?invite="),
        QStringLiteral("tg://join?invite=abc&foo=1"),
        QStringLiteral("tg://join?invite=abc/def"),
        QStringLiteral("tg://user"),
        QStringLiteral("tg://user?id=0"),
        QStringLiteral("tg://user?id=-1"),
        QStringLiteral("tg://user?id=abc"),
        QStringLiteral("tg://user?id=99999999999999999999"),
        QStringLiteral("tg://user?id=42&foo=1"),
        // A phone link carries a phone and nothing else.
        QStringLiteral("tg://resolve?phone=abc"),
        QStringLiteral("tg://resolve?phone=%2B1"),
        QStringLiteral("tg://resolve?phone=15551234567&post=1"),
        QStringLiteral("tg://settings?foo=1"),
    };
    for (const QString& url : urls)
        verifyUnknown(url);
}

void DeepLinksTest::rejectsUnsupportedRoutes()
{
    const QStringList urls{
        // Reserved first segments are features, not usernames.
        QStringLiteral("https://t.me/login"),
        QStringLiteral("https://t.me/addstickers/pack"),
        QStringLiteral("https://t.me/addemoji/pack"),
        QStringLiteral("https://t.me/share/url?url=https://example.com"),
        QStringLiteral("https://t.me/invoice/abc"),
        QStringLiteral("https://t.me/giftcode/abc"),
        QStringLiteral("https://t.me/proxy?server=example.com&port=443"),
        QStringLiteral("https://t.me/socks?server=example.com"),
        QStringLiteral("https://t.me/s/durov"),
        QStringLiteral("https://t.me/bg/abc"),
        QStringLiteral("https://t.me/contact/abc"),
        // A private channel id is not a username and DeepLink cannot carry it.
        QStringLiteral("https://t.me/c/123/456"),
        // tg:// actions this client does not act on stay Unknown.
        QStringLiteral("tg://proxy?server=example.com&port=443"),
        QStringLiteral("tg://socks?server=example.com"),
        QStringLiteral("tg://addstickers?set=pack"),
        QStringLiteral("tg://unknown"),
        QStringLiteral("tg://resolve?domain=durov/extra"),
        // The QR login token is recognised only to be ignored.
        QStringLiteral("tg://login?token=AAHdF6IQAAg"),
    };
    for (const QString& url : urls)
        verifyUnknown(url);
}

void DeepLinksTest::decodesPercentEncoding()
{
    QCOMPARE(parseDeepLink(QStringLiteral("https://t.me/%64urov")).value,
             QStringLiteral("durov"));

    const DeepLink mixed =
        parseDeepLink(QStringLiteral("tg://resolve?domain=Some%5FChat"));
    QCOMPARE(mixed.kind, DeepLink::Kind::PublicChat);
    QCOMPARE(mixed.value, QStringLiteral("Some_Chat"));

    QCOMPARE(parseDeepLink(QStringLiteral("https://t.me/%2BAbCdEf123")).kind,
             DeepLink::Kind::Invite);
    QCOMPARE(parseDeepLink(QStringLiteral("https://t.me/%2BAbCdEf123")).value,
             QStringLiteral("AbCdEf123"));

    QCOMPARE(parseDeepLink(QStringLiteral("tg://resolve?domain=durov&post=%31%32%33"))
                 .messageId,
             qint64(123));

    const DeepLink start =
        parseDeepLink(QStringLiteral("https://t.me/somebot?start=hello%20world"));
    QCOMPARE(start.kind, DeepLink::Kind::BotStart);
    QCOMPARE(start.botStartParameter, QStringLiteral("hello world"));
}

void DeepLinksTest::rejectsBrokenPercentEncoding()
{
    const QStringList urls{
        QStringLiteral("https://t.me/du%ZZrov"),
        QStringLiteral("https://t.me/durov%2"),
        QStringLiteral("https://t.me/durov%"),
        QStringLiteral("https://t.me/%E2%82"),
        QStringLiteral("https://t.me/durov?start=%FF"),
        QStringLiteral("https://t.me/durov%00"),
        QStringLiteral("tg://resolve?domain=du%2Frov"),
        QStringLiteral("tg://resolve?domain=%D1%82%D0%B5%D1%81%D1%82"),
        QStringLiteral("https://t.me/durov?start=a%0Ab"),
    };
    for (const QString& url : urls)
        verifyUnknown(url);
}

void DeepLinksTest::buildsResolvePublicChatRequest()
{
    const QJsonObject request = resolvePublicChatRequest(QStringLiteral("durov"));
    QCOMPARE(request.size(), 2);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("searchPublicChat"));
    QCOMPARE(request.value(QStringLiteral("username")).toString(),
             QStringLiteral("durov"));
    // Only a username is sent; the builder never guesses a chat id.
    QVERIFY(!request.contains(QStringLiteral("chat_id")));

    // A leading '@' and surrounding whitespace are accepted.
    QCOMPARE(resolvePublicChatRequest(QStringLiteral("@durov")), request);
    QCOMPARE(resolvePublicChatRequest(QStringLiteral("  @Durov  "))
                 .value(QStringLiteral("username"))
                 .toString(),
             QStringLiteral("Durov"));

    const QStringList invalid{
        QString(),
        QStringLiteral("   "),
        QStringLiteral("@"),
        QStringLiteral("du rov"),
        QStringLiteral("https://t.me/durov"),
        QStringLiteral("durov/123"),
        QStringLiteral("_durov"),
        QStringLiteral("12345"),
        QStringLiteral("durov!"),
        QStringLiteral("дом"),
        QString(33, QLatin1Char('a')),
    };
    for (const QString& username : invalid) {
        QVERIFY2(resolvePublicChatRequest(username).isEmpty(), qPrintable(username));
    }
}

void DeepLinksTest::buildsJoinChatRequest()
{
    const QString canonical = QStringLiteral("https://t.me/+AbCdEf123");

    const QStringList accepted{
        QStringLiteral("https://t.me/+AbCdEf123"),
        QStringLiteral("https://t.me/joinchat/AbCdEf123"),
        QStringLiteral("tg://join?invite=AbCdEf123"),
        QStringLiteral("AbCdEf123"),
        QStringLiteral("+AbCdEf123"),
        QStringLiteral("  https://t.me/+AbCdEf123  "),
    };
    for (const QString& link : accepted) {
        const QJsonObject request = joinChatRequest(link);
        QCOMPARE(request.size(), 2);
        QCOMPARE(request.value(QStringLiteral("@type")).toString(),
                 QStringLiteral("joinChatByInviteLink"));
        // Every spelling is canonicalised, so raw input never reaches TDLib.
        QCOMPARE(request.value(QStringLiteral("invite_link")).toString(), canonical);
    }

    const QStringList rejected{
        QString(),
        QStringLiteral("+"),
        QStringLiteral("https://t.me/durov"),
        QStringLiteral("https://t.me/+15551234567"),
        QStringLiteral("javascript:alert(1)"),
        QStringLiteral("https://evil.com/+AbCdEf123"),
        QStringLiteral("https://t.me/+bad hash"),
        QStringLiteral("https://t.me/+AbCdEf123/extra"),
    };
    for (const QString& link : rejected) {
        QVERIFY2(joinChatRequest(link).isEmpty(), qPrintable(link));
    }

    // The request survives the transport encoding unchanged.
    const QJsonObject request = joinChatRequest(QStringLiteral("https://t.me/+AbCdEf123"));
    const QByteArray json = QJsonDocument(request).toJson(QJsonDocument::Compact);
    QCOMPARE(QJsonDocument::fromJson(json).object(), request);
}

QTEST_GUILESS_MAIN(DeepLinksTest)
#include "tst_deep_links.moc"
