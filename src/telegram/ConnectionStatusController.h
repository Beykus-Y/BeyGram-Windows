#pragma once

#include <QObject>
#include <QString>

namespace beygram::telegram {

/// Client-side connection status, mirroring TDLib's updateConnectionState.
/// The UI never sees TDLib's raw JSON, only these states.
enum class ConnectionState {
    Unknown,            ///< no updateConnectionState received yet
    WaitingForNetwork,  ///< TDLib is offline and retrying
    Connecting,         ///< TDLib is opening the connection
    Updating,           ///< connected, fetching the difference from the server
    Ready,              ///< fully online
};

/// Tracks the connection status and the retry delay that TDLib asks for.
///
/// The retry counter only means something while the client is offline, so it is
/// clamped to 0..3600 seconds and kept only for WaitingForNetwork and
/// Connecting; every other state forces it back to 0. Every mutator is a no-op
/// when it would not change the stored values, so changed() is emitted once per
/// real transition and status views never repaint for nothing.
class ConnectionStatusController final : public QObject {
    Q_OBJECT

public:
    explicit ConnectionStatusController(QObject* parent = nullptr);

    /// Stores the new connection state. Leaving the offline states clears the
    /// retry counter; staying in one keeps it.
    void setState(ConnectionState state);
    [[nodiscard]] ConnectionState state() const { return m_state; }

    /// Sets the retry delay in seconds. The value is clamped to 0..3600 and is
    /// ignored, leaving the counter at 0, unless the state is WaitingForNetwork
    /// or Connecting.
    void setRetryAfter(int seconds);
    [[nodiscard]] int retryAfter() const { return m_retry; }

    /// Decrements the retry counter by one second, never below 0. A counter
    /// that already reached 0 stays there and does not emit anything.
    void tick();

    /// True only in Ready: TDLib can carry traffic.
    [[nodiscard]] bool online() const { return m_state == ConnectionState::Ready; }

    /// Human readable status for the status bar: "Offline", "Waiting for
    /// network", "Connecting…", "Updating…" or "Connected".
    [[nodiscard]] QString label() const;

    /// True while the client is offline or still establishing the connection.
    [[nodiscard]] bool degraded() const;

signals:
    /// Emitted whenever the state or the retry counter actually changed.
    void changed();

private:
    ConnectionState m_state = ConnectionState::Unknown;
    int m_retry = 0;
};

} // namespace beygram::telegram
