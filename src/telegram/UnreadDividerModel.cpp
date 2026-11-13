#include "UnreadDividerModel.h"

namespace beygram::telegram {

UnreadDividerModel::UnreadDividerModel(QObject* parent) : QObject(parent)
{
}

void UnreadDividerModel::setFirstUnread(qint64 messageId)
{
    if (messageId <= 0 || messageId == m_firstUnread)
        return;

    m_firstUnread = messageId;
    emit changed();
}

qint64 UnreadDividerModel::firstUnread() const
{
    return m_firstUnread;
}

void UnreadDividerModel::setVisible(bool visible)
{
    if (visible == m_visible)
        return;

    m_visible = visible;
    emit changed();
}

bool UnreadDividerModel::visible() const
{
    return m_visible;
}

bool UnreadDividerModel::shouldShowBefore(qint64 messageId) const
{
    return m_visible && messageId > 0 && messageId == m_firstUnread;
}

void UnreadDividerModel::markRead(qint64 messageId)
{
    // Only the divider's own message consumes it. Ids before it were already
    // read, and ids after it are handled by the next setFirstUnread() from
    // TDLib, so neither may move the divider here.
    if (messageId <= 0 || messageId != m_firstUnread)
        return;

    clear();
}

void UnreadDividerModel::clear()
{
    if (m_firstUnread == 0 && m_visible)
        return;

    m_firstUnread = 0;
    m_visible = true;
    emit changed();
}

} // namespace beygram::telegram
