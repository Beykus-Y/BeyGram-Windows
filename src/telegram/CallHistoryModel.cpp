#include "CallHistoryModel.h"

#include <algorithm>
#include <utility>

namespace beygram::telegram {

namespace {

bool sameRecord(const CallRecord& lhs, const CallRecord& rhs)
{
    return lhs.messageId == rhs.messageId && lhs.chatId == rhs.chatId
        && lhs.outgoing == rhs.outgoing && lhs.video == rhs.video
        && lhs.status == rhs.status && lhs.duration == rhs.duration && lhs.date == rhs.date;
}

bool sameRecords(const QVector<CallRecord>& lhs, const QVector<CallRecord>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    for (qsizetype index = 0; index < lhs.size(); ++index) {
        if (!sameRecord(lhs.at(index), rhs.at(index)))
            return false;
    }

    return true;
}

bool isMissed(const CallRecord& record)
{
    // CallHistory.h reports a missed incoming call as "missed" and an abandoned
    // outgoing one as "cancelled", so the status alone separates the two.
    return record.status == QStringLiteral("missed");
}

} // namespace

CallHistoryModel::CallHistoryModel(QObject* parent)
    : QObject(parent)
{
}

void CallHistoryModel::setRecords(QVector<CallRecord> records)
{
    QVector<CallRecord> prepared;
    prepared.reserve(records.size());

    for (CallRecord record : records) {
        if (record.messageId <= 0)
            continue;

        record.duration = std::max(0, record.duration);

        const bool duplicate = std::any_of(
            prepared.cbegin(), prepared.cend(), [&record](const CallRecord& existing) {
                return existing.messageId == record.messageId;
            });
        if (duplicate)
            continue;

        prepared.append(record);
    }

    std::stable_sort(
        prepared.begin(), prepared.end(), [](const CallRecord& lhs, const CallRecord& rhs) {
            if (lhs.date != rhs.date)
                return lhs.date > rhs.date;
            return lhs.messageId > rhs.messageId;
        });

    if (sameRecords(prepared, m_records))
        return;

    m_records = std::move(prepared);
    emit changed();
}

QVector<CallRecord> CallHistoryModel::records() const
{
    return m_records;
}

int CallHistoryModel::count() const
{
    return static_cast<int>(m_records.size());
}

QVector<CallRecord> CallHistoryModel::missed() const
{
    QVector<CallRecord> result;
    result.reserve(m_records.size());

    for (const CallRecord& record : m_records) {
        if (isMissed(record))
            result.append(record);
    }

    return result;
}

QVector<CallRecord> CallHistoryModel::withUser(qint64 userId) const
{
    QVector<CallRecord> result;
    if (userId <= 0)
        return result;

    result.reserve(m_records.size());

    for (const CallRecord& record : m_records) {
        if (record.chatId == userId)
            result.append(record);
    }

    return result;
}

int CallHistoryModel::missedCount() const
{
    return static_cast<int>(std::count_if(m_records.cbegin(), m_records.cend(), isMissed));
}

void CallHistoryModel::clear()
{
    if (m_records.isEmpty())
        return;

    m_records.clear();
    emit changed();
}

} // namespace beygram::telegram
