#include "telegram/VoiceWaveformModel.h"

#include <QSignalSpy>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

class VoiceWaveformModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setSamplesStoresValues();
    void setSamplesClampsNegatives();
    void setSamplesDropsNothing();
    void setSamplesReplacesPrevious();
    void setSamplesEmitsOnlyOnRealChange();
    void countTracksSamples();
    void normalizedScalesToPeak();
    void normalizedEmptyIsEmpty();
    void normalizedAllZerosIsZero();
    void normalizedAllEqualIsOne();
    void normalizedKeepsOrderAndSize();
    void progressStartsAtZero();
    void setProgressClampsRange();
    void setProgressIgnoresNonFinite();
    void setProgressEmitsOnlyOnRealChange();
    void playedSampleCountWithoutSamples();
    void playedSampleCountFloorsPartialProgress();
    void playedSampleCountAtFullProgress();
    void playedSampleCountReactsToNewSamples();
    void clearResetsEverything();
    void clearOnEmptyIsNoOp();
    void clearWithOnlyProgressEmits();
    void changedOnlyOnRealChanges();
    void setSamplesAfterClearWorks();
};

void VoiceWaveformModelTest::startsEmpty()
{
    const VoiceWaveformModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.samples().isEmpty());
    QVERIFY(model.normalized().isEmpty());
    QCOMPARE(model.progress(), 0.0);
    QCOMPARE(model.playedSampleCount(), 0);
}

void VoiceWaveformModelTest::setSamplesStoresValues()
{
    VoiceWaveformModel model;

    model.setSamples({0, 5, 12, 31});

    QCOMPARE(model.count(), 4);
    QCOMPARE(model.samples(), QVector<int>({0, 5, 12, 31}));
}

void VoiceWaveformModelTest::setSamplesClampsNegatives()
{
    VoiceWaveformModel model;

    model.setSamples({-1, -1000, 0, 7, std::numeric_limits<int>::min()});

    QCOMPARE(model.count(), 5);
    QCOMPARE(model.samples(), QVector<int>({0, 0, 0, 7, 0}));
}

void VoiceWaveformModelTest::setSamplesDropsNothing()
{
    VoiceWaveformModel model;

    // Duplicates, zeros and trailing values all survive.
    model.setSamples({3, 3, 0, 0, 3, 1});

    QCOMPARE(model.count(), 6);
    QCOMPARE(model.samples(), QVector<int>({3, 3, 0, 0, 3, 1}));
}

void VoiceWaveformModelTest::setSamplesReplacesPrevious()
{
    VoiceWaveformModel model;

    model.setSamples({1, 2, 3});
    model.setSamples({9});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.samples(), QVector<int>({9}));
}

void VoiceWaveformModelTest::setSamplesEmitsOnlyOnRealChange()
{
    VoiceWaveformModel model;
    QSignalSpy spy(&model, &VoiceWaveformModel::changed);

    model.setSamples({0, 2, 3});
    QCOMPARE(spy.count(), 1);

    // Same envelope again: silent.
    model.setSamples({0, 2, 3});
    QCOMPARE(spy.count(), 1);

    // Negative input that clamps to the stored envelope changes nothing.
    model.setSamples({-7, 2, 3});
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.samples(), QVector<int>({0, 2, 3}));

    model.setSamples({0, 2, 4});
    QCOMPARE(spy.count(), 2);
}

void VoiceWaveformModelTest::countTracksSamples()
{
    VoiceWaveformModel model;

    QCOMPARE(model.count(), 0);

    model.setSamples({4});
    QCOMPARE(model.count(), 1);

    model.setSamples(QVector<int>(64, 7));
    QCOMPARE(model.count(), 64);

    model.setSamples({});
    QCOMPARE(model.count(), 0);
}

void VoiceWaveformModelTest::normalizedScalesToPeak()
{
    VoiceWaveformModel model;

    model.setSamples({0, 5, 10, 20});

    const QVector<double> result = model.normalized();
    QCOMPARE(result.size(), 4);
    QCOMPARE(result.at(0), 0.0);
    QCOMPARE(result.at(1), 0.25);
    QCOMPARE(result.at(2), 0.5);
    QCOMPARE(result.at(3), 1.0);
}

