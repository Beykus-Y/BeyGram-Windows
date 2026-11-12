#include "ui/DeepLinksFacade.h"

#include <QByteArray>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QtTest>

using namespace beygram::ui;

namespace {

// The facade must expose the router's state as the documented strings, so a
// rejection is checked in one place: action "none", no target, no message id.
void verifyRejected(const DeepLinksFacade& facade, const QString& expectedLink)
{
    const QByteArray context = expectedLink.toUtf8();
    QVERIFY2(facade.action() == QStringLiteral("none"), context.constData());
    QVERIFY2(facade.target().isEmpty(), context.constData());
    QVERIFY2(facade.messageId().isEmpty(), context.constData());
    QVERIFY2(!facade.valid(), context.constData());
    QVERIFY2(facade.link() == expectedLink, context.constData());
}

// confirm() must announce exactly the action/target/message triple the
// properties report, so QML and the shell cannot disagree.
void verifyConfirmPayload(DeepLinksFacade& facade, const QString& expectedAction,
                          const QString& expectedTarget, const QString& expectedMessage)
{
    QSignalSpy spy(&facade, &DeepLinksFacade::navigateRequested);
    QVERIFY(facade.confirm());
    QCOMPARE(spy.count(), 1);

    const QList<QVariant> arguments = spy.at(0);
    QCOMPARE(arguments.size(), 3);
    QCOMPARE(arguments.at(0).toString(), expectedAction);
    QCOMPARE(arguments.at(1).toString(), expectedTarget);
    QCOMPARE(arguments.at(2).toString(), expectedMessage);
}

} // namespace

class DeepLinksFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void routesChatLink();
    void routesMessageLink();
    void routesInviteLink();
    void routesPhoneLink();
    void routesProfileLink();
    void routesBotLink();
    void routesSettingsLink();
    void acceptsWebAndBarePrefixes();
    void rejectsInvalidLink();
    void rejectsStickerSets();
    void keepsTrimmedLinkText();
    void changedOnlyOnRealChange();
    void messageIdEmptyForNonMessageLinks();

    void confirmEmitsPayload();
    void confirmCarriesMessageId();
    void confirmIsSilentWhenInvalid();
    void confirmKeepsState();
    void confirmAfterResetIsSilent();

    void resetClearsState();
    void resetIsSilentWhenEmpty();
};

void DeepLinksFacadeTest::startsEmpty()
{
    const DeepLinksFacade facade;

    QCOMPARE(facade.link(), QString());
    QCOMPARE(facade.action(), QStringLiteral("none"));
    QCOMPARE(facade.target(), QString());
    QCOMPARE(facade.messageId(), QString());
    QVERIFY(!facade.valid());
}

void DeepLinksFacadeTest::routesChatLink()
{
    DeepLinksFacade facade;

    facade.setLink(QStringLiteral("https://t.me/Durov"));
    QCOMPARE(facade.action(), QStringLiteral("openChat"));
    QCOMPARE(facade.target(), QStringLiteral("durov"));
    QVERIFY(facade.messageId().isEmpty());
    QVERIFY(facade.valid());
    QCOMPARE(facade.link(), QStringLiteral("https://t.me/Durov"));
}

void DeepLinksFacadeTest::routesMessageLink()
{
    DeepLinksFacade facade;

    facade.setLink(QStringLiteral("https://t.me/Durov/42"));
    QCOMPARE(facade.action(), QStringLiteral("openMessage"));
    QCOMPARE(facade.target(), QStringLiteral("durov"));
    QCOMPARE(facade.messageId(), QStringLiteral("42"));
    QVERIFY(facade.valid());

    // A forum link carries a topic id, but the post is the message id.
    facade.setLink(QStringLiteral("https://t.me/Durov/7/43"));
    QCOMPARE(facade.action(), QStringLiteral("openMessage"));
    QCOMPARE(facade.messageId(), QStringLiteral("43"));

    facade.setLink(QStringLiteral("tg://resolve?domain=durov&post=44"));
    QCOMPARE(facade.action(), QStringLiteral("openMessage"));
    QCOMPARE(facade.messageId(), QStringLiteral("44"));
}

void DeepLinksFacadeTest::routesInviteLink()
{
    DeepLinksFacade facade;

    // Invite hashes are base64url, so case must survive the facade untouched.
    facade.setLink(QStringLiteral("https://t.me/+AbCdEfGh"));
    QCOMPARE(facade.action(), QStringLiteral("joinChat"));
    QCOMPARE(facade.target(), QStringLiteral("AbCdEfGh"));
    QVERIFY(facade.messageId().isEmpty());

    facade.setLink(QStringLiteral("tg://join?invite=AbCd-_12"));
    QCOMPARE(facade.action(), QStringLiteral("joinChat"));
    QCOMPARE(facade.target(), QStringLiteral("AbCd-_12"));
}

