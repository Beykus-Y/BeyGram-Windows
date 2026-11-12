#include "StorageStatsModel.h"

#include <QSet>

#include <algorithm>
#include <limits>
#include <utility>

namespace beygram::telegram {
namespace {

constexpr int MaxFiles = std::numeric_limits<int>::max();
constexpr qint64 MaxBytes = std::numeric_limits<qint64>::max();

bool sameRow(const StorageRow& left, const StorageRow& right)
{
    return left.kind == right.kind && left.fileCount == right.fileCount
        && left.bytes == right.bytes;
}

bool sameRows(const QVector<StorageRow>& left, const QVector<StorageRow>& right)
{
    if (left.size() != right.size())
        return false;

    for (qsizetype index = 0; index < left.size(); ++index) {
        if (!sameRow(left.at(index), right.at(index)))
            return false;
    }
    return true;
}

/// A negative count cannot describe stored files, so it is clamped to 0.
StorageRow cleanedRow(const StorageRow& row)
{
    StorageRow cleaned = row;
    if (cleaned.fileCount < 0)
        cleaned.fileCount = 0;
    if (cleaned.bytes < 0)
        cleaned.bytes = 0;
    return cleaned;
}

/// A kind identifies a row, so a repeated kind keeps its first copy and a row
/// without a kind is dropped. The stored order follows the arrival order.
QVector<StorageRow> cleanRows(const QVector<StorageRow>& rows)
{
    QVector<StorageRow> kept;
    kept.reserve(rows.size());

    QSet<QString> seenKinds;
    seenKinds.reserve(rows.size());

    for (const StorageRow& row : rows) {
        if (row.kind.isEmpty())
            continue;
        if (seenKinds.contains(row.kind))
            continue;
        seenKinds.insert(row.kind);
        kept.append(cleanedRow(row));
    }

    return kept;
}

} // namespace

StorageStatsModel::StorageStatsModel(QObject* parent) : QObject(parent)
{
}

void StorageStatsModel::setRows(QVector<StorageRow> rows)
{
    QVector<StorageRow> next = cleanRows(rows);
    // Replacing the rows with an equivalent set is not a change, even when the
    // incoming copy carried values that were cleaned away.
    if (sameRows(next, m_rows))
        return;

    m_rows = std::move(next);
    emit changed();
}

QVector<StorageRow> StorageStatsModel::rows() const
{
    return m_rows;
}

int StorageStatsModel::count() const
{
    return static_cast<int>(m_rows.size());
}

int StorageStatsModel::totalFiles() const
{
    qint64 total = 0;
    for (const StorageRow& row : m_rows) {
        // Both operands are non-negative and bounded by INT_MAX, so the sum
        // below cannot overflow qint64 before it is compared.
        if (total + row.fileCount > MaxFiles)
            return MaxFiles;
        total += row.fileCount;
    }
    return static_cast<int>(total);
}

qint64 StorageStatsModel::totalBytes() const
{
    qint64 total = 0;
    for (const StorageRow& row : m_rows) {
        // The gap to the maximum is checked first, so the addition never wraps.
        if (row.bytes > MaxBytes - total)
            return MaxBytes;
        total += row.bytes;
    }
    return total;
}

QVector<StorageRow> StorageStatsModel::sortedBySize(bool descending) const
{
    QVector<StorageRow> sorted = m_rows;
    // stable_sort keeps rows of equal size in the order they were stored.
    std::stable_sort(sorted.begin(), sorted.end(),
                     [descending](const StorageRow& left, const StorageRow& right) {
                         return descending ? left.bytes > right.bytes : left.bytes < right.bytes;
                     });
    return sorted;
}

void StorageStatsModel::clear()
{
    if (m_rows.isEmpty())
        return;

    m_rows.clear();
    emit changed();
}

} // namespace beygram::telegram
