#pragma once

#include <QHash>
#include <QObject>
#include <QPair>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Holds the per-chat unread state that drives the app badge.
///
/// Each tracked chat stores its unread count together with the muted flag, so
/// the badge can report both the full total and the share that is allowed to
/// raise a notification. Counts follow the UnreadCounter conventions: a
/// negative value is clamped to zero and sums saturate at the int range instead
/// of wrapping.
///
/// A chat whose unread count reaches zero keeps its entry, because the muted
/// flag still has to be remembered for the next message. Only removeChat()
/// drops the entry, so chatCount() counts tracked chats rather than unread
/// ones.
///
/// Every mutator is a no-op when it would not change the stored state, so
/// changed() fires once per real transition and the badge is never repainted
/// for nothing.
class NotificationBadgeController final : public QObject {
    Q_OBJECT

public:
    explicit NotificationBadgeController(QObject* parent = nullptr);

    /// Records the unread count and muted flag of one chat. A zero chat id is
    /// ignored, and a negative unread count is clamped to zero. Repeating the
    /// value the chat already has changes nothing.
    void setChatUnread(qint64 chatId, int unread, bool muted);

    /// Drops a tracked chat. Unknown ids are ignored.
    void removeChat(qint64 chatId);

    /// Unread messages across every tracked chat, muted or not.
    [[nodiscard]] int totalUnread() const;

    /// Unread messages of the chats that are not muted.
    [[nodiscard]] int unmutedUnread() const;

    /// Number of tracked chats, including chats that are currently at zero.
    [[nodiscard]] int chatCount() const;

    /// The label for the badge: empty at zero, the exact number up to maxShown,
    /// and "<maxShown>+" above it. maxShown below 1 is treated as 1.
    [[nodiscard]] QString badgeText(int maxShown = 99) const;

    /// Whether any tracked chat has unread messages.
    [[nodiscard]] bool hasBadge() const;

    /// Forgets every tracked chat. Does nothing when nothing is tracked.
    void clear();

signals:
    /// Emitted whenever a count, a muted flag or the tracked set actually
    /// changed.
    void changed();

private:
    QHash<qint64, QPair<int, bool>> m_chats;
};

} // namespace beygram::telegram
