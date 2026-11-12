#include "QrLoginController.h"

#include <QLatin1String>

namespace beygram::telegram {
namespace {

/// The two link shapes TDLib accepts for QR login. A token URI is what
/// exportLoginToken returns; an https link is what the user can paste back.
bool isSupportedLink(const QString& link)
{
    return link.startsWith(QLatin1String("tg://login?token="))
        || link.startsWith(QLatin1String("https://"));
}

} // namespace

QrLoginController::QrLoginController(QObject* parent) : QObject(parent)
{
}

void QrLoginController::begin(const QString& link)
{
    const QString accepted = link.trimmed();
    if (accepted.isEmpty() || !isSupportedLink(accepted))
        return;

    const bool linkDiffers = accepted != m_link;
    const bool stateDiffers = m_state != State::Waiting;
    if (!linkDiffers && !stateDiffers)
        return;

    m_link = accepted;
    m_state = State::Waiting;

    if (linkDiffers)
        emit linkChanged(m_link);
    if (stateDiffers)
        emit changed();
}

void QrLoginController::markScanned()
{
    // Only the waiting code can be scanned. A repeated update, or one that
    // arrives after the code expired, must not move the attempt.
    if (m_state != State::Waiting)
        return;

    m_state = State::Scanned;
    emit changed();
}

void QrLoginController::markConfirmed()
{
    // TDLib may skip the scanned update when the phone confirms quickly, so a
    // direct waiting -> confirmed transition is valid as well.
    if (m_state != State::Waiting && m_state != State::Scanned)
        return;

    m_state = State::Confirmed;
    emit changed();
}

void QrLoginController::markExpired()
{
    // Confirmed and expired are final; only an attempt still in flight can
    // time out.
    if (!active())
        return;

    m_state = State::Expired;
    emit changed();
}

void QrLoginController::reset()
{
    if (m_state == State::Idle)
        return;

    const bool linkDiffers = !m_link.isEmpty();
    m_link.clear();
    m_state = State::Idle;

    if (linkDiffers)
        emit linkChanged(m_link);
    emit changed();
}

bool QrLoginController::active() const
{
    return m_state == State::Waiting || m_state == State::Scanned;
}

QString QrLoginController::link() const
{
    return m_link;
}

QString QrLoginController::state() const
{
    switch (m_state) {
    case State::Idle:
        return QStringLiteral("idle");
    case State::Waiting:
        return QStringLiteral("waiting");
    case State::Scanned:
        return QStringLiteral("scanned");
    case State::Confirmed:
        return QStringLiteral("confirmed");
    case State::Expired:
        return QStringLiteral("expired");
    }
    return QStringLiteral("idle");
}

bool QrLoginController::expired() const
{
    return m_state == State::Expired;
}

} // namespace beygram::telegram
