#include "ChatNotificationController.h"

#include "ChatNotificationRequests.h"

namespace beygram::telegram {

ChatNotificationController::ChatNotificationController(QObject* parent)
    : QObject(parent)
{
}

void ChatNotificationController::setChat(qint64 chatId, bool muted, bool sound, bool preview)
{
    const bool wasDirty = dirty();
    const bool sameChat = m_chatId == chatId;
    const bool sameFlags =
        m_muted == muted && m_sound == sound && m_preview == preview;

    m_chatId = chatId;
    m_muted = muted;
    m_sound = sound;
    m_preview = preview;

    // The freshly loaded flags are the saved ones: loading a chat never leaves
    // an unsaved edit behind, and a reload also discards the previous edit.
    o_muted = muted;
    o_sound = sound;
    o_preview = preview;

    if (!sameChat || !sameFlags || wasDirty)
        emit changed();
}

void ChatNotificationController::setMuted(bool muted)
{
    if (m_chatId == 0 || m_muted == muted)
        return;
    m_muted = muted;
    emit changed();
}

void ChatNotificationController::setSound(bool sound)
{
    if (m_chatId == 0 || m_sound == sound)
        return;
    m_sound = sound;
    emit changed();
}

void ChatNotificationController::setPreview(bool preview)
{
    if (m_chatId == 0 || m_preview == preview)
        return;
    m_preview = preview;
    emit changed();
}

bool ChatNotificationController::dirty() const
{
    if (m_chatId == 0)
        return false;
    return m_muted != o_muted || m_sound != o_sound || m_preview != o_preview;
}

QJsonObject ChatNotificationController::request() const
{
    if (!dirty())
        return {};

    ChatNotificationState state;
    state.muted = m_muted;
    // This screen edits the chat's own mute. Inheriting it would make TDLib
    // ignore mute_for and the mute would not stick.
    state.useDefaultMute = false;
    state.sound = m_sound;
    state.showPreview = m_preview;

    return setChatNotificationSettingsRequest(m_chatId, state);
}

void ChatNotificationController::markSaved()
{
    const bool wasDirty = dirty();

    o_muted = m_muted;
    o_sound = m_sound;
    o_preview = m_preview;

    // Only the dirty flag moved, but that is exactly what a save button shows.
    if (wasDirty)
        emit changed();
}

} // namespace beygram::telegram
