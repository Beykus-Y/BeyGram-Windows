#include "ReadMarkerTracker.h"

namespace beygram::telegram {

namespace {

// Advances a marker without ever moving it backwards. Returns true when the
// value changed, so callers emit changed() once per real transition.
bool advance(qint64& marker, qint64 messageId)
{
    if (messageId <= 0 || messageId <= marker)
        return false;
    marker = messageId;
    return true;
}

} // namespace

ReadMarkerTracker::ReadMarkerTracker(QObject* parent)
    : QObject(parent)
{
}

void ReadMarkerTracker::reset()
{
    if (m_inbox == 0 && m_outbox == 0)
        return;
    m_inbox = 0;
    m_outbox = 0;
    emit changed();
}

void ReadMarkerTracker::applyInbox(qint64 messageId)
{
    if (advance(m_inbox, messageId))
        emit changed();
}

void ReadMarkerTracker::applyOutbox(qint64 messageId)
{
    if (advance(m_outbox, messageId))
        emit changed();
}

bool ReadMarkerTracker::shouldMark(qint64 visibleNewestId) const
{
    return visibleNewestId > m_inbox;
}

void ReadMarkerTracker::commit(qint64 messageId)
{
    if (advance(m_inbox, messageId))
        emit changed();
}

} // namespace beygram::telegram
