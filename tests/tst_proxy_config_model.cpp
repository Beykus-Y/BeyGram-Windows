#include "telegram/ProxyConfigModel.h"

#include <QSignalSpy>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

constexpr int MaxPort = 65535;

} // namespace

class ProxyConfigModelTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultsDescribeDirectConnection();
    void setTypeEmitsChangedOnlyForRealChange();
    void setServerTrimsHost();
    void setServerClampsPortIntoRange();
    void setServerTreatsZeroAndNegativePortAsUnset();
    void setServerEmitsChangedOnlyForRealChange();
    void setCredentialsAreStoredVerbatim();
    void setCredentialsEmitsChangedOnlyForRealChange();
    void setEnabledEmitsChangedOnlyForRealChange();
    void validRequiresEnabledTypeHostAndPort();
    void validAcceptsAnonymousProxy();
    void displayTextFormatsEachScheme();
    void displayTextShowsPartialEndpoint();
    void displayTextNeverIncludesCredentials();
    void switchingTypeKeepsEndpoint();
    void resetRestoresDefaults();
    void resetIsSilentAtDefaults();
    void resetEmitsOnceForPartialConfiguration();
};

void ProxyConfigModelTest::defaultsDescribeDirectConnection()
{
    ProxyConfigModel model;

    QCOMPARE(model.type(), ProxyType::None);
    QVERIFY(model.host().isEmpty());
    QCOMPARE(model.port(), 0);
    QVERIFY(model.username().isEmpty());
    QVERIFY(model.password().isEmpty());
    QVERIFY(!model.enabled());
    QVERIFY(!model.valid());
    QCOMPARE(model.displayText(), QStringLiteral("Direct connection"));
}

void ProxyConfigModelTest::setTypeEmitsChangedOnlyForRealChange()
{
    ProxyConfigModel model;
    QSignalSpy spy(&model, &ProxyConfigModel::changed);

    model.setType(ProxyType::Socks5);
    QCOMPARE(model.type(), ProxyType::Socks5);
    QCOMPARE(spy.count(), 1);

    // Writing the value already stored must not repaint the screen.
    model.setType(ProxyType::Socks5);
    QCOMPARE(spy.count(), 1);

    model.setType(ProxyType::Mtproto);
    QCOMPARE(model.type(), ProxyType::Mtproto);
    QCOMPARE(spy.count(), 2);

    model.setType(ProxyType::None);
    QCOMPARE(model.type(), ProxyType::None);
    QCOMPARE(spy.count(), 3);
}

void ProxyConfigModelTest::setServerTrimsHost()
{
    ProxyConfigModel model;

    model.setServer(QStringLiteral("  proxy.example  "), 1080);
    QCOMPARE(model.host(), QStringLiteral("proxy.example"));
    QCOMPARE(model.port(), 1080);

    // A host of only whitespace is the same as no host at all.
    model.setServer(QStringLiteral("   "), 1080);
    QVERIFY(model.host().isEmpty());
    QCOMPARE(model.port(), 1080);
}

void ProxyConfigModelTest::setServerClampsPortIntoRange()
{
    ProxyConfigModel model;

    model.setServer(QStringLiteral("proxy.example"), MaxPort + 1);
    QCOMPARE(model.port(), MaxPort);

    model.setServer(QStringLiteral("proxy.example"), MaxPort);
    QCOMPARE(model.port(), MaxPort);

    model.setServer(QStringLiteral("proxy.example"), 1);
    QCOMPARE(model.port(), 1);
}

void ProxyConfigModelTest::setServerTreatsZeroAndNegativePortAsUnset()
{
    ProxyConfigModel model;

    model.setServer(QStringLiteral("proxy.example"), 1080);
    model.setServer(QStringLiteral("proxy.example"), 0);
    QCOMPARE(model.port(), 0);

    model.setServer(QStringLiteral("proxy.example"), -5);
    QCOMPARE(model.port(), 0);

    // Unset ports are never valid, even with a host and the right type.
    model.setType(ProxyType::Socks5);
    model.setEnabled(true);
    QVERIFY(!model.valid());
}

void ProxyConfigModelTest::setServerEmitsChangedOnlyForRealChange()
{
    ProxyConfigModel model;
    QSignalSpy spy(&model, &ProxyConfigModel::changed);

    model.setServer(QStringLiteral("proxy.example"), 1080);
    QCOMPARE(spy.count(), 1);

    // The same endpoint written with stray whitespace is not a change.
    model.setServer(QStringLiteral(" proxy.example "), 1080);
    QCOMPARE(spy.count(), 1);

    model.setServer(QStringLiteral("proxy.example"), 1081);
    QCOMPARE(spy.count(), 2);

    model.setServer(QStringLiteral("other.example"), 1081);
    QCOMPARE(spy.count(), 3);

    // A port that clamps to the value already stored is silent too.
    model.setServer(QStringLiteral("other.example"), MaxPort + 100);
    QCOMPARE(spy.count(), 4);
    model.setServer(QStringLiteral("other.example"), MaxPort);
    QCOMPARE(spy.count(), 4);
}

