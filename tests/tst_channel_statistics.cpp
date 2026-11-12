#include "telegram/ChannelStatistics.h"
#include "telegram/ChannelStatisticsModel.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

#include <initializer_list>
#include <utility>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

QJsonObject graphJson(std::initializer_list<double> xValues,
                      std::initializer_list<double> yValues)
{
    QJsonArray x;
    QJsonArray y;
    for (const double value : xValues)
        x.append(value);
    for (const double value : yValues)
        y.append(value);
    return QJsonObject{
        {QStringLiteral("x"), x},
        {QStringLiteral("y"), y},
    };
}

QJsonObject responseWithStatistics(const QJsonObject& statistics)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("chatStatisticsChannel")},
        {QStringLiteral("statistics"), statistics},
    };
}

StatisticsGraph makeGraph(const QString& name, QVector<StatisticsGraphPoint> points)
{
    StatisticsGraph graph;
    graph.name = name;
    graph.points = std::move(points);
    return graph;
}

ChatStatisticsSnapshot sampleSnapshot()
{
    ChatStatisticsSnapshot snapshot;
    snapshot.periodStart = 1700000000;
    snapshot.periodEnd = 1702592000;
    snapshot.graphs.append(makeGraph(
        QStringLiteral("growth_graph"),
        {{1700000000, 10.0}, {1700086400, 25.5}}));
    snapshot.graphs.append(makeGraph(
        QStringLiteral("followers_graph"), {{1700000000, 3.0}}));
    return snapshot;
}

} // namespace

class ChannelStatisticsTest final : public QObject {
    Q_OBJECT

private slots:
    void chatRequestShape();
    void chatRequestCarriesLightTheme();
    void chatRequestRejectsNonPositiveId();
    void chatRequestPreservesLargeId();

    void messageRequestShape();
    void messageRequestCarriesLightTheme();
    void messageRequestRejectsNonPositiveIds();
    void messageRequestPreservesLargeIds();

    void parsesPeriodAndEveryGraph();
    void graphNamesFollowDocumentedOrder();
    void keepsPointOrder();
    void skipsMalformedPoints();
    void skipsGraphsWithoutPoints();
    void skipsGraphsOfWrongType();
    void parsesMissingPeriodAsZero();
    void missingStatisticsYieldsEmptySnapshot();
    void nonObjectStatisticsYieldsEmptySnapshot();

    void modelStartsEmpty();
    void modelStoresSnapshot();
    void modelGraphLookup();
    void modelMissingGraphIsEmpty();
    void modelTotalForSumsPoints();
    void modelTotalForMissingNameIsZero();
    void modelFirstDuplicateNameWins();
    void modelSnapshotIsIndependent();

    void setSnapshotEmitsChanged();
    void setSnapshotIsSilentWhenUnchanged();
    void setSnapshotEmitsOnPeriodChange();
    void setSnapshotEmitsOnPointChange();
    void clearDropsSnapshotAndEmits();
    void clearOnEmptyModelIsSilent();
    void clearOnPeriodOnlySnapshotEmits();

    void parsedSnapshotFeedsModel();
};

void ChannelStatisticsTest::chatRequestShape()
{
    const QJsonObject request = serialized(getChatStatisticsRequest(42, true));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getChatStatistics"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(42));
    QVERIFY(request.value(QStringLiteral("is_dark")).toBool());
    QCOMPARE(static_cast<int>(request.size()), 3);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void ChannelStatisticsTest::chatRequestCarriesLightTheme()
{
    const QJsonObject request = getChatStatisticsRequest(7, false);

    QVERIFY(request.contains(QStringLiteral("is_dark")));
    QVERIFY(!request.value(QStringLiteral("is_dark")).toBool());
}

void ChannelStatisticsTest::chatRequestRejectsNonPositiveId()
{
    // A non-positive id names no chat, so the caller gets an empty object
    // instead of a request TDLib would reject.
    QVERIFY(getChatStatisticsRequest(0, false).isEmpty());
    QVERIFY(getChatStatisticsRequest(-1, true).isEmpty());
    QVERIFY(getChatStatisticsRequest(-1001234567890LL, true).isEmpty());
}

void ChannelStatisticsTest::chatRequestPreservesLargeId()
{
    const qint64 chatId = 1234567890123LL;
    const QJsonObject request = serialized(getChatStatisticsRequest(chatId, true));

    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), chatId);
}

