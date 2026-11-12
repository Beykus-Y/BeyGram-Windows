#include "ChannelStatistics.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>

namespace beygram::telegram {
namespace {

// The graph keys this client renders, in display order. Iterating this list
// rather than the JSON object keeps graphNames() stable across responses.
constexpr const char* kGraphKeys[] = {
    "top_hours_graph",
    "growth_graph",
    "followers_graph",
    "interaction_graph",
    "views_by_source_graph",
    "languages_graph",
    "views_by_hour_graph",
};

// 2^63. qint64::max() is not exactly representable as a double, so the upper
// bound is exclusive: a value below it is safe to cast.
constexpr double kInt64Limit = 9223372036854775808.0;

bool isIntegerDouble(const QJsonValue& value, qint64& result)
{
    if (!value.isDouble())
        return false;

    const double number = value.toDouble();
    if (!std::isfinite(number) || std::trunc(number) != number)
        return false;
    if (number < -kInt64Limit || number >= kInt64Limit)
        return false;

    result = static_cast<qint64>(number);
    return true;
}

bool isFiniteDouble(const QJsonValue& value, double& result)
{
    if (!value.isDouble())
        return false;

    const double number = value.toDouble();
    if (!std::isfinite(number))
        return false;

    result = number;
    return true;
}

/// Reads the parallel x/y arrays of one graph. Indexes beyond the shorter
/// array name no complete point and are ignored; an entry of the wrong type is
/// skipped without discarding its neighbours.
QVector<StatisticsGraphPoint> pointsFromGraph(const QJsonObject& graph)
{
    const QJsonArray xValues = graph.value(QStringLiteral("x")).toArray();
    const QJsonArray yValues = graph.value(QStringLiteral("y")).toArray();
    const qsizetype count = std::min(xValues.size(), yValues.size());

    QVector<StatisticsGraphPoint> points;
    points.reserve(count);
    for (qsizetype index = 0; index < count; ++index) {
        qint64 date = 0;
        double value = 0;
        if (!isIntegerDouble(xValues.at(index), date))
            continue;
        if (!isFiniteDouble(yValues.at(index), value))
            continue;
        points.append(StatisticsGraphPoint{date, value});
    }
    return points;
}

} // namespace

QJsonObject getChatStatisticsRequest(qint64 chatId, bool isDark)
{
    if (chatId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getChatStatistics")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("is_dark"), isDark},
    };
}

QJsonObject getMessageStatisticsRequest(qint64 chatId, qint64 messageId, bool isDark)
{
    if (chatId <= 0 || messageId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getMessageStatistics")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
        {QStringLiteral("is_dark"), isDark},
    };
}

ChatStatisticsSnapshot chatStatisticsFromResponse(const QJsonObject& response)
{
    ChatStatisticsSnapshot snapshot;

    const QJsonObject statistics = response.value(QStringLiteral("statistics")).toObject();
    if (statistics.isEmpty())
        return snapshot;

    const QJsonObject period = statistics.value(QStringLiteral("period")).toObject();
    snapshot.periodStart = jsonInt64(period.value(QStringLiteral("min_date")));
    snapshot.periodEnd = jsonInt64(period.value(QStringLiteral("max_date")));

    snapshot.graphs.reserve(static_cast<qsizetype>(std::size(kGraphKeys)));
    for (const char* key : kGraphKeys) {
        StatisticsGraph graph;
        graph.name = QString::fromLatin1(key);
        graph.points =
            pointsFromGraph(statistics.value(graph.name).toObject());
        // A graph with no usable point has nothing a view could draw; keeping
        // it would only produce an empty chart with a title.
        if (!graph.points.isEmpty())
            snapshot.graphs.append(std::move(graph));
    }

    return snapshot;
}

} // namespace beygram::telegram