void VoiceWaveformModelTest::normalizedEmptyIsEmpty()
{
    const VoiceWaveformModel model;
    QVERIFY(model.normalized().isEmpty());

    VoiceWaveformModel cleared;
    cleared.setSamples({3, 4});
    cleared.clear();
    QVERIFY(cleared.normalized().isEmpty());
}

void VoiceWaveformModelTest::normalizedAllZerosIsZero()
{
    VoiceWaveformModel model;

    model.setSamples({0, 0, 0});

    const QVector<double> result = model.normalized();
    QCOMPARE(result.size(), 3);
    for (const double value : result)
        QCOMPARE(value, 0.0);
}

void VoiceWaveformModelTest::normalizedAllEqualIsOne()
{
    VoiceWaveformModel model;

    model.setSamples({31, 31, 31});

    const QVector<double> result = model.normalized();
    QCOMPARE(result.size(), 3);
    for (const double value : result)
        QCOMPARE(value, 1.0);
}

void VoiceWaveformModelTest::normalizedKeepsOrderAndSize()
{
    VoiceWaveformModel model;

    // Peak in the middle: order is preserved and the peak maps to 1.0.
    model.setSamples({2, 4, 8, 4, 0});

    const QVector<double> result = model.normalized();
    QCOMPARE(result.size(), 5);
    QCOMPARE(result.at(0), 0.25);
    QCOMPARE(result.at(1), 0.5);
    QCOMPARE(result.at(2), 1.0);
    QCOMPARE(result.at(3), 0.5);
    QCOMPARE(result.at(4), 0.0);
}

void VoiceWaveformModelTest::progressStartsAtZero()
{
    const VoiceWaveformModel model;
    QCOMPARE(model.progress(), 0.0);
}

void VoiceWaveformModelTest::setProgressClampsRange()
{
    VoiceWaveformModel model;

    model.setProgress(-0.5);
    QCOMPARE(model.progress(), 0.0);

    model.setProgress(1.5);
    QCOMPARE(model.progress(), 1.0);

    model.setProgress(-1e9);
    QCOMPARE(model.progress(), 0.0);

    model.setProgress(1e9);
    QCOMPARE(model.progress(), 1.0);

    model.setProgress(0.25);
    QCOMPARE(model.progress(), 0.25);
}

