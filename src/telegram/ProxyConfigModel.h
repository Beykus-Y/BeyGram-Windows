#pragma once

#include <QObject>
#include <QString>

namespace beygram::telegram {

/// The connection type shown in the proxy section of the settings screen.
///
/// The names line up with the `type` strings TelegramProxySettings accepts
/// ("socks5", "http", "mtproto"); None means "no proxy configured".
enum class ProxyType { None, Socks5, Http, Mtproto };

/// Editable state of the proxy settings screen: type, server, port, credentials
/// and the on/off switch.
///
/// This is UI state only. It does not read the environment, does not touch
/// proxy.txt, never talks to TDLib and performs no I/O, so a view binds its
/// controls to the getters, calls the setters and repaints on changed().
/// Mapping the result onto TelegramProxySettings (and thereby onto
/// setProxyRequest) is the caller's job. TelegramProxySettings in
/// TelegramConfig.h stays the source of truth for the session; a value built
/// here only reaches it when the caller decides to apply the screen.
///
/// Invariants:
///
///  - A fresh model is a disabled direct connection: type None, empty host,
///    port 0, empty credentials, enabled false. That is exactly the state
///    reset() installs.
///  - The host is stored trimmed, so a host of only whitespace is the same as
///    an empty host and never becomes valid.
///  - The port is clamped into {0} U [1, 65535]. 0 means "not set"; a negative
///    port and an explicit 0 both store 0, and anything above 65535 stores
///    65535, so the model can never hold a port that no socket could use.
///  - The username and password are stored verbatim, without trimming: a
///    password may legitimately contain leading or trailing spaces, and for an
///    MTProto proxy the password is the secret. Neither is ever written to a
///    log by this class.
///  - Switching the type never clears the host, port or credentials. Turning
///    the proxy off with setEnabled(false), or switching to None and back,
///    therefore restores the previous endpoint instead of making the user type
///    it again.
///  - changed() is emitted only when a value really differs. Writing the value
///    already stored is silent, so a refresh that brings the same settings does
///    not repaint the screen or re-trigger a binding loop.
///  - reset() is silent while the model already is at its defaults and emits
///    changed() exactly once when it actually restores something.
class ProxyConfigModel final : public QObject {
    Q_OBJECT

public:
    explicit ProxyConfigModel(QObject* parent = nullptr);

    /// Selects the connection type. None means "direct connection". The host,
    /// port and credentials are left untouched. Emits changed() only when the
    /// type really differs.
    void setType(ProxyType type);
    [[nodiscard]] ProxyType type() const;

    /// Stores the endpoint. `host` is trimmed; `port` is clamped as described
    /// above, so 0 (or a negative value) records "not set" rather than an
    /// invalid port. Emits changed() once when either value really differs.
    void setServer(const QString& host, int port);
    /// The trimmed host, empty while unset.
    [[nodiscard]] QString host() const;
    /// The port in [1, 65535], or 0 while unset.
    [[nodiscard]] int port() const;

    /// Stores the credentials verbatim, without trimming. Emits changed() once
    /// when either value really differs.
    void setCredentials(const QString& username, const QString& password);
    [[nodiscard]] QString username() const;
    [[nodiscard]] QString password() const;

    /// The on/off switch of the screen. A configured endpoint is not used while
    /// this is false, which valid() reflects. Emits changed() only when the flag
    /// really differs.
    void setEnabled(bool enabled);
    [[nodiscard]] bool enabled() const;

    /// True only when the proxy would actually be used: enabled, a type other
    /// than None, a non-empty host and a port in [1, 65535]. Credentials are
    /// not part of the condition, because a SOCKS5 or HTTP proxy may allow
    /// anonymous access. Whether an MTProto proxy additionally needs its secret
    /// in the password is a question for the caller that builds the
    /// TelegramProxySettings, not for this screen model.
    [[nodiscard]] bool valid() const;

    /// Text for the settings row: "Direct connection" while the type is None,
    /// otherwise "scheme://host:port" with scheme one of socks5, http or
    /// mtproto. A partially filled endpoint is shown as far as it is known
    /// ("socks5://" or "socks5://host") instead of inventing a port. The
    /// username and password are never included, so the text is safe to display
    /// and to copy into a bug report; only valid() promises the full form.
    [[nodiscard]] QString displayText() const;

    /// Restores the defaults (see the class comment). Emits changed() once if
    /// anything actually moved, and stays silent when the model already is at
    /// its defaults.
    void reset();

signals:
    /// Emitted once per real change to any value above, and once for a reset()
    /// that actually restored something.
    void changed();

private:
    ProxyType m_type = ProxyType::None;
    QString m_host;
    QString m_user;
    QString m_pass;
    int m_port = 0;
    bool m_enabled = false;
};

} // namespace beygram::telegram
