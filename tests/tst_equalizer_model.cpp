#include "telegram/music/EqualizerModel.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTest>

#include <cmath>
#include <initializer_list>
#include <limits>

using namespace beygram::telegram;

namespace {

QVector<int> frequenciesOf(const QVector<EqualizerBand>& bands)
{
    QVector<int> frequencies;
    frequencies.reserve(bands.size());
    for (const EqualizerBand& band : bands)
        frequencies.append(band.frequencyHz);
    return frequencies;
}

void compareGains(const QVector<EqualizerBand>& bands, std::initializer_list<double> expectedGains)
{
    QCOMPARE(static_cast<int>(bands.size()), static_cast<int>(expectedGains.size()));

    int index = 0;
    for (const double expected : expectedGains) {
        const double actual = bands.at(index).gainDb;
        QVERIFY2(std::abs(actual - expected) < 1e-9,
                 qPrintable(QStringLiteral("band %1: %2 dB, expected %3 dB")
                                .arg(index)
                                .arg(actual)
                                .arg(expected)));
        ++index;
    }
}

} // namespace

class EqualizerModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithDefaultBandsAndFlatPreset();
    void presetsListIsStable();

    void setBandsSortsDropsInvalidAndDedupes();
    void setBandsKeepsGainsOfSurvivingFrequencies();
    void setBandsIgnoresAllInvalidInput();
    void setBandsSameLayoutIsNoOp();
    void bandsIsACopy();

    void setGainClampsToRange();
    void setGainUnknownFrequencyReturnsFalse();
    void setGainRejectsNan();
    void setGainSameValueIsNoOp();
    void setGainMarksCurveCustom();

    void applyPresetFillsBassCurve();
    void applyPresetFillsVocalCurve();
    void applyPresetFillsRockCurve();
    void applyPresetFillsElectronicCurve();
    void applyPresetFillsNightCurve();
    void applyPresetFlatRestoresZeroGains();
    void applyPresetUnknownIsIgnored();
    void applyPresetSamePresetIsNoOp();
    void presetsResolveOnCustomLayout();
    void presetGainsStayWithinRange();

    void currentPresetEmptyForCustomCurve();
    void currentPresetMatchesManualCurve();

    void resetRestoresDefaults();
    void resetDropsCustomGains();
    void resetWhenAlreadyDefaultIsNoOp();

    void changedOnlyOnRealChanges();
};

void EqualizerModelTest::startsWithDefaultBandsAndFlatPreset()
{
    const EqualizerModel model;

    QCOMPARE(frequenciesOf(model.bands()), QVector<int>({60, 230, 910, 3600, 14000}));
    compareGains(model.bands(), {0.0, 0.0, 0.0, 0.0, 0.0});
    QCOMPARE(model.currentPreset(), QStringLiteral("Flat"));
}

void EqualizerModelTest::presetsListIsStable()
{
    const EqualizerModel model;

    QCOMPARE(model.presets(), QStringList({QStringLiteral("Flat"), QStringLiteral("Bass"),
                                           QStringLiteral("Vocal"), QStringLiteral("Rock"),
                                           QStringLiteral("Electronic"),
                                           QStringLiteral("Night")}));
}

void EqualizerModelTest::setBandsSortsDropsInvalidAndDedupes()
{
    EqualizerModel model;
    QSignalSpy changed(&model, &EqualizerModel::changed);

    model.setBands({910, 60, 910, -5, 0, 3600, 60});

    QCOMPARE(frequenciesOf(model.bands()), QVector<int>({60, 910, 3600}));
    compareGains(model.bands(), {0.0, 0.0, 0.0});
    QCOMPARE(changed.count(), 1);
}

void EqualizerModelTest::setBandsKeepsGainsOfSurvivingFrequencies()
{
    EqualizerModel model;
    QVERIFY(model.setGain(60, 3.5));
    QVERIFY(model.setGain(910, -2.0));

    model.setBands({60, 120, 240});

    QCOMPARE(frequenciesOf(model.bands()), QVector<int>({60, 120, 240}));
    compareGains(model.bands(), {3.5, 0.0, 0.0});
}

void EqualizerModelTest::setBandsIgnoresAllInvalidInput()
{
    EqualizerModel model;
    QVERIFY(model.setGain(60, 2.0));
    QSignalSpy changed(&model, &EqualizerModel::changed);

    model.setBands({0, -1, -100});

    QCOMPARE(frequenciesOf(model.bands()), QVector<int>({60, 230, 910, 3600, 14000}));
    compareGains(model.bands(), {2.0, 0.0, 0.0, 0.0, 0.0});
    QCOMPARE(changed.count(), 0);
}