void ProxyConfigModelTest::setCredentialsAreStoredVerbatim()
{
    ProxyConfigModel model;

    model.setCredentials(QStringLiteral(" alice "), QStringLiteral(" s3cret "));
    QCOMPARE(model.username(), QStringLiteral(" alice "));
    QCOMPARE(model.password(), QStringLiteral(" s3cret "));
}

void ProxyConfigModelTest::setCredentialsEmitsChangedOnlyForRealChange()
{
    ProxyConfigModel model;
    QSignalSpy spy(&model, &ProxyConfigModel::changed);

    model.setCredentials(QStringLiteral("alice"), QStringLiteral("secret"));
    QCOMPARE(spy.count(), 1);

    model.setCredentials(QStringLiteral("alice"), QStringLiteral("secret"));
    QCOMPARE(spy.count(), 1);

    model.setCredentials(QStringLiteral("alice"), QStringLiteral("other"));
    QCOMPARE(spy.count(), 2);

    model.setCredentials(QStringLiteral("bob"), QStringLiteral("other"));
    QCOMPARE(spy.count(), 3);

    // A trailing space is a different password, not noise to be trimmed.
    model.setCredentials(QStringLiteral("bob"), QStringLiteral("other "));
    QCOMPARE(spy.count(), 4);
}

void ProxyConfigModelTest::setEnabledEmitsChangedOnlyForRealChange()
{
    ProxyConfigModel model;
    QSignalSpy spy(&model, &ProxyConfigModel::changed);

    model.setEnabled(true);
    QVERIFY(model.enabled());
    QCOMPARE(spy.count(), 1);

    model.setEnabled(true);
    QCOMPARE(spy.count(), 1);

    model.setEnabled(false);
    QVERIFY(!model.enabled());
    QCOMPARE(spy.count(), 2);
}

void ProxyConfigModelTest::validRequiresEnabledTypeHostAndPort()
{
    ProxyConfigModel model;
    model.setType(ProxyType::Socks5);
    model.setServer(QStringLiteral("proxy.example"), 1080);

    // Configured but switched off: not used, therefore not valid.
    QVERIFY(!model.valid());

    model.setEnabled(true);
    QVERIFY(model.valid());

    // A host without a port is not a usable endpoint.
    model.setServer(QStringLiteral("proxy.example"), 0);
    QVERIFY(!model.valid());

    model.setServer(QStringLiteral("proxy.example"), 1080);
    QVERIFY(model.valid());

    // A port without a host is not one either.
    model.setServer(QString(), 1080);
    QVERIFY(!model.valid());

    model.setServer(QStringLiteral("proxy.example"), 1080);
    QVERIFY(model.valid());

    // Type None means a direct connection, whatever the endpoint says.
    model.setType(ProxyType::None);
    QVERIFY(!model.valid());
}

void ProxyConfigModelTest::validAcceptsAnonymousProxy()
{
    ProxyConfigModel model;
    model.setType(ProxyType::Http);
    model.setServer(QStringLiteral("proxy.example"), 8080);
    model.setEnabled(true);

    // Credentials are optional: an anonymous proxy is still valid.
    QVERIFY(model.username().isEmpty());
    QVERIFY(model.password().isEmpty());
    QVERIFY(model.valid());
}

void ProxyConfigModelTest::displayTextFormatsEachScheme()
{
    ProxyConfigModel model;
    model.setServer(QStringLiteral("proxy.example"), 1080);

    model.setType(ProxyType::Socks5);
    QCOMPARE(model.displayText(), QStringLiteral("socks5://proxy.example:1080"));

    model.setType(ProxyType::Http);
    QCOMPARE(model.displayText(), QStringLiteral("http://proxy.example:1080"));

    model.setType(ProxyType::Mtproto);
    QCOMPARE(model.displayText(), QStringLiteral("mtproto://proxy.example:1080"));

    model.setType(ProxyType::None);
    QCOMPARE(model.displayText(), QStringLiteral("Direct connection"));
}

