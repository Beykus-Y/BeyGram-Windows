#include "ui/EqualizerFacade.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtTest>

#include <limits>

using namespace beygram::ui;

namespace {

/// The five gains the default layout starts with, all flat.
QStringList flatGains()
{
    return {QStringLiteral("0.0 dB"), QStringLiteral("0.0 dB"), QStringLiteral("0.0 dB"),
            QStringLiteral("0.0 dB"), QStringLiteral("0.0 dB")};
}

} // namespace

class EqualizerFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsDisabledWithFlatCurve();
    void bandLabelsNameTheFrequencies();
    void presetsAreTheModelPresets();

    void gainsAreFormattedWithOneDecimal();
    void gainsFollowTheBandIndex();
    void setGainClampsToModelRange();
    void setGainIgnoresAnUnknownIndex();
    void setGainRejectsNan();
    void setGainSameValueIsSilent();
    void setGainDropsThePresetName();

    void applyPresetFillsEveryBand();
    void applyPresetUnknownIsIgnored();
    void applyPresetSameIsSilent();

    void presetIsEmptyForCustomCurve();

    void resetRestoresTheFlatCurve();
    void resetLeavesTheSwitchAlone();
    void resetWhenFlatIsSilent();

    void setEnabledFlipsTheSwitch();
    void setEnabledSameValueIsSilent();
    void enabledPropertyIsWritable();

    void changedCoversEveryRealChange();
    void listsAreCopies();
};

void EqualizerFacadeTest::startsDisabledWithFlatCurve()
{
    const EqualizerFacade facade;

    QVERIFY(!facade.enabled());
    QCOMPARE(facade.preset(), QStringLiteral("Flat"));
    QCOMPARE(facade.gains(), flatGains());
    QCOMPARE(static_cast<int>(facade.gains().size()), 5);
}

void EqualizerFacadeTest::bandLabelsNameTheFrequencies()
{
    const EqualizerFacade facade;

    QCOMPARE(facade.bandLabels(),
             QStringList({QStringLiteral("60 Hz"), QStringLiteral("230 Hz"),
                          QStringLiteral("910 Hz"), QStringLiteral("3.6 kHz"),
                          QStringLiteral("14 kHz")}));
}

void EqualizerFacadeTest::presetsAreTheModelPresets()
{
    const EqualizerFacade facade;

    QCOMPARE(facade.presets(),
             QStringList({QStringLiteral("Flat"), QStringLiteral("Bass"),
                          QStringLiteral("Vocal"), QStringLiteral("Rock"),
                          QStringLiteral("Electronic"), QStringLiteral("Night")}));
    QCOMPARE(static_cast<int>(facade.presets().size()), 6);
}

void EqualizerFacadeTest::gainsAreFormattedWithOneDecimal()
{
    EqualizerFacade facade;

    facade.setGain(0, -3.0);
    facade.setGain(1, 2.5);
    facade.setGain(2, 0.4);
    facade.setGain(3, -9.5);

    QCOMPARE(facade.gains(),
             QStringList({QStringLiteral("-3.0 dB"), QStringLiteral("2.5 dB"),
                          QStringLiteral("0.4 dB"), QStringLiteral("-9.5 dB"),
                          QStringLiteral("0.0 dB")}));

    // A stored -0.0 is shown as a flat band, not as "-0.0 dB".
    facade.setGain(4, 1.0);
    facade.setGain(4, -0.0);
    QCOMPARE(facade.gains().at(4), QStringLiteral("0.0 dB"));
}

void EqualizerFacadeTest::gainsFollowTheBandIndex()
{
    EqualizerFacade facade;

    facade.setGain(4, -3.0);

    QStringList expected = flatGains();
    expected[4] = QStringLiteral("-3.0 dB");
    QCOMPARE(facade.gains(), expected);
    QCOMPARE(facade.bandLabels().at(4), QStringLiteral("14 kHz"));
}

void EqualizerFacadeTest::setGainClampsToModelRange()
{
    EqualizerFacade facade;

    facade.setGain(0, 30.0);
    QCOMPARE(facade.gains().at(0), QStringLiteral("12.0 dB"));

    facade.setGain(0, -30.0);
    QCOMPARE(facade.gains().at(0), QStringLiteral("-12.0 dB"));
}

void EqualizerFacadeTest::setGainIgnoresAnUnknownIndex()
{
    EqualizerFacade facade;
    QSignalSpy changed(&facade, &EqualizerFacade::changed);

    facade.setGain(-1, 3.0);
    facade.setGain(5, 3.0);
    facade.setGain(1000, 3.0);

    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.gains(), flatGains());
}

void EqualizerFacadeTest::setGainRejectsNan()
{
    EqualizerFacade facade;
    QSignalSpy changed(&facade, &EqualizerFacade::changed);

    facade.setGain(0, std::numeric_limits<double>::quiet_NaN());

    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.gains(), flatGains());
}

void EqualizerFacadeTest::setGainSameValueIsSilent()
{
    EqualizerFacade facade;
    facade.setGain(1, 2.5);
    QSignalSpy changed(&facade, &EqualizerFacade::changed);

    facade.setGain(1, 2.5);

    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.gains().at(1), QStringLiteral("2.5 dB"));
}

void EqualizerFacadeTest::setGainDropsThePresetName()
{
    EqualizerFacade facade;
    facade.applyPreset(QStringLiteral("Bass"));
    QCOMPARE(facade.preset(), QStringLiteral("Bass"));

    facade.setGain(0, 0.0);

    QCOMPARE(facade.preset(), QString());
}