void DeepLinksFacadeTest::routesPhoneLink()
{
    DeepLinksFacade facade;

    facade.setLink(QStringLiteral("https://t.me/+15551234567"));
    QCOMPARE(facade.action(), QStringLiteral("confirmPhone"));
    QCOMPARE(facade.target(), QStringLiteral("+15551234567"));
    QVERIFY(facade.messageId().isEmpty());

    facade.setLink(QStringLiteral("tg://resolve?phone=+4915112345678"));
    QCOMPARE(facade.action(), QStringLiteral("confirmPhone"));
    QCOMPARE(facade.target(), QStringLiteral("+4915112345678"));
}

void DeepLinksFacadeTest::routesProfileLink()
{
    DeepLinksFacade facade;

    facade.setLink(QStringLiteral("tg://user?id=42"));
    QCOMPARE(facade.action(), QStringLiteral("openProfile"));
    QCOMPARE(facade.target(), QStringLiteral("42"));
    QVERIFY(facade.messageId().isEmpty());
    QVERIFY(facade.valid());
}

void DeepLinksFacadeTest::routesBotLink()
{
    DeepLinksFacade facade;

    // The /start payload is not part of the facade's contract: only the bot.
    facade.setLink(QStringLiteral("https://t.me/SomeBot?start=payload"));
    QCOMPARE(facade.action(), QStringLiteral("openBot"));
    QCOMPARE(facade.target(), QStringLiteral("somebot"));
    QVERIFY(facade.messageId().isEmpty());
}

void DeepLinksFacadeTest::routesSettingsLink()
{
    DeepLinksFacade facade;

    facade.setLink(QStringLiteral("tg://settings"));
    QCOMPARE(facade.action(), QStringLiteral("openSettings"));
    QCOMPARE(facade.target(), QString());
    QVERIFY(facade.messageId().isEmpty());
    QVERIFY(facade.valid());
}

void DeepLinksFacadeTest::acceptsWebAndBarePrefixes()
{
    DeepLinksFacade facade;

    facade.setLink(QStringLiteral("http://t.me/Durov"));
    QCOMPARE(facade.action(), QStringLiteral("openChat"));
    QCOMPARE(facade.target(), QStringLiteral("durov"));

    facade.setLink(QStringLiteral("t.me/Durov/9"));
    QCOMPARE(facade.action(), QStringLiteral("openMessage"));
    QCOMPARE(facade.messageId(), QStringLiteral("9"));

    // The router only canonicalises the scheme, so a foreign host stays invalid.
    facade.setLink(QStringLiteral("http://t.me.evil.com/durov"));
    verifyRejected(facade, QStringLiteral("http://t.me.evil.com/durov"));
}

void DeepLinksFacadeTest::rejectsInvalidLink()
{
    DeepLinksFacade facade;

    facade.setLink(QStringLiteral("https://example.com/durov"));
    verifyRejected(facade, QStringLiteral("https://example.com/durov"));

    facade.setLink(QStringLiteral("javascript:alert(1)"));
    verifyRejected(facade, QStringLiteral("javascript:alert(1)"));

    facade.setLink(QStringLiteral("tg://login?token=abc"));
    verifyRejected(facade, QStringLiteral("tg://login?token=abc"));

    // A rejected link replaces a routed one completely.
    facade.setLink(QStringLiteral("https://t.me/durov/42"));
    QVERIFY(facade.valid());
    facade.setLink(QStringLiteral("https://t.me/share/url?url=x"));
    verifyRejected(facade, QStringLiteral("https://t.me/share/url?url=x"));
}

void DeepLinksFacadeTest::rejectsStickerSets()
{
    DeepLinksFacade facade;

    // The parser reserves these routes, so the facade must not invent a
    // sticker-set target of its own.
    facade.setLink(QStringLiteral("https://t.me/addstickers/cats"));
    verifyRejected(facade, QStringLiteral("https://t.me/addstickers/cats"));

    facade.setLink(QStringLiteral("https://t.me/addemoji/cats"));
    verifyRejected(facade, QStringLiteral("https://t.me/addemoji/cats"));
}

void DeepLinksFacadeTest::keepsTrimmedLinkText()
{
    DeepLinksFacade facade;

    facade.setLink(QStringLiteral("  https://t.me/Durov  "));
    QCOMPARE(facade.link(), QStringLiteral("https://t.me/Durov"));
    QCOMPARE(facade.action(), QStringLiteral("openChat"));
    QCOMPARE(facade.target(), QStringLiteral("durov"));
}

