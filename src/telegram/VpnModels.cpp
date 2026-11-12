#include "VpnModels.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int MinPort = 1;
constexpr int MaxPort = 65535;

/// A server is usable only when a client could actually connect to it: it has
/// an identity, an address and a port from the TCP range.
bool serverUsable(const VpnServer& server)
{
    return !server.id.trimmed().isEmpty() && !server.host.trimmed().isEmpty()
        && server.port >= MinPort && server.port <= MaxPort;
}

VpnServer serverFromJson(const QJsonObject& json)
{
    VpnServer server;
    server.id = json.value(QStringLiteral("id")).toString().trimmed();
    server.name = json.value(QStringLiteral("name")).toString();
    server.country = json.value(QStringLiteral("country")).toString();
    server.host = json.value(QStringLiteral("host")).toString().trimmed();
    server.port = json.value(QStringLiteral("port")).toInt();
    server.protocol = json.value(QStringLiteral("protocol")).toString();
    return server;
}

QJsonObject serverToJson(const VpnServer& server)
{
    QJsonObject json;
    json.insert(QStringLiteral("id"), server.id.trimmed());
    json.insert(QStringLiteral("name"), server.name);
    json.insert(QStringLiteral("country"), server.country);
    json.insert(QStringLiteral("host"), server.host.trimmed());
    json.insert(QStringLiteral("port"), server.port);
    json.insert(QStringLiteral("protocol"), server.protocol);
    return json;
}

} // namespace

VpnConfig vpnConfigFromJson(const QJsonObject& json)
{
    VpnConfig config;
    config.id = json.value(QStringLiteral("id")).toString().trimmed();
    config.title = json.value(QStringLiteral("title")).toString().trimmed();
    config.enabled = json.value(QStringLiteral("enabled")).toBool();

    const QJsonArray servers = json.value(QStringLiteral("servers")).toArray();
    config.servers.reserve(servers.size());
    for (const QJsonValue& value : servers) {
        if (!value.isObject())
            continue;
        const VpnServer server = serverFromJson(value.toObject());
        if (!serverUsable(server))
            continue;
        config.servers.append(server);
    }
    return config;
}

QJsonObject vpnConfigToJson(const VpnConfig& config)
{
    QJsonArray servers;
    for (const VpnServer& server : config.servers) {
        // Writing only usable servers keeps the output canonical: parsing it
        // back cannot drop anything, so repeated round trips do not drift.
        if (!serverUsable(server))
            continue;
        servers.append(serverToJson(server));
    }

    QJsonObject json;
    json.insert(QStringLiteral("id"), config.id.trimmed());
    json.insert(QStringLiteral("title"), config.title.trimmed());
    json.insert(QStringLiteral("servers"), servers);
    json.insert(QStringLiteral("enabled"), config.enabled);
    return json;
}

bool vpnConfigValid(const VpnConfig& config)
{
    if (config.id.trimmed().isEmpty() || config.title.trimmed().isEmpty())
        return false;
    if (!config.enabled)
        return true;
    return std::any_of(config.servers.cbegin(), config.servers.cend(), serverUsable);
}

} // namespace beygram::telegram
