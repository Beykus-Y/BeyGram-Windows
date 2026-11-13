#include "telegram/music/PlaybackSettings.h"

#include <QSignalSpy>
#include <QVector>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

class PlaybackSettingsTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithDefaults();
    void speedOptionsAreStable();

    void setSpeedClampsToRange();
    void setSpeedRoundsToTwoDecimals();
    void setSpeedAcceptsEveryOption();
    void setSpeedIgnoresNan();
    void setSpeedClampsInfinity();
    void setSpeedSameValueIsNoOp();

    void setCrossfadeClampsToRange();
    void setCrossfadeSameValueIsNoOp();

    void switchesEmitOnlyOnChange();

    void resetRestoresDefaults();
    void resetAfterSingleChangeEmitsOnce();
    void resetWhenAlreadyDefaultIsNoOp();

    void changedOnlyOnRealChanges();
};

void PlaybackSettingsTest::startsWithDefaults()
{
    const PlaybackSettings settings;

    QCOMPARE(settings.speed(), 1.0);
    QCOMPARE(settings.crossfadeMs(), 4000);
    QVERIFY(!settings.gapless());
    QVERIFY(!settings.normalizeVolume());
    QVERIFY(!settings.skipSilence());
}

void PlaybackSettingsTest::speedOptionsAreStable()
{
    const PlaybackSettings settings;

    const QVector<double> options = settings.speedOptions();
    QCOMPARE(static_cast<int>(options.size()), 7);
    const QVector<double> expected = {0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0};
    for (int index = 0; index < expected.size(); ++index)
        QCOMPARE(options.at(index), expected.at(index));
}

void PlaybackSettingsTest::setSpeedClampsToRange()
{
    PlaybackSettings settings;
    QSignalSpy changed(&settings, &PlaybackSettings::changed);

    settings.setSpeed(0.3);
    QCOMPARE(settings.speed(), 0.5);

    settings.setSpeed(-4.0);
    QCOMPARE(settings.speed(), 0.5);

    settings.setSpeed(2.7);
    QCOMPARE(settings.speed(), 2.0);

    settings.setSpeed(42.0);
    QCOMPARE(settings.speed(), 2.0);

    QCOMPARE(changed.count(), 2);
}

void PlaybackSettingsTest::setSpeedRoundsToTwoDecimals()
{
    PlaybackSettings settings;

    settings.setSpeed(1.234);
    QCOMPARE(settings.speed(), 1.23);

    settings.setSpeed(1.236);
    QCOMPARE(settings.speed(), 1.24);

    settings.setSpeed(0.812);
    QCOMPARE(settings.speed(), 0.81);

    settings.setSpeed(1.999);
    QCOMPARE(settings.speed(), 2.0);

    settings.setSpeed(0.501);
    QCOMPARE(settings.speed(), 0.5);
}

void PlaybackSettingsTest::setSpeedAcceptsEveryOption()
{
    PlaybackSettings settings;
    QSignalSpy changed(&settings, &PlaybackSettings::changed);

    int expectedChanges = 0;
    double previous = settings.speed();
    for (const double option : settings.speedOptions()) {
        settings.setSpeed(option);
        QCOMPARE(settings.speed(), option);
        if (option != previous)
            ++expectedChanges;
        QCOMPARE(changed.count(), expectedChanges);
        previous = option;
    }
}

void PlaybackSettingsTest::setSpeedIgnoresNan()
{
    PlaybackSettings settings;
    settings.setSpeed(1.5);
    QSignalSpy changed(&settings, &PlaybackSettings::changed);

    settings.setSpeed(std::numeric_limits<double>::quiet_NaN());

    QCOMPARE(settings.speed(), 1.5);
    QCOMPARE(changed.count(), 0);
}

void PlaybackSettingsTest::setSpeedClampsInfinity()
{
    PlaybackSettings settings;

    settings.setSpeed(std::numeric_limits<double>::infinity());
    QCOMPARE(settings.speed(), 2.0);

    settings.setSpeed(-std::numeric_limits<double>::infinity());
    QCOMPARE(settings.speed(), 0.5);
}

void PlaybackSettingsTest::setSpeedSameValueIsNoOp()
{
    PlaybackSettings settings;
    settings.setSpeed(1.25);
    QSignalSpy changed(&settings, &PlaybackSettings::changed);

    settings.setSpeed(1.25);
    settings.setSpeed(1.2501);
    settings.setSpeed(1.25004);

    QCOMPARE(settings.speed(), 1.25);
    QCOMPARE(changed.count(), 0);

    // Clamping onto the stored value is a no-op too.
    settings.setSpeed(2.0);
    QCOMPARE(changed.count(), 1);

    settings.setSpeed(9.0);
    QCOMPARE(settings.speed(), 2.0);
    QCOMPARE(changed.count(), 1);
}