void ProxyConfigModelTest::displayTextShowsPartialEndpoint()
{
    ProxyConfigModel model;
    model.setType(ProxyType::Socks5);

    // Nothing typed yet: show the scheme rather than a bogus "host:0".
    QCOMPARE(model.displayText(), QStringLiteral("socks5://"));

    model.setServer(QStringLiteral("proxy.example"), 0);
    QCOMPARE(model.displayText(), QStringLiteral("socks5://proxy.example"));

    model.setServer(QStringLiteral("proxy.example"), 1080);
    QCOMPARE(model.displayText(), QStringLiteral("socks5://proxy.example:1080"));
}

void ProxyConfigModelTest::displayTextNeverIncludesCredentials()
{
    ProxyConfigModel model;
    model.setType(ProxyType::Mtproto);
    model.setServer(QStringLiteral("proxy.example"), 443);
    model.setCredentials(QStringLiteral("alice"), QStringLiteral("secret"));

    const QString text = model.displayText();
    QCOMPARE(text, QStringLiteral("mtproto://proxy.example:443"));
    QVERIFY(!text.contains(QStringLiteral("alice")));
    QVERIFY(!text.contains(QStringLiteral("secret")));
}

void ProxyConfigModelTest::switchingTypeKeepsEndpoint()
{
    ProxyConfigModel model;
    model.setType(ProxyType::Socks5);
    model.setServer(QStringLiteral("proxy.example"), 1080);
    model.setCredentials(QStringLiteral("alice"), QStringLiteral("secret"));

    model.setType(ProxyType::None);
    QCOMPARE(model.host(), QStringLiteral("proxy.example"));
    QCOMPARE(model.port(), 1080);
    QCOMPARE(model.username(), QStringLiteral("alice"));
    QCOMPARE(model.password(), QStringLiteral("secret"));

    model.setType(ProxyType::Http);
    QCOMPARE(model.host(), QStringLiteral("proxy.example"));
    QCOMPARE(model.port(), 1080);
    QCOMPARE(model.username(), QStringLiteral("alice"));

    // Switching the switch off must not forget the endpoint either.
    model.setEnabled(false);
    QCOMPARE(model.host(), QStringLiteral("proxy.example"));
    QCOMPARE(model.port(), 1080);
}

void ProxyConfigModelTest::resetRestoresDefaults()
{
    ProxyConfigModel model;
    model.setType(ProxyType::Mtproto);
    model.setServer(QStringLiteral("proxy.example"), 443);
    model.setCredentials(QStringLiteral("alice"), QStringLiteral("secret"));
    model.setEnabled(true);
    QVERIFY(model.valid());

    QSignalSpy spy(&model, &ProxyConfigModel::changed);
    model.reset();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.type(), ProxyType::None);
    QVERIFY(model.host().isEmpty());
    QCOMPARE(model.port(), 0);
    QVERIFY(model.username().isEmpty());
    QVERIFY(model.password().isEmpty());
    QVERIFY(!model.enabled());
    QVERIFY(!model.valid());
    QCOMPARE(model.displayText(), QStringLiteral("Direct connection"));

    // A reset model behaves like a freshly constructed one.
    model.setType(ProxyType::Socks5);
    model.setServer(QStringLiteral("other.example"), 1080);
    model.setEnabled(true);
    QVERIFY(model.valid());
}

void ProxyConfigModelTest::resetIsSilentAtDefaults()
{
    ProxyConfigModel model;
    QSignalSpy spy(&model, &ProxyConfigModel::changed);

    model.reset();
    QCOMPARE(spy.count(), 0);

    // Back at defaults after a round trip: reset is silent again.
    model.setType(ProxyType::Socks5);
    model.reset();
    QCOMPARE(spy.count(), 2);
    model.reset();
    QCOMPARE(spy.count(), 2);
}

void ProxyConfigModelTest::resetEmitsOnceForPartialConfiguration()
{
    ProxyConfigModel model;
    QSignalSpy spy(&model, &ProxyConfigModel::changed);

    // A host alone is enough to be "not at defaults".
    model.setServer(QStringLiteral("proxy.example"), 0);
    QCOMPARE(spy.count(), 1);

    model.reset();
    QCOMPARE(spy.count(), 2);
    QVERIFY(model.host().isEmpty());
    QCOMPARE(model.port(), 0);

    // Credentials alone are "not at defaults" as well.
    model.setCredentials(QStringLiteral("alice"), QStringLiteral("secret"));
    QCOMPARE(spy.count(), 3);
    model.reset();
    QCOMPARE(spy.count(), 4);
    QVERIFY(model.username().isEmpty());
}

QTEST_GUILESS_MAIN(ProxyConfigModelTest)
#include "tst_proxy_config_model.moc"
