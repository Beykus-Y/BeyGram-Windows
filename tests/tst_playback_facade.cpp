#include "ui/PlaybackFacade.h"

#include <QSignalSpy>
#include <QStringList>
#include <QVariantList>
#include <QtTest>

#include <limits>

using namespace beygram::ui;

namespace {

// "Performer — Title" with a real em dash, kept ASCII in this source file.
QString joined(const QString& performer, const QString& title)
{
    return performer + QLatin1Char(' ') + QChar(0x2014) + QLatin1Char(' ')
        + title;
}

QString firstArgument(const QSignalSpy& spy, int index)
{
    return spy.at(index).at(0).toString();
}

} // namespace

class PlaybackFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsInactive();

    void openVideoActivatesClip();
    void openVideoIgnoresInvalidId();
    void openVideoIgnoresNonPositiveDuration();
    void openAudioUsesDisplayTitle();
    void openAudioFallsBackToPresentTag();
    void openAudioWithoutTagsIsUnknown();
    void openVideoReplacesOpenAudio();

    void playPauseTogglesAndAnnounces();
    void playPauseWithoutClipIsSilent();

    void stopClosesAndAnnouncesPause();
    void stopWithoutClipIsSilent();
    void stopKeepsPreferences();

    void seekClampsToClip();
    void seekWithoutClipIsSilent();

    void volumeClampsAndSurvivesReopen();
    void volumeIgnoredWithoutClip();
    void rateClampsAndIgnoresNan();
    void rateIgnoredWithoutClip();

    void waveformIsNormalized();
    void waveformKeepsAlignmentForUnreadableValues();
    void waveformOfZerosIsFlat();
    void waveformOfEmptyListIsEmpty();
    void waveformClearsWhenClipOpens();

    void queueHasNextWithoutOpenClip();
    void queueNextAnnouncesEachEntry();
    void queueNextStopsAtEnd();
    void queuePreviousStartsAtLastEntry();
    void queuePreviousStopsAtStart();
    void setQueueDropsInvalidAndDuplicateIds();
    void setQueueSyncsCursorToOpenClip();

    void changedOnlyOnRealChanges();
};

void PlaybackFacadeTest::startsInactive()
{
    const PlaybackFacade facade;

    QVERIFY(!facade.active());
    QVERIFY(facade.title().isEmpty());
    QVERIFY(!facade.playing());
    QCOMPARE(facade.positionMs(), 0);
    QCOMPARE(facade.durationMs(), 0);
    QCOMPARE(facade.volume(), 100);
    QCOMPARE(facade.rate(), 1.0);
    QVERIFY(facade.waveform().isEmpty());
    QVERIFY(!facade.hasNext());
}

void PlaybackFacadeTest::openVideoActivatesClip()
{
    PlaybackFacade facade;

    facade.openVideo(QStringLiteral("42"), QStringLiteral("Holiday"), 90000);

    QVERIFY(facade.active());
    QCOMPARE(facade.title(), QStringLiteral("Holiday"));
    QCOMPARE(facade.durationMs(), 90000);
    QCOMPARE(facade.positionMs(), 0);
    QVERIFY(!facade.playing());
}

void PlaybackFacadeTest::openVideoIgnoresInvalidId()
{
    PlaybackFacade facade;

    facade.openVideo(QString(), QStringLiteral("A"), 1000);
    facade.openVideo(QStringLiteral("0"), QStringLiteral("B"), 1000);
    facade.openVideo(QStringLiteral("-7"), QStringLiteral("C"), 1000);
    facade.openVideo(QStringLiteral("abc"), QStringLiteral("D"), 1000);
    facade.openVideo(QStringLiteral("1.5"), QStringLiteral("E"), 1000);

    QVERIFY(!facade.active());
    QVERIFY(facade.title().isEmpty());
    QCOMPARE(facade.durationMs(), 0);
}

