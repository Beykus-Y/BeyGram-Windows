#pragma once

#include <QObject>
#include <QtGlobal>

namespace beygram::telegram {

/// Tracks where the "unread messages" divider belongs in one chat.
///
/// Telegram reports the first unread message of a chat as a message id, and the
/// view draws a divider immediately before that row. The divider only makes
/// sense while the chat still has unread messages, so the model stores a single
/// id (zero means "no divider") plus a visibility flag that lets the view hide
/// the divider without forgetting its position, e.g. while the user scrolls
/// through older history.
class UnreadDividerModel final : public QObject {
    Q_OBJECT

public:
    explicit UnreadDividerModel(QObject* parent = nullptr);

    /// Places the divider before messageId. Non-positive ids are ignored; use
    /// clear() to remove the divider. The position may move in either
    /// direction because TDLib is authoritative about what is still unread.
    void setFirstUnread(qint64 messageId);
    [[nodiscard]] qint64 firstUnread() const;

    /// Shows or hides the divider without forgetting its position. The divider
    /// starts visible; clear() restores that default.
    void setVisible(bool visible);
    [[nodiscard]] bool visible() const;

    /// True only for the row the divider belongs in front of, and only while
    /// the divider is visible. Every other id, including zero and negative
    /// ids, returns false.
    [[nodiscard]] bool shouldShowBefore(qint64 messageId) const;

    /// Marks messageId as read. When it is the divider's own message the
    /// divider is consumed and the model returns to its default state; any
    /// other id is a silent no-op.
    void markRead(qint64 messageId);

    /// Removes the divider and restores the default visible state, e.g. when
    /// the chat is closed or everything has been read.
    void clear();

signals:
    /// Emitted whenever the divider position or its visibility actually
    /// changed.
    void changed();

private:
    qint64 m_firstUnread = 0;
    bool m_visible = true;
};

} // namespace beygram::telegram
