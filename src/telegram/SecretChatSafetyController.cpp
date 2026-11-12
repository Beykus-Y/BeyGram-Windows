#include "SecretChatSafetyController.h"

#include <algorithm>

namespace beygram::telegram {
namespace {

// TDLib's self-destruct timer bounds: off, or up to one week in seconds.
constexpr int kMinTtlSeconds = 0;
constexpr int kMaxTtlSeconds = 604800;

} // namespace

SecretChatSafetyController::SecretChatSafetyController(QObject* parent)
    : QObject(parent)
{
}

void SecretChatSafetyController::setChat(qint64 chatId, bool secret)
{
    // Chat id 0 is "no chat", so loading it unloads the previous one instead of
    // leaving a chat id that no chat owns.
    if (chatId == 0) {
        clear();
        return;
    }

    if (m_chatId == chatId && m_secret == secret)
        return;

    m_chatId = chatId;
    m_secret = secret;
    emit changed();
}

void SecretChatSafetyController::setTtlSeconds(int seconds)
{
    // TDLib rejects a TTL outside its range, so an out-of-range value is pulled
    // to the nearest legal one instead of being stored and failing later.
    const int clamped = std::clamp(seconds, kMinTtlSeconds, kMaxTtlSeconds);
    if (m_ttl == clamped)
        return;

    m_ttl = clamped;
    emit changed();
}

void SecretChatSafetyController::setScreenshotAllowed(bool allowed)
{
    if (m_screenshots == allowed)
        return;

    m_screenshots = allowed;
    emit changed();
}

bool SecretChatSafetyController::shouldWarnBeforeScreenshot() const
{
    // Only a secret chat can tell the other side about a screenshot, so a
    // regular chat never needs the warning.
    return m_secret && m_screenshots;
}

bool SecretChatSafetyController::notificationsAllowed() const
{
    return !m_secret;
}

bool SecretChatSafetyController::copyAllowed() const
{
    return !m_secret;
}

void SecretChatSafetyController::clear()
{
    const bool alreadyDefault =
        m_chatId == 0 && !m_secret && m_screenshots && m_ttl == 0;
    if (alreadyDefault)
        return;

    m_chatId = 0;
    m_secret = false;
    m_screenshots = true;
    m_ttl = 0;
    emit changed();
}

} // namespace beygram::telegram