void PlaybackFacadeTest::openVideoIgnoresNonPositiveDuration()
{
    PlaybackFacade facade;

    facade.openVideo(QStringLiteral("1"), QStringLiteral("A"), 0);
    facade.openVideo(QStringLiteral("2"), QStringLiteral("B"), -3000);

    QVERIFY(!facade.active());
    QCOMPARE(facade.durationMs(), 0);
    QVERIFY(!facade.playing());
}

void PlaybackFacadeTest::openAudioUsesDisplayTitle()
{
    PlaybackFacade facade;

    facade.openAudio(QStringLiteral("7"), QStringLiteral("Nightfall"),
                     QStringLiteral("Anna"), 245000);

    QVERIFY(facade.active());
    QCOMPARE(facade.title(), joined(QStringLiteral("Anna"), QStringLiteral("Nightfall")));
    QCOMPARE(facade.durationMs(), 245000);
    QVERIFY(!facade.playing());
}

void PlaybackFacadeTest::openAudioFallsBackToPresentTag()
{
    PlaybackFacade facade;

    facade.openAudio(QStringLiteral("7"), QString(), QStringLiteral("Anna"), 1000);
    QCOMPARE(facade.title(), QStringLiteral("Anna"));

    facade.openAudio(QStringLiteral("8"), QStringLiteral("Nightfall"), QString(), 1000);
    QCOMPARE(facade.title(), QStringLiteral("Nightfall"));
}

void PlaybackFacadeTest::openAudioWithoutTagsIsUnknown()
{
    PlaybackFacade facade;

    facade.openAudio(QStringLiteral("9"), QString(), QString(), 1000);

    QVERIFY(facade.active());
    QCOMPARE(facade.title(), QStringLiteral("Unknown title"));
}

void PlaybackFacadeTest::openVideoReplacesOpenAudio()
{
    PlaybackFacade facade;

    facade.openAudio(QStringLiteral("7"), QStringLiteral("Nightfall"),
                     QStringLiteral("Anna"), 60000);
    facade.setWaveform(QVariantList{10, 20, 30});
    QCOMPARE(int(facade.waveform().size()), 3);

    facade.openVideo(QStringLiteral("8"), QStringLiteral("Clip"), 5000);

    QCOMPARE(facade.title(), QStringLiteral("Clip"));
    QCOMPARE(facade.durationMs(), 5000);
    QVERIFY(facade.waveform().isEmpty());
}

void PlaybackFacadeTest::playPauseTogglesAndAnnounces()
{
    PlaybackFacade facade;
    facade.openVideo(QStringLiteral("42"), QStringLiteral("Clip"), 60000);

    QSignalSpy playSpy(&facade, &PlaybackFacade::playRequested);
    QSignalSpy pauseSpy(&facade, &PlaybackFacade::pauseRequested);

    facade.playPause();

    QVERIFY(facade.playing());
    QCOMPARE(playSpy.count(), 1);
    QCOMPARE(firstArgument(playSpy, 0), QStringLiteral("42"));
    QCOMPARE(pauseSpy.count(), 0);

    facade.playPause();

    QVERIFY(!facade.playing());
    QCOMPARE(playSpy.count(), 1);
    QCOMPARE(pauseSpy.count(), 1);
    QCOMPARE(firstArgument(pauseSpy, 0), QStringLiteral("42"));
}

void PlaybackFacadeTest::playPauseWithoutClipIsSilent()
{
    PlaybackFacade facade;

    QSignalSpy playSpy(&facade, &PlaybackFacade::playRequested);
    QSignalSpy pauseSpy(&facade, &PlaybackFacade::pauseRequested);
    QSignalSpy changedSpy(&facade, &PlaybackFacade::changed);

    facade.playPause();

    QVERIFY(!facade.playing());
    QCOMPARE(playSpy.count(), 0);
    QCOMPARE(pauseSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 0);
}

