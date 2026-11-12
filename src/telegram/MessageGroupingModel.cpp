#include "MessageGroupingModel.h"

#include <QDate>
#include <QDateTime>
#include <QTime>

#include <utility>

namespace beygram::telegram {

namespace {

// The local calendar day of a Unix timestamp. An out-of-range timestamp yields
// an invalid date, which the caller treats as "cannot continue a day".
[[nodiscard]] QDate calendarDay(qint64 secsSinceEpoch)
{
    return QDateTime::fromSecsSinceEpoch(secsSinceEpoch).date();
}

// Midnight of `day` in local time, in Unix seconds. An invalid day has no
// start, so it falls back to the epoch.
[[nodiscard]] qint64 startOfDaySecs(const QDate& day)
{
    if (!day.isValid())
        return 0;
    return QDateTime(day, QTime(0, 0)).toSecsSinceEpoch();
}

[[nodiscard]] bool sameRow(const GroupingRow& left, const GroupingRow& right)
{
    return left.messageId == right.messageId
        && left.showDateSeparator == right.showDateSeparator
        && left.separatorDate == right.separatorDate
        && left.showUnreadDivider == right.showUnreadDivider
        && left.sameSenderAsPrevious == right.sameSenderAsPrevious;
}

[[nodiscard]] bool sameRows(const QVector<GroupingRow>& left, const QVector<GroupingRow>& right)
{
    if (left.size() != right.size())
        return false;
    for (qsizetype index = 0; index < left.size(); ++index) {
        if (!sameRow(left.at(index), right.at(index)))
            return false;
    }
    return true;
}

} // namespace

MessageGroupingModel::MessageGroupingModel(QObject* parent)
    : QObject(parent)
{
}

void MessageGroupingModel::setMessages(QVector<MessageData> messages, qint64 firstUnreadMessageId)
{
    QVector<GroupingRow> nextRows;
    nextRows.reserve(messages.size());

    bool havePrevious = false;
    qint64 previousSenderId = 0;
    QDate previousDay;
    bool dividerPlaced = firstUnreadMessageId <= 0;

    for (const MessageData& message : messages) {
        GroupingRow row;
        row.messageId = message.id;

        const QDate day = calendarDay(message.date);
        // A timestamp we cannot place on a calendar always starts a section,
        // and so does the first message of the list.
        row.showDateSeparator = !havePrevious || !day.isValid() || day != previousDay;
        if (row.showDateSeparator)
            row.separatorDate = startOfDaySecs(day);

        if (!dividerPlaced && message.id == firstUnreadMessageId) {
            row.showUnreadDivider = true;
            dividerPlaced = true;
        }

        row.sameSenderAsPrevious = havePrevious
            && message.senderUserId != 0
            && message.senderUserId == previousSenderId;

        nextRows.push_back(row);

        havePrevious = true;
        previousSenderId = message.senderUserId;
        previousDay = day;
    }

    if (sameRows(nextRows, m_rows))
        return;

    m_rows = std::move(nextRows);
    emit changed();
}

QVector<GroupingRow> MessageGroupingModel::rows() const
{
    return m_rows;
}

int MessageGroupingModel::count() const
{
    return static_cast<int>(m_rows.size());
}

int MessageGroupingModel::unreadDividerIndex() const
{
    for (qsizetype index = 0; index < m_rows.size(); ++index) {
        if (m_rows.at(index).showUnreadDivider)
            return static_cast<int>(index);
    }
    return -1;
}

void MessageGroupingModel::clear()
{
    if (m_rows.isEmpty())
        return;
    m_rows.clear();
    emit changed();
}

} // namespace beygram::telegram
