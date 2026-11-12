#include "ConnectionStatusController.h"

#include <QtGlobal>

namespace beygram::telegram {
namespace {

// TDLib never asks for a longer pause than an hour; anything outside the range
// is treated as a malformed payload rather than shown to the user.
constexpr int kMaxRetryAfterSeconds = 3600;

// Only the two offline states carry a retry delay; every other state has no
// pending retry to count down.
bool tracksRetry(ConnectionState state)
{
    return state == ConnectionState::WaitingForNetwork || state == ConnectionState::Connecting;
}

} // namespace

ConnectionStatusController::ConnectionStatusController(QObject* parent)
    : QObject(parent)
{
}

void ConnectionStatusController::setState(ConnectionState state)
{
    bool dirty = false;

    if (m_state != state) {
        m_state = state;
        dirty = true;
    }

    // Entering a state that cannot be retried drops a stale delay, so a later
    // offline state never inherits a counter from an old outage.
    if (!tracksRetry(m_state) && m_retry != 0) {
        m_retry = 0;
        dirty = true;
    }

    if (dirty)
        emit changed();
}

void ConnectionStatusController::setRetryAfter(int seconds)
{
    const int clamped = qBound(0, seconds, kMaxRetryAfterSeconds);
    const int next = tracksRetry(m_state) ? clamped : 0;
    if (next == m_retry)
        return;
    m_retry = next;
    emit changed();
}

void ConnectionStatusController::tick()
{
    if (m_retry <= 0)
        return;
    --m_retry;
    emit changed();
}

QString ConnectionStatusController::label() const
{
    switch (m_state) {
    case ConnectionState::Unknown:
        return QStringLiteral("Offline");
    case ConnectionState::WaitingForNetwork:
        return QStringLiteral("Waiting for network");
    case ConnectionState::Connecting:
        return QStringLiteral("Connecting…");
    case ConnectionState::Updating:
        return QStringLiteral("Updating…");
    case ConnectionState::Ready:
        return QStringLiteral("Connected");
    }
    return QStringLiteral("Unknown");
}

bool ConnectionStatusController::degraded() const
{
    return tracksRetry(m_state);
}

} // namespace beygram::telegram