void PlaybackFacadeTest::stopClosesAndAnnouncesPause()
{
    PlaybackFacade facade;
    facade.openVideo(QStringLiteral("42"), QStringLiteral("Clip"), 90000);
    facade.seek(1000);
    facade.playPause();

    QSignalSpy changedSpy(&facade, &PlaybackFacade::changed);
    QSignalSpy pauseSpy(&facade, &PlaybackFacade::pauseRequested);

    facade.stop();

    QVERIFY(!facade.active());
    QVERIFY(facade.title().isEmpty());
    QCOMPARE(facade.positionMs(), 0);
    QCOMPARE(facade.durationMs(), 0);
    QVERIFY(!facade.playing());

    QCOMPARE(pauseSpy.count(), 1);
    QCOMPARE(firstArgument(pauseSpy, 0), QStringLiteral("42"));
    QCOMPARE(changedSpy.count(), 1);
}

void PlaybackFacadeTest::stopWithoutClipIsSilent()
{
    PlaybackFacade facade;

    QSignalSpy changedSpy(&facade, &PlaybackFacade::changed);
    QSignalSpy pauseSpy(&facade, &PlaybackFacade::pauseRequested);

    facade.stop();

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(pauseSpy.count(), 0);
}

void PlaybackFacadeTest::stopKeepsPreferences()
{
    PlaybackFacade facade;
    facade.openVideo(QStringLiteral("1"), QStringLiteral("A"), 1000);
    facade.setVolume(35);
    facade.setRate(1.5);

    facade.stop();

    QCOMPARE(facade.volume(), 35);
    QCOMPARE(facade.rate(), 1.5);

    facade.openVideo(QStringLiteral("2"), QStringLiteral("B"), 2000);

    QCOMPARE(facade.volume(), 35);
    QCOMPARE(facade.rate(), 1.5);
}

void PlaybackFacadeTest::seekClampsToClip()
{
    PlaybackFacade facade;
    facade.openVideo(QStringLiteral("1"), QStringLiteral("A"), 10000);

    facade.seek(2500);
    QCOMPARE(facade.positionMs(), 2500);

    facade.seek(-500);
    QCOMPARE(facade.positionMs(), 0);

    facade.seek(999999);
    QCOMPARE(facade.positionMs(), 10000);
}

void PlaybackFacadeTest::seekWithoutClipIsSilent()
{
    PlaybackFacade facade;

    QSignalSpy changedSpy(&facade, &PlaybackFacade::changed);
    facade.seek(5000);

    QCOMPARE(facade.positionMs(), 0);
    QCOMPARE(changedSpy.count(), 0);
}

void PlaybackFacadeTest::volumeClampsAndSurvivesReopen()
{
    PlaybackFacade facade;
    facade.openVideo(QStringLiteral("1"), QStringLiteral("A"), 1000);

    facade.setVolume(40);
    QCOMPARE(facade.volume(), 40);

    facade.setVolume(150);
    QCOMPARE(facade.volume(), 100);

    facade.setVolume(-5);
    QCOMPARE(facade.volume(), 0);

    facade.setVolume(35);
    facade.stop();
    facade.openVideo(QStringLiteral("2"), QStringLiteral("B"), 2000);

    QCOMPARE(facade.volume(), 35);
}

void PlaybackFacadeTest::volumeIgnoredWithoutClip()
{
    PlaybackFacade facade;

    facade.setVolume(30);

    QCOMPARE(facade.volume(), 100);
}

