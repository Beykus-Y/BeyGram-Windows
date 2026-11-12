#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One endpoint of a VPN configuration.
///
/// `id` identifies the server inside its configuration and `host` is the
/// address a client would connect to; both are required for the server to be
/// usable. `name`, `country` and `protocol` are descriptive: they are carried
/// through unchanged so the UI can show exactly what the subscription
/// delivered.
struct VpnServer {
    QString id;
    QString name;
    QString country;
    QString host;
    int port = 0;
    QString protocol;
};

/// A saved VPN configuration: an identity, a display title, the servers it
/// offers and the on/off switch.
struct VpnConfig {
    QString id;
    QString title;
    QVector<VpnServer> servers;
    bool enabled = false;
};

/// Reads a stored VPN configuration.
///
/// The JSON shape is `{"id": string, "title": string, "servers": [...],
/// "enabled": bool}`; every server is `{"id", "name", "country", "host",
/// "port", "protocol"}`. Missing or wrongly typed members fall back to their
/// defaults, so an empty object yields an empty disabled configuration.
///
/// Normalisation performed here (and repeated by vpnConfigToJson, which makes
/// the pair round-trip stable):
///
///  - The configuration id, the configuration title and each server id are
///    stored trimmed.
///  - Each server host is stored trimmed, so a host of only whitespace counts
///    as missing.
///  - A server whose trimmed id or host is empty is dropped, as is a server
///    whose port is outside [1, 65535]. A server without a usable port is not
///    a server a client could connect to, so it never reaches the model.
///  - Non-object entries in `servers` are ignored.
///
/// Server names, countries and protocols are kept verbatim; they are display
/// text rather than identifiers.
[[nodiscard]] VpnConfig vpnConfigFromJson(const QJsonObject& json);

/// Writes the canonical JSON form of `config`, using the same normalisation as
/// vpnConfigFromJson: ids, titles and hosts trimmed and unusable servers
/// dropped. All four top-level members are always present, including
/// `"servers": []` and `"enabled": false`, so the output has a stable shape.
/// Consequently `vpnConfigToJson(vpnConfigFromJson(json))` is stable under
/// repeated serialisation, and parsing the result returns the same values.
[[nodiscard]] QJsonObject vpnConfigToJson(const VpnConfig& config);

/// True when the configuration could be used.
///
/// The id and title are required and must contain something besides
/// whitespace. A disabled configuration needs nothing else: it is saved but
/// not applied, so it may legitimately have no servers yet. An enabled
/// configuration needs at least one usable server (non-empty trimmed id and
/// host, port in [1, 65535]); a list of half-filled rows is not valid.
[[nodiscard]] bool vpnConfigValid(const VpnConfig& config);

} // namespace beygram::telegram