void EqualizerFacadeTest::applyPresetFillsEveryBand()
{
    EqualizerFacade facade;
    QSignalSpy changed(&facade, &EqualizerFacade::changed);

    facade.applyPreset(QStringLiteral("Bass"));

    QCOMPARE(facade.preset(), QStringLiteral("Bass"));
    QCOMPARE(facade.gains(),
             QStringList({QStringLiteral("5.1 dB"), QStringLiteral("1.2 dB"),
                          QStringLiteral("0.0 dB"), QStringLiteral("0.0 dB"),
                          QStringLiteral("0.0 dB")}));
    QCOMPARE(changed.count(), 1);
}

void EqualizerFacadeTest::applyPresetUnknownIsIgnored()
{
    EqualizerFacade facade;
    QSignalSpy changed(&facade, &EqualizerFacade::changed);

    facade.applyPreset(QStringLiteral("Pop"));
    facade.applyPreset(QString());

    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.preset(), QStringLiteral("Flat"));
    QCOMPARE(facade.gains(), flatGains());
}

void EqualizerFacadeTest::applyPresetSameIsSilent()
{
    EqualizerFacade facade;
    facade.applyPreset(QStringLiteral("Rock"));
    QSignalSpy changed(&facade, &EqualizerFacade::changed);

    facade.applyPreset(QStringLiteral("Rock"));

    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.preset(), QStringLiteral("Rock"));
}

void EqualizerFacadeTest::presetIsEmptyForCustomCurve()
{
    EqualizerFacade facade;

    facade.setGain(2, 1.5);

    QCOMPARE(facade.preset(), QString());
}

void EqualizerFacadeTest::resetRestoresTheFlatCurve()
{
    EqualizerFacade facade;
    facade.applyPreset(QStringLiteral("Night"));
    facade.setGain(2, 4.0);
    QSignalSpy changed(&facade, &EqualizerFacade::changed);

    facade.reset();

    QCOMPARE(changed.count(), 1);
    QCOMPARE(facade.preset(), QStringLiteral("Flat"));
    QCOMPARE(facade.gains(), flatGains());
    QCOMPARE(static_cast<int>(facade.bandLabels().size()), 5);
}

void EqualizerFacadeTest::resetLeavesTheSwitchAlone()
{
    EqualizerFacade facade;
    facade.setEnabled(true);
    facade.applyPreset(QStringLiteral("Vocal"));
    QSignalSpy changed(&facade, &EqualizerFacade::changed);

    facade.reset();

    QVERIFY(facade.enabled());
    QCOMPARE(changed.count(), 1);
}

void EqualizerFacadeTest::resetWhenFlatIsSilent()
{
    EqualizerFacade facade;
    QSignalSpy changed(&facade, &EqualizerFacade::changed);

    facade.reset();

    QCOMPARE(changed.count(), 0);
}

void EqualizerFacadeTest::setEnabledFlipsTheSwitch()
{
    EqualizerFacade facade;
    QSignalSpy changed(&facade, &EqualizerFacade::changed);

    facade.setEnabled(true);

    QVERIFY(facade.enabled());
    QCOMPARE(changed.count(), 1);

    facade.setEnabled(false);

    QVERIFY(!facade.enabled());
    QCOMPARE(changed.count(), 2);
}

void EqualizerFacadeTest::setEnabledSameValueIsSilent()
{
    EqualizerFacade facade;
    QSignalSpy changed(&facade, &EqualizerFacade::changed);

    facade.setEnabled(false);

    QVERIFY(!facade.enabled());
    QCOMPARE(changed.count(), 0);
}

void EqualizerFacadeTest::enabledPropertyIsWritable()
{
    EqualizerFacade facade;
    QSignalSpy changed(&facade, &EqualizerFacade::changed);

    // QML writes the property through the meta-object, so the WRITE path is
    // exercised the way a binding would use it.
    QVERIFY(facade.setProperty("enabled", true));

    QVERIFY(facade.enabled());
    QCOMPARE(changed.count(), 1);
}

void EqualizerFacadeTest::changedCoversEveryRealChange()
{
    EqualizerFacade facade;
    QSignalSpy changed(&facade, &EqualizerFacade::changed);

    facade.setGain(0, 3.0);
    facade.setGain(0, 3.0);
    facade.applyPreset(QStringLiteral("Bass"));
    facade.applyPreset(QStringLiteral("Bass"));
    facade.setEnabled(true);
    facade.setEnabled(true);
    facade.reset();
    facade.reset();

    QCOMPARE(changed.count(), 4);
}

void EqualizerFacadeTest::listsAreCopies()
{
    EqualizerFacade facade;

    QStringList labels = facade.bandLabels();
    labels[0] = QStringLiteral("changed");
    QStringList gains = facade.gains();
    gains[0] = QStringLiteral("changed");
    QStringList presets = facade.presets();
    presets.clear();

    QCOMPARE(facade.bandLabels().at(0), QStringLiteral("60 Hz"));
    QCOMPARE(facade.gains().at(0), QStringLiteral("0.0 dB"));
    QCOMPARE(static_cast<int>(facade.presets().size()), 6);
}

QTEST_GUILESS_MAIN(EqualizerFacadeTest)
#include "tst_equalizer_facade.moc"
