#include "NotificationBadgeController.h"

#include <limits>

namespace beygram::telegram {
namespace {

// Mirrors the UnreadCounter clamping: a negative count is zero and a sum that
// runs past the int range saturates instead of wrapping into a negative badge.
int clampCount(qint64 value)
{
    if (value <= 0)
        return 0;
    constexpr qint64 kMaxCount = std::numeric_limits<int>::max();
    return static_cast<int>(value > kMaxCount ? kMaxCount : value);
}

int saturatedSum(int current, int unread)
{
    const qint64 sum = static_cast<qint64>(current) + static_cast<qint64>(unread);
    return clampCount(sum);
}

} // namespace

NotificationBadgeController::NotificationBadgeController(QObject* parent)
    : QObject(parent)
{
}

void NotificationBadgeController::setChatUnread(qint64 chatId, int unread, bool muted)
{
    if (chatId == 0)
        return;

    const QPair<int, bool> value{clampCount(unread), muted};
    const auto existing = m_chats.constFind(chatId);
    if (existing != m_chats.constEnd() && existing.value() == value)
        return;

    m_chats.insert(chatId, value);
    emit changed();
}

void NotificationBadgeController::removeChat(qint64 chatId)
{
    if (m_chats.remove(chatId) > 0)
        emit changed();
}

int NotificationBadgeController::totalUnread() const
{
    int total = 0;
    for (auto it = m_chats.constBegin(); it != m_chats.constEnd(); ++it)
        total = saturatedSum(total, it.value().first);
    return total;
}

int NotificationBadgeController::unmutedUnread() const
{
    int total = 0;
    for (auto it = m_chats.constBegin(); it != m_chats.constEnd(); ++it) {
        if (!it.value().second)
            total = saturatedSum(total, it.value().first);
    }
    return total;
}

int NotificationBadgeController::chatCount() const
{
    return static_cast<int>(m_chats.size());
}

QString NotificationBadgeController::badgeText(int maxShown) const
{
    const int total = totalUnread();
    if (total <= 0)
        return {};

    const int limit = maxShown < 1 ? 1 : maxShown;
    if (total > limit)
        return QString::number(limit) + QLatin1Char('+');
    return QString::number(total);
}

bool NotificationBadgeController::hasBadge() const
{
    return totalUnread() > 0;
}

void NotificationBadgeController::clear()
{
    if (m_chats.isEmpty())
        return;

    m_chats.clear();
    emit changed();
}

} // namespace beygram::telegram
