#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One point of a statistics graph.
///
/// `date` is the graph's x value: a UNIX timestamp in seconds for time series
/// and an ordinal (for example an hour of the day) for category axes. `value`
/// is the y value. The struct holds no pointers, so a snapshot can be copied
/// across threads and cached per account.
struct StatisticsGraphPoint {
    qint64 date = 0;
    double value = 0;

    bool operator==(const StatisticsGraphPoint&) const = default;
};

/// One named graph of a chat's statistics.
///
/// `name` is the key the graph arrived under (`growth_graph`,
/// `views_by_source_graph`, ...), so a view can pick a localized title for it
/// without guessing from the position. `points` keeps the server's order.
struct StatisticsGraph {
    QString name;
    QVector<StatisticsGraphPoint> points;

    bool operator==(const StatisticsGraph&) const = default;
};

/// A parsed chat statistics answer: the reported period plus every graph that
/// carried at least one usable point.
struct ChatStatisticsSnapshot {
    qint64 periodStart = 0;
    qint64 periodEnd = 0;
    QVector<StatisticsGraph> graphs;

    bool operator==(const ChatStatisticsSnapshot&) const = default;
};

/// Builds the TDLib request that returns a chat's statistics.
///
/// Shape follows td_api.tl:
///   getChatStatistics chat_id:int53 is_dark:Bool = ChatStatistics;
///
/// `chatId` must be positive; anything else names no chat and returns an empty
/// object, which the caller must not send. `isDark` asks TDLib to colour the
/// graph data for the dark theme. No logging, no secrets and no @extra (the
/// transport adds the correlation id).
[[nodiscard]] QJsonObject getChatStatisticsRequest(qint64 chatId, bool isDark);

/// Builds the TDLib request that returns the statistics of one message.
///
/// Shape follows td_api.tl:
///   getMessageStatistics chat_id:int53 message_id:int53 is_dark:Bool
///                       = MessageStatistics;
///
/// Both ids must be positive; anything else returns an empty object, which the
/// caller must not send. `isDark` selects the theme TDLib colours the graph
/// data for. No logging, no secrets and no @extra.
[[nodiscard]] QJsonObject getMessageStatisticsRequest(qint64 chatId, qint64 messageId,
                                                      bool isDark);

/// Parses a chat statistics answer.
///
/// The graphs live inside the response's `statistics` object. Its `period`
/// object carries `min_date` and `max_date`, and each known graph key carries
/// an object with parallel `x` and `y` arrays:
///
///   statistics: {
///     period: { min_date: int, max_date: int },
///     top_hours_graph: { x: [...], y: [...] },
///     growth_graph: { x: [...], y: [...] },
///     followers_graph: { x: [...], y: [...] },
///     interaction_graph: { x: [...], y: [...] },
///     views_by_source_graph: { x: [...], y: [...] },
///     languages_graph: { x: [...], y: [...] },
///     views_by_hour_graph: { x: [...], y: [...] }
///   }
///
/// Graphs are returned in the key order above, whatever order the JSON object
/// happened to use. A point is kept only when both arrays carry a finite
/// number at that index; x must also be a whole number that fits qint64.
/// Missing, mismatched or malformed entries are skipped individually, and a
/// graph that ends up with no points is dropped entirely. A response without a
/// `statistics` object, such as a TDLib error answer, yields an empty snapshot
/// rather than failing the whole screen.
[[nodiscard]] ChatStatisticsSnapshot chatStatisticsFromResponse(const QJsonObject& response);

} // namespace beygram::telegram