void ChannelStatisticsTest::messageRequestShape()
{
    const QJsonObject request = serialized(getMessageStatisticsRequest(42, 900, true));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getMessageStatistics"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(numberAt(request, QStringLiteral("message_id")), qint64(900));
    QVERIFY(request.value(QStringLiteral("is_dark")).toBool());
    QCOMPARE(static_cast<int>(request.size()), 4);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void ChannelStatisticsTest::messageRequestCarriesLightTheme()
{
    const QJsonObject request = getMessageStatisticsRequest(7, 8, false);

    QVERIFY(request.contains(QStringLiteral("is_dark")));
    QVERIFY(!request.value(QStringLiteral("is_dark")).toBool());
}

void ChannelStatisticsTest::messageRequestRejectsNonPositiveIds()
{
    QVERIFY(getMessageStatisticsRequest(0, 5, false).isEmpty());
    QVERIFY(getMessageStatisticsRequest(5, 0, false).isEmpty());
    QVERIFY(getMessageStatisticsRequest(-1, 5, false).isEmpty());
    QVERIFY(getMessageStatisticsRequest(5, -1, false).isEmpty());
}

void ChannelStatisticsTest::messageRequestPreservesLargeIds()
{
    const qint64 chatId = 1234567890123LL;
    const qint64 messageId = 9876543210123LL;
    const QJsonObject request =
        serialized(getMessageStatisticsRequest(chatId, messageId, true));

    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), chatId);
    QCOMPARE(numberAt(request, QStringLiteral("message_id")), messageId);
}

void ChannelStatisticsTest::parsesPeriodAndEveryGraph()
{
    const QJsonObject response = responseWithStatistics(QJsonObject{
        {QStringLiteral("period"),
         QJsonObject{{QStringLiteral("min_date"), 1700000000},
                     {QStringLiteral("max_date"), 1702592000}}},
        {QStringLiteral("top_hours_graph"), graphJson({0, 1}, {10, 20})},
        {QStringLiteral("growth_graph"), graphJson({1700000000}, {5})},
        {QStringLiteral("followers_graph"), graphJson({1700000000, 1700086400}, {1, 2})},
        {QStringLiteral("interaction_graph"), graphJson({3}, {30})},
        {QStringLiteral("views_by_source_graph"), graphJson({4}, {40})},
        {QStringLiteral("languages_graph"), graphJson({5}, {50})},
        {QStringLiteral("views_by_hour_graph"), graphJson({6}, {60})},
    });

    const ChatStatisticsSnapshot snapshot = chatStatisticsFromResponse(response);

    QCOMPARE(snapshot.periodStart, qint64(1700000000));
    QCOMPARE(snapshot.periodEnd, qint64(1702592000));
    QCOMPARE(snapshot.graphs.size(), 7);
    QCOMPARE(snapshot.graphs.at(0).name, QStringLiteral("top_hours_graph"));
    QCOMPARE(snapshot.graphs.at(0).points.size(), 2);
    QCOMPARE(snapshot.graphs.at(0).points.at(0).date, qint64(0));
    QCOMPARE(snapshot.graphs.at(0).points.at(0).value, 10.0);
    QCOMPARE(snapshot.graphs.at(0).points.at(1).date, qint64(1));
    QCOMPARE(snapshot.graphs.at(0).points.at(1).value, 20.0);
}

