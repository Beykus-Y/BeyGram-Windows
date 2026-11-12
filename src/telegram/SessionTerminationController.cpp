#include "SessionTerminationController.h"

namespace beygram::telegram {

SessionTerminationController::SessionTerminationController(QObject* parent)
    : QObject(parent)
{
}

void SessionTerminationController::requestTerminate(qint64 sessionId)
{
    // A row without a positive id cannot have come from the parser, so there is
    // nothing this confirmation could terminate.
    if (sessionId <= 0)
        return;

    // The in-flight action owns the target until it finishes or is cancelled.
    if (m_running)
        return;

    if (m_confirming && m_pending == sessionId)
        return;

    m_pending = sessionId;
    m_confirming = true;
    emit changed();
}

void SessionTerminationController::requestTerminateAllOthers()
{
    if (m_running)
        return;

    if (m_confirming && m_pending == 0)
        return;

    m_pending = 0;
    m_confirming = true;
    emit changed();
}

bool SessionTerminationController::confirming() const
{
    return m_confirming;
}

qint64 SessionTerminationController::pendingSessionId() const
{
    return m_pending;
}

void SessionTerminationController::cancel()
{
    if (!m_confirming)
        return;

    m_pending = 0;
    m_running = false;
    m_confirming = false;
    emit changed();
}

void SessionTerminationController::setRunning(bool running)
{
    // Running outside a confirmation would leave an idle controller busy with
    // no target to finish.
    if (!m_confirming)
        return;

    if (m_running == running)
        return;

    m_running = running;
    emit changed();
}

bool SessionTerminationController::running() const
{
    return m_running;
}

bool SessionTerminationController::canConfirm() const
{
    return m_confirming && !m_running;
}

void SessionTerminationController::clear()
{
    if (!m_confirming && m_pending == 0 && !m_running)
        return;

    m_pending = 0;
    m_confirming = false;
    m_running = false;
    emit changed();
}

} // namespace beygram::telegram
