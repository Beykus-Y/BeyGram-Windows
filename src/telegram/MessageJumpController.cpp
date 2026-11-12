#include "MessageJumpController.h"

namespace beygram::telegram {

namespace {

bool validId(qint64 id)
{
    return id > 0;
}

} // namespace

MessageJumpController::MessageJumpController(QObject* parent)
    : QObject(parent)
{
}

void MessageJumpController::begin(qint64 chatId, qint64 messageId)
{
    if (!validId(chatId) || !validId(messageId))
        return;

    if (m_chatId == chatId && m_target == messageId && m_highlight == messageId)
        return;

    m_chatId = chatId;
    m_target = messageId;
    m_highlight = messageId;
    emit changed();
}

bool MessageJumpController::active() const
{
    return validId(m_chatId) && validId(m_target);
}

qint64 MessageJumpController::chatId() const
{
    return m_chatId;
}

qint64 MessageJumpController::targetId() const
{
    return m_target;
}

qint64 MessageJumpController::highlightId() const
{
    return m_highlight;
}

void MessageJumpController::setHighlight(qint64 messageId)
{
    if (!active() || !validId(messageId))
        return;

    if (m_highlight == messageId)
        return;

    m_highlight = messageId;
    emit changed();
}

void MessageJumpController::finish()
{
    if (!active())
        return;

    m_chatId = 0;
    m_target = 0;
    m_highlight = 0;
    emit changed();
}

void MessageJumpController::cancel()
{
    finish();
}

} // namespace beygram::telegram
