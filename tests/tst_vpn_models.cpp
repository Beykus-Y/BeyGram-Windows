#include "telegram/VpnModels.h"

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

QJsonObject serverJson(const QString& id, const QString& host, int port)
{
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), QStringLiteral("Node ") + id},
        {QStringLiteral("country"), QStringLiteral("DE")},
        {QStringLiteral("host"), host},
        {QStringLiteral("port"), port},
        {QStringLiteral("protocol"), QStringLiteral("vless")},
    };
}

VpnServer usableServer(const QString& id, const QString& host, int port)
{
    VpnServer server;
    server.id = id;
    server.name = QStringLiteral("Node ") + id;
    server.country = QStringLiteral("DE");
    server.host = host;
    server.port = port;
    server.protocol = QStringLiteral("vless");
    return server;
}

} // namespace

class VpnModelsTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesCompleteConfig();
    void defaultsForEmptyObject();
    void toleratesWrongTypes();
    void trimsConfigIdentity();
    void trimsServerIdAndHost();
    void dropsServersWithoutIdOrHost();
    void dropsServersWithUnusablePort();
    void keepsBoundaryPorts();
    void ignoresNonObjectServerEntries();
    void toJsonWritesStableShape();
    void toJsonDropsUnusableServers();
    void roundTripThroughSerializedJsonIsStable();
    void validRequiresIdAndTitle();
    void validAllowsDisabledConfigWithoutServers();
    void validRequiresUsableServerWhenEnabled();
    void validAcceptsEnabledConfig();
};

void VpnModelsTest::parsesCompleteConfig()
{
    const QJsonObject json = parseObject(R"({
        "id": "vpn-main",
        "title": "Main VPN",
        "enabled": true,
        "servers": [
            {"id": "de-1", "name": "Frankfurt", "country": "DE",
             "host": "de.example.com", "port": 443, "protocol": "vless"},
            {"id": "nl-1", "name": "Amsterdam", "country": "NL",
             "host": "nl.example.com", "port": 8443, "protocol": "trojan"}
        ]
    })");

    const VpnConfig config = vpnConfigFromJson(json);

    QCOMPARE(config.id, QStringLiteral("vpn-main"));
    QCOMPARE(config.title, QStringLiteral("Main VPN"));
    QVERIFY(config.enabled);
    QCOMPARE(config.servers.size(), 2);

    const VpnServer& first = config.servers.at(0);
    QCOMPARE(first.id, QStringLiteral("de-1"));
    QCOMPARE(first.name, QStringLiteral("Frankfurt"));
    QCOMPARE(first.country, QStringLiteral("DE"));
    QCOMPARE(first.host, QStringLiteral("de.example.com"));
    QCOMPARE(first.port, 443);
    QCOMPARE(first.protocol, QStringLiteral("vless"));

    const VpnServer& second = config.servers.at(1);
    QCOMPARE(second.id, QStringLiteral("nl-1"));
    QCOMPARE(second.port, 8443);
    QCOMPARE(second.protocol, QStringLiteral("trojan"));

    QVERIFY(vpnConfigValid(config));
}

void VpnModelsTest::defaultsForEmptyObject()
{
    const VpnConfig config = vpnConfigFromJson(QJsonObject{});

    QVERIFY(config.id.isEmpty());
    QVERIFY(config.title.isEmpty());
    QVERIFY(config.servers.isEmpty());
    QVERIFY(!config.enabled);
    QVERIFY(!vpnConfigValid(config));
}

void VpnModelsTest::toleratesWrongTypes()
{
    const VpnConfig config = vpnConfigFromJson(parseObject(R"({
        "id": 42,
        "title": true,
        "enabled": "yes",
        "servers": {"id": "not-an-array"}
    })"));

    QVERIFY(config.id.isEmpty());
    QVERIFY(config.title.isEmpty());
    QVERIFY(config.servers.isEmpty());
    QVERIFY(!config.enabled);
}

