#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One line of the storage breakdown: what a group of cached files is (a TDLib
/// file kind such as "photo" or "document"), how many files it holds and how
/// much room they take on disk.
struct StorageRow {
    QString kind;
    int fileCount = 0;
    qint64 bytes = 0;
};

/// The storage usage breakdown behind the settings screen.
///
/// A kind identifies a row, so setRows() keeps the first copy of a repeated
/// kind and drops rows that name no kind. A negative count cannot describe
/// stored files, so it is clamped to 0. The order the rows arrive in is the
/// order they are stored and shown in.
///
/// totalFiles() and totalBytes() saturate instead of overflowing, so a
/// breakdown that sums past the range of their types still reports a usable
/// maximum rather than a wrapped value.
///
/// sortedBySize() returns a re-ordered copy and leaves the stored rows alone.
/// The sort is stable, so rows of equal size keep the order they were stored
/// in.
///
/// Every mutation that changes the stored rows emits changed(); a call that
/// leaves the rows as they were stays silent.
class StorageStatsModel final : public QObject {
    Q_OBJECT

public:
    explicit StorageStatsModel(QObject* parent = nullptr);

    /// Replaces the rows with a cleaned copy of \a rows: empty kinds are
    /// dropped, repeated kinds keep their first copy and negative counts are
    /// clamped to 0.
    void setRows(QVector<StorageRow> rows);
    [[nodiscard]] QVector<StorageRow> rows() const;
    [[nodiscard]] int count() const;
    /// Files across every row, saturated at INT_MAX.
    [[nodiscard]] int totalFiles() const;
    /// Bytes across every row, saturated at INT64_MAX.
    [[nodiscard]] qint64 totalBytes() const;
    /// A copy of the rows ordered by size, largest first by default; rows of
    /// equal size keep their stored order.
    [[nodiscard]] QVector<StorageRow> sortedBySize(bool descending = true) const;
    /// Drops every row.
    void clear();

signals:
    void changed();

private:
    QVector<StorageRow> m_rows;
};

} // namespace beygram::telegram
