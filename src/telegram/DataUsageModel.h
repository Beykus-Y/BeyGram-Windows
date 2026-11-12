#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One line of the data-usage breakdown: a category name plus the traffic it
/// accounts for.
///
/// `category` is the already-localized label ("Photos", "Videos"), `bytesSent`
/// and `bytesReceived` are raw byte counts. The struct holds no pointers, so it
/// can be cached per account and copied across threads.
struct UsageRow {
    QString category;
    qint64 bytesSent = 0;
    qint64 bytesReceived = 0;
};

/// Holds the per-category traffic breakdown shown on the data-usage screen.
///
/// The model is a plain value holder: it stores already-parsed numbers and never
/// touches TDLib, the network or the UI. Rows with an empty category carry
/// nothing a view could render, so they are dropped on assignment; categories
/// are de-duplicated exactly (case-sensitive), the first row wins, and the
/// incoming order is otherwise kept. Negative byte counts are clamped to 0 so
/// the totals a view renders can never shrink below zero.
class DataUsageModel final : public QObject {
    Q_OBJECT

public:
    explicit DataUsageModel(QObject* parent = nullptr);

    /// Replaces the breakdown, dropping rows with an empty category, later
    /// duplicates of an earlier category, and clamping negative byte counts to
    /// 0. Emits changed() only when the resulting rows differ from the stored
    /// ones.
    void setRows(QVector<UsageRow> rows);

    /// Returns a copy of the stored rows, in display order.
    [[nodiscard]] QVector<UsageRow> rows() const;

    /// Number of stored rows; one per distinct non-empty category.
    [[nodiscard]] int count() const;

    /// Sum of bytesSent and bytesReceived over every stored row. Returns 0 when
    /// the model is empty.
    [[nodiscard]] qint64 totalBytes() const;

    /// Sum of bytesSent and bytesReceived for `category`. Categories are unique
    /// after de-duplication, so this is that one row's traffic. Returns 0 when
    /// the category is not stored.
    [[nodiscard]] qint64 bytesFor(const QString& category) const;

    /// Drops every row, emitting changed() once when there was anything to drop.
    /// Resetting an already empty model is a no-op.
    void reset();

    /// Renders a byte count for display, using a 1024 base and the B/KB/MB/GB
    /// ladder. Values below 1 KB keep no decimals ("512 B"), larger ones use one
    /// ("1.5 MB"); the unit stops growing at GB. Negative input renders as
    /// "0 B".
    static QString formatBytes(qint64 bytes);

signals:
    void changed();

private:
    QVector<UsageRow> m_rows;
};

} // namespace beygram::telegram
