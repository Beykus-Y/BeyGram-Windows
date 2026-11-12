#include "telegram/ProxyLatencyModel.h"

#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

using namespace beygram::telegram;

class ProxyLatencyModelTest final : public QObject {
    Q_OBJECT

private slots:
    void unknownProxyHasMinusOne();
    void firstSampleIsVisible();
    void medianOfOddSamples();
    void medianOfEvenSamplesAveragesMiddle();
    void medianTruncatesFraction();
    void keepsOnlyLastFiveSamples();
    void clampsNegativeLatencyToZero();
    void clampsLatencyAboveMaximum();
    void clampsBeforeTakingMedian();
    void zeroIsARealLatency();
    void proxyIdsAreSorted();
    void proxiesAreIndependent();
    void removeForgetsOnlyThatProxy();
    void removeUnknownIsSilent();
    void clearForgetsEverything();
    void clearOnEmptyIsSilent();
    void addEmitsChangedForNewProxy();
    void addEmitsChangedWhenMedianMoves();
    void addIsSilentWhenMedianStays();
    void clampingCanLeaveMedianUnchanged();
    void emptyProxyIdIsIgnored();
};

void ProxyLatencyModelTest::unknownProxyHasMinusOne()
{
    ProxyLatencyModel model;

    QCOMPARE(model.latencyFor(QStringLiteral("missing")), -1);
    QVERIFY(model.proxyIds().isEmpty());
}

void ProxyLatencyModelTest::firstSampleIsVisible()
{
    ProxyLatencyModel model;
    QSignalSpy spy(&model, &ProxyLatencyModel::changed);

    model.addSample(QStringLiteral("de-1"), 120);

    QCOMPARE(model.latencyFor(QStringLiteral("de-1")), 120);
    QCOMPARE(model.proxyIds(), QStringList{QStringLiteral("de-1")});
    QCOMPARE(spy.count(), 1);
}

void ProxyLatencyModelTest::medianOfOddSamples()
{
    ProxyLatencyModel model;

    model.addSample(QStringLiteral("de-1"), 90);
    model.addSample(QStringLiteral("de-1"), 20);
    model.addSample(QStringLiteral("de-1"), 40);

    // Sorted: 20, 40, 90.
    QCOMPARE(model.latencyFor(QStringLiteral("de-1")), 40);
}

void ProxyLatencyModelTest::medianOfEvenSamplesAveragesMiddle()
{
    ProxyLatencyModel twoSamples;
    twoSamples.addSample(QStringLiteral("de-1"), 10);
    twoSamples.addSample(QStringLiteral("de-1"), 20);
    QCOMPARE(twoSamples.latencyFor(QStringLiteral("de-1")), 15);

    ProxyLatencyModel fourSamples;
    fourSamples.addSample(QStringLiteral("de-1"), 10);
    fourSamples.addSample(QStringLiteral("de-1"), 20);
    fourSamples.addSample(QStringLiteral("de-1"), 30);
    fourSamples.addSample(QStringLiteral("de-1"), 40);
    QCOMPARE(fourSamples.latencyFor(QStringLiteral("de-1")), 25);
}

void ProxyLatencyModelTest::medianTruncatesFraction()
{
    ProxyLatencyModel model;

    model.addSample(QStringLiteral("de-1"), 10);
    model.addSample(QStringLiteral("de-1"), 21);

    // (10 + 21) / 2 = 15.5, truncated to 15.
    QCOMPARE(model.latencyFor(QStringLiteral("de-1")), 15);
}

void ProxyLatencyModelTest::keepsOnlyLastFiveSamples()
{
    ProxyLatencyModel model;
    const QString id = QStringLiteral("de-1");

    for (const int latency : {100, 200, 300, 400, 500})
        model.addSample(id, latency);
    QCOMPARE(model.latencyFor(id), 300);

    // 100 falls out; 200, 300, 400, 500, 600 remain.
    model.addSample(id, 600);
    QCOMPARE(model.latencyFor(id), 400);

    // 200 falls out; 300, 400, 500, 600, 1000 remain.
    model.addSample(id, 1000);
    QCOMPARE(model.latencyFor(id), 500);
}

