#pragma once

#include "TelegramTypes.h"

#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One rendered line of the message list: a message plus the chrome that is
/// drawn above it.
///
/// A row never carries text of its own; the view looks the message up by
/// `messageId`. `separatorDate` is the local start of the separator's day in
/// Unix seconds, so the view can format it without redoing the date maths.
struct GroupingRow {
    qint64 messageId = 0;
    bool showDateSeparator = false;
    qint64 separatorDate = 0;
    bool showUnreadDivider = false;
    bool sameSenderAsPrevious = false;
};

/// Computes the date separators, the unread divider and the sender grouping of
/// a message list.
///
/// The model is a pure projection of the messages it was given: it keeps the
/// input order (it never sorts, so a caller that shows newest-first keeps
/// newest-first) and derives one row per message. A date separator is shown on
/// the first row and whenever the local calendar day of a message differs from
/// the day of the message before it, so a day that appears twice in an
/// out-of-order list gets a separator both times. The unread divider is placed
/// on the first row whose message id equals the id passed to `setMessages`,
/// once at most, and is dropped entirely when that id is zero or absent.
/// `sameSenderAsPrevious` is true when the sender id repeats the previous
/// message's id; a zero sender (a service message or an unknown author) never
/// matches, and the first row is never a continuation.
class MessageGroupingModel final : public QObject {
    Q_OBJECT

public:
    explicit MessageGroupingModel(QObject* parent = nullptr);

    /// Rebuilds the rows from `messages`, which are taken in the order given.
    /// `firstUnreadMessageId` marks where the unread divider belongs; zero
    /// means "no divider". Emits changed() only when the resulting rows differ
    /// from the current ones, so a refresh that changes nothing stays silent.
    void setMessages(QVector<MessageData> messages, qint64 firstUnreadMessageId = 0);

    [[nodiscard]] QVector<GroupingRow> rows() const;
    [[nodiscard]] int count() const;

    /// Index into rows() of the row carrying the unread divider, or -1 when no
    /// row has it.
    [[nodiscard]] int unreadDividerIndex() const;

    /// Drops every row. Emits changed() only when there was something to drop.
    void clear();

signals:
    /// Emitted whenever the rows actually changed.
    void changed();

private:
    QVector<GroupingRow> m_rows;
};

} // namespace beygram::telegram
