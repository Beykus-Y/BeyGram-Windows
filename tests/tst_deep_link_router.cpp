#include "telegram/DeepLinkRouter.h"

#include <QByteArray>
#include <QSignalSpy>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

// A rejected link must be indistinguishable from a reset router except for the
// link text: no action, no target and no message id, so nothing can be looked
// up from a link the parser did not accept.
void verifyNone(const DeepLinkRouter& router, const QString& expectedLink)
{
    const QByteArray context = expectedLink.toUtf8();
    QVERIFY2(router.action() == DeepLinkAction::None, context.constData());
    QVERIFY2(router.target().isEmpty(), context.constData());
    QVERIFY2(router.messageId() == 0, context.constData());
    QVERIFY2(!router.valid(), context.constData());
    QVERIFY2(router.link() == expectedLink, context.constData());
}

} // namespace

class DeepLinkRouterTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void routesPublicChat();
    void routesMessage();
    void routesBot();
    void routesInvite();
    void routesPhone();
    void routesProfile();
    void routesSettings();
    void acceptsHttpAndBarePrefixes();
    void rejectsUnrecognisedLinks();
    void stickerSetsStayUnrouted();
    void keepsTrimmedLinkText();
    void whitespaceLinkResets();
    void resetIsSilentWhenEmpty();
    void changedOnlyOnRealChange();
    void messageIdOnlyForMessages();
};

void DeepLinkRouterTest::startsEmpty()
{
    const DeepLinkRouter router;

    QCOMPARE(router.link(), QString());
    QCOMPARE(router.action(), DeepLinkAction::None);
    QCOMPARE(router.target(), QString());
    QCOMPARE(router.messageId(), qint64(0));
    QVERIFY(!router.valid());
}

void DeepLinkRouterTest::routesPublicChat()
{
    DeepLinkRouter router;

    router.setLink(QStringLiteral("https://t.me/Durov"));
    QCOMPARE(router.action(), DeepLinkAction::OpenChat);
    QCOMPARE(router.target(), QStringLiteral("durov"));
    QCOMPARE(router.messageId(), qint64(0));
    QVERIFY(router.valid());

    // The alias hosts and the app scheme are the same route.
    router.setLink(QStringLiteral("https://telegram.me/durov"));
    QCOMPARE(router.action(), DeepLinkAction::OpenChat);
    QCOMPARE(router.target(), QStringLiteral("durov"));

    router.setLink(QStringLiteral("tg://resolve?domain=durov"));
    QCOMPARE(router.action(), DeepLinkAction::OpenChat);
    QCOMPARE(router.target(), QStringLiteral("durov"));
}

void DeepLinkRouterTest::routesMessage()
{
    DeepLinkRouter router;

    router.setLink(QStringLiteral("https://t.me/Durov/42"));
    QCOMPARE(router.action(), DeepLinkAction::OpenMessage);
    QCOMPARE(router.target(), QStringLiteral("durov"));
    QCOMPARE(router.messageId(), qint64(42));

    // A forum link carries a topic id, but the message id is the post.
    router.setLink(QStringLiteral("https://t.me/Durov/7/43"));
    QCOMPARE(router.action(), DeepLinkAction::OpenMessage);
    QCOMPARE(router.target(), QStringLiteral("durov"));
    QCOMPARE(router.messageId(), qint64(43));

    router.setLink(QStringLiteral("tg://resolve?domain=durov&post=44"));
    QCOMPARE(router.action(), DeepLinkAction::OpenMessage);
    QCOMPARE(router.target(), QStringLiteral("durov"));
    QCOMPARE(router.messageId(), qint64(44));
}

