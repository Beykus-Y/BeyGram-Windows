#include "ProxyConfigModel.h"

#include <algorithm>

namespace beygram::telegram {
namespace {

/// The scheme TelegramProxySettings::type uses for this proxy type. Only called
/// for types other than None.
QString schemeFor(ProxyType type)
{
    switch (type) {
    case ProxyType::Socks5:
        return QStringLiteral("socks5");
    case ProxyType::Http:
        return QStringLiteral("http");
    case ProxyType::Mtproto:
        return QStringLiteral("mtproto");
    case ProxyType::None:
        break;
    }
    return {};
}

/// Clamps into {0} U [1, 65535]: 0 and negatives mean "unset", anything above
/// the largest TCP port becomes 65535. The model therefore never stores a port
/// that could not be used in a request.
int clampPort(int port)
{
    if (port <= 0)
        return 0;
    return std::min(port, 65535);
}

} // namespace

ProxyConfigModel::ProxyConfigModel(QObject* parent)
    : QObject(parent)
{
}

void ProxyConfigModel::setType(ProxyType type)
{
    if (m_type == type)
        return;
    m_type = type;
    emit changed();
}

ProxyType ProxyConfigModel::type() const
{
    return m_type;
}

void ProxyConfigModel::setServer(const QString& host, int port)
{
    const QString trimmed = host.trimmed();
    const int clamped = clampPort(port);
    if (m_host == trimmed && m_port == clamped)
        return;
    m_host = trimmed;
    m_port = clamped;
    emit changed();
}

QString ProxyConfigModel::host() const
{
    return m_host;
}

int ProxyConfigModel::port() const
{
    return m_port;
}

void ProxyConfigModel::setCredentials(const QString& username, const QString& password)
{
    // Stored verbatim: a password may end in a space and an MTProto secret is
    // the password, so trimming would silently change what the user typed.
    if (m_user == username && m_pass == password)
        return;
    m_user = username;
    m_pass = password;
    emit changed();
}

QString ProxyConfigModel::username() const
{
    return m_user;
}

QString ProxyConfigModel::password() const
{
    return m_pass;
}

void ProxyConfigModel::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit changed();
}

bool ProxyConfigModel::enabled() const
{
    return m_enabled;
}

bool ProxyConfigModel::valid() const
{
    return m_enabled && m_type != ProxyType::None && !m_host.isEmpty() && m_port >= 1
        && m_port <= 65535;
}

QString ProxyConfigModel::displayText() const
{
    if (m_type == ProxyType::None)
        return QStringLiteral("Direct connection");

    const QString scheme = schemeFor(m_type);
    if (m_host.isEmpty())
        return scheme + QStringLiteral("://");
    if (m_port == 0)
        return scheme + QStringLiteral("://") + m_host;
    return QStringLiteral("%1://%2:%3").arg(scheme, m_host).arg(m_port);
}

void ProxyConfigModel::reset()
{
    const bool atDefaults = m_type == ProxyType::None && m_host.isEmpty() && m_port == 0
        && m_user.isEmpty() && m_pass.isEmpty() && !m_enabled;
    if (atDefaults)
        return;

    m_type = ProxyType::None;
    m_host.clear();
    m_port = 0;
    m_user.clear();
    m_pass.clear();
    m_enabled = false;
    emit changed();
}

} // namespace beygram::telegram