void PlaybackSettingsTest::setCrossfadeClampsToRange()
{
    PlaybackSettings settings;
    QSignalSpy changed(&settings, &PlaybackSettings::changed);

    settings.setCrossfadeMs(-1);
    QCOMPARE(settings.crossfadeMs(), 0);
    QCOMPARE(changed.count(), 1);

    settings.setCrossfadeMs(500);
    QCOMPARE(settings.crossfadeMs(), 500);
    QCOMPARE(changed.count(), 2);

    settings.setCrossfadeMs(12000);
    QCOMPARE(settings.crossfadeMs(), 12000);
    QCOMPARE(changed.count(), 3);

    settings.setCrossfadeMs(999999);
    QCOMPARE(settings.crossfadeMs(), 12000);
    QCOMPARE(changed.count(), 3);
}

void PlaybackSettingsTest::setCrossfadeSameValueIsNoOp()
{
    PlaybackSettings settings;
    settings.setCrossfadeMs(12000);
    QSignalSpy changed(&settings, &PlaybackSettings::changed);

    settings.setCrossfadeMs(12000);
    settings.setCrossfadeMs(20000);

    QCOMPARE(settings.crossfadeMs(), 12000);
    QCOMPARE(changed.count(), 0);
}

void PlaybackSettingsTest::switchesEmitOnlyOnChange()
{
    PlaybackSettings settings;
    QSignalSpy changed(&settings, &PlaybackSettings::changed);

    QVERIFY(!settings.gapless());
    settings.setGapless(true);
    QVERIFY(settings.gapless());
    QCOMPARE(changed.count(), 1);
    settings.setGapless(true);
    QCOMPARE(changed.count(), 1);
    settings.setGapless(false);
    QVERIFY(!settings.gapless());
    QCOMPARE(changed.count(), 2);

    QVERIFY(!settings.normalizeVolume());
    settings.setNormalizeVolume(true);
    QVERIFY(settings.normalizeVolume());
    QCOMPARE(changed.count(), 3);
    settings.setNormalizeVolume(true);
    QCOMPARE(changed.count(), 3);
    settings.setNormalizeVolume(false);
    QVERIFY(!settings.normalizeVolume());
    QCOMPARE(changed.count(), 4);

    QVERIFY(!settings.skipSilence());
    settings.setSkipSilence(true);
    QVERIFY(settings.skipSilence());
    QCOMPARE(changed.count(), 5);
    settings.setSkipSilence(true);
    QCOMPARE(changed.count(), 5);
    settings.setSkipSilence(false);
    QVERIFY(!settings.skipSilence());
    QCOMPARE(changed.count(), 6);
}

void PlaybackSettingsTest::resetRestoresDefaults()
{
    PlaybackSettings settings;
    settings.setSpeed(1.5);
    settings.setCrossfadeMs(9000);
    settings.setGapless(true);
    settings.setNormalizeVolume(true);
    settings.setSkipSilence(true);
    QSignalSpy changed(&settings, &PlaybackSettings::changed);

    settings.reset();

    QCOMPARE(settings.speed(), 1.0);
    QCOMPARE(settings.crossfadeMs(), 4000);
    QVERIFY(!settings.gapless());
    QVERIFY(!settings.normalizeVolume());
    QVERIFY(!settings.skipSilence());
    QCOMPARE(changed.count(), 1);
}

void PlaybackSettingsTest::resetAfterSingleChangeEmitsOnce()
{
    PlaybackSettings settings;
    settings.setSkipSilence(true);
    QSignalSpy changed(&settings, &PlaybackSettings::changed);

    settings.reset();

    QVERIFY(!settings.skipSilence());
    QCOMPARE(changed.count(), 1);
}

void PlaybackSettingsTest::resetWhenAlreadyDefaultIsNoOp()
{
    PlaybackSettings settings;
    QSignalSpy changed(&settings, &PlaybackSettings::changed);

    settings.reset();

    QCOMPARE(settings.speed(), 1.0);
    QCOMPARE(settings.crossfadeMs(), 4000);
    QCOMPARE(changed.count(), 0);
}

void PlaybackSettingsTest::changedOnlyOnRealChanges()
{
    PlaybackSettings settings;
    settings.setSpeed(1.25);
    settings.setCrossfadeMs(6000);
    settings.setGapless(true);
    settings.setNormalizeVolume(true);
    settings.setSkipSilence(true);
    QSignalSpy changed(&settings, &PlaybackSettings::changed);

    settings.setSpeed(1.25);
    settings.setSpeed(1.251);
    settings.setCrossfadeMs(6000);
    settings.setGapless(true);
    settings.setNormalizeVolume(true);
    settings.setSkipSilence(true);

    QCOMPARE(changed.count(), 0);
}

QTEST_GUILESS_MAIN(PlaybackSettingsTest)
#include "tst_playback_settings.moc"