void DeepLinkRouterTest::routesBot()
{
    DeepLinkRouter router;

    // The /start payload is not part of the router's API: only the bot to open.
    router.setLink(QStringLiteral("https://t.me/SomeBot?start=payload"));
    QCOMPARE(router.action(), DeepLinkAction::OpenBot);
    QCOMPARE(router.target(), QStringLiteral("somebot"));
    QCOMPARE(router.messageId(), qint64(0));

    router.setLink(QStringLiteral("tg://resolve?domain=SomeBot&start=payload"));
    QCOMPARE(router.action(), DeepLinkAction::OpenBot);
    QCOMPARE(router.target(), QStringLiteral("somebot"));

    // An empty payload is a plain chat link, not a bot start.
    router.setLink(QStringLiteral("https://t.me/SomeBot?start="));
    QCOMPARE(router.action(), DeepLinkAction::OpenChat);
    QCOMPARE(router.target(), QStringLiteral("somebot"));
}

void DeepLinkRouterTest::routesInvite()
{
    DeepLinkRouter router;

    // Invite hashes are base64url, so case must survive the round trip.
    router.setLink(QStringLiteral("https://t.me/+AbCdEfGh"));
    QCOMPARE(router.action(), DeepLinkAction::JoinChat);
    QCOMPARE(router.target(), QStringLiteral("AbCdEfGh"));
    QCOMPARE(router.messageId(), qint64(0));

    router.setLink(QStringLiteral("https://t.me/joinchat/AbCd-_12"));
    QCOMPARE(router.action(), DeepLinkAction::JoinChat);
    QCOMPARE(router.target(), QStringLiteral("AbCd-_12"));

    router.setLink(QStringLiteral("tg://join?invite=AbCdEf"));
    QCOMPARE(router.action(), DeepLinkAction::JoinChat);
    QCOMPARE(router.target(), QStringLiteral("AbCdEf"));
}

void DeepLinkRouterTest::routesPhone()
{
    DeepLinkRouter router;

    router.setLink(QStringLiteral("https://t.me/+15551234567"));
    QCOMPARE(router.action(), DeepLinkAction::ConfirmPhone);
    QCOMPARE(router.target(), QStringLiteral("+15551234567"));
    QCOMPARE(router.messageId(), qint64(0));

    // tg://resolve?phone= normalises to the same "+<digits>" form.
    router.setLink(QStringLiteral("tg://resolve?phone=+4915112345678"));
    QCOMPARE(router.action(), DeepLinkAction::ConfirmPhone);
    QCOMPARE(router.target(), QStringLiteral("+4915112345678"));
}

void DeepLinkRouterTest::routesProfile()
{
    DeepLinkRouter router;

    // tg://user?id= is the only route the parser gives a numeric id, and it is
    // kept verbatim rather than lower-cased.
    router.setLink(QStringLiteral("tg://user?id=42"));
    QCOMPARE(router.action(), DeepLinkAction::OpenProfile);
    QCOMPARE(router.target(), QStringLiteral("42"));
    QCOMPARE(router.messageId(), qint64(0));
    QVERIFY(router.valid());
}

void DeepLinkRouterTest::routesSettings()
{
    DeepLinkRouter router;

    router.setLink(QStringLiteral("tg://settings"));
    QCOMPARE(router.action(), DeepLinkAction::OpenSettings);
    QCOMPARE(router.target(), QString());
    QCOMPARE(router.messageId(), qint64(0));
    QVERIFY(router.valid());
}

