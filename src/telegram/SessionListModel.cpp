#include "SessionListModel.h"

#include <utility>

namespace beygram::telegram {
namespace {

// SessionInfo carries no operator== (it is a wire-shaped value type), so the
// model compares the fields it holds. Every field is compared: a reload that
// only moves lastActive or flips current is still a real change for a row that
// renders "last active" and the "this device" badge from those fields.
bool sameSession(const SessionInfo& left, const SessionInfo& right)
{
    return left.id == right.id
        && left.device == right.device
        && left.location == right.location
        && left.ip == right.ip
        && left.lastActive == right.lastActive
        && left.current == right.current;
}

bool sameSessions(const QVector<SessionInfo>& left, const QVector<SessionInfo>& right)
{
    if (left.size() != right.size())
        return false;
    for (qsizetype i = 0; i < left.size(); ++i) {
        if (!sameSession(left.at(i), right.at(i)))
            return false;
    }
    return true;
}

// -1 when no session has that id. The list is small enough (one account's
// devices) that a linear scan beats keeping a second index in sync.
qsizetype indexOfSession(const QVector<SessionInfo>& sessions, qint64 sessionId)
{
    for (qsizetype i = 0; i < sessions.size(); ++i) {
        if (sessions.at(i).id == sessionId)
            return i;
    }
    return -1;
}

// A session without a positive id cannot be looked up or terminated, so it is
// dropped when the list is set.
bool hasSession(const SessionInfo& session)
{
    return session.id > 0;
}

} // namespace

SessionListModel::SessionListModel(QObject* parent)
    : QObject(parent)
{
}

void SessionListModel::setSessions(QVector<SessionInfo> sessions)
{
    // Keep the first session of each id: sessions arrive in the order the
    // caller merged its answers, and the first answer is the one already on
    // screen, so it wins over a later duplicate instead of making the visible
    // row depend on merge order.
    QVector<SessionInfo> unique;
    unique.reserve(sessions.size());
    bool currentSeen = false;
    for (SessionInfo& session : sessions) {
        if (!hasSession(session))
            continue;
        if (indexOfSession(unique, session.id) >= 0)
            continue;
        // At most one row may claim to be this device. The first flagged one
        // keeps the flag so the screen does not move the badge between rows on
        // a reload; the rest are demoted rather than dropped, because they are
        // still real sessions the user may terminate.
        if (session.current) {
            if (currentSeen)
                session.current = false;
            else
                currentSeen = true;
        }
        unique.append(std::move(session));
    }

    // A reload that brings the same data is not a change: repainting the list
    // for it would also restart every animation bound to changed().
    if (sameSessions(unique, m_sessions))
        return;

    m_sessions = std::move(unique);
    emit changed();
}

QVector<SessionInfo> SessionListModel::sessions() const
{
    return m_sessions;
}

int SessionListModel::count() const
{
    return static_cast<int>(m_sessions.size());
}

QVector<SessionInfo> SessionListModel::others() const
{
    QVector<SessionInfo> others;
    others.reserve(m_sessions.size());
    for (const SessionInfo& session : m_sessions) {
        if (!session.current)
            others.append(session);
    }
    return others;
}

bool SessionListModel::remove(qint64 sessionId)
{
    const qsizetype index = indexOfSession(m_sessions, sessionId);
    if (index < 0)
        return false;

    m_sessions.removeAt(index);
    emit changed();
    return true;
}

void SessionListModel::clear()
{
    if (m_sessions.isEmpty())
        return;

    m_sessions.clear();
    emit changed();
}

} // namespace beygram::telegram
