#include "ui/SessionsFacade.h"

#include <QChar>
#include <QLatin1Char>
#include <QString>
#include <QStringList>
#include <QVector>

#include <utility>

namespace beygram::ui {
namespace {

// Builds "device - location" with a real em dash. The source stays pure ASCII
// so the literal does not depend on the compiler's source charset, matching the
// convention used by the other labels in this tree.
QString deviceText(const telegram::SessionRow& session)
{
    const QString device = session.device.trimmed();
    const QString location = session.location.trimmed();

    if (device.isEmpty())
        return location.isEmpty() ? QStringLiteral("Unknown device") : location;
    if (location.isEmpty())
        return device;

    return device + QLatin1Char(' ') + QChar(0x2014) + QLatin1Char(' ') + location;
}

} // namespace

SessionsFacade::SessionsFacade(QObject* parent)
    : QObject(parent)
    , m_sessions(this)
    , m_termination(this)
{
    // The facade holds no state of its own: every property is derived from one
    // of the two collaborators, so their changes are the facade's changes.
    connect(&m_sessions, &telegram::SessionListModel::changed,
            this, &SessionsFacade::changed);
    connect(&m_termination, &telegram::SessionTerminationController::changed,
            this, &SessionsFacade::changed);
}

QStringList SessionsFacade::devices() const
{
    const QVector<telegram::SessionRow> sessions = m_sessions.others();

    QStringList labels;
    labels.reserve(sessions.size());
    for (const telegram::SessionRow& session : sessions)
        labels.append(deviceText(session));
    return labels;
}

QStringList SessionsFacade::sessionIds() const
{
    const QVector<telegram::SessionRow> sessions = m_sessions.others();

    QStringList ids;
    ids.reserve(sessions.size());
    for (const telegram::SessionRow& session : sessions)
        ids.append(QString::number(session.id));
    return ids;
}

void SessionsFacade::setSessions(const QStringList& ids, const QStringList& devices,
                                 const QStringList& locations)
{
    QVector<telegram::SessionRow> sessions;
    sessions.reserve(ids.size());

    for (int index = 0; index < ids.size(); ++index) {
        bool ok = false;
        const qint64 id = ids.at(index).trimmed().toLongLong(&ok);
        if (!ok || id <= 0)
            continue; // The model drops it anyway; skipping keeps labels aligned.

        telegram::SessionRow session;
        session.id = id;
        session.device = devices.value(index);
        session.location = locations.value(index);
        sessions.append(session);
    }

    m_sessions.setSessions(std::move(sessions));

    // A confirmation for a session that this answer no longer lists would sign
    // out a device the user can no longer see, so it is dropped. A pending "all
    // other sessions" (id 0) survives: it applies to the list as it is now.
    const qint64 pending = m_termination.pendingSessionId();
    if (m_termination.confirming() && pending > 0 && !isTerminable(pending))
        m_termination.cancel();
}

void SessionsFacade::requestTerminate(const QString& sessionId)
{
    bool ok = false;
    const qint64 id = sessionId.trimmed().toLongLong(&ok);
    if (!ok || !isTerminable(id))
        return;

    m_termination.requestTerminate(id);
}

void SessionsFacade::requestTerminateAllOthers()
{
    // Nothing to sign out: do not open a destructive confirmation the user could
    // only cancel.
    if (m_sessions.others().isEmpty())
        return;

    m_termination.requestTerminateAllOthers();
}

void SessionsFacade::cancel()
{
    m_termination.cancel();
}

bool SessionsFacade::confirming() const
{
    return m_termination.confirming();
}

QString SessionsFacade::pendingTarget() const
{
    if (!m_termination.confirming())
        return {};

    const qint64 pending = m_termination.pendingSessionId();
    return pending > 0 ? QString::number(pending) : QStringLiteral("all");
}

bool SessionsFacade::running() const
{
    return m_termination.running();
}

void SessionsFacade::setRunning(bool running)
{
    m_termination.setRunning(running);
}

bool SessionsFacade::confirm()
{
    if (!m_termination.canConfirm())
        return false;

    const qint64 pending = m_termination.pendingSessionId();
    const QString target = pending > 0 ? QString::number(pending) : QString();

    // Running first, then the signal: a caller that finishes synchronously and
    // calls setRunning(false) must not be overwritten by this call.
    m_termination.setRunning(true);
    emit terminateRequested(target);
    return true;
}

bool SessionsFacade::isTerminable(qint64 sessionId) const
{
    if (sessionId <= 0)
        return false;

    const QVector<telegram::SessionRow> sessions = m_sessions.others();
    for (const telegram::SessionRow& session : sessions) {
        if (session.id == sessionId)
            return true;
    }
    return false;
}

} // namespace beygram::ui
