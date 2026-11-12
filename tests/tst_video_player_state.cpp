#include "telegram/VideoPlayerState.h"

#include <QSignalSpy>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

// Convenience for the tests that need an open video before exercising a
// setter.
void openSample(VideoPlayerState& state)
{
    state.open(42, 10000);
}

} // namespace

class VideoPlayerStateTest final : public QObject {
    Q_OBJECT

private slots:
    void startsInactiveWithDefaults();
    void openActivatesVideo();
    void openRejectsNonPositiveMessageId();
    void openRejectsNonPositiveDuration();
    void invalidOpenLeavesOpenVideoUntouched();
    void openResetsPositionAndPlaying();
    void openKeepsVolumeMuteAndRate();
    void openAnotherVideoReplacesTheFirst();
    void openEmitsChangedOnce();
    void reopeningSameVideoAtStartIsNoOp();
    void reopeningSameVideoMidPlaybackRewinds();
    void closeDeactivatesVideo();
    void closeDropsTransientFields();
    void closeKeepsUserPreferences();
    void closeWhenInactiveIsNoOp();
    void settersIgnoredWhenInactive();
    void positionClampsToDurationRange();
    void positionEmitsOnlyOnRealChange();
    void playingToggles();
    void volumeClampsToPercentRange();
    void mutedToggles();
    void rateClampsToAllowedRange();
    void rateRejectsNan();
    void progressTracksPosition();
    void progressIsZeroWithoutDuration();
    void progressNeverLeavesUnitRange();
    void resetRestoresDefaults();
    void resetWhenAlreadyDefaultIsNoOp();
    void resetClearsUserPreferences();
    void changedOnlyOnRealChanges();
};

void VideoPlayerStateTest::startsInactiveWithDefaults()
{
    const VideoPlayerState state;

    QVERIFY(!state.active());
    QCOMPARE(state.messageId(), 0);
    QCOMPARE(state.durationMs(), 0);
    QCOMPARE(state.positionMs(), 0);
    QVERIFY(!state.playing());
    QCOMPARE(state.volume(), 100);
    QVERIFY(!state.muted());
    QCOMPARE(state.rate(), 1.0);
    QCOMPARE(state.progress(), 0.0);
}

void VideoPlayerStateTest::openActivatesVideo()
{
    VideoPlayerState state;
    state.open(7, 60000);

    QVERIFY(state.active());
    QCOMPARE(state.messageId(), 7);
    QCOMPARE(state.durationMs(), 60000);
    QCOMPARE(state.positionMs(), 0);
    QVERIFY(!state.playing());
    QCOMPARE(state.progress(), 0.0);
}

void VideoPlayerStateTest::openRejectsNonPositiveMessageId()
{
    VideoPlayerState state;
    QSignalSpy spy(&state, &VideoPlayerState::changed);

    state.open(0, 1000);
    state.open(-1, 1000);
    state.open(-999, 1000);

    QVERIFY(!state.active());
    QCOMPARE(state.messageId(), 0);
    QCOMPARE(state.durationMs(), 0);
    QCOMPARE(spy.count(), 0);
}

void VideoPlayerStateTest::openRejectsNonPositiveDuration()
{
    VideoPlayerState state;
    QSignalSpy spy(&state, &VideoPlayerState::changed);

    state.open(7, 0);
    state.open(7, -1);
    state.open(7, std::numeric_limits<int>::min());

    QVERIFY(!state.active());
    QCOMPARE(state.messageId(), 0);
    QCOMPARE(state.durationMs(), 0);
    QCOMPARE(spy.count(), 0);
}

void VideoPlayerStateTest::invalidOpenLeavesOpenVideoUntouched()
{
    VideoPlayerState state;
    openSample(state);
    state.setPosition(2500);
    state.setPlaying(true);
    QSignalSpy spy(&state, &VideoPlayerState::changed);

    state.open(0, 5000);
    state.open(99, 0);
    state.open(-5, -5);

    QVERIFY(state.active());
    QCOMPARE(state.messageId(), 42);
    QCOMPARE(state.durationMs(), 10000);
    QCOMPARE(state.positionMs(), 2500);
    QVERIFY(state.playing());
    QCOMPARE(spy.count(), 0);
}

void VideoPlayerStateTest::openResetsPositionAndPlaying()
{
    VideoPlayerState state;
    openSample(state);
    state.setPosition(4000);
    state.setPlaying(true);

    state.open(43, 20000);

    QCOMPARE(state.messageId(), 43);
    QCOMPARE(state.durationMs(), 20000);
    QCOMPARE(state.positionMs(), 0);
    QVERIFY(!state.playing());
}

