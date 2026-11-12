#include "telegram/ChatStatisticsModel.h"

#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

using namespace beygram::telegram;

namespace {

StatisticsValue stat(const QString& label, double value, const QString& unit = {})
{
    return StatisticsValue{label, value, unit};
}

QStringList labelsOf(const QVector<StatisticsValue>& values)
{
    QStringList labels;
    labels.reserve(values.size());
    for (const StatisticsValue& value : values)
        labels.append(value.label);
    return labels;
}

} // namespace

class ChatStatisticsModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void keepsOverviewOrder();
    void dropsEmptyLabels();
    void dedupesLabelsKeepingFirst();
    void dedupeIsCaseSensitive();
    void replacesPreviousOverview();
    void chartStoresLabelsAndSeriesAsGiven();
    void chartTruncatesLongSeries();
    void chartPadsShortSeriesWithZero();
    void chartWithNoLabelsDropsSeries();
    void hasDataReflectsOverviewAndChart();
    void maxValueCoversOverviewAndSeries();
    void maxValueClampsNegatives();
    void maxValueReturnsZeroForOnlyNegatives();
    void maxValueDoesNotRewriteStoredValues();
    void clearDropsEverything();
    void clearOnEmptyModelIsNoOp();
    void emitsChangedOnOverviewChange();
    void emitsChangedOnChartChange();
    void silentWhenOverviewIsUnchanged();
    void silentWhenChartIsUnchanged();
    void silentWhenOnlyEmptyLabelsArrive();
    void returnsIndependentCopies();
};

void ChatStatisticsModelTest::startsEmpty()
{
    ChatStatisticsModel model;
    QVERIFY(model.overview().isEmpty());
    QVERIFY(model.chartLabels().isEmpty());
    QVERIFY(model.chartSeries().isEmpty());
    QVERIFY(!model.hasData());
    QCOMPARE(model.maxValue(), 0.0);
}

void ChatStatisticsModelTest::keepsOverviewOrder()
{
    ChatStatisticsModel model;
    model.setOverview({
        stat(QStringLiteral("Messages"), 1200.0),
        stat(QStringLiteral("Shares"), 34.0),
        stat(QStringLiteral("Views"), 56000.0),
    });

    QCOMPARE(model.overview().size(), 3);
    QCOMPARE(labelsOf(model.overview()),
             QStringList({QStringLiteral("Messages"), QStringLiteral("Shares"),
                          QStringLiteral("Views")}));
    QCOMPARE(model.overview().at(0).value, 1200.0);
    QCOMPARE(model.overview().at(2).value, 56000.0);
}

void ChatStatisticsModelTest::dropsEmptyLabels()
{
    ChatStatisticsModel model;
    model.setOverview({
        stat(QString(), 1.0),
        stat(QStringLiteral("Members"), 42.0),
        stat(QString(), 2.0),
        stat(QStringLiteral("Online"), 7.0),
    });

    QCOMPARE(model.overview().size(), 2);
    QCOMPARE(labelsOf(model.overview()),
             QStringList({QStringLiteral("Members"), QStringLiteral("Online")}));
    QCOMPARE(model.overview().at(0).value, 42.0);
}

void ChatStatisticsModelTest::dedupesLabelsKeepingFirst()
{
    ChatStatisticsModel model;
    model.setOverview({
        stat(QStringLiteral("Messages"), 10.0, QStringLiteral("k")),
        stat(QStringLiteral("Views"), 20.0),
        stat(QStringLiteral("Messages"), 99.0, QStringLiteral("M")),
    });

    QCOMPARE(model.overview().size(), 2);
    QCOMPARE(labelsOf(model.overview()),
             QStringList({QStringLiteral("Messages"), QStringLiteral("Views")}));
    QCOMPARE(model.overview().at(0).value, 10.0);
    QCOMPARE(model.overview().at(0).unit, QStringLiteral("k"));
}

void ChatStatisticsModelTest::dedupeIsCaseSensitive()
{
    ChatStatisticsModel model;
    model.setOverview({
        stat(QStringLiteral("Messages"), 1.0),
        stat(QStringLiteral("messages"), 2.0),
    });

    QCOMPARE(model.overview().size(), 2);
    QCOMPARE(labelsOf(model.overview()),
             QStringList({QStringLiteral("Messages"), QStringLiteral("messages")}));
}

void ChatStatisticsModelTest::replacesPreviousOverview()
{
    ChatStatisticsModel model;
    model.setOverview({stat(QStringLiteral("Messages"), 10.0),
                       stat(QStringLiteral("Views"), 20.0)});
    model.setOverview({stat(QStringLiteral("Shares"), 3.0)});

    QCOMPARE(model.overview().size(), 1);
    QCOMPARE(model.overview().at(0).label, QStringLiteral("Shares"));
    QCOMPARE(model.overview().at(0).value, 3.0);
}

