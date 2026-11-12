#pragma once

#include <QObject>
#include <QString>

namespace beygram::telegram {
class ConnectionStatusController;
class ProxyConfigModel;
} // namespace beygram::telegram

namespace beygram::ui {

/// The network panel of the settings screen as one QML-facing object.
///
/// This is a thin presentation facade, not a second source of truth: the
/// connection state and its retry delay live in ConnectionStatusController, the
/// proxy form lives in ProxyConfigModel, and every property is read straight
/// from those two objects. The facade only does what QML cannot: it accepts the
/// state and proxy-type names that arrive as strings, and it packages the
/// proxy form into one signal for the caller.
///
/// The two controllers' changed() signals are forwarded as this object's
/// changed(), so a QML binding on any property repaints on a real update and
/// stays quiet on a redundant one (both controllers already suppress no-op
/// changes).
///
/// Formatting follows the controllers so the screen has one voice:
///   - stateText() is ConnectionStatusController::label(): "Offline", "Waiting
///     for network", "Connecting…", "Updating…" or "Connected".
///   - retryAfter() is the controller's clamped countdown, meaningful only while
///     the client is offline.
///   - proxyType() is one of "none", "socks5", "http" or "mtproto" and is never
///     empty; it reads "none" for a direct connection.
///   - proxyText() is ProxyConfigModel::displayText(), e.g.
///     "socks5://proxy.example:1080" or "Direct connection". The username and
///     password are never part of it, so the text is safe to display.
///
/// The facade never touches TDLib, the network or storage. applyProxy() only
/// reports the form; building TelegramProxySettings and calling setProxyRequest
/// is the caller's job.
///
/// Invariants:
///  - A fresh facade is offline (Unknown) with no retry and a disabled direct
///    connection, exactly the defaults of the two controllers.
///  - setConnectionState() and setProxyType() ignore a name they do not
///    recognize instead of guessing: an unknown value never moves the stored
///    state.
///  - proxyHost() is trimmed and proxyPort() is 0 (unset) or in [1, 65535],
///    because ProxyConfigModel enforces both.
///  - proxyValid() is true exactly when the proxy would be used: enabled, a type
///    other than None, a non-empty host and a usable port.
///  - applyProxy() emits proxyApplyRequested() only while proxyValid() is true,
///    and does nothing otherwise, so an incomplete form cannot be applied.
///  - The password is write-only. There is no getter, it is never part of
///    proxyText() and it is only ever handed to proxyApplyRequested().
class NetworkFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString stateText READ stateText NOTIFY changed)
    Q_PROPERTY(bool online READ online NOTIFY changed)
    Q_PROPERTY(int retryAfter READ retryAfter NOTIFY changed)
    Q_PROPERTY(bool proxyEnabled READ proxyEnabled WRITE setProxyEnabled NOTIFY changed)
    Q_PROPERTY(QString proxyType READ proxyType WRITE setProxyType NOTIFY changed)
    Q_PROPERTY(QString proxyHost READ proxyHost WRITE setProxyHost NOTIFY changed)
    Q_PROPERTY(int proxyPort READ proxyPort WRITE setProxyPort NOTIFY changed)
    Q_PROPERTY(QString proxyUser READ proxyUser WRITE setProxyUser NOTIFY changed)
    Q_PROPERTY(bool proxyValid READ proxyValid NOTIFY changed)
    Q_PROPERTY(QString proxyText READ proxyText NOTIFY changed)

public:
    explicit NetworkFacade(QObject* parent = nullptr);

    /// The status line: ConnectionStatusController::label().
    [[nodiscard]] QString stateText() const;

    /// True only in Ready, i.e. TDLib can carry traffic.
    [[nodiscard]] bool online() const;

    /// The retry delay in seconds, 0 while there is nothing to count down.
    [[nodiscard]] int retryAfter() const;

    /// Stores the connection state from a name. Accepted, case-insensitively
    /// and ignoring spaces, underscores and hyphens: "unknown" or "offline",
    /// "waiting" or "waitingForNetwork", "connecting", "updating", "ready" or
    /// "connected". The TDLib spelling with its "connectionState" prefix
    /// ("connectionStateReady") is accepted too, and an empty string means
    /// Unknown. Any other value is ignored and leaves the state untouched.
    Q_INVOKABLE void setConnectionState(const QString& state);

    /// Feeds the retry delay TDLib asked for, in seconds. The controller clamps
    /// it to 0..3600 and keeps it only while the client is offline, so a value
    /// set in Ready is dropped rather than resurrected later.
    Q_INVOKABLE void setRetryAfter(int seconds);

    /// Advances the retry countdown by one second, never below 0. A countdown
    /// already at 0 stays there and announces nothing.
    Q_INVOKABLE void tick();

    [[nodiscard]] bool proxyEnabled() const;
    void setProxyEnabled(bool on);

    /// One of "none", "socks5", "http" or "mtproto". An unrecognized name is
    /// ignored; "direct" and "off" are accepted as aliases for "none".
    [[nodiscard]] QString proxyType() const;
    void setProxyType(const QString& type);

    /// The trimmed host, empty while unset. The port is kept.
    [[nodiscard]] QString proxyHost() const;
    void setProxyHost(const QString& host);

    /// The port in [1, 65535], or 0 while unset. Out-of-range values are
    /// clamped by ProxyConfigModel, so 70000 reads back as 65535 and -1 as 0.
    [[nodiscard]] int proxyPort() const;
    void setProxyPort(int port);

    /// The username, stored verbatim without trimming. The password is kept.
    [[nodiscard]] QString proxyUser() const;
    void setProxyUser(const QString& user);

    /// Replaces the password (also the MTProto secret). There is deliberately
    /// no getter: the value only leaves the facade through
    /// proxyApplyRequested(). The username is kept.
    Q_INVOKABLE void setProxyPassword(const QString& password);

    /// True exactly when the proxy would be used; see the class comment.
    [[nodiscard]] bool proxyValid() const;

    /// The settings row text, e.g. "socks5://proxy.example:1080". Never
    /// contains the username or the password.
    [[nodiscard]] QString proxyText() const;

    /// Announces the current form through proxyApplyRequested() when
    /// proxyValid() is true, and does nothing otherwise.
    Q_INVOKABLE void applyProxy();

signals:
    /// Emitted whenever any observable state of the facade changed.
    void changed();

    /// Emitted by applyProxy() with a snapshot of the form: type is "socks5",
    /// "http" or "mtproto" (never "none", because the form is valid), host and
    /// user are the stored values and password is the write-only secret. The
    /// caller maps this onto TelegramProxySettings.
    void proxyApplyRequested(const QString& type, const QString& host, int port,
                             const QString& user, const QString& password);

private:
    telegram::ConnectionStatusController* m_status = nullptr;
    telegram::ProxyConfigModel* m_proxy = nullptr;
};

} // namespace beygram::ui