void VideoPlayerStateTest::openKeepsVolumeMuteAndRate()
{
    VideoPlayerState state;
    openSample(state);
    state.setVolume(35);
    state.setMuted(true);
    state.setRate(1.75);

    state.open(43, 20000);

    QCOMPARE(state.volume(), 35);
    QVERIFY(state.muted());
    QCOMPARE(state.rate(), 1.75);
}

void VideoPlayerStateTest::openAnotherVideoReplacesTheFirst()
{
    VideoPlayerState state;
    openSample(state);

    state.open(43, 5000);

    QVERIFY(state.active());
    QCOMPARE(state.messageId(), 43);
    QCOMPARE(state.durationMs(), 5000);

    // The previous video's duration no longer bounds the playhead.
    state.setPosition(9000);
    QCOMPARE(state.positionMs(), 5000);
}

void VideoPlayerStateTest::openEmitsChangedOnce()
{
    VideoPlayerState state;
    QSignalSpy spy(&state, &VideoPlayerState::changed);

    state.open(7, 1000);

    QCOMPARE(spy.count(), 1);
}

void VideoPlayerStateTest::reopeningSameVideoAtStartIsNoOp()
{
    VideoPlayerState state;
    openSample(state);
    QSignalSpy spy(&state, &VideoPlayerState::changed);

    state.open(42, 10000);

    QCOMPARE(spy.count(), 0);
    QVERIFY(state.active());
    QCOMPARE(state.messageId(), 42);
}

void VideoPlayerStateTest::reopeningSameVideoMidPlaybackRewinds()
{
    VideoPlayerState state;
    openSample(state);
    state.setPosition(3000);
    state.setPlaying(true);
    QSignalSpy spy(&state, &VideoPlayerState::changed);

    state.open(42, 10000);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(state.positionMs(), 0);
    QVERIFY(!state.playing());

    // A new duration for the same message is a real change too.
    state.open(42, 12000);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(state.durationMs(), 12000);
}

void VideoPlayerStateTest::closeDeactivatesVideo()
{
    VideoPlayerState state;
    openSample(state);
    QSignalSpy spy(&state, &VideoPlayerState::changed);

    state.close();

    QVERIFY(!state.active());
    QCOMPARE(state.messageId(), 0);
    QCOMPARE(state.durationMs(), 0);
    QCOMPARE(state.progress(), 0.0);
    QCOMPARE(spy.count(), 1);
}

void VideoPlayerStateTest::closeDropsTransientFields()
{
    VideoPlayerState state;
    openSample(state);
    state.setPosition(5000);
    state.setPlaying(true);

    state.close();

    QCOMPARE(state.positionMs(), 0);
    QVERIFY(!state.playing());
}

void VideoPlayerStateTest::closeKeepsUserPreferences()
{
    VideoPlayerState state;
    openSample(state);
    state.setVolume(20);
    state.setMuted(true);
    state.setRate(0.5);

    state.close();

    QCOMPARE(state.volume(), 20);
    QVERIFY(state.muted());
    QCOMPARE(state.rate(), 0.5);

    // The next video starts with those preferences.
    state.open(43, 1000);
    QCOMPARE(state.volume(), 20);
    QVERIFY(state.muted());
    QCOMPARE(state.rate(), 0.5);
}

void VideoPlayerStateTest::closeWhenInactiveIsNoOp()
{
    VideoPlayerState state;
    QSignalSpy spy(&state, &VideoPlayerState::changed);

    state.close();

    QVERIFY(!state.active());
    QCOMPARE(spy.count(), 0);

    // Closing twice only reports the first one.
    openSample(state);
    spy.clear();
    state.close();
    state.close();
    QCOMPARE(spy.count(), 1);
}

void VideoPlayerStateTest::settersIgnoredWhenInactive()
{
    VideoPlayerState state;
    QSignalSpy spy(&state, &VideoPlayerState::changed);

    state.setPosition(500);
    state.setPlaying(true);
    state.setVolume(10);
    state.setMuted(true);
    state.setRate(2.0);

    QCOMPARE(state.positionMs(), 0);
    QVERIFY(!state.playing());
    QCOMPARE(state.volume(), 100);
    QVERIFY(!state.muted());
    QCOMPARE(state.rate(), 1.0);
    QCOMPARE(spy.count(), 0);

    // Setters are ignored again once the video is closed.
    openSample(state);
    state.setVolume(10);
    state.setMuted(true);
    state.setRate(2.0);
    state.close();
    spy.clear();

    state.setPosition(500);
    state.setPlaying(true);
    state.setVolume(10);
    state.setMuted(true);
    state.setRate(2.0);

    QCOMPARE(state.volume(), 10);
    QVERIFY(state.muted());
    QCOMPARE(state.rate(), 2.0);
    QCOMPARE(state.positionMs(), 0);
    QVERIFY(!state.playing());
    QCOMPARE(spy.count(), 0);
}

