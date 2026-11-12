#include "ui/NetworkFacade.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QtTest>

using namespace beygram::ui;

class NetworkFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsOfflineWithDirectConnection();
    void setConnectionStateAcceptsPlainNames();
    void setConnectionStateAcceptsTdlibNames();
    void setConnectionStateTreatsEmptyAsUnknown();
    void setConnectionStateIgnoresUnknownNames();
    void setConnectionStateEmitsOnlyOnRealChange();
    void retryCountdownTicksDown();
    void retryIsDroppedOnceOnline();
    void proxyFieldsRoundTrip();
    void proxyHostIsTrimmed();
    void proxyPortIsClamped();
    void proxyTypeAcceptsAliases();
    void proxyTypeIgnoresUnknownNames();
    void proxyValidRequiresEnabledTypeHostAndPort();
    void proxyTextShowsEndpointWithoutCredentials();
    void applyProxyEmitsPayload();
    void applyProxyIsSilentWhileInvalid();
    void passwordIsWriteOnly();
    void changedIsForwardedFromBothControllers();
};

void NetworkFacadeTest::startsOfflineWithDirectConnection()
{
    const NetworkFacade facade;

    QCOMPARE(facade.stateText(), QStringLiteral("Offline"));
    QVERIFY(!facade.online());
    QCOMPARE(facade.retryAfter(), 0);

    QVERIFY(!facade.proxyEnabled());
    QCOMPARE(facade.proxyType(), QStringLiteral("none"));
    QVERIFY(facade.proxyHost().isEmpty());
    QCOMPARE(facade.proxyPort(), 0);
    QVERIFY(facade.proxyUser().isEmpty());
    QVERIFY(!facade.proxyValid());
    QCOMPARE(facade.proxyText(), QStringLiteral("Direct connection"));
}

void NetworkFacadeTest::setConnectionStateAcceptsPlainNames()
{
    NetworkFacade facade;

    facade.setConnectionState(QStringLiteral("waitingForNetwork"));
    QCOMPARE(facade.stateText(), QStringLiteral("Waiting for network"));
    QVERIFY(!facade.online());

    facade.setConnectionState(QStringLiteral("connecting"));
    QCOMPARE(facade.stateText(), QStringLiteral("Connecting…"));
    QVERIFY(!facade.online());

    facade.setConnectionState(QStringLiteral("updating"));
    QCOMPARE(facade.stateText(), QStringLiteral("Updating…"));
    QVERIFY(!facade.online());

    facade.setConnectionState(QStringLiteral("ready"));
    QCOMPARE(facade.stateText(), QStringLiteral("Connected"));
    QVERIFY(facade.online());

    facade.setConnectionState(QStringLiteral("offline"));
    QCOMPARE(facade.stateText(), QStringLiteral("Offline"));
    QVERIFY(!facade.online());
}

void NetworkFacadeTest::setConnectionStateAcceptsTdlibNames()
{
    NetworkFacade facade;

    // TDLib's updateConnectionState spells the states with a prefix.
    facade.setConnectionState(QStringLiteral("connectionStateWaitingForNetwork"));
    QCOMPARE(facade.stateText(), QStringLiteral("Waiting for network"));

    facade.setConnectionState(QStringLiteral("connectionStateConnecting"));
    QCOMPARE(facade.stateText(), QStringLiteral("Connecting…"));

    facade.setConnectionState(QStringLiteral("connectionStateUpdating"));
    QCOMPARE(facade.stateText(), QStringLiteral("Updating…"));

    facade.setConnectionState(QStringLiteral("connectionStateReady"));
    QCOMPARE(facade.stateText(), QStringLiteral("Connected"));
    QVERIFY(facade.online());

    // Case and separators do not matter either.
    facade.setConnectionState(QStringLiteral("  WAITING_FOR_NETWORK  "));
    QCOMPARE(facade.stateText(), QStringLiteral("Waiting for network"));
    QVERIFY(!facade.online());
}

void NetworkFacadeTest::setConnectionStateTreatsEmptyAsUnknown()
{
    NetworkFacade facade;
    facade.setConnectionState(QStringLiteral("ready"));
    QVERIFY(facade.online());

    facade.setConnectionState(QString());
    QCOMPARE(facade.stateText(), QStringLiteral("Offline"));
    QVERIFY(!facade.online());
}