void VpnModelsTest::trimsConfigIdentity()
{
    const VpnConfig config = vpnConfigFromJson(parseObject(R"({
        "id": "  vpn-main  ",
        "title": "  Main VPN  ",
        "servers": []
    })"));

    QCOMPARE(config.id, QStringLiteral("vpn-main"));
    QCOMPARE(config.title, QStringLiteral("Main VPN"));
}

void VpnModelsTest::trimsServerIdAndHost()
{
    const VpnConfig config = vpnConfigFromJson(parseObject(R"({
        "id": "vpn-main",
        "title": "Main VPN",
        "servers": [
            {"id": "  de-1  ", "name": "Frankfurt", "country": "DE",
             "host": "  de.example.com  ", "port": 443, "protocol": "vless"}
        ]
    })"));

    QCOMPARE(config.servers.size(), 1);
    QCOMPARE(config.servers.at(0).id, QStringLiteral("de-1"));
    QCOMPARE(config.servers.at(0).host, QStringLiteral("de.example.com"));
}

void VpnModelsTest::dropsServersWithoutIdOrHost()
{
    QJsonArray servers;
    servers.append(serverJson(QString(), QStringLiteral("no-id.example"), 443));
    servers.append(serverJson(QStringLiteral("   "), QStringLiteral("blank-id.example"), 443));
    servers.append(serverJson(QStringLiteral("no-host"), QString(), 443));
    servers.append(serverJson(QStringLiteral("blank-host"), QStringLiteral("   "), 443));
    servers.append(serverJson(QStringLiteral("good"), QStringLiteral("good.example"), 443));

    QJsonObject json;
    json.insert(QStringLiteral("id"), QStringLiteral("vpn-main"));
    json.insert(QStringLiteral("title"), QStringLiteral("Main VPN"));
    json.insert(QStringLiteral("servers"), servers);

    const VpnConfig config = vpnConfigFromJson(json);
    QCOMPARE(config.servers.size(), 1);
    QCOMPARE(config.servers.at(0).id, QStringLiteral("good"));
}

void VpnModelsTest::dropsServersWithUnusablePort()
{
    QJsonArray servers;
    servers.append(serverJson(QStringLiteral("zero"), QStringLiteral("zero.example"), 0));
    servers.append(serverJson(QStringLiteral("negative"), QStringLiteral("negative.example"), -1));
    servers.append(serverJson(QStringLiteral("too-big"), QStringLiteral("big.example"), 65536));
    servers.append(QJsonObject{
        {QStringLiteral("id"), QStringLiteral("missing")},
        {QStringLiteral("host"), QStringLiteral("missing.example")},
    });
    servers.append(serverJson(QStringLiteral("good"), QStringLiteral("good.example"), 443));

    QJsonObject json;
    json.insert(QStringLiteral("id"), QStringLiteral("vpn-main"));
    json.insert(QStringLiteral("title"), QStringLiteral("Main VPN"));
    json.insert(QStringLiteral("servers"), servers);

    const VpnConfig config = vpnConfigFromJson(json);
    QCOMPARE(config.servers.size(), 1);
    QCOMPARE(config.servers.at(0).id, QStringLiteral("good"));
}

void VpnModelsTest::keepsBoundaryPorts()
{
    QJsonArray servers;
    servers.append(serverJson(QStringLiteral("low"), QStringLiteral("low.example"), 1));
    servers.append(serverJson(QStringLiteral("high"), QStringLiteral("high.example"), 65535));

    QJsonObject json;
    json.insert(QStringLiteral("id"), QStringLiteral("vpn-main"));
    json.insert(QStringLiteral("title"), QStringLiteral("Main VPN"));
    json.insert(QStringLiteral("servers"), servers);

    const VpnConfig config = vpnConfigFromJson(json);
    QCOMPARE(config.servers.size(), 2);
    QCOMPARE(config.servers.at(0).port, 1);
    QCOMPARE(config.servers.at(1).port, 65535);
}