void DeepLinkRouterTest::acceptsHttpAndBarePrefixes()
{
    DeepLinkRouter router;

    // Web pages hand out http links; the router canonicalises the scheme only.
    router.setLink(QStringLiteral("http://t.me/durov"));
    QCOMPARE(router.action(), DeepLinkAction::OpenChat);
    QCOMPARE(router.target(), QStringLiteral("durov"));

    router.setLink(QStringLiteral("HTTP://T.ME/Durov"));
    QCOMPARE(router.action(), DeepLinkAction::OpenChat);
    QCOMPARE(router.target(), QStringLiteral("durov"));

    // Bare host prefixes are what chat clients copy.
    router.setLink(QStringLiteral("t.me/durov"));
    QCOMPARE(router.action(), DeepLinkAction::OpenChat);
    QCOMPARE(router.target(), QStringLiteral("durov"));

    router.setLink(QStringLiteral("telegram.me/durov"));
    QCOMPARE(router.action(), DeepLinkAction::OpenChat);
    QCOMPARE(router.target(), QStringLiteral("durov"));

    router.setLink(QStringLiteral("www.t.me/Durov/9"));
    QCOMPARE(router.action(), DeepLinkAction::OpenMessage);
    QCOMPARE(router.target(), QStringLiteral("durov"));
    QCOMPARE(router.messageId(), qint64(9));

    // The rewrite touches the scheme only: a foreign host stays foreign.
    router.setLink(QStringLiteral("http://t.me.evil.com/durov"));
    verifyNone(router, QStringLiteral("http://t.me.evil.com/durov"));
}

void DeepLinkRouterTest::rejectsUnrecognisedLinks()
{
    DeepLinkRouter router;

    router.setLink(QStringLiteral("https://example.com/durov"));
    verifyNone(router, QStringLiteral("https://example.com/durov"));

    // A known host without a route, an unsupported tg:// action, a rejected
    // scheme and a non-numeric post id are all None.
    router.setLink(QStringLiteral("https://t.me/"));
    verifyNone(router, QStringLiteral("https://t.me/"));

    router.setLink(QStringLiteral("tg://login?token=abc"));
    verifyNone(router, QStringLiteral("tg://login?token=abc"));

    router.setLink(QStringLiteral("javascript:alert(1)"));
    verifyNone(router, QStringLiteral("javascript:alert(1)"));

    router.setLink(QStringLiteral("https://t.me/durov/notanumber"));
    verifyNone(router, QStringLiteral("https://t.me/durov/notanumber"));

    // A rejected link replaces a previously routed one completely.
    router.setLink(QStringLiteral("https://t.me/durov/42"));
    QCOMPARE(router.action(), DeepLinkAction::OpenMessage);
    router.setLink(QStringLiteral("https://t.me/share/url?url=x"));
    verifyNone(router, QStringLiteral("https://t.me/share/url?url=x"));
}

void DeepLinkRouterTest::stickerSetsStayUnrouted()
{
    DeepLinkRouter router;

    // The parser reserves /addstickers and /addemoji, so OpenStickerSet is not
    // reachable today; the router must not invent a sticker target of its own.
    router.setLink(QStringLiteral("https://t.me/addstickers/cats"));
    verifyNone(router, QStringLiteral("https://t.me/addstickers/cats"));

    router.setLink(QStringLiteral("https://t.me/addemoji/cats"));
    verifyNone(router, QStringLiteral("https://t.me/addemoji/cats"));
}

void DeepLinkRouterTest::keepsTrimmedLinkText()
{
    DeepLinkRouter router;

    router.setLink(QStringLiteral("  https://t.me/Durov  "));
    QCOMPARE(router.link(), QStringLiteral("https://t.me/Durov"));
    QCOMPARE(router.action(), DeepLinkAction::OpenChat);
    QCOMPARE(router.target(), QStringLiteral("durov"));

    // The stored text is the input, not the canonical form, so the UI can show
    // what the user pasted.
    router.setLink(QStringLiteral("t.me/Durov"));
    QCOMPARE(router.link(), QStringLiteral("t.me/Durov"));
}

void DeepLinkRouterTest::whitespaceLinkResets()
{
    DeepLinkRouter router;
    router.setLink(QStringLiteral("https://t.me/durov/42"));

    QSignalSpy spy(&router, &DeepLinkRouter::changed);
    router.setLink(QStringLiteral("   "));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(router.link(), QString());
    QCOMPARE(router.action(), DeepLinkAction::None);
    QCOMPARE(router.target(), QString());
    QCOMPARE(router.messageId(), qint64(0));
    QVERIFY(!router.valid());

    // Tabs and newlines are whitespace too.
    router.setLink(QStringLiteral("https://t.me/durov"));
    router.setLink(QStringLiteral("\t\n"));
    QCOMPARE(router.link(), QString());
    QCOMPARE(router.action(), DeepLinkAction::None);
}