void ChannelStatisticsTest::graphNamesFollowDocumentedOrder()
{
    const QJsonObject response = responseWithStatistics(QJsonObject{
        {QStringLiteral("views_by_hour_graph"), graphJson({1}, {1})},
        {QStringLiteral("languages_graph"), graphJson({1}, {1})},
        {QStringLiteral("views_by_source_graph"), graphJson({1}, {1})},
        {QStringLiteral("interaction_graph"), graphJson({1}, {1})},
        {QStringLiteral("followers_graph"), graphJson({1}, {1})},
        {QStringLiteral("growth_graph"), graphJson({1}, {1})},
        {QStringLiteral("top_hours_graph"), graphJson({1}, {1})},
    });

    const ChatStatisticsSnapshot snapshot = chatStatisticsFromResponse(response);
    QStringList names;
    for (const StatisticsGraph& graph : snapshot.graphs)
        names.append(graph.name);

    QCOMPARE(names, QStringList({QStringLiteral("top_hours_graph"),
                                 QStringLiteral("growth_graph"),
                                 QStringLiteral("followers_graph"),
                                 QStringLiteral("interaction_graph"),
                                 QStringLiteral("views_by_source_graph"),
                                 QStringLiteral("languages_graph"),
                                 QStringLiteral("views_by_hour_graph")}));
}

void ChannelStatisticsTest::keepsPointOrder()
{
    const QJsonObject response = responseWithStatistics(QJsonObject{
        {QStringLiteral("growth_graph"), graphJson({300, 100, 200}, {3, 1, 2})},
    });

    const ChatStatisticsSnapshot snapshot = chatStatisticsFromResponse(response);
    QCOMPARE(snapshot.graphs.size(), 1);
    QCOMPARE(snapshot.graphs.at(0).points.size(), 3);
    QCOMPARE(snapshot.graphs.at(0).points.at(0).date, qint64(300));
    QCOMPARE(snapshot.graphs.at(0).points.at(1).date, qint64(100));
    QCOMPARE(snapshot.graphs.at(0).points.at(2).date, qint64(200));
}

void ChannelStatisticsTest::skipsMalformedPoints()
{
    QJsonArray malformedX{1, QStringLiteral("bad"), 3, 4, 5.5, 6, true};
    QJsonArray malformedY{10, 20, QStringLiteral("oops"), 40, 50, 60, 70};

    const QJsonObject response = responseWithStatistics(QJsonObject{
        {QStringLiteral("views_by_source_graph"),
         QJsonObject{{QStringLiteral("x"), malformedX},
                     {QStringLiteral("y"), malformedY}}},
        // y is shorter than x: the tail has no complete point.
        {QStringLiteral("languages_graph"),
         QJsonObject{{QStringLiteral("x"), QJsonArray{1, 2, 3}},
                     {QStringLiteral("y"), QJsonArray{10, 20}}}},
    });

    const ChatStatisticsSnapshot snapshot = chatStatisticsFromResponse(response);
    QCOMPARE(snapshot.graphs.size(), 2);

    const StatisticsGraph& views = snapshot.graphs.at(0);
    QCOMPARE(views.name, QStringLiteral("views_by_source_graph"));
    QCOMPARE(views.points.size(), 3);
    QCOMPARE(views.points.at(0).date, qint64(1));
    QCOMPARE(views.points.at(0).value, 10.0);
    QCOMPARE(views.points.at(1).date, qint64(4));
    QCOMPARE(views.points.at(1).value, 40.0);
    QCOMPARE(views.points.at(2).date, qint64(6));
    QCOMPARE(views.points.at(2).value, 60.0);

    const StatisticsGraph& languages = snapshot.graphs.at(1);
    QCOMPARE(languages.name, QStringLiteral("languages_graph"));
    QCOMPARE(languages.points.size(), 2);
    QCOMPARE(languages.points.at(1).date, qint64(2));
    QCOMPARE(languages.points.at(1).value, 20.0);
}

