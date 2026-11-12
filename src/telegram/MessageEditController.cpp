#include "MessageEditController.h"

namespace beygram::telegram {

namespace {

bool validId(qint64 id)
{
    return id > 0;
}

} // namespace

MessageEditController::MessageEditController(QObject* parent)
    : QObject(parent)
{
}

void MessageEditController::begin(qint64 chatId, qint64 messageId, const QString& text)
{
    if (!validId(chatId) || !validId(messageId) || text.trimmed().isEmpty())
        return;

    // Restarting the exact same edit changes nothing observable.
    if (active() && m_chatId == chatId && m_messageId == messageId
        && m_original == text && m_text == text) {
        return;
    }

    m_chatId = chatId;
    m_messageId = messageId;
    m_original = text;
    m_text = text;
    emit changed();
}

void MessageEditController::cancel()
{
    if (!active())
        return;

    m_chatId = 0;
    m_messageId = 0;
    m_text.clear();
    m_original.clear();
    emit changed();
}

bool MessageEditController::active() const
{
    return validId(m_chatId) && validId(m_messageId);
}

qint64 MessageEditController::chatId() const
{
    return m_chatId;
}

qint64 MessageEditController::messageId() const
{
    return m_messageId;
}

QString MessageEditController::text() const
{
    return m_text;
}

void MessageEditController::setText(const QString& text)
{
    if (!active() || m_text == text)
        return;

    m_text = text;
    emit changed();
}

bool MessageEditController::dirty() const
{
    return m_text != m_original;
}

bool MessageEditController::canSave() const
{
    return active() && dirty() && !m_text.trimmed().isEmpty()
        && m_text.length() <= kMaxTextLength;
}

void MessageEditController::markSaved()
{
    if (!active() || !dirty())
        return;

    m_original = m_text;
    emit changed();
}

} // namespace beygram::telegram