void PlaybackFacadeTest::rateClampsAndIgnoresNan()
{
    PlaybackFacade facade;
    facade.openVideo(QStringLiteral("1"), QStringLiteral("A"), 1000);

    facade.setRate(0.1);
    QCOMPARE(facade.rate(), 0.25);

    facade.setRate(9.0);
    QCOMPARE(facade.rate(), 3.0);

    facade.setRate(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(facade.rate(), 3.0);

    facade.setRate(1.5);
    QCOMPARE(facade.rate(), 1.5);
}

void PlaybackFacadeTest::rateIgnoredWithoutClip()
{
    PlaybackFacade facade;

    facade.setRate(2.0);

    QCOMPARE(facade.rate(), 1.0);
}

void PlaybackFacadeTest::waveformIsNormalized()
{
    PlaybackFacade facade;
    facade.openAudio(QStringLiteral("7"), QStringLiteral("Song"),
                     QStringLiteral("Anna"), 60000);

    facade.setWaveform(QVariantList{0, 50, 100, -20});

    QCOMPARE(facade.waveform(), (QStringList{QStringLiteral("0.000"),
                                             QStringLiteral("0.500"),
                                             QStringLiteral("1.000"),
                                             QStringLiteral("0.000")}));
}

void PlaybackFacadeTest::waveformKeepsAlignmentForUnreadableValues()
{
    PlaybackFacade facade;

    // A value that cannot be read as a number becomes a zero bar instead of
    // being skipped, so the envelope keeps one bar per source sample.
    facade.setWaveform(QVariantList{QVariant(), 40});

    QCOMPARE(facade.waveform(), (QStringList{QStringLiteral("0.000"),
                                             QStringLiteral("1.000")}));
}

void PlaybackFacadeTest::waveformOfZerosIsFlat()
{
    PlaybackFacade facade;

    facade.setWaveform(QVariantList{0, 0, 0});

    QCOMPARE(facade.waveform(), (QStringList{QStringLiteral("0.000"),
                                             QStringLiteral("0.000"),
                                             QStringLiteral("0.000")}));
}

void PlaybackFacadeTest::waveformOfEmptyListIsEmpty()
{
    PlaybackFacade facade;
    facade.setWaveform(QVariantList{1, 2});

    facade.setWaveform(QVariantList());

    QVERIFY(facade.waveform().isEmpty());
}

void PlaybackFacadeTest::waveformClearsWhenClipOpens()
{
    PlaybackFacade facade;
    facade.openAudio(QStringLiteral("7"), QStringLiteral("Song"),
                     QStringLiteral("Anna"), 60000);
    facade.setWaveform(QVariantList{10, 20});
    QCOMPARE(int(facade.waveform().size()), 2);

    facade.openVideo(QStringLiteral("8"), QStringLiteral("Clip"), 5000);

    QVERIFY(facade.waveform().isEmpty());
}

void PlaybackFacadeTest::queueHasNextWithoutOpenClip()
{
    PlaybackFacade facade;
    QVERIFY(!facade.hasNext());

    facade.setQueue({QStringLiteral("5"), QStringLiteral("6")},
                    {QStringLiteral("A"), QStringLiteral("B")});

    QVERIFY(facade.hasNext());
    QVERIFY(!facade.active());
}

void PlaybackFacadeTest::queueNextAnnouncesEachEntry()
{
    PlaybackFacade facade;
    facade.setQueue({QStringLiteral("5"), QStringLiteral("6"), QStringLiteral("7")},
                    {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")});

    QSignalSpy playSpy(&facade, &PlaybackFacade::playRequested);

    facade.next();
    QCOMPARE(playSpy.count(), 1);
    QCOMPARE(firstArgument(playSpy, 0), QStringLiteral("5"));
    QVERIFY(facade.hasNext());

    facade.next();
    QCOMPARE(playSpy.count(), 2);
    QCOMPARE(firstArgument(playSpy, 1), QStringLiteral("6"));
    QVERIFY(facade.hasNext());

    facade.next();
    QCOMPARE(playSpy.count(), 3);
    QCOMPARE(firstArgument(playSpy, 2), QStringLiteral("7"));
    QVERIFY(!facade.hasNext());
}

void PlaybackFacadeTest::queueNextStopsAtEnd()
{
    PlaybackFacade facade;
    facade.setQueue({QStringLiteral("5"), QStringLiteral("6")},
                    {QStringLiteral("A"), QStringLiteral("B")});

    QSignalSpy playSpy(&facade, &PlaybackFacade::playRequested);
    facade.next();
    facade.next();
    QCOMPARE(playSpy.count(), 2);

    facade.next();
    QCOMPARE(playSpy.count(), 2);
}

void PlaybackFacadeTest::queuePreviousStartsAtLastEntry()
{
    PlaybackFacade facade;
    facade.setQueue({QStringLiteral("5"), QStringLiteral("6"), QStringLiteral("7")},
                    {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")});

    QSignalSpy playSpy(&facade, &PlaybackFacade::playRequested);

    facade.previous();

    QCOMPARE(playSpy.count(), 1);
    QCOMPARE(firstArgument(playSpy, 0), QStringLiteral("7"));
    QVERIFY(!facade.hasNext());
}

void PlaybackFacadeTest::queuePreviousStopsAtStart()
{
    PlaybackFacade facade;
    facade.setQueue({QStringLiteral("5"), QStringLiteral("6")},
                    {QStringLiteral("A"), QStringLiteral("B")});

    QSignalSpy playSpy(&facade, &PlaybackFacade::playRequested);
    facade.next();
    QCOMPARE(playSpy.count(), 1);

    facade.previous();
    QCOMPARE(playSpy.count(), 1);
}

void PlaybackFacadeTest::setQueueDropsInvalidAndDuplicateIds()
{
    PlaybackFacade facade;
    facade.setQueue({QString(), QStringLiteral("0"), QStringLiteral("abc"),
                     QStringLiteral("5"), QStringLiteral("5"), QStringLiteral("-2")},
                    {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"),
                     QStringLiteral("D"), QStringLiteral("E"), QStringLiteral("F")});

    QSignalSpy playSpy(&facade, &PlaybackFacade::playRequested);

    facade.next();
    QCOMPARE(playSpy.count(), 1);
    QCOMPARE(firstArgument(playSpy, 0), QStringLiteral("5"));
    QVERIFY(!facade.hasNext());

    facade.next();
    QCOMPARE(playSpy.count(), 1);
}

void PlaybackFacadeTest::setQueueSyncsCursorToOpenClip()
{
    PlaybackFacade facade;
    facade.openVideo(QStringLiteral("11"), QStringLiteral("B"), 1000);
    QVERIFY(!facade.hasNext());

    facade.setQueue({QStringLiteral("10"), QStringLiteral("11"), QStringLiteral("12")},
                    {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")});

    QVERIFY(facade.hasNext());

    QSignalSpy playSpy(&facade, &PlaybackFacade::playRequested);
    facade.next();

    QCOMPARE(playSpy.count(), 1);
    QCOMPARE(firstArgument(playSpy, 0), QStringLiteral("12"));
    QVERIFY(!facade.hasNext());
}

void PlaybackFacadeTest::changedOnlyOnRealChanges()
{
    PlaybackFacade facade;
    facade.openVideo(QStringLiteral("1"), QStringLiteral("A"), 1000);

    QSignalSpy changedSpy(&facade, &PlaybackFacade::changed);

    facade.openVideo(QStringLiteral("2"), QStringLiteral("B"), 2000);
    QCOMPARE(changedSpy.count(), 1);

    // Reopening the same clip at the start changes nothing observable.
    facade.openVideo(QStringLiteral("2"), QStringLiteral("B"), 2000);
    QCOMPARE(changedSpy.count(), 1);

    facade.setVolume(facade.volume());
    QCOMPARE(changedSpy.count(), 1);

    facade.seek(0);
    QCOMPARE(changedSpy.count(), 1);

    facade.setRate(facade.rate());
    QCOMPARE(changedSpy.count(), 1);

    facade.setVolume(50);
    QCOMPARE(changedSpy.count(), 2);

    facade.seek(500);
    QCOMPARE(changedSpy.count(), 3);
}

QTEST_GUILESS_MAIN(PlaybackFacadeTest)

#include "tst_playback_facade.moc"