void ChannelStatisticsTest::skipsGraphsWithoutPoints()
{
    const QJsonObject response = responseWithStatistics(QJsonObject{
        {QStringLiteral("period"),
         QJsonObject{{QStringLiteral("min_date"), 100},
                     {QStringLiteral("max_date"), 200}}},
        {QStringLiteral("growth_graph"),
         QJsonObject{{QStringLiteral("x"), QJsonArray{}},
                     {QStringLiteral("y"), QJsonArray{}}}},
        {QStringLiteral("followers_graph"), QJsonObject{}},
        {QStringLiteral("interaction_graph"),
         QJsonObject{{QStringLiteral("x"), QJsonArray{1}},
                     {QStringLiteral("y"), QJsonArray{}}}},
        {QStringLiteral("views_by_source_graph"), graphJson({1}, {2})},
    });

    const ChatStatisticsSnapshot snapshot = chatStatisticsFromResponse(response);

    QCOMPARE(snapshot.periodStart, qint64(100));
    QCOMPARE(snapshot.periodEnd, qint64(200));
    QCOMPARE(snapshot.graphs.size(), 1);
    QCOMPARE(snapshot.graphs.at(0).name, QStringLiteral("views_by_source_graph"));
}

void ChannelStatisticsTest::skipsGraphsOfWrongType()
{
    const QJsonObject response = responseWithStatistics(QJsonObject{
        {QStringLiteral("growth_graph"), QStringLiteral("not an object")},
        {QStringLiteral("followers_graph"), QJsonArray{1, 2, 3}},
        {QStringLiteral("languages_graph"), graphJson({1}, {2})},
    });

    const ChatStatisticsSnapshot snapshot = chatStatisticsFromResponse(response);

    QCOMPARE(snapshot.graphs.size(), 1);
    QCOMPARE(snapshot.graphs.at(0).name, QStringLiteral("languages_graph"));
}

void ChannelStatisticsTest::parsesMissingPeriodAsZero()
{
    const QJsonObject response = responseWithStatistics(QJsonObject{
        {QStringLiteral("growth_graph"), graphJson({1}, {2})},
    });

    const ChatStatisticsSnapshot snapshot = chatStatisticsFromResponse(response);

    QCOMPARE(snapshot.periodStart, qint64(0));
    QCOMPARE(snapshot.periodEnd, qint64(0));
    QCOMPARE(snapshot.graphs.size(), 1);
}

void ChannelStatisticsTest::missingStatisticsYieldsEmptySnapshot()
{
    const ChatStatisticsSnapshot snapshot =
        chatStatisticsFromResponse(QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("chatStatisticsChannel")},
        });

    QCOMPARE(snapshot.periodStart, qint64(0));
    QCOMPARE(snapshot.periodEnd, qint64(0));
    QVERIFY(snapshot.graphs.isEmpty());
}

void ChannelStatisticsTest::nonObjectStatisticsYieldsEmptySnapshot()
{
    const ChatStatisticsSnapshot snapshot =
        chatStatisticsFromResponse(QJsonObject{
            {QStringLiteral("statistics"), 42},
        });

    QVERIFY(snapshot.graphs.isEmpty());
}

void ChannelStatisticsTest::modelStartsEmpty()
{
    const ChannelStatisticsModel model;

    QCOMPARE(model.snapshot().periodStart, qint64(0));
    QCOMPARE(model.snapshot().periodEnd, qint64(0));
    QVERIFY(model.snapshot().graphs.isEmpty());
    QCOMPARE(model.graphCount(), 0);
    QVERIFY(model.graphNames().isEmpty());
    QVERIFY(model.graph(QStringLiteral("growth_graph")).points.isEmpty());
    QCOMPARE(model.totalFor(QStringLiteral("growth_graph")), 0.0);
}