void VpnModelsTest::ignoresNonObjectServerEntries()
{
    const VpnConfig config = vpnConfigFromJson(parseObject(R"({
        "id": "vpn-main",
        "title": "Main VPN",
        "servers": [
            "not a server",
            7,
            null,
            {"id": "good", "host": "good.example", "port": 443}
        ]
    })"));

    QCOMPARE(config.servers.size(), 1);
    QCOMPARE(config.servers.at(0).id, QStringLiteral("good"));
}

void VpnModelsTest::toJsonWritesStableShape()
{
    VpnConfig config;
    config.id = QStringLiteral("  vpn-main  ");
    config.title = QStringLiteral("  Main VPN  ");
    config.enabled = true;
    VpnServer server = usableServer(QStringLiteral("  de-1  "),
                                    QStringLiteral("  de.example.com  "), 443);
    server.name = QStringLiteral("Frankfurt");
    config.servers.append(server);

    const QJsonObject json = vpnConfigToJson(config);

    QVERIFY(json.contains(QStringLiteral("id")));
    QVERIFY(json.contains(QStringLiteral("title")));
    QVERIFY(json.contains(QStringLiteral("servers")));
    QVERIFY(json.contains(QStringLiteral("enabled")));

    QCOMPARE(json.value(QStringLiteral("id")).toString(), QStringLiteral("vpn-main"));
    QCOMPARE(json.value(QStringLiteral("title")).toString(), QStringLiteral("Main VPN"));
    QCOMPARE(json.value(QStringLiteral("enabled")).toBool(), true);

    const QJsonArray servers = json.value(QStringLiteral("servers")).toArray();
    QCOMPARE(servers.size(), 1);
    const QJsonObject serverObject = servers.at(0).toObject();
    QCOMPARE(serverObject.value(QStringLiteral("id")).toString(), QStringLiteral("de-1"));
    QCOMPARE(serverObject.value(QStringLiteral("host")).toString(),
             QStringLiteral("de.example.com"));
    QCOMPARE(serverObject.value(QStringLiteral("port")).toInt(), 443);
    QCOMPARE(serverObject.value(QStringLiteral("name")).toString(),
             QStringLiteral("Frankfurt"));
    QCOMPARE(serverObject.value(QStringLiteral("country")).toString(), QStringLiteral("DE"));
    QCOMPARE(serverObject.value(QStringLiteral("protocol")).toString(),
             QStringLiteral("vless"));
}

void VpnModelsTest::toJsonDropsUnusableServers()
{
    VpnConfig config;
    config.id = QStringLiteral("vpn-main");
    config.title = QStringLiteral("Main VPN");
    config.servers.append(usableServer(QString(), QStringLiteral("no-id.example"), 443));
    config.servers.append(usableServer(QStringLiteral("no-host"), QString(), 443));
    config.servers.append(
        usableServer(QStringLiteral("bad-port"), QStringLiteral("bad.example"), 0));
    config.servers.append(
        usableServer(QStringLiteral("good"), QStringLiteral("good.example"), 443));

    const QJsonObject json = vpnConfigToJson(config);
    const QJsonArray servers = json.value(QStringLiteral("servers")).toArray();
    QCOMPARE(servers.size(), 1);
    QCOMPARE(servers.at(0).toObject().value(QStringLiteral("id")).toString(),
             QStringLiteral("good"));
}