void NetworkFacadeTest::setConnectionStateIgnoresUnknownNames()
{
    NetworkFacade facade;
    facade.setConnectionState(QStringLiteral("ready"));

    QSignalSpy spy(&facade, &NetworkFacade::changed);

    const QStringList garbage = {
        QStringLiteral("bogus"),
        QStringLiteral("connectionStateBogus"),
        QStringLiteral("online"),
        QStringLiteral("ready-ish"),
    };

    for (const QString& name : garbage) {
        facade.setConnectionState(name);
        QCOMPARE(facade.stateText(), QStringLiteral("Connected"));
        QVERIFY(facade.online());
    }

    // Nothing was stored, so nothing was announced.
    QCOMPARE(spy.count(), 0);
}

void NetworkFacadeTest::setConnectionStateEmitsOnlyOnRealChange()
{
    NetworkFacade facade;
    QSignalSpy spy(&facade, &NetworkFacade::changed);

    facade.setConnectionState(QStringLiteral("unknown")); // already Unknown
    QCOMPARE(spy.count(), 0);

    facade.setConnectionState(QStringLiteral("connecting"));
    QCOMPARE(spy.count(), 1);

    facade.setConnectionState(QStringLiteral("connecting"));
    QCOMPARE(spy.count(), 1);

    facade.setConnectionState(QStringLiteral("ready"));
    QCOMPARE(spy.count(), 2);

    facade.setConnectionState(QStringLiteral("connected"));
    QCOMPARE(spy.count(), 2);
}

void NetworkFacadeTest::retryCountdownTicksDown()
{
    NetworkFacade facade;
    facade.setConnectionState(QStringLiteral("waitingForNetwork"));
    facade.setRetryAfter(3);
    QCOMPARE(facade.retryAfter(), 3);

    QSignalSpy spy(&facade, &NetworkFacade::changed);

    facade.tick();
    QCOMPARE(facade.retryAfter(), 2);
    QCOMPARE(spy.count(), 1);

    facade.tick();
    QCOMPARE(facade.retryAfter(), 1);
    QCOMPARE(spy.count(), 2);

    facade.tick();
    QCOMPARE(facade.retryAfter(), 0);
    QCOMPARE(spy.count(), 3);

    // The countdown stops at zero and stays silent instead of wrapping around.
    facade.tick();
    facade.tick();
    QCOMPARE(facade.retryAfter(), 0);
    QCOMPARE(spy.count(), 3);
    QCOMPARE(facade.stateText(), QStringLiteral("Waiting for network"));
}

void NetworkFacadeTest::retryIsDroppedOnceOnline()
{
    NetworkFacade facade;
    facade.setConnectionState(QStringLiteral("waitingForNetwork"));
    facade.setRetryAfter(30);
    QCOMPARE(facade.retryAfter(), 30);

    facade.setConnectionState(QStringLiteral("ready"));
    QCOMPARE(facade.retryAfter(), 0);
    QVERIFY(facade.online());

    // A delay that arrives while online is ignored, not stored for later.
    facade.setRetryAfter(45);
    QCOMPARE(facade.retryAfter(), 0);

    facade.setConnectionState(QStringLiteral("waitingForNetwork"));
    QCOMPARE(facade.retryAfter(), 0);
}

void NetworkFacadeTest::proxyFieldsRoundTrip()
{
    NetworkFacade facade;

    facade.setProxyEnabled(true);
    facade.setProxyType(QStringLiteral("socks5"));
    facade.setProxyHost(QStringLiteral("proxy.example"));
    facade.setProxyPort(1080);
    facade.setProxyUser(QStringLiteral("alice"));

    QVERIFY(facade.proxyEnabled());
    QCOMPARE(facade.proxyType(), QStringLiteral("socks5"));
    QCOMPARE(facade.proxyHost(), QStringLiteral("proxy.example"));
    QCOMPARE(facade.proxyPort(), 1080);
    QCOMPARE(facade.proxyUser(), QStringLiteral("alice"));
    QVERIFY(facade.proxyValid());
    QCOMPARE(facade.proxyText(), QStringLiteral("socks5://proxy.example:1080"));

    // Switching the endpoint off keeps the fields but makes the form unused.
    facade.setProxyEnabled(false);
    QVERIFY(!facade.proxyEnabled());
    QVERIFY(!facade.proxyValid());
    QCOMPARE(facade.proxyHost(), QStringLiteral("proxy.example"));
    QCOMPARE(facade.proxyPort(), 1080);
}

void NetworkFacadeTest::proxyHostIsTrimmed()
{
    NetworkFacade facade;

    facade.setProxyHost(QStringLiteral("  proxy.example  "));
    QCOMPARE(facade.proxyHost(), QStringLiteral("proxy.example"));

    // A host of only whitespace is the same as no host at all.
    facade.setProxyHost(QStringLiteral("   "));
    QVERIFY(facade.proxyHost().isEmpty());

    // Editing the host must not disturb the port, and vice versa.
    facade.setProxyPort(8080);
    facade.setProxyHost(QStringLiteral("other.example"));
    QCOMPARE(facade.proxyPort(), 8080);
}