void ChannelStatisticsTest::modelStoresSnapshot()
{
    const ChatStatisticsSnapshot source = sampleSnapshot();
    ChannelStatisticsModel model;
    model.setSnapshot(source);

    const ChatStatisticsSnapshot stored = model.snapshot();
    QCOMPARE(stored.periodStart, qint64(1700000000));
    QCOMPARE(stored.periodEnd, qint64(1702592000));
    QCOMPARE(stored.graphs.size(), 2);
    QCOMPARE(model.graphCount(), 2);
    QCOMPARE(model.graphNames(),
             QStringList({QStringLiteral("growth_graph"),
                          QStringLiteral("followers_graph")}));
}

void ChannelStatisticsTest::modelGraphLookup()
{
    ChannelStatisticsModel model;
    model.setSnapshot(sampleSnapshot());

    const StatisticsGraph growth = model.graph(QStringLiteral("growth_graph"));
    QCOMPARE(growth.name, QStringLiteral("growth_graph"));
    QCOMPARE(growth.points.size(), 2);
    QCOMPARE(growth.points.at(0).date, qint64(1700000000));
    QCOMPARE(growth.points.at(0).value, 10.0);
    QCOMPARE(growth.points.at(1).value, 25.5);
}

void ChannelStatisticsTest::modelMissingGraphIsEmpty()
{
    ChannelStatisticsModel model;
    model.setSnapshot(sampleSnapshot());

    const StatisticsGraph missing = model.graph(QStringLiteral("languages_graph"));
    QVERIFY(missing.name.isEmpty());
    QVERIFY(missing.points.isEmpty());

    // Lookups are case-sensitive.
    QVERIFY(model.graph(QStringLiteral("Growth_Graph")).points.isEmpty());
}

void ChannelStatisticsTest::modelTotalForSumsPoints()
{
    ChatStatisticsSnapshot snapshot = sampleSnapshot();
    snapshot.graphs.append(makeGraph(
        QStringLiteral("interaction_graph"), {{1, 10.0}, {2, -2.5}}));

    ChannelStatisticsModel model;
    model.setSnapshot(snapshot);

    QCOMPARE(model.totalFor(QStringLiteral("growth_graph")), 35.5);
    QCOMPARE(model.totalFor(QStringLiteral("followers_graph")), 3.0);
    QCOMPARE(model.totalFor(QStringLiteral("interaction_graph")), 7.5);
}

void ChannelStatisticsTest::modelTotalForMissingNameIsZero()
{
    ChannelStatisticsModel model;
    model.setSnapshot(sampleSnapshot());

    QCOMPARE(model.totalFor(QStringLiteral("views_by_hour_graph")), 0.0);
}

void ChannelStatisticsTest::modelFirstDuplicateNameWins()
{
    ChatStatisticsSnapshot snapshot;
    snapshot.graphs.append(makeGraph(
        QStringLiteral("growth_graph"), {{1, 5.0}}));
    snapshot.graphs.append(makeGraph(
        QStringLiteral("growth_graph"), {{1, 99.0}}));

    ChannelStatisticsModel model;
    model.setSnapshot(snapshot);

    QCOMPARE(model.graphCount(), 2);
    QCOMPARE(model.graph(QStringLiteral("growth_graph")).points.at(0).value, 5.0);
    QCOMPARE(model.totalFor(QStringLiteral("growth_graph")), 5.0);
}

void ChannelStatisticsTest::modelSnapshotIsIndependent()
{
    ChannelStatisticsModel model;
    model.setSnapshot(sampleSnapshot());

    ChatStatisticsSnapshot copy = model.snapshot();
    copy.periodStart = 1;
    copy.graphs[0].points[0].value = 999.0;

    QCOMPARE(model.snapshot().periodStart, qint64(1700000000));
    QCOMPARE(model.snapshot().graphs.at(0).points.at(0).value, 10.0);

    StatisticsGraph graphCopy = model.graph(QStringLiteral("growth_graph"));
    graphCopy.points.clear();
    QCOMPARE(model.graph(QStringLiteral("growth_graph")).points.size(), 2);
}

