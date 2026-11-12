#include "ChatBackgroundController.h"

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int kMinPercent = 0;
constexpr int kMaxPercent = 100;

} // namespace

ChatBackgroundController::ChatBackgroundController(QObject* parent)
    : QObject(parent)
{
}

void ChatBackgroundController::setBackground(BackgroundKind kind, const QString& value)
{
    // A non-None background without a value could not be painted, so the whole
    // request is dropped instead of leaving a kind that renders nothing.
    if (kind != BackgroundKind::None && value.trimmed().isEmpty())
        return;

    const QString stored = kind == BackgroundKind::None ? QString() : value;

    if (m_kind == kind && m_value == stored)
        return;

    m_kind = kind;
    m_value = stored;
    emit changed();
}

void ChatBackgroundController::setDimmer(int percent)
{
    const int clamped = std::clamp(percent, kMinPercent, kMaxPercent);
    if (m_dimmer == clamped)
        return;
    m_dimmer = clamped;
    emit changed();
}

void ChatBackgroundController::setBlur(int percent)
{
    const int clamped = std::clamp(percent, kMinPercent, kMaxPercent);
    if (m_blur == clamped)
        return;
    m_blur = clamped;
    emit changed();
}

void ChatBackgroundController::setForChat(qint64 chatId)
{
    if (m_chatId == chatId)
        return;
    m_chatId = chatId;
    emit changed();
}

void ChatBackgroundController::reset()
{
    // reset() is one user action, so it announces itself at most once even when
    // several fields move at the same time.
    if (isDefault() && m_chatId == 0)
        return;

    m_kind = BackgroundKind::None;
    m_value.clear();
    m_dimmer = 0;
    m_blur = 0;
    m_chatId = 0;
    emit changed();
}

bool ChatBackgroundController::isDefault() const
{
    return m_kind == BackgroundKind::None && m_dimmer == 0 && m_blur == 0;
}

} // namespace beygram::telegram