void ProxyLatencyModelTest::clampsNegativeLatencyToZero()
{
    ProxyLatencyModel model;

    model.addSample(QStringLiteral("de-1"), -250);
    QCOMPARE(model.latencyFor(QStringLiteral("de-1")), 0);

    model.addSample(QStringLiteral("de-1"), 10);
    QCOMPARE(model.latencyFor(QStringLiteral("de-1")), 5);
}

void ProxyLatencyModelTest::clampsLatencyAboveMaximum()
{
    ProxyLatencyModel model;

    model.addSample(QStringLiteral("de-1"), 60001);
    QCOMPARE(model.latencyFor(QStringLiteral("de-1")), 60000);

    model.addSample(QStringLiteral("de-1"), 70000);
    QCOMPARE(model.latencyFor(QStringLiteral("de-1")), 60000);
}

void ProxyLatencyModelTest::clampsBeforeTakingMedian()
{
    ProxyLatencyModel model;

    model.addSample(QStringLiteral("de-1"), -100);
    model.addSample(QStringLiteral("de-1"), 60001);

    // Clamped to 0 and 60000, so the median is 30000.
    QCOMPARE(model.latencyFor(QStringLiteral("de-1")), 30000);
}

void ProxyLatencyModelTest::zeroIsARealLatency()
{
    ProxyLatencyModel model;
    QSignalSpy spy(&model, &ProxyLatencyModel::changed);

    model.addSample(QStringLiteral("de-1"), 0);

    QCOMPARE(model.latencyFor(QStringLiteral("de-1")), 0);
    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.proxyIds().isEmpty());
}

void ProxyLatencyModelTest::proxyIdsAreSorted()
{
    ProxyLatencyModel model;

    model.addSample(QStringLiteral("nl-1"), 100);
    model.addSample(QStringLiteral("de-1"), 100);
    model.addSample(QStringLiteral("us-1"), 100);

    QCOMPARE(model.proxyIds(),
             (QStringList{QStringLiteral("de-1"), QStringLiteral("nl-1"),
                          QStringLiteral("us-1")}));
}

void ProxyLatencyModelTest::proxiesAreIndependent()
{
    ProxyLatencyModel model;

    model.addSample(QStringLiteral("de-1"), 100);
    model.addSample(QStringLiteral("nl-1"), 500);
    QCOMPARE(model.latencyFor(QStringLiteral("de-1")), 100);
    QCOMPARE(model.latencyFor(QStringLiteral("nl-1")), 500);

    model.addSample(QStringLiteral("de-1"), 300);
    QCOMPARE(model.latencyFor(QStringLiteral("de-1")), 200);
    QCOMPARE(model.latencyFor(QStringLiteral("nl-1")), 500);
}

void ProxyLatencyModelTest::removeForgetsOnlyThatProxy()
{
    ProxyLatencyModel model;

    model.addSample(QStringLiteral("de-1"), 100);
    model.addSample(QStringLiteral("nl-1"), 500);
    model.remove(QStringLiteral("de-1"));

    QCOMPARE(model.latencyFor(QStringLiteral("de-1")), -1);
    QCOMPARE(model.latencyFor(QStringLiteral("nl-1")), 500);
    QCOMPARE(model.proxyIds(), QStringList{QStringLiteral("nl-1")});
}

void ProxyLatencyModelTest::removeUnknownIsSilent()
{
    ProxyLatencyModel model;
    QSignalSpy spy(&model, &ProxyLatencyModel::changed);

    model.addSample(QStringLiteral("de-1"), 100);
    QCOMPARE(spy.count(), 1);

    model.remove(QStringLiteral("missing"));
    QCOMPARE(spy.count(), 1);

    model.remove(QStringLiteral("de-1"));
    QCOMPARE(spy.count(), 2);
}