void VoiceWaveformModelTest::setProgressIgnoresNonFinite()
{
    VoiceWaveformModel model;
    model.setProgress(0.4);

    model.setProgress(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(model.progress(), 0.4);

    model.setProgress(std::numeric_limits<double>::infinity());
    QCOMPARE(model.progress(), 0.4);

    model.setProgress(-std::numeric_limits<double>::infinity());
    QCOMPARE(model.progress(), 0.4);

    QSignalSpy spy(&model, &VoiceWaveformModel::changed);
    model.setProgress(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(spy.count(), 0);
}

void VoiceWaveformModelTest::setProgressEmitsOnlyOnRealChange()
{
    VoiceWaveformModel model;
    QSignalSpy spy(&model, &VoiceWaveformModel::changed);

    model.setProgress(0.0);
    QCOMPARE(spy.count(), 0);

    model.setProgress(0.5);
    QCOMPARE(spy.count(), 1);

    model.setProgress(0.5);
    QCOMPARE(spy.count(), 1);

    // Clamping to the current value is silent.
    model.setProgress(2.0);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.progress(), 1.0);

    model.setProgress(7.0);
    QCOMPARE(spy.count(), 2);
}

void VoiceWaveformModelTest::playedSampleCountWithoutSamples()
{
    VoiceWaveformModel model;

    model.setProgress(0.5);
    QCOMPARE(model.playedSampleCount(), 0);

    model.setProgress(1.0);
    QCOMPARE(model.playedSampleCount(), 0);
}

void VoiceWaveformModelTest::playedSampleCountFloorsPartialProgress()
{
    VoiceWaveformModel model;
    model.setSamples({1, 1, 1, 1, 1, 1, 1, 1, 1, 1});

    model.setProgress(0.0);
    QCOMPARE(model.playedSampleCount(), 0);

    model.setProgress(0.05);
    QCOMPARE(model.playedSampleCount(), 0);

    model.setProgress(0.1);
    QCOMPARE(model.playedSampleCount(), 1);

    model.setProgress(0.39);
    QCOMPARE(model.playedSampleCount(), 3);

    model.setProgress(0.5);
    QCOMPARE(model.playedSampleCount(), 5);

    model.setProgress(0.99);
    QCOMPARE(model.playedSampleCount(), 9);
}

void VoiceWaveformModelTest::playedSampleCountAtFullProgress()
{
    VoiceWaveformModel model;
    model.setSamples({2, 3, 4});

    model.setProgress(1.0);
    QCOMPARE(model.playedSampleCount(), 3);

    // Beyond 1.0 clamps, so the count never exceeds the envelope.
    model.setProgress(42.0);
    QCOMPARE(model.playedSampleCount(), 3);
}

void VoiceWaveformModelTest::playedSampleCountReactsToNewSamples()
{
    VoiceWaveformModel model;

    model.setProgress(0.5);
    QCOMPARE(model.playedSampleCount(), 0);

    model.setSamples({1, 1, 1, 1});
    QCOMPARE(model.playedSampleCount(), 2);

    model.setSamples({1, 1, 1, 1, 1, 1});
    QCOMPARE(model.playedSampleCount(), 3);

    // Progress survives a sample change; it is a fraction, not an index.
    QCOMPARE(model.progress(), 0.5);
}

void VoiceWaveformModelTest::clearResetsEverything()
{
    VoiceWaveformModel model;
    model.setSamples({1, 2, 3});
    model.setProgress(0.5);

    model.clear();

    QCOMPARE(model.count(), 0);
    QVERIFY(model.samples().isEmpty());
    QVERIFY(model.normalized().isEmpty());
    QCOMPARE(model.progress(), 0.0);
    QCOMPARE(model.playedSampleCount(), 0);
}

void VoiceWaveformModelTest::clearOnEmptyIsNoOp()
{
    VoiceWaveformModel model;
    QSignalSpy spy(&model, &VoiceWaveformModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);

    // Progress zero but samples present: not a no-op.
    model.setSamples({5});
    QCOMPARE(spy.count(), 1);

    model.clear();
    QCOMPARE(spy.count(), 2);

    model.clear();
    QCOMPARE(spy.count(), 2);
}

void VoiceWaveformModelTest::clearWithOnlyProgressEmits()
{
    VoiceWaveformModel model;
    model.setProgress(0.75);

    QSignalSpy spy(&model, &VoiceWaveformModel::changed);
    model.clear();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.progress(), 0.0);
}

void VoiceWaveformModelTest::changedOnlyOnRealChanges()
{
    VoiceWaveformModel model;
    QSignalSpy spy(&model, &VoiceWaveformModel::changed);

    model.setSamples({0, 1, 2});
    QCOMPARE(spy.count(), 1);

    model.setProgress(0.0);
    QCOMPARE(spy.count(), 1);

    model.setProgress(0.5);
    QCOMPARE(spy.count(), 2);

    model.setProgress(0.5);
    QCOMPARE(spy.count(), 2);

    model.setSamples({0, 1, 2});
    QCOMPARE(spy.count(), 2);

    model.clear();
    QCOMPARE(spy.count(), 3);

    model.clear();
    QCOMPARE(spy.count(), 3);
}

void VoiceWaveformModelTest::setSamplesAfterClearWorks()
{
    VoiceWaveformModel model;
    model.setSamples({4, 8});
    model.setProgress(1.0);
    model.clear();

    model.setSamples({8, 4});

    QCOMPARE(model.count(), 2);
    QCOMPARE(model.samples(), QVector<int>({8, 4}));
    QCOMPARE(model.normalized(), QVector<double>({1.0, 0.5}));
    QCOMPARE(model.progress(), 0.0);
    QCOMPARE(model.playedSampleCount(), 0);
}

QTEST_GUILESS_MAIN(VoiceWaveformModelTest)
#include "tst_voice_waveform_model.moc"
