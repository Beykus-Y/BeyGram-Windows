#pragma once

#include <QJsonObject>
#include <QString>

namespace beygram::telegram {

/// The unread totals of the main chat list, as shown on the app badge and the
/// "All chats" entry.
///
/// TDLib reports the totals through two separate updates: updateUnreadChatCount
/// carries the number of unread chats, updateUnreadMessageCount the number of
/// unread messages. Both also carry chat_list, so a value for the archive or a
/// folder describes a different list than this aggregate; only the main list
/// (chatListMain, or a missing list which TDLib treats as the main one)
/// contributes here.
struct UnreadCounts {
    int chats = 0;         // unread chats in the main list
    int messages = 0;      // unread messages in the main list
    int mentions = 0;      // unread mentions in the main list
    bool markedUnread = false; // at least one chat is manually marked unread
};

/// Parses updateUnreadChatCount or updateUnreadMessageCount into main-list
/// totals.
///
/// Only the counter named by the update is filled: a chat-count update sets
/// `chats`, a message-count update sets `messages`; the remaining counters stay
/// zero, so callers merge the fields they care about instead of replacing the
/// whole state with the result. `unread_count` is used when present and
/// `unread_unmuted_count` is the fallback, so a payload that only reports the
/// unmuted share still yields a count. Mentions come from `mention_count` or
/// `unread_mention_count`, and `markedUnread` from `is_marked_as_unread` or a
/// positive `marked_as_unread_count`. Every count is clamped to zero or above;
/// negative payloads never produce negative totals.
///
/// An update of another type, or one whose chat_list is the archive or a folder,
/// yields a zeroed value.
[[nodiscard]] UnreadCounts unreadCountsFromUpdate(const QJsonObject& update);

/// Applies a delta from one chat list to the main-list totals.
///
/// The delta is ignored unless listType names the main list (an empty string or
/// chatListMain), so the archive and folder updates cannot drift the aggregate.
/// Each counter is clamped to zero after the delta, and an overflowing sum
/// saturates instead of wrapping. `markedUnread` is left untouched because the
/// updates that change it are not count deltas.
[[nodiscard]] UnreadCounts applyUnreadDelta(UnreadCounts current, const QString& listType,
                                            int chatDelta, int messageDelta, int mentionDelta);

} // namespace beygram::telegram
