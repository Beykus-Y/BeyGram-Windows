#include "DataUsageModel.h"

#include <algorithm>
#include <utility>

namespace beygram::telegram {

namespace {

bool sameRow(const UsageRow& lhs, const UsageRow& rhs)
{
    return lhs.category == rhs.category && lhs.bytesSent == rhs.bytesSent
        && lhs.bytesReceived == rhs.bytesReceived;
}

bool sameRows(const QVector<UsageRow>& lhs, const QVector<UsageRow>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    for (qsizetype index = 0; index < lhs.size(); ++index) {
        if (!sameRow(lhs.at(index), rhs.at(index)))
            return false;
    }

    return true;
}

} // namespace

DataUsageModel::DataUsageModel(QObject* parent)
    : QObject(parent)
{
}

void DataUsageModel::setRows(QVector<UsageRow> rows)
{
    QVector<UsageRow> prepared;
    prepared.reserve(rows.size());

    for (UsageRow row : rows) {
        if (row.category.isEmpty())
            continue;

        row.bytesSent = std::max<qint64>(0, row.bytesSent);
        row.bytesReceived = std::max<qint64>(0, row.bytesReceived);

        const bool duplicate = std::any_of(
            prepared.cbegin(), prepared.cend(), [&row](const UsageRow& existing) {
                return existing.category == row.category;
            });
        if (duplicate)
            continue;

        prepared.append(row);
    }

    if (sameRows(prepared, m_rows))
        return;

    m_rows = std::move(prepared);
    emit changed();
}

QVector<UsageRow> DataUsageModel::rows() const
{
    return m_rows;
}

int DataUsageModel::count() const
{
    return static_cast<int>(m_rows.size());
}

qint64 DataUsageModel::totalBytes() const
{
    qint64 total = 0;

    for (const UsageRow& row : m_rows)
        total += row.bytesSent + row.bytesReceived;

    return total;
}

qint64 DataUsageModel::bytesFor(const QString& category) const
{
    const auto match = std::find_if(
        m_rows.cbegin(), m_rows.cend(), [&category](const UsageRow& row) {
            return row.category == category;
        });

    if (match == m_rows.cend())
        return 0;

    return match->bytesSent + match->bytesReceived;
}

void DataUsageModel::reset()
{
    if (m_rows.isEmpty())
        return;

    m_rows.clear();
    emit changed();
}

QString DataUsageModel::formatBytes(qint64 bytes)
{
    if (bytes < 0)
        return QStringLiteral("0 B");

    // The ladder stops at GB, so anything larger stays expressed in gigabytes
    // rather than growing a unit the screen never shows.
    static constexpr qint64 kibibyte = 1024;
    static constexpr int maxUnit = 3;

    double value = static_cast<double>(bytes);
    int unit = 0;

    while (value >= static_cast<double>(kibibyte) && unit < maxUnit) {
        value /= static_cast<double>(kibibyte);
        ++unit;
    }

    if (unit == 0)
        return QStringLiteral("%1 B").arg(bytes);

    static const char* const units[] = {"B", "KB", "MB", "GB"};
    return QStringLiteral("%1 %2")
        .arg(QString::number(value, 'f', 1), QLatin1String(units[unit]));
}

} // namespace beygram::telegram
