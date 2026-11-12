#include "ChatStatisticsModel.h"

#include <algorithm>
#include <utility>

namespace beygram::telegram {

namespace {

bool sameValue(const StatisticsValue& lhs, const StatisticsValue& rhs)
{
    return lhs.label == rhs.label && lhs.value == rhs.value && lhs.unit == rhs.unit;
}

bool sameOverview(const QVector<StatisticsValue>& lhs, const QVector<StatisticsValue>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    for (qsizetype index = 0; index < lhs.size(); ++index) {
        if (!sameValue(lhs.at(index), rhs.at(index)))
            return false;
    }

    return true;
}

} // namespace

ChatStatisticsModel::ChatStatisticsModel(QObject* parent)
    : QObject(parent)
{
}

void ChatStatisticsModel::setOverview(QVector<StatisticsValue> values)
{
    QVector<StatisticsValue> filtered;
    filtered.reserve(values.size());

    for (const StatisticsValue& value : values) {
        if (value.label.isEmpty())
            continue;

        const bool duplicate = std::any_of(
            filtered.cbegin(), filtered.cend(), [&value](const StatisticsValue& existing) {
                return existing.label == value.label;
            });
        if (!duplicate)
            filtered.append(value);
    }

    if (sameOverview(filtered, m_overview))
        return;

    m_overview = std::move(filtered);
    emit changed();
}

void ChatStatisticsModel::setChart(QStringList labels, QVector<double> series)
{
    // resize() truncates a longer series and pads a shorter one with 0.0, so one
    // call keeps the series aligned with the labels either way.
    series.resize(labels.size());

    if (labels == m_labels && series == m_series)
        return;

    m_labels = std::move(labels);
    m_series = std::move(series);
    emit changed();
}

QVector<StatisticsValue> ChatStatisticsModel::overview() const
{
    return m_overview;
}

QStringList ChatStatisticsModel::chartLabels() const
{
    return m_labels;
}

QVector<double> ChatStatisticsModel::chartSeries() const
{
    return m_series;
}

bool ChatStatisticsModel::hasData() const
{
    return !m_overview.isEmpty() || !m_labels.isEmpty() || !m_series.isEmpty();
}

double ChatStatisticsModel::maxValue() const
{
    double maximum = 0.0;

    for (const StatisticsValue& value : m_overview)
        maximum = std::max(maximum, std::max(0.0, value.value));

    for (const double point : m_series)
        maximum = std::max(maximum, std::max(0.0, point));

    return maximum;
}

void ChatStatisticsModel::clear()
{
    if (m_overview.isEmpty() && m_labels.isEmpty() && m_series.isEmpty())
        return;

    m_overview.clear();
    m_labels.clear();
    m_series.clear();
    emit changed();
}

} // namespace beygram::telegram