void DeepLinksFacadeTest::changedOnlyOnRealChange()
{
    DeepLinksFacade facade;
    QSignalSpy spy(&facade, &DeepLinksFacade::changed);

    facade.setLink(QStringLiteral("https://t.me/durov"));
    QCOMPARE(spy.count(), 1);

    // Same text, and the same text with surrounding whitespace: no signal.
    facade.setLink(QStringLiteral("https://t.me/durov"));
    facade.setLink(QStringLiteral(" https://t.me/durov "));
    QCOMPARE(spy.count(), 1);

    // A different post in the same chat changes messageId(), so it is real.
    facade.setLink(QStringLiteral("https://t.me/durov/42"));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(facade.messageId(), QStringLiteral("42"));

    // A rejected link is a change from a routed one, and is reported once.
    facade.setLink(QStringLiteral("https://example.com/x"));
    QCOMPARE(spy.count(), 3);
    facade.setLink(QStringLiteral("https://example.com/x"));
    QCOMPARE(spy.count(), 3);
}

void DeepLinksFacadeTest::messageIdEmptyForNonMessageLinks()
{
    DeepLinksFacade facade;

    facade.setLink(QStringLiteral("https://t.me/durov/42"));
    QCOMPARE(facade.messageId(), QStringLiteral("42"));

    // Leaving a message link clears the id instead of keeping a stale post.
    facade.setLink(QStringLiteral("https://t.me/durov"));
    QVERIFY(facade.messageId().isEmpty());

    facade.setLink(QStringLiteral("https://t.me/+AbCdEfGh"));
    QVERIFY(facade.messageId().isEmpty());

    facade.setLink(QStringLiteral("tg://settings"));
    QVERIFY(facade.messageId().isEmpty());
}

void DeepLinksFacadeTest::confirmEmitsPayload()
{
    DeepLinksFacade facade;
    facade.setLink(QStringLiteral("https://t.me/Durov"));

    verifyConfirmPayload(facade, QStringLiteral("openChat"), QStringLiteral("durov"),
                         QString());
}

void DeepLinksFacadeTest::confirmCarriesMessageId()
{
    DeepLinksFacade facade;
    facade.setLink(QStringLiteral("https://t.me/Durov/42"));

    verifyConfirmPayload(facade, QStringLiteral("openMessage"), QStringLiteral("durov"),
                         QStringLiteral("42"));
}

void DeepLinksFacadeTest::confirmIsSilentWhenInvalid()
{
    DeepLinksFacade facade;
    QSignalSpy spy(&facade, &DeepLinksFacade::navigateRequested);

    QVERIFY(!facade.confirm());
    QCOMPARE(spy.count(), 0);

    facade.setLink(QStringLiteral("https://example.com/x"));
    QVERIFY(!facade.confirm());
    QCOMPARE(spy.count(), 0);
}

void DeepLinksFacadeTest::confirmKeepsState()
{
    DeepLinksFacade facade;
    facade.setLink(QStringLiteral("tg://user?id=42"));
    QSignalSpy spy(&facade, &DeepLinksFacade::navigateRequested);

    // A failed navigation must not lose the link: confirm() announces and
    // leaves the state for the caller to reset when it is done.
    QVERIFY(facade.confirm());
    QVERIFY(facade.confirm());
    QCOMPARE(spy.count(), 2);
    QCOMPARE(facade.action(), QStringLiteral("openProfile"));
    QCOMPARE(facade.target(), QStringLiteral("42"));
}

void DeepLinksFacadeTest::confirmAfterResetIsSilent()
{
    DeepLinksFacade facade;
    facade.setLink(QStringLiteral("tg://settings"));
    facade.reset();

    QSignalSpy spy(&facade, &DeepLinksFacade::navigateRequested);
    QVERIFY(!facade.confirm());
    QCOMPARE(spy.count(), 0);
}

void DeepLinksFacadeTest::resetClearsState()
{
    DeepLinksFacade facade;
    facade.setLink(QStringLiteral("https://t.me/durov/42"));

    facade.reset();
    QCOMPARE(facade.link(), QString());
    QCOMPARE(facade.action(), QStringLiteral("none"));
    QCOMPARE(facade.target(), QString());
    QCOMPARE(facade.messageId(), QString());
    QVERIFY(!facade.valid());

    // A whitespace-only link is the same as a reset.
    facade.setLink(QStringLiteral("https://t.me/durov"));
    facade.setLink(QStringLiteral("   "));
    QCOMPARE(facade.link(), QString());
    QVERIFY(!facade.valid());
}

void DeepLinksFacadeTest::resetIsSilentWhenEmpty()
{
    DeepLinksFacade facade;
    QSignalSpy spy(&facade, &DeepLinksFacade::changed);

    facade.reset();
    facade.setLink(QString());
    facade.setLink(QStringLiteral("   "));
    QCOMPARE(spy.count(), 0);

    facade.setLink(QStringLiteral("https://t.me/durov"));
    QCOMPARE(spy.count(), 1);
    facade.reset();
    QCOMPARE(spy.count(), 2);

    facade.reset();
    QCOMPARE(spy.count(), 2);
}

QTEST_GUILESS_MAIN(DeepLinksFacadeTest)

#include "tst_deep_links_facade.moc"