void EqualizerModelTest::setBandsSameLayoutIsNoOp()
{
    EqualizerModel model;
    QSignalSpy changed(&model, &EqualizerModel::changed);

    model.setBands({14000, 3600, 910, 230, 60});

    QCOMPARE(changed.count(), 0);
}

void EqualizerModelTest::bandsIsACopy()
{
    EqualizerModel model;

    QVector<EqualizerBand> copy = model.bands();
    copy[0].gainDb = 5.0;

    QCOMPARE(model.bands().at(0).gainDb, 0.0);
}

void EqualizerModelTest::setGainClampsToRange()
{
    EqualizerModel model;

    QVERIFY(model.setGain(60, 20.0));
    QCOMPARE(model.bands().at(0).gainDb, 12.0);

    QVERIFY(model.setGain(60, -20.0));
    QCOMPARE(model.bands().at(0).gainDb, -12.0);

    QVERIFY(model.setGain(60, 3.5));
    QCOMPARE(model.bands().at(0).gainDb, 3.5);
}

void EqualizerModelTest::setGainUnknownFrequencyReturnsFalse()
{
    EqualizerModel model;
    QSignalSpy changed(&model, &EqualizerModel::changed);

    QVERIFY(!model.setGain(100, 1.0));

    compareGains(model.bands(), {0.0, 0.0, 0.0, 0.0, 0.0});
    QCOMPARE(changed.count(), 0);
}

void EqualizerModelTest::setGainRejectsNan()
{
    EqualizerModel model;
    QSignalSpy changed(&model, &EqualizerModel::changed);

    QVERIFY(!model.setGain(60, std::numeric_limits<double>::quiet_NaN()));

    compareGains(model.bands(), {0.0, 0.0, 0.0, 0.0, 0.0});
    QCOMPARE(changed.count(), 0);
}

void EqualizerModelTest::setGainSameValueIsNoOp()
{
    EqualizerModel model;
    QVERIFY(model.setGain(230, 2.5));
    QSignalSpy changed(&model, &EqualizerModel::changed);

    QVERIFY(model.setGain(230, 2.5));

    QCOMPARE(model.bands().at(1).gainDb, 2.5);
    QCOMPARE(changed.count(), 0);
}

void EqualizerModelTest::setGainMarksCurveCustom()
{
    EqualizerModel model;
    model.applyPreset(QStringLiteral("Bass"));
    QCOMPARE(model.currentPreset(), QStringLiteral("Bass"));

    QVERIFY(model.setGain(60, 0.0));

    QCOMPARE(model.currentPreset(), QString());
}

void EqualizerModelTest::applyPresetFillsBassCurve()
{
    EqualizerModel model;
    QSignalSpy changed(&model, &EqualizerModel::changed);

    model.applyPreset(QStringLiteral("Bass"));

    compareGains(model.bands(), {5.1, 1.2, 0.0, 0.0, 0.0});
    QCOMPARE(model.currentPreset(), QStringLiteral("Bass"));
    QCOMPARE(changed.count(), 1);
}

void EqualizerModelTest::applyPresetFillsVocalCurve()
{
    EqualizerModel model;

    model.applyPreset(QStringLiteral("Vocal"));

    compareGains(model.bands(), {-1.1, 0.9, 2.9, 2.3, 0.2});
    QCOMPARE(model.currentPreset(), QStringLiteral("Vocal"));
}

void EqualizerModelTest::applyPresetFillsRockCurve()
{
    EqualizerModel model;

    model.applyPreset(QStringLiteral("Rock"));

    compareGains(model.bands(), {3.1, 1.1, -1.0, 0.8, 2.8});
    QCOMPARE(model.currentPreset(), QStringLiteral("Rock"));
}

void EqualizerModelTest::applyPresetFillsElectronicCurve()
{
    EqualizerModel model;

    model.applyPreset(QStringLiteral("Electronic"));

    compareGains(model.bands(), {4.1, 0.2, 0.7, 2.8, 4.0});
    QCOMPARE(model.currentPreset(), QStringLiteral("Electronic"));
}

void EqualizerModelTest::applyPresetFillsNightCurve()
{
    EqualizerModel model;

    model.applyPreset(QStringLiteral("Night"));

    compareGains(model.bands(), {-3.1, -1.1, 0.0, -1.8, -3.8});
    QCOMPARE(model.currentPreset(), QStringLiteral("Night"));
}