void DeepLinkRouterTest::resetIsSilentWhenEmpty()
{
    DeepLinkRouter router;
    QSignalSpy spy(&router, &DeepLinkRouter::changed);

    router.reset();
    QCOMPARE(spy.count(), 0);

    // An empty setLink is the same no-op.
    router.setLink(QString());
    router.setLink(QStringLiteral("   "));
    QCOMPARE(spy.count(), 0);

    // Once there is state, reset clears it exactly once.
    router.setLink(QStringLiteral("https://t.me/durov"));
    QCOMPARE(spy.count(), 1);
    router.reset();
    QCOMPARE(spy.count(), 2);
    QCOMPARE(router.link(), QString());
    QVERIFY(!router.valid());

    router.reset();
    QCOMPARE(spy.count(), 2);
}

void DeepLinkRouterTest::changedOnlyOnRealChange()
{
    DeepLinkRouter router;
    QSignalSpy spy(&router, &DeepLinkRouter::changed);

    router.setLink(QStringLiteral("https://t.me/durov"));
    QCOMPARE(spy.count(), 1);

    // Same text, and the same text with surrounding whitespace: no signal.
    router.setLink(QStringLiteral("https://t.me/durov"));
    router.setLink(QStringLiteral(" https://t.me/durov "));
    QCOMPARE(spy.count(), 1);

    // Same route written differently changes link(), so it is a real change.
    router.setLink(QStringLiteral("tg://resolve?domain=durov"));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(router.action(), DeepLinkAction::OpenChat);
    QCOMPARE(router.target(), QStringLiteral("durov"));

    // A different post in the same chat changes the message id only.
    router.setLink(QStringLiteral("https://t.me/durov/42"));
    QCOMPARE(spy.count(), 3);
    router.setLink(QStringLiteral("https://t.me/durov/43"));
    QCOMPARE(spy.count(), 4);
    QCOMPARE(router.messageId(), qint64(43));

    // A rejected link is a change from a routed one, and is reported once.
    router.setLink(QStringLiteral("https://example.com/x"));
    QCOMPARE(spy.count(), 5);
    router.setLink(QStringLiteral("https://example.com/x"));
    QCOMPARE(spy.count(), 5);
}

void DeepLinkRouterTest::messageIdOnlyForMessages()
{
    DeepLinkRouter router;

    router.setLink(QStringLiteral("https://t.me/durov/42"));
    QCOMPARE(router.messageId(), qint64(42));

    // Leaving a message link clears the id instead of keeping a stale post.
    router.setLink(QStringLiteral("https://t.me/durov"));
    QCOMPARE(router.action(), DeepLinkAction::OpenChat);
    QCOMPARE(router.messageId(), qint64(0));

    router.setLink(QStringLiteral("https://t.me/+AbCdEfGh"));
    QCOMPARE(router.action(), DeepLinkAction::JoinChat);
    QCOMPARE(router.messageId(), qint64(0));

    router.setLink(QStringLiteral("https://t.me/+15551234567"));
    QCOMPARE(router.action(), DeepLinkAction::ConfirmPhone);
    QCOMPARE(router.messageId(), qint64(0));

    router.setLink(QStringLiteral("tg://user?id=42"));
    QCOMPARE(router.action(), DeepLinkAction::OpenProfile);
    QCOMPARE(router.messageId(), qint64(0));

    router.setLink(QStringLiteral("tg://settings"));
    QCOMPARE(router.action(), DeepLinkAction::OpenSettings);
    QCOMPARE(router.messageId(), qint64(0));
}

QTEST_GUILESS_MAIN(DeepLinkRouterTest)

#include "tst_deep_link_router.moc"
