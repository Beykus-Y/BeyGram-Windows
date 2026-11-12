#include "ChatListSortController.h"

#include <QString>

#include <algorithm>

namespace beygram::telegram {
namespace {

// Negative when left belongs before right, positive when it belongs after, and
// zero when the two chats tie on the current key. Direction is applied by the
// caller so ties stay visible to the stable sort.
int compareByMode(const ChatData& left, const ChatData& right, ChatSortMode mode)
{
    switch (mode) {
    case ChatSortMode::Position:
        if (left.order == right.order)
            return 0;
        return left.order < right.order ? -1 : 1;
    case ChatSortMode::Date:
        if (left.lastMessageDate == right.lastMessageDate)
            return 0;
        return left.lastMessageDate < right.lastMessageDate ? -1 : 1;
    case ChatSortMode::Unread:
        if (left.unreadCount == right.unreadCount)
            return 0;
        return left.unreadCount < right.unreadCount ? -1 : 1;
    case ChatSortMode::Name: {
        const int result = QString::localeAwareCompare(left.title, right.title);
        if (result == 0)
            return 0;
        return result < 0 ? -1 : 1;
    }
    }
    return 0;
}

} // namespace

ChatListSortController::ChatListSortController(QObject* parent)
    : QObject(parent)
{
}

void ChatListSortController::setMode(ChatSortMode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
    emit changed();
}

void ChatListSortController::setAscending(bool ascending)
{
    if (m_ascending == ascending)
        return;
    m_ascending = ascending;
    emit changed();
}

void ChatListSortController::setPinnedFirst(bool pinnedFirst)
{
    if (m_pinnedFirst == pinnedFirst)
        return;
    m_pinnedFirst = pinnedFirst;
    emit changed();
}

QVector<ChatData> ChatListSortController::sorted(QVector<ChatData> chats) const
{
    const ChatSortMode mode = m_mode;
    const bool ascending = m_ascending;
    const bool pinnedFirst = m_pinnedFirst;

    // The pin rule is a primary key, the selected mode is the secondary one.
    // A stable sort keeps the input order for chats that tie on both.
    std::stable_sort(chats.begin(), chats.end(),
                     [mode, ascending, pinnedFirst](const ChatData& left, const ChatData& right) {
                         if (pinnedFirst && left.pinned != right.pinned)
                             return left.pinned;

                         const int comparison = compareByMode(left, right, mode);
                         if (comparison == 0)
                             return false;
                         return ascending ? comparison < 0 : comparison > 0;
                     });

    return chats;
}

} // namespace beygram::telegram