void ChatStatisticsModelTest::chartStoresLabelsAndSeriesAsGiven()
{
    ChatStatisticsModel model;
    model.setChart({QStringLiteral("Mon"), QStringLiteral("Tue"), QStringLiteral("Wed")},
                   {1.5, 2.5, 3.5});

    QCOMPARE(model.chartLabels(),
             QStringList({QStringLiteral("Mon"), QStringLiteral("Tue"),
                          QStringLiteral("Wed")}));
    QCOMPARE(model.chartSeries(), QVector<double>({1.5, 2.5, 3.5}));
}

void ChatStatisticsModelTest::chartTruncatesLongSeries()
{
    ChatStatisticsModel model;
    model.setChart({QStringLiteral("Mon"), QStringLiteral("Tue")}, {1.0, 2.0, 3.0, 4.0});

    QCOMPARE(model.chartLabels().size(), 2);
    QCOMPARE(model.chartSeries(), QVector<double>({1.0, 2.0}));
}

void ChatStatisticsModelTest::chartPadsShortSeriesWithZero()
{
    ChatStatisticsModel model;
    model.setChart({QStringLiteral("Mon"), QStringLiteral("Tue"), QStringLiteral("Wed")},
                   {4.0});

    QCOMPARE(model.chartSeries(), QVector<double>({4.0, 0.0, 0.0}));
}

void ChatStatisticsModelTest::chartWithNoLabelsDropsSeries()
{
    ChatStatisticsModel model;
    model.setChart({}, {1.0, 2.0});

    QVERIFY(model.chartLabels().isEmpty());
    QVERIFY(model.chartSeries().isEmpty());
    QVERIFY(!model.hasData());
}

void ChatStatisticsModelTest::hasDataReflectsOverviewAndChart()
{
    ChatStatisticsModel model;
    QVERIFY(!model.hasData());

    model.setOverview({stat(QStringLiteral("Messages"), 1.0)});
    QVERIFY(model.hasData());

    model.clear();
    QVERIFY(!model.hasData());

    model.setChart({QStringLiteral("Mon")}, {1.0});
    QVERIFY(model.hasData());
}

void ChatStatisticsModelTest::maxValueCoversOverviewAndSeries()
{
    ChatStatisticsModel model;
    model.setOverview({stat(QStringLiteral("Messages"), 120.0),
                       stat(QStringLiteral("Views"), 480.0)});
    QCOMPARE(model.maxValue(), 480.0);

    model.setChart({QStringLiteral("Mon"), QStringLiteral("Tue")}, {12.0, 900.0});
    QCOMPARE(model.maxValue(), 900.0);

    model.setChart({QStringLiteral("Mon"), QStringLiteral("Tue")}, {12.0, 90.0});
    QCOMPARE(model.maxValue(), 480.0);
}

void ChatStatisticsModelTest::maxValueClampsNegatives()
{
    ChatStatisticsModel model;
    model.setOverview({stat(QStringLiteral("Change"), -25.0),
                       stat(QStringLiteral("Members"), 30.0)});
    QCOMPARE(model.maxValue(), 30.0);

    model.setChart({QStringLiteral("Mon"), QStringLiteral("Tue")}, {-5.0, -1.0});
    QCOMPARE(model.maxValue(), 30.0);
}

void ChatStatisticsModelTest::maxValueReturnsZeroForOnlyNegatives()
{
    ChatStatisticsModel model;
    model.setOverview({stat(QStringLiteral("Change"), -25.0)});
    QCOMPARE(model.maxValue(), 0.0);

    model.setChart({QStringLiteral("Mon")}, {-7.0});
    QCOMPARE(model.maxValue(), 0.0);
}

void ChatStatisticsModelTest::maxValueDoesNotRewriteStoredValues()
{
    ChatStatisticsModel model;
    model.setOverview({stat(QStringLiteral("Change"), -25.0)});
    model.setChart({QStringLiteral("Mon")}, {-7.0});

    QCOMPARE(model.maxValue(), 0.0);
    QCOMPARE(model.overview().at(0).value, -25.0);
    QCOMPARE(model.chartSeries().at(0), -7.0);
}