void ProxyLatencyModelTest::clearForgetsEverything()
{
    ProxyLatencyModel model;
    QSignalSpy spy(&model, &ProxyLatencyModel::changed);

    model.addSample(QStringLiteral("de-1"), 100);
    model.addSample(QStringLiteral("nl-1"), 500);
    QCOMPARE(spy.count(), 2);

    model.clear();
    QCOMPARE(spy.count(), 3);
    QVERIFY(model.proxyIds().isEmpty());
    QCOMPARE(model.latencyFor(QStringLiteral("de-1")), -1);
    QCOMPARE(model.latencyFor(QStringLiteral("nl-1")), -1);
}

void ProxyLatencyModelTest::clearOnEmptyIsSilent()
{
    ProxyLatencyModel model;
    QSignalSpy spy(&model, &ProxyLatencyModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);
}

void ProxyLatencyModelTest::addEmitsChangedForNewProxy()
{
    ProxyLatencyModel model;
    QSignalSpy spy(&model, &ProxyLatencyModel::changed);

    model.addSample(QStringLiteral("de-1"), 100);
    QCOMPARE(spy.count(), 1);

    model.addSample(QStringLiteral("nl-1"), 100);
    QCOMPARE(spy.count(), 2);

    // The id already exists and its median stays 100, so no repaint.
    model.addSample(QStringLiteral("de-1"), 100);
    QCOMPARE(spy.count(), 2);
}

void ProxyLatencyModelTest::addEmitsChangedWhenMedianMoves()
{
    ProxyLatencyModel model;
    QSignalSpy spy(&model, &ProxyLatencyModel::changed);

    model.addSample(QStringLiteral("de-1"), 10);
    QCOMPARE(spy.count(), 1);

    model.addSample(QStringLiteral("de-1"), 20);
    QCOMPARE(model.latencyFor(QStringLiteral("de-1")), 15);
    QCOMPARE(spy.count(), 2);

    model.addSample(QStringLiteral("de-1"), 30);
    QCOMPARE(model.latencyFor(QStringLiteral("de-1")), 20);
    QCOMPARE(spy.count(), 3);
}

void ProxyLatencyModelTest::addIsSilentWhenMedianStays()
{
    ProxyLatencyModel model;
    QSignalSpy spy(&model, &ProxyLatencyModel::changed);

    model.addSample(QStringLiteral("de-1"), 100);
    QCOMPARE(spy.count(), 1);

    model.addSample(QStringLiteral("de-1"), 100);
    QCOMPARE(spy.count(), 1);

    // 100, 100, 200 still has median 100.
    model.addSample(QStringLiteral("de-1"), 200);
    QCOMPARE(model.latencyFor(QStringLiteral("de-1")), 100);
    QCOMPARE(spy.count(), 1);

    // 100, 100, 200, 200 has median 150.
    model.addSample(QStringLiteral("de-1"), 200);
    QCOMPARE(model.latencyFor(QStringLiteral("de-1")), 150);
    QCOMPARE(spy.count(), 2);
}

void ProxyLatencyModelTest::clampingCanLeaveMedianUnchanged()
{
    ProxyLatencyModel model;
    QSignalSpy spy(&model, &ProxyLatencyModel::changed);

    model.addSample(QStringLiteral("de-1"), 60000);
    QCOMPARE(spy.count(), 1);

    model.addSample(QStringLiteral("de-1"), 60001);
    QCOMPARE(model.latencyFor(QStringLiteral("de-1")), 60000);
    QCOMPARE(spy.count(), 1);
}

void ProxyLatencyModelTest::emptyProxyIdIsIgnored()
{
    ProxyLatencyModel model;
    QSignalSpy spy(&model, &ProxyLatencyModel::changed);

    model.addSample(QString(), 50);

    QCOMPARE(model.latencyFor(QString()), -1);
    QVERIFY(model.proxyIds().isEmpty());
    QCOMPARE(spy.count(), 0);
}

QTEST_GUILESS_MAIN(ProxyLatencyModelTest)
#include "tst_proxy_latency_model.moc"
