#include "NotificationDeduper.h"

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int kMinWindowSeconds = 0;
constexpr int kMaxWindowSeconds = 3600;

// A record is live while `now` is no earlier than the stored time and the
// elapsed seconds are still inside the window. The upper bound is exclusive, so
// a record expires exactly at the boundary and a window of 0 dedupes nothing.
//
// The difference is taken in unsigned arithmetic: `now >= lastTime` guarantees
// the true distance fits in quint64, while a signed subtraction could overflow
// on two extreme timestamps and wrap into a false "inside".
bool isInsideWindow(qint64 lastTime, qint64 now, int window)
{
    if (window <= 0 || now < lastTime)
        return false;
    const quint64 elapsed =
        static_cast<quint64>(now) - static_cast<quint64>(lastTime);
    return elapsed < static_cast<quint64>(window);
}

} // namespace

NotificationDeduper::NotificationDeduper(QObject* parent)
    : QObject(parent)
{
}

void NotificationDeduper::setWindowSeconds(int seconds)
{
    const int clamped = std::clamp(seconds, kMinWindowSeconds, kMaxWindowSeconds);
    if (clamped == m_window)
        return;
    m_window = clamped;
    emit changed();
}

int NotificationDeduper::windowSeconds() const
{
    return m_window;
}

bool NotificationDeduper::shouldNotify(qint64 chatId, qint64 messageId, qint64 nowUnix)
{
    if (chatId <= 0 || messageId <= 0)
        return false;

    auto it = m_last.find(chatId);
    if (it == m_last.end()) {
        m_last.insert(chatId, QPair<qint64, qint64>{messageId, nowUnix});
        emit changed();
        return true;
    }

    // The duplicate is dropped without refreshing the record: the window keeps
    // running from the first delivery instead of sliding with every replay.
    if (isInsideWindow(it->second, nowUnix, m_window) && it->first == messageId)
        return false;

    // A different message, or a stale record: this message notifies and starts
    // its own window in place of the old one, which keeps the chat count steady.
    it->first = messageId;
    it->second = nowUnix;
    return true;
}

void NotificationDeduper::forget(qint64 chatId)
{
    if (m_last.remove(chatId) > 0)
        emit changed();
}

void NotificationDeduper::clear()
{
    if (m_last.isEmpty())
        return;
    m_last.clear();
    emit changed();
}

int NotificationDeduper::trackedCount() const
{
    return int(m_last.size());
}

} // namespace beygram::telegram