void ChatStatisticsModelTest::clearDropsEverything()
{
    ChatStatisticsModel model;
    model.setOverview({stat(QStringLiteral("Messages"), 1.0)});
    model.setChart({QStringLiteral("Mon")}, {1.0});

    model.clear();

    QVERIFY(model.overview().isEmpty());
    QVERIFY(model.chartLabels().isEmpty());
    QVERIFY(model.chartSeries().isEmpty());
    QVERIFY(!model.hasData());
    QCOMPARE(model.maxValue(), 0.0);
}

void ChatStatisticsModelTest::clearOnEmptyModelIsNoOp()
{
    ChatStatisticsModel model;
    QSignalSpy spy(&model, &ChatStatisticsModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);

    // A chart that was truncated away to nothing leaves nothing to clear.
    model.setChart({}, {1.0});
    QCOMPARE(spy.count(), 0);

    model.clear();
    QCOMPARE(spy.count(), 0);
}

void ChatStatisticsModelTest::emitsChangedOnOverviewChange()
{
    ChatStatisticsModel model;
    QSignalSpy spy(&model, &ChatStatisticsModel::changed);

    model.setOverview({stat(QStringLiteral("Messages"), 1.0)});
    QCOMPARE(spy.count(), 1);

    // A different unit is a real change even though the label matches.
    model.setOverview({stat(QStringLiteral("Messages"), 1.0, QStringLiteral("k"))});
    QCOMPARE(spy.count(), 2);

    // Replacing the overview with an empty one clears it.
    model.setOverview({});
    QCOMPARE(spy.count(), 3);
    QVERIFY(model.overview().isEmpty());
}

void ChatStatisticsModelTest::emitsChangedOnChartChange()
{
    ChatStatisticsModel model;
    QSignalSpy spy(&model, &ChatStatisticsModel::changed);

    model.setChart({QStringLiteral("Mon")}, {1.0});
    QCOMPARE(spy.count(), 1);

    model.setChart({QStringLiteral("Mon"), QStringLiteral("Tue")}, {1.0});
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.chartSeries(), QVector<double>({1.0, 0.0}));
}

void ChatStatisticsModelTest::silentWhenOverviewIsUnchanged()
{
    ChatStatisticsModel model;
    model.setOverview({stat(QStringLiteral("Messages"), 1.0),
                       stat(QStringLiteral("Views"), 2.0)});
    QSignalSpy spy(&model, &ChatStatisticsModel::changed);

    model.setOverview({stat(QStringLiteral("Messages"), 1.0),
                       stat(QStringLiteral("Views"), 2.0)});
    QCOMPARE(spy.count(), 0);

    // Entries that are filtered out entirely do not count as a change.
    model.setOverview({stat(QString(), 5.0),
                       stat(QStringLiteral("Messages"), 1.0),
                       stat(QStringLiteral("Views"), 2.0)});
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.overview().size(), 2);
}

void ChatStatisticsModelTest::silentWhenChartIsUnchanged()
{
    ChatStatisticsModel model;
    model.setChart({QStringLiteral("Mon")}, {1.0, 2.0});
    QSignalSpy spy(&model, &ChatStatisticsModel::changed);

    // The extra point was truncated on the first call, so this matches it.
    model.setChart({QStringLiteral("Mon")}, {1.0, 9.0});
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.chartSeries(), QVector<double>({1.0}));

    model.setChart({QStringLiteral("Mon")}, {1.0});
    QCOMPARE(spy.count(), 0);
}

void ChatStatisticsModelTest::silentWhenOnlyEmptyLabelsArrive()
{
    ChatStatisticsModel model;
    QSignalSpy spy(&model, &ChatStatisticsModel::changed);

    model.setOverview({stat(QString(), 1.0), stat(QString(), 2.0)});
    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.hasData());
}

void ChatStatisticsModelTest::returnsIndependentCopies()
{
    ChatStatisticsModel model;
    model.setOverview({stat(QStringLiteral("Messages"), 1.0)});
    model.setChart({QStringLiteral("Mon")}, {1.0});

    QVector<StatisticsValue> overview = model.overview();
    overview[0].label = QStringLiteral("mutated");
    overview.append(stat(QStringLiteral("extra"), 9.0));

    QStringList labels = model.chartLabels();
    labels[0] = QStringLiteral("mutated");

    QVector<double> series = model.chartSeries();
    series[0] = 99.0;

    QCOMPARE(model.overview().size(), 1);
    QCOMPARE(model.overview().at(0).label, QStringLiteral("Messages"));
    QCOMPARE(model.chartLabels(), QStringList({QStringLiteral("Mon")}));
    QCOMPARE(model.chartSeries(), QVector<double>({1.0}));
}

QTEST_GUILESS_MAIN(ChatStatisticsModelTest)
#include "tst_chat_statistics_model.moc"