void NetworkFacadeTest::proxyPortIsClamped()
{
    NetworkFacade facade;
    facade.setProxyHost(QStringLiteral("proxy.example"));

    facade.setProxyPort(-1);
    QCOMPARE(facade.proxyPort(), 0);

    facade.setProxyPort(0);
    QCOMPARE(facade.proxyPort(), 0);

    facade.setProxyPort(1);
    QCOMPARE(facade.proxyPort(), 1);

    facade.setProxyPort(65535);
    QCOMPARE(facade.proxyPort(), 65535);

    facade.setProxyPort(70000);
    QCOMPARE(facade.proxyPort(), 65535);

    // The clamp happened in the model, so the host is still there.
    QCOMPARE(facade.proxyHost(), QStringLiteral("proxy.example"));
}

void NetworkFacadeTest::proxyTypeAcceptsAliases()
{
    NetworkFacade facade;

    facade.setProxyType(QStringLiteral("SOCKS5"));
    QCOMPARE(facade.proxyType(), QStringLiteral("socks5"));

    facade.setProxyType(QStringLiteral("http"));
    QCOMPARE(facade.proxyType(), QStringLiteral("http"));

    facade.setProxyType(QStringLiteral("MTProto"));
    QCOMPARE(facade.proxyType(), QStringLiteral("mtproto"));

    facade.setProxyType(QStringLiteral("direct"));
    QCOMPARE(facade.proxyType(), QStringLiteral("none"));

    facade.setProxyType(QStringLiteral("off"));
    QCOMPARE(facade.proxyType(), QStringLiteral("none"));
}

void NetworkFacadeTest::proxyTypeIgnoresUnknownNames()
{
    NetworkFacade facade;
    facade.setProxyType(QStringLiteral("http"));

    QSignalSpy spy(&facade, &NetworkFacade::changed);

    facade.setProxyType(QStringLiteral("socks4"));
    facade.setProxyType(QString());
    facade.setProxyType(QStringLiteral("wireguard"));

    QCOMPARE(facade.proxyType(), QStringLiteral("http"));
    QCOMPARE(spy.count(), 0);
}

void NetworkFacadeTest::proxyValidRequiresEnabledTypeHostAndPort()
{
    NetworkFacade facade;
    QVERIFY(!facade.proxyValid());

    // Enabled but still a direct connection.
    facade.setProxyEnabled(true);
    QVERIFY(!facade.proxyValid());

    // A type and a host, but no port yet.
    facade.setProxyType(QStringLiteral("http"));
    facade.setProxyHost(QStringLiteral("proxy.example"));
    QVERIFY(!facade.proxyValid());

    facade.setProxyPort(8080);
    QVERIFY(facade.proxyValid());

    // Credentials are optional: an anonymous proxy is valid.
    QVERIFY(facade.proxyUser().isEmpty());

    // A type of none disables the proxy even with a full endpoint.
    facade.setProxyType(QStringLiteral("none"));
    QVERIFY(!facade.proxyValid());

    facade.setProxyType(QStringLiteral("socks5"));
    QVERIFY(facade.proxyValid());

    facade.setProxyEnabled(false);
    QVERIFY(!facade.proxyValid());
}

void NetworkFacadeTest::proxyTextShowsEndpointWithoutCredentials()
{
    NetworkFacade facade;
    QCOMPARE(facade.proxyText(), QStringLiteral("Direct connection"));

    facade.setProxyType(QStringLiteral("socks5"));
    QCOMPARE(facade.proxyText(), QStringLiteral("socks5://"));

    facade.setProxyHost(QStringLiteral("proxy.example"));
    QCOMPARE(facade.proxyText(), QStringLiteral("socks5://proxy.example"));

    facade.setProxyPort(1080);
    QCOMPARE(facade.proxyText(), QStringLiteral("socks5://proxy.example:1080"));

    // The credentials are never part of the text.
    facade.setProxyUser(QStringLiteral("alice"));
    facade.setProxyPassword(QStringLiteral("s3cret"));
    QCOMPARE(facade.proxyText(), QStringLiteral("socks5://proxy.example:1080"));
    QVERIFY(!facade.proxyText().contains(QStringLiteral("alice")));
    QVERIFY(!facade.proxyText().contains(QStringLiteral("s3cret")));
}