void VideoPlayerStateTest::positionClampsToDurationRange()
{
    VideoPlayerState state;
    openSample(state);

    state.setPosition(-1);
    QCOMPARE(state.positionMs(), 0);

    state.setPosition(4000);
    QCOMPARE(state.positionMs(), 4000);

    state.setPosition(10001);
    QCOMPARE(state.positionMs(), 10000);

    state.setPosition(std::numeric_limits<int>::max());
    QCOMPARE(state.positionMs(), 10000);

    state.setPosition(std::numeric_limits<int>::min());
    QCOMPARE(state.positionMs(), 0);
}

void VideoPlayerStateTest::positionEmitsOnlyOnRealChange()
{
    VideoPlayerState state;
    openSample(state);
    QSignalSpy spy(&state, &VideoPlayerState::changed);

    state.setPosition(0);
    QCOMPARE(spy.count(), 0);

    state.setPosition(1000);
    QCOMPARE(spy.count(), 1);

    state.setPosition(1000);
    QCOMPARE(spy.count(), 1);

    // Clamping to the value already held is not a change either.
    state.setPosition(10000);
    QCOMPARE(spy.count(), 2);
    state.setPosition(20000);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(state.positionMs(), 10000);
}

void VideoPlayerStateTest::playingToggles()
{
    VideoPlayerState state;
    openSample(state);
    QSignalSpy spy(&state, &VideoPlayerState::changed);

    state.setPlaying(true);
    QVERIFY(state.playing());
    QCOMPARE(spy.count(), 1);

    state.setPlaying(true);
    QCOMPARE(spy.count(), 1);

    state.setPlaying(false);
    QVERIFY(!state.playing());
    QCOMPARE(spy.count(), 2);

    state.setPlaying(false);
    QCOMPARE(spy.count(), 2);
}

void VideoPlayerStateTest::volumeClampsToPercentRange()
{
    VideoPlayerState state;
    openSample(state);

    state.setVolume(0);
    QCOMPARE(state.volume(), 0);

    state.setVolume(-5);
    QCOMPARE(state.volume(), 0);

    state.setVolume(55);
    QCOMPARE(state.volume(), 55);

    state.setVolume(101);
    QCOMPARE(state.volume(), 100);

    state.setVolume(std::numeric_limits<int>::max());
    QCOMPARE(state.volume(), 100);

    state.setVolume(std::numeric_limits<int>::min());
    QCOMPARE(state.volume(), 0);
}

void VideoPlayerStateTest::mutedToggles()
{
    VideoPlayerState state;
    openSample(state);
    QSignalSpy spy(&state, &VideoPlayerState::changed);

    state.setMuted(true);
    QVERIFY(state.muted());
    QCOMPARE(spy.count(), 1);

    state.setMuted(true);
    QCOMPARE(spy.count(), 1);

    state.setMuted(false);
    QVERIFY(!state.muted());
    QCOMPARE(spy.count(), 2);

    state.setMuted(false);
    QCOMPARE(spy.count(), 2);
}

void VideoPlayerStateTest::rateClampsToAllowedRange()
{
    VideoPlayerState state;
    openSample(state);

    state.setRate(0.1);
    QCOMPARE(state.rate(), 0.25);

    state.setRate(-1.0);
    QCOMPARE(state.rate(), 0.25);

    state.setRate(2.5);
    QCOMPARE(state.rate(), 2.5);

    state.setRate(3.5);
    QCOMPARE(state.rate(), 3.0);

    state.setRate(std::numeric_limits<double>::infinity());
    QCOMPARE(state.rate(), 3.0);

    state.setRate(-std::numeric_limits<double>::infinity());
    QCOMPARE(state.rate(), 0.25);

    // The bounds themselves are kept as given.
    state.setRate(0.25);
    QCOMPARE(state.rate(), 0.25);
    state.setRate(3.0);
    QCOMPARE(state.rate(), 3.0);
}

void VideoPlayerStateTest::rateRejectsNan()
{
    VideoPlayerState state;
    openSample(state);
    state.setRate(1.5);
    QSignalSpy spy(&state, &VideoPlayerState::changed);

    state.setRate(std::numeric_limits<double>::quiet_NaN());

    QCOMPARE(state.rate(), 1.5);
    QCOMPARE(spy.count(), 0);
}

