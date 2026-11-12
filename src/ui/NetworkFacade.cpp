#include "NetworkFacade.h"

#include "telegram/ConnectionStatusController.h"
#include "telegram/ProxyConfigModel.h"

#include <optional>

namespace beygram::ui {

namespace {

/// Length of the "connectionstate" prefix TDLib puts in front of its state
/// names ("connectionStateReady"). Checked after the key was normalized, so the
/// prefix is already lower case and free of separators.
constexpr int TdlibStatePrefixLength = 15;

/// Folds a name to the form the parsers compare against: trimmed, lower case
/// and without spaces, underscores or hyphens, so "Waiting for network",
/// "waiting_for_network" and "connectionStateWaitingForNetwork" all agree.
QString normalizedKey(const QString& value)
{
    QString key = value.trimmed().toLower();
    key.remove(QLatin1Char(' '));
    key.remove(QLatin1Char('_'));
    key.remove(QLatin1Char('-'));

    if (key.startsWith(QLatin1String("connectionstate")))
        key = key.mid(TdlibStatePrefixLength);

    return key;
}

/// Parses a connection state name. An empty name means Unknown, like the
/// controller's own initial state; anything unrecognized yields nullopt so the
/// caller can leave the stored state alone.
std::optional<telegram::ConnectionState> parseConnectionState(const QString& value)
{
    const QString key = normalizedKey(value);

    if (key.isEmpty() || key == QStringLiteral("unknown") || key == QStringLiteral("offline"))
        return telegram::ConnectionState::Unknown;

    if (key == QStringLiteral("waiting") || key == QStringLiteral("waitingfornetwork"))
        return telegram::ConnectionState::WaitingForNetwork;

    if (key == QStringLiteral("connecting"))
        return telegram::ConnectionState::Connecting;

    if (key == QStringLiteral("updating"))
        return telegram::ConnectionState::Updating;

    if (key == QStringLiteral("ready") || key == QStringLiteral("connected"))
        return telegram::ConnectionState::Ready;

    return std::nullopt;
}

/// Parses a proxy type name. "direct" and "off" are accepted as aliases for
/// None, "socks" and "mt" as short forms of their schemes.
std::optional<telegram::ProxyType> parseProxyType(const QString& value)
{
    const QString key = normalizedKey(value);

    if (key == QStringLiteral("none") || key == QStringLiteral("direct")
        || key == QStringLiteral("off")) {
        return telegram::ProxyType::None;
    }

    if (key == QStringLiteral("socks5") || key == QStringLiteral("socks"))
        return telegram::ProxyType::Socks5;

    if (key == QStringLiteral("http") || key == QStringLiteral("https"))
        return telegram::ProxyType::Http;

    if (key == QStringLiteral("mtproto") || key == QStringLiteral("mt"))
        return telegram::ProxyType::Mtproto;

    return std::nullopt;
}

/// The name QML binds to, matching TelegramProxySettings::type.
QString proxyTypeName(telegram::ProxyType type)
{
    switch (type) {
    case telegram::ProxyType::Socks5:
        return QStringLiteral("socks5");
    case telegram::ProxyType::Http:
        return QStringLiteral("http");
    case telegram::ProxyType::Mtproto:
        return QStringLiteral("mtproto");
    case telegram::ProxyType::None:
        break;
    }

    return QStringLiteral("none");
}

} // namespace

NetworkFacade::NetworkFacade(QObject* parent)
    : QObject(parent)
    , m_status(new telegram::ConnectionStatusController(this))
    , m_proxy(new telegram::ProxyConfigModel(this))
{
    connect(m_status, &telegram::ConnectionStatusController::changed,
            this, &NetworkFacade::changed);
    connect(m_proxy, &telegram::ProxyConfigModel::changed,
            this, &NetworkFacade::changed);
}

QString NetworkFacade::stateText() const
{
    return m_status->label();
}

bool NetworkFacade::online() const
{
    return m_status->online();
}

int NetworkFacade::retryAfter() const
{
    return m_status->retryAfter();
}

void NetworkFacade::setConnectionState(const QString& state)
{
    const std::optional<telegram::ConnectionState> parsed = parseConnectionState(state);
    if (!parsed)
        return;

    m_status->setState(*parsed);
}

void NetworkFacade::setRetryAfter(int seconds)
{
    m_status->setRetryAfter(seconds);
}

void NetworkFacade::tick()
{
    m_status->tick();
}

bool NetworkFacade::proxyEnabled() const
{
    return m_proxy->enabled();
}

void NetworkFacade::setProxyEnabled(bool on)
{
    m_proxy->setEnabled(on);
}

QString NetworkFacade::proxyType() const
{
    return proxyTypeName(m_proxy->type());
}

void NetworkFacade::setProxyType(const QString& type)
{
    const std::optional<telegram::ProxyType> parsed = parseProxyType(type);
    if (!parsed)
        return;

    m_proxy->setType(*parsed);
}

QString NetworkFacade::proxyHost() const
{
    return m_proxy->host();
}

void NetworkFacade::setProxyHost(const QString& host)
{
    // The port lives in the same model call, so it is carried over unchanged.
    m_proxy->setServer(host, m_proxy->port());
}

int NetworkFacade::proxyPort() const
{
    return m_proxy->port();
}

void NetworkFacade::setProxyPort(int port)
{
    m_proxy->setServer(m_proxy->host(), port);
}

QString NetworkFacade::proxyUser() const
{
    return m_proxy->username();
}

void NetworkFacade::setProxyUser(const QString& user)
{
    // The password lives in the same model call and must survive a user edit.
    m_proxy->setCredentials(user, m_proxy->password());
}

void NetworkFacade::setProxyPassword(const QString& password)
{
    m_proxy->setCredentials(m_proxy->username(), password);
}

bool NetworkFacade::proxyValid() const
{
    return m_proxy->valid();
}

QString NetworkFacade::proxyText() const
{
    return m_proxy->displayText();
}

void NetworkFacade::applyProxy()
{
    if (!proxyValid())
        return;

    // Snapshot everything before emitting: a slot may re-enter the facade and
    // edit the form while the signal is still being delivered.
    const QString type = proxyTypeName(m_proxy->type());
    const QString host = m_proxy->host();
    const int port = m_proxy->port();
    const QString user = m_proxy->username();
    const QString password = m_proxy->password();

    emit proxyApplyRequested(type, host, port, user, password);
}

} // namespace beygram::ui
