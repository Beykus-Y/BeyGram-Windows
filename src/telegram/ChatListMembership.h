#pragma once

#include <QHash>
#include <QJsonObject>
#include <QtGlobal>

namespace beygram::telegram {

/// One chat's position inside a single chat list. TDLib's order is an int64
/// where a larger value sorts higher; order 0 means the chat is not in that
/// list at all.
struct ChatListPosition {
    qint64 order = 0;
    bool pinned = false;
};

/// The chat lists one chat belongs to, reduced from TDLib chat positions. The
/// main and archive lists are named because TDLib addresses them without an
/// identifier; folder lists are keyed by their chat_folder_id.
struct ChatListMembershipState {
    bool inMain = false;
    bool inArchive = false;
    QHash<qint64, ChatListPosition> folderPositions;
};

/// Parses one updateChatPosition payload (or any object carrying a positions
/// array, such as a chat). A `positions` array is the chat's complete
/// membership, so it replaces the state instead of merging with it; a single
/// `position` object is applied on top of an empty state. The update type is
/// not checked here, so callers should gate on it first. Missing or malformed
/// fields leave the corresponding list untouched.
[[nodiscard]] ChatListMembershipState membershipFromPositionUpdate(const QJsonObject& update);

/// Applies one chatPosition object to a membership state and returns the
/// result. `list` selects the chat list (chatListMain, chatListArchive or
/// chatListFolder with a chat_folder_id); `order` and `is_pinned` are read from
/// the position. An order of 0 removes the chat from that list. Lists the
/// position does not name, and unknown or malformed lists, keep their previous
/// membership. No logging, no side effects.
[[nodiscard]] ChatListMembershipState applyPosition(ChatListMembershipState state,
                                                    const QJsonObject& position);

} // namespace beygram::telegram