void VideoPlayerStateTest::progressTracksPosition()
{
    VideoPlayerState state;
    openSample(state);

    QCOMPARE(state.progress(), 0.0);

    state.setPosition(2500);
    QCOMPARE(state.progress(), 0.25);

    state.setPosition(5000);
    QCOMPARE(state.progress(), 0.5);

    state.setPosition(10000);
    QCOMPARE(state.progress(), 1.0);
}

void VideoPlayerStateTest::progressIsZeroWithoutDuration()
{
    VideoPlayerState state;
    QCOMPARE(state.progress(), 0.0);

    openSample(state);
    state.setPosition(5000);
    QCOMPARE(state.progress(), 0.5);

    state.close();
    QCOMPARE(state.progress(), 0.0);

    state.reset();
    QCOMPARE(state.progress(), 0.0);
}

void VideoPlayerStateTest::progressNeverLeavesUnitRange()
{
    VideoPlayerState state;
    openSample(state);

    state.setPosition(-1000);
    QCOMPARE(state.progress(), 0.0);

    state.setPosition(10000);
    QCOMPARE(state.progress(), 1.0);

    state.setPosition(999999);
    QCOMPARE(state.progress(), 1.0);
}

void VideoPlayerStateTest::resetRestoresDefaults()
{
    VideoPlayerState state;
    openSample(state);
    state.setPosition(3000);
    state.setPlaying(true);
    state.setVolume(10);
    state.setMuted(true);
    state.setRate(2.0);
    QSignalSpy spy(&state, &VideoPlayerState::changed);

    state.reset();

    QCOMPARE(spy.count(), 1);
    QVERIFY(!state.active());
    QCOMPARE(state.messageId(), 0);
    QCOMPARE(state.durationMs(), 0);
    QCOMPARE(state.positionMs(), 0);
    QVERIFY(!state.playing());
    QCOMPARE(state.volume(), 100);
    QVERIFY(!state.muted());
    QCOMPARE(state.rate(), 1.0);
    QCOMPARE(state.progress(), 0.0);
}

void VideoPlayerStateTest::resetWhenAlreadyDefaultIsNoOp()
{
    VideoPlayerState state;
    QSignalSpy spy(&state, &VideoPlayerState::changed);

    state.reset();
    state.reset();

    QCOMPARE(spy.count(), 0);
    QVERIFY(!state.active());
    QCOMPARE(state.volume(), 100);
    QCOMPARE(state.rate(), 1.0);

    // After a close() the preferences still differ from the defaults, so a
    // reset is a real change and reports it.
    openSample(state);
    state.setVolume(10);
    state.close();
    spy.clear();

    state.reset();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(state.volume(), 100);
}

void VideoPlayerStateTest::resetClearsUserPreferences()
{
    VideoPlayerState state;
    openSample(state);
    state.setVolume(15);
    state.setMuted(true);
    state.setRate(0.5);

    state.reset();
    state.open(43, 1000);

    QCOMPARE(state.volume(), 100);
    QVERIFY(!state.muted());
    QCOMPARE(state.rate(), 1.0);
}

void VideoPlayerStateTest::changedOnlyOnRealChanges()
{
    VideoPlayerState state;
    QSignalSpy spy(&state, &VideoPlayerState::changed);

    // Ignored calls while inactive.
    state.close();
    state.setPlaying(true);
    state.setPosition(10);
    state.setVolume(50);
    state.setMuted(true);
    state.setRate(2.0);
    state.reset();
    QCOMPARE(spy.count(), 0);

    // One event per real change, in order.
    state.open(1, 1000);
    QCOMPARE(spy.count(), 1);

    state.setPlaying(true);
    QCOMPARE(spy.count(), 2);

    state.setPosition(500);
    QCOMPARE(spy.count(), 3);

    state.setVolume(50);
    QCOMPARE(spy.count(), 4);

    state.setMuted(true);
    QCOMPARE(spy.count(), 5);

    state.setRate(1.5);
    QCOMPARE(spy.count(), 6);

    // Repeated identical calls add nothing.
    state.setPlaying(true);
    state.setPosition(500);
    state.setVolume(50);
    state.setMuted(true);
    state.setRate(1.5);
    QCOMPARE(spy.count(), 6);

    state.close();
    QCOMPARE(spy.count(), 7);

    state.close();
    QCOMPARE(spy.count(), 7);
}

QTEST_GUILESS_MAIN(VideoPlayerStateTest)
#include "tst_video_player_state.moc"