void EqualizerModelTest::applyPresetFlatRestoresZeroGains()
{
    EqualizerModel model;
    model.applyPreset(QStringLiteral("Bass"));
    QSignalSpy changed(&model, &EqualizerModel::changed);

    model.applyPreset(QStringLiteral("Flat"));

    compareGains(model.bands(), {0.0, 0.0, 0.0, 0.0, 0.0});
    QCOMPARE(model.currentPreset(), QStringLiteral("Flat"));
    QCOMPARE(changed.count(), 1);
}

void EqualizerModelTest::applyPresetUnknownIsIgnored()
{
    EqualizerModel model;
    QSignalSpy changed(&model, &EqualizerModel::changed);

    model.applyPreset(QStringLiteral("Pop"));
    model.applyPreset(QString());

    compareGains(model.bands(), {0.0, 0.0, 0.0, 0.0, 0.0});
    QCOMPARE(model.currentPreset(), QStringLiteral("Flat"));
    QCOMPARE(changed.count(), 0);
}

void EqualizerModelTest::applyPresetSamePresetIsNoOp()
{
    EqualizerModel model;
    model.applyPreset(QStringLiteral("Rock"));
    QSignalSpy changed(&model, &EqualizerModel::changed);

    model.applyPreset(QStringLiteral("Rock"));

    QCOMPARE(changed.count(), 0);
}

void EqualizerModelTest::presetsResolveOnCustomLayout()
{
    EqualizerModel model;

    model.setBands({1000, 100});
    QCOMPARE(frequenciesOf(model.bands()), QVector<int>({100, 1000}));

    model.applyPreset(QStringLiteral("Bass"));

    compareGains(model.bands(), {3.7, 0.0});
    QCOMPARE(model.currentPreset(), QStringLiteral("Bass"));
}

void EqualizerModelTest::presetGainsStayWithinRange()
{
    EqualizerModel model;

    for (const QString& name : model.presets()) {
        model.applyPreset(name);
        QCOMPARE(model.currentPreset(), name);
        for (const EqualizerBand& band : model.bands()) {
            QVERIFY2(band.gainDb >= -12.0 && band.gainDb <= 12.0,
                     qPrintable(QStringLiteral("%1 at %2 Hz is %3 dB")
                                    .arg(name)
                                    .arg(band.frequencyHz)
                                    .arg(band.gainDb)));
        }
    }
}

void EqualizerModelTest::currentPresetEmptyForCustomCurve()
{
    EqualizerModel model;

    QVERIFY(model.setGain(230, 1.5));

    QCOMPARE(model.currentPreset(), QString());
}

void EqualizerModelTest::currentPresetMatchesManualCurve()
{
    EqualizerModel model;

    QVERIFY(model.setGain(60, 5.1));
    QVERIFY(model.setGain(230, 1.2));

    QCOMPARE(model.currentPreset(), QStringLiteral("Bass"));
}

void EqualizerModelTest::resetRestoresDefaults()
{
    EqualizerModel model;
    model.setBands({100, 200});
    model.applyPreset(QStringLiteral("Bass"));
    QSignalSpy changed(&model, &EqualizerModel::changed);

    model.reset();

    QCOMPARE(frequenciesOf(model.bands()), QVector<int>({60, 230, 910, 3600, 14000}));
    compareGains(model.bands(), {0.0, 0.0, 0.0, 0.0, 0.0});
    QCOMPARE(model.currentPreset(), QStringLiteral("Flat"));
    QCOMPARE(changed.count(), 1);
}

void EqualizerModelTest::resetDropsCustomGains()
{
    EqualizerModel model;
    QVERIFY(model.setGain(60, 3.0));
    QSignalSpy changed(&model, &EqualizerModel::changed);

    model.reset();

    compareGains(model.bands(), {0.0, 0.0, 0.0, 0.0, 0.0});
    QCOMPARE(changed.count(), 1);
}

void EqualizerModelTest::resetWhenAlreadyDefaultIsNoOp()
{
    EqualizerModel model;
    QSignalSpy changed(&model, &EqualizerModel::changed);

    model.reset();

    QCOMPARE(changed.count(), 0);
}

void EqualizerModelTest::changedOnlyOnRealChanges()
{
    EqualizerModel model;
    model.setBands({60, 230, 910, 3600, 14000});
    model.applyPreset(QStringLiteral("Flat"));
    QSignalSpy changed(&model, &EqualizerModel::changed);

    model.setBands({14000, 3600, 910, 230, 60});
    model.applyPreset(QStringLiteral("Flat"));
    QVERIFY(model.setGain(60, 0.0));

    QCOMPARE(changed.count(), 0);
}

QTEST_GUILESS_MAIN(EqualizerModelTest)
#include "tst_equalizer_model.moc"