void ChannelStatisticsTest::setSnapshotEmitsChanged()
{
    ChannelStatisticsModel model;
    QSignalSpy spy(&model, &ChannelStatisticsModel::changed);

    model.setSnapshot(sampleSnapshot());

    QCOMPARE(spy.count(), 1);
}

void ChannelStatisticsTest::setSnapshotIsSilentWhenUnchanged()
{
    ChannelStatisticsModel model;
    model.setSnapshot(sampleSnapshot());

    QSignalSpy spy(&model, &ChannelStatisticsModel::changed);
    model.setSnapshot(sampleSnapshot());

    QCOMPARE(spy.count(), 0);
}

void ChannelStatisticsTest::setSnapshotEmitsOnPeriodChange()
{
    ChannelStatisticsModel model;
    model.setSnapshot(sampleSnapshot());

    ChatStatisticsSnapshot changed = sampleSnapshot();
    changed.periodEnd += 1;

    QSignalSpy spy(&model, &ChannelStatisticsModel::changed);
    model.setSnapshot(changed);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.snapshot().periodEnd, qint64(1702592001));
}

void ChannelStatisticsTest::setSnapshotEmitsOnPointChange()
{
    ChannelStatisticsModel model;
    model.setSnapshot(sampleSnapshot());

    ChatStatisticsSnapshot changed = sampleSnapshot();
    changed.graphs[0].points[1].value = 26.0;

    QSignalSpy spy(&model, &ChannelStatisticsModel::changed);
    model.setSnapshot(changed);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.totalFor(QStringLiteral("growth_graph")), 36.0);
}

void ChannelStatisticsTest::clearDropsSnapshotAndEmits()
{
    ChannelStatisticsModel model;
    model.setSnapshot(sampleSnapshot());

    QSignalSpy spy(&model, &ChannelStatisticsModel::changed);
    model.clear();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.snapshot().periodStart, qint64(0));
    QCOMPARE(model.snapshot().periodEnd, qint64(0));
    QVERIFY(model.snapshot().graphs.isEmpty());
    QCOMPARE(model.graphCount(), 0);
    QVERIFY(model.graphNames().isEmpty());
}

void ChannelStatisticsTest::clearOnEmptyModelIsSilent()
{
    ChannelStatisticsModel model;
    QSignalSpy spy(&model, &ChannelStatisticsModel::changed);

    model.clear();

    QCOMPARE(spy.count(), 0);
}

void ChannelStatisticsTest::clearOnPeriodOnlySnapshotEmits()
{
    ChatStatisticsSnapshot snapshot;
    snapshot.periodStart = 10;
    snapshot.periodEnd = 20;

    ChannelStatisticsModel model;
    model.setSnapshot(snapshot);

    QSignalSpy spy(&model, &ChannelStatisticsModel::changed);
    model.clear();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.snapshot().periodStart, qint64(0));
}

void ChannelStatisticsTest::parsedSnapshotFeedsModel()
{
    const QJsonObject response = responseWithStatistics(QJsonObject{
        {QStringLiteral("period"),
         QJsonObject{{QStringLiteral("min_date"), 1000},
                     {QStringLiteral("max_date"), 2000}}},
        {QStringLiteral("growth_graph"), graphJson({1000, 2000}, {5, 7})},
        {QStringLiteral("followers_graph"), graphJson({1000}, {3})},
    });

    ChannelStatisticsModel model;
    model.setSnapshot(chatStatisticsFromResponse(response));

    QCOMPARE(model.snapshot().periodStart, qint64(1000));
    QCOMPARE(model.graphCount(), 2);
    QCOMPARE(model.graphNames(),
             QStringList({QStringLiteral("growth_graph"),
                          QStringLiteral("followers_graph")}));
    QCOMPARE(model.totalFor(QStringLiteral("growth_graph")), 12.0);
    QCOMPARE(model.graph(QStringLiteral("followers_graph")).points.size(), 1);
}

QTEST_GUILESS_MAIN(ChannelStatisticsTest)
#include "tst_channel_statistics.moc"