void NetworkFacadeTest::applyProxyEmitsPayload()
{
    NetworkFacade facade;
    facade.setProxyEnabled(true);
    facade.setProxyType(QStringLiteral("socks5"));
    facade.setProxyHost(QStringLiteral("  proxy.example  "));
    facade.setProxyPort(1080);
    facade.setProxyUser(QStringLiteral("alice"));
    facade.setProxyPassword(QStringLiteral("s3 cret"));

    QSignalSpy spy(&facade, &NetworkFacade::proxyApplyRequested);
    facade.applyProxy();

    QCOMPARE(spy.count(), 1);

    const QList<QVariant> args = spy.at(0);
    QCOMPARE(args.size(), 5);
    QCOMPARE(args.at(0).toString(), QStringLiteral("socks5"));
    QCOMPARE(args.at(1).toString(), QStringLiteral("proxy.example"));
    QCOMPARE(args.at(2).toInt(), 1080);
    QCOMPARE(args.at(3).toString(), QStringLiteral("alice"));
    QCOMPARE(args.at(4).toString(), QStringLiteral("s3 cret"));

    // Applying does not clear the form.
    QVERIFY(facade.proxyValid());
    QCOMPARE(facade.proxyHost(), QStringLiteral("proxy.example"));
}

void NetworkFacadeTest::applyProxyIsSilentWhileInvalid()
{
    NetworkFacade facade;
    QSignalSpy spy(&facade, &NetworkFacade::proxyApplyRequested);

    // A fresh facade is a disabled direct connection.
    facade.applyProxy();
    QCOMPARE(spy.count(), 0);

    facade.setProxyEnabled(true);
    facade.applyProxy();
    QCOMPARE(spy.count(), 0);

    // Type and host but no port.
    facade.setProxyType(QStringLiteral("mtproto"));
    facade.setProxyHost(QStringLiteral("proxy.example"));
    facade.applyProxy();
    QCOMPARE(spy.count(), 0);

    // Port and host but the switch was turned off again.
    facade.setProxyPort(443);
    facade.setProxyEnabled(false);
    facade.applyProxy();
    QCOMPARE(spy.count(), 0);

    facade.setProxyEnabled(true);
    facade.applyProxy();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("mtproto"));
}

void NetworkFacadeTest::passwordIsWriteOnly()
{
    NetworkFacade facade;
    facade.setProxyEnabled(true);
    facade.setProxyType(QStringLiteral("socks5"));
    facade.setProxyHost(QStringLiteral("proxy.example"));
    facade.setProxyPort(1080);

    facade.setProxyPassword(QStringLiteral("  spaced secret  "));

    // Editing the user must not wipe the password: it is carried through the
    // same model call. Stored verbatim, spaces included.
    facade.setProxyUser(QStringLiteral("alice"));

    QSignalSpy spy(&facade, &NetworkFacade::proxyApplyRequested);
    facade.applyProxy();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(3).toString(), QStringLiteral("alice"));
    QCOMPARE(spy.at(0).at(4).toString(), QStringLiteral("  spaced secret  "));

    // And the secret never shows up in the display text.
    QVERIFY(!facade.proxyText().contains(QStringLiteral("spaced")));
}

void NetworkFacadeTest::changedIsForwardedFromBothControllers()
{
    NetworkFacade facade;
    QSignalSpy spy(&facade, &NetworkFacade::changed);

    facade.setConnectionState(QStringLiteral("connecting"));
    QCOMPARE(spy.count(), 1);

    facade.setProxyEnabled(true);
    QCOMPARE(spy.count(), 2);

    facade.setProxyType(QStringLiteral("http"));
    QCOMPARE(spy.count(), 3);

    facade.setProxyHost(QStringLiteral("proxy.example"));
    QCOMPARE(spy.count(), 4);

    facade.setProxyPort(8080);
    QCOMPARE(spy.count(), 5);

    facade.setProxyUser(QStringLiteral("alice"));
    QCOMPARE(spy.count(), 6);

    facade.setProxyPassword(QStringLiteral("secret"));
    QCOMPARE(spy.count(), 7);

    // Redundant writes stay silent on both sides.
    facade.setProxyHost(QStringLiteral("proxy.example"));
    facade.setProxyPort(8080);
    facade.setConnectionState(QStringLiteral("connecting"));
    QCOMPARE(spy.count(), 7);

    facade.setRetryAfter(10);
    QCOMPARE(spy.count(), 8);

    facade.tick();
    QCOMPARE(spy.count(), 9);
}

QTEST_GUILESS_MAIN(NetworkFacadeTest)
#include "tst_network_facade.moc"
