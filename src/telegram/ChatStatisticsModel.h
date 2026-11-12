#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace beygram::telegram {

/// One labelled number shown in a chat's statistics screen.
///
/// `label` is the already-localized caption ("Messages", "Shares"), `value` is
/// the raw number, and `unit` is an optional suffix ("%", "MB") that may be
/// empty. The struct holds no pointers, so it can be cached per account and
/// copied across threads.
struct StatisticsValue {
    QString label;
    double value = 0;
    QString unit;
};

/// Holds one chat's statistics: a labelled overview plus an optional chart.
///
/// The model is a plain value holder: it stores already-parsed values and never
/// touches TDLib, the network or the UI. Overview entries with an empty label
/// carry nothing a view could render, so they are dropped on assignment; labels
/// are de-duplicated exactly (case-sensitive), the first entry wins, and the
/// incoming order is otherwise kept. The chart keeps its labels verbatim and
/// always stores one series point per label, so a view can index both lists with
/// the same position.
class ChatStatisticsModel final : public QObject {
    Q_OBJECT

public:
    explicit ChatStatisticsModel(QObject* parent = nullptr);

    /// Replaces the overview, dropping entries with an empty label and later
    /// duplicates of an earlier label. Emits changed() only when the resulting
    /// list differs from the stored one.
    void setOverview(QVector<StatisticsValue> values);

    /// Replaces the chart. Labels are stored as given; the series is resized to
    /// the label count, truncated when longer and padded with 0.0 when shorter.
    /// Emits changed() only when the resulting chart differs from the stored one.
    void setChart(QStringList labels, QVector<double> series);

    /// Returns a copy of the stored overview, in display order.
    [[nodiscard]] QVector<StatisticsValue> overview() const;

    /// Returns a copy of the stored chart labels.
    [[nodiscard]] QStringList chartLabels() const;

    /// Returns a copy of the stored chart series, aligned with chartLabels().
    [[nodiscard]] QVector<double> chartSeries() const;

    /// Whether the model holds anything to render: an overview entry or a chart.
    [[nodiscard]] bool hasData() const;

    /// Largest overview value or chart point, with negatives treated as 0.
    /// Returns 0 when the model is empty. Stored values are never rewritten.
    [[nodiscard]] double maxValue() const;

    /// Drops the overview and the chart, emitting changed() once when there was
    /// anything to drop. Clearing an already empty model is a no-op.
    void clear();

signals:
    void changed();

private:
    QVector<StatisticsValue> m_overview;
    QStringList m_labels;
    QVector<double> m_series;
};

} // namespace beygram::telegram
