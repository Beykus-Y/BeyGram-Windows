#pragma once

#include "TelegramTypes.h"

#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// The orders the chat list can be shown in. Position is TDLib's own order,
/// the others are computed locally from the cached ChatData.
enum class ChatSortMode {
    Position,
    Date,
    Unread,
    Name,
};

/// Sorts a copy of the chat list for the view without touching the model.
///
/// sorted() takes the chats by value, performs a stable sort and returns the
/// reordered copy, so equal keys always keep their input order and the caller's
/// vector is never mutated. ascending() picks the direction of every mode
/// (false means descending, which matches Telegram's newest-first default);
/// pinned chats are hoisted above the rest when pinnedFirst() is set. Every
/// setter is a no-op for the value it already holds, so changed() fires once per
/// real transition and views do not re-sort for nothing.
class ChatListSortController final : public QObject {
    Q_OBJECT

public:
    explicit ChatListSortController(QObject* parent = nullptr);

    /// Selects the comparison key. Changing it re-emits changed().
    void setMode(ChatSortMode mode);
    [[nodiscard]] ChatSortMode mode() const { return m_mode; }

    /// Picks the direction applied to the current mode. Defaults to descending.
    void setAscending(bool ascending);
    [[nodiscard]] bool ascending() const { return m_ascending; }

    /// Keeps pinned chats ahead of unpinned ones, whatever the mode is.
    void setPinnedFirst(bool pinnedFirst);
    [[nodiscard]] bool pinnedFirst() const { return m_pinnedFirst; }

    /// Returns a stably sorted copy of the given chats.
    [[nodiscard]] QVector<ChatData> sorted(QVector<ChatData> chats) const;

signals:
    /// Emitted whenever the mode, the direction or the pin rule really changed.
    void changed();

private:
    ChatSortMode m_mode = ChatSortMode::Position;
    bool m_ascending = false;
    bool m_pinnedFirst = true;
};

} // namespace beygram::telegram