void VpnModelsTest::roundTripThroughSerializedJsonIsStable()
{
    const QJsonObject original = parseObject(R"({
        "id": "  vpn-main  ",
        "title": "  Main VPN  ",
        "enabled": true,
        "servers": [
            {"id": "  de-1  ", "name": "Frankfurt", "country": "DE",
             "host": "  de.example.com  ", "port": 443, "protocol": "vless"},
            {"id": "", "name": "Broken", "country": "DE",
             "host": "broken.example", "port": 443, "protocol": "vless"},
            {"id": "bad-port", "name": "Broken", "country": "DE",
             "host": "bad.example", "port": 70000, "protocol": "vless"}
        ]
    })");

    const VpnConfig parsed = vpnConfigFromJson(original);
    const QJsonObject canonical = vpnConfigToJson(parsed);
    QCOMPARE(canonical.value(QStringLiteral("id")).toString(), QStringLiteral("vpn-main"));
    QCOMPARE(canonical.value(QStringLiteral("title")).toString(), QStringLiteral("Main VPN"));
    QCOMPARE(canonical.value(QStringLiteral("servers")).toArray().size(), 1);

    const QByteArray text = QJsonDocument(canonical).toJson(QJsonDocument::Compact);
    const QJsonObject reparsedJson = QJsonDocument::fromJson(text).object();
    const VpnConfig reparsed = vpnConfigFromJson(reparsedJson);
    const QJsonObject second = vpnConfigToJson(reparsed);

    QCOMPARE(second, canonical);
    QCOMPARE(QJsonDocument(second).toJson(QJsonDocument::Compact), text);
    QCOMPARE(reparsed.id, parsed.id);
    QCOMPARE(reparsed.title, parsed.title);
    QCOMPARE(reparsed.enabled, parsed.enabled);
    QCOMPARE(reparsed.servers.size(), parsed.servers.size());
    QCOMPARE(reparsed.servers.at(0).id, parsed.servers.at(0).id);
    QCOMPARE(reparsed.servers.at(0).host, parsed.servers.at(0).host);
    QCOMPARE(reparsed.servers.at(0).port, parsed.servers.at(0).port);
}

void VpnModelsTest::validRequiresIdAndTitle()
{
    VpnConfig config;
    QVERIFY(!vpnConfigValid(config));

    config.id = QStringLiteral("vpn-main");
    QVERIFY(!vpnConfigValid(config));

    config.title = QStringLiteral("Main VPN");
    QVERIFY(vpnConfigValid(config));

    config.id = QStringLiteral("   ");
    QVERIFY(!vpnConfigValid(config));

    config.id = QStringLiteral("vpn-main");
    config.title = QStringLiteral("   ");
    QVERIFY(!vpnConfigValid(config));
}

void VpnModelsTest::validAllowsDisabledConfigWithoutServers()
{
    VpnConfig config;
    config.id = QStringLiteral("vpn-main");
    config.title = QStringLiteral("Main VPN");
    config.enabled = false;

    QVERIFY(config.servers.isEmpty());
    QVERIFY(vpnConfigValid(config));

    config.enabled = true;
    QVERIFY(!vpnConfigValid(config));
}

void VpnModelsTest::validRequiresUsableServerWhenEnabled()
{
    VpnConfig config;
    config.id = QStringLiteral("vpn-main");
    config.title = QStringLiteral("Main VPN");
    config.enabled = true;

    QVERIFY(!vpnConfigValid(config));

    config.servers.append(usableServer(QStringLiteral("no-host"), QString(), 443));
    QVERIFY(!vpnConfigValid(config));

    config.servers.append(usableServer(QStringLiteral("bad-port"),
                                       QStringLiteral("bad.example"), 0));
    QVERIFY(!vpnConfigValid(config));

    config.servers.append(usableServer(QStringLiteral("good"),
                                       QStringLiteral("good.example"), 443));
    QVERIFY(vpnConfigValid(config));
}

void VpnModelsTest::validAcceptsEnabledConfig()
{
    VpnConfig config;
    config.id = QStringLiteral("vpn-main");
    config.title = QStringLiteral("Main VPN");
    config.enabled = true;
    config.servers.append(usableServer(QStringLiteral("high"),
                                       QStringLiteral("high.example"), 65535));

    QVERIFY(vpnConfigValid(config));
}

QTEST_GUILESS_MAIN(VpnModelsTest)
#include "tst_vpn_models.moc"
