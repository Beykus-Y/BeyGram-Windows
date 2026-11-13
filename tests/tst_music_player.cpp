#include "telegram/music/MusicPlayer.h"

#include <QSignalSpy>
#include <QString>
#include <QVector>
#include <QtTest>

#include <initializer_list>

using namespace beygram::telegram;

namespace {

QVector<QString> queueOf(std::initializer_list<const char*> ids)
{
    QVector<QString> queue;
    queue.reserve(static_cast<int>(ids.size()));
    for (const char* id : ids)
        queue.append(QString::fromLatin1(id));
    return queue;
}

QString trackIdOf(const QSignalSpy& spy, int index = 0)
{
    return spy.at(index).at(0).toString();
}

} // namespace

class MusicPlayerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmptyWithDefaults();

    void setQueueDropsEmptyIdsAndKeepsFirstDuplicate();
    void setQueueSameContentIsNoOp();
    void setQueueWhileNothingCurrentSelectsNothing();
    void queueIsACopy();

    void playAtSelectsTrackAndStartsIt();
    void playAtIgnoresOutOfRangeIndex();
    void playAtOnEmptyQueueIsIgnored();
    void playAtSameTrackRewindsWithoutTrackChange();
    void playAtSameTrackAtStartIsNoOp();
    void playAtAnotherTrackResetsDurationAndPosition();

    void nextAdvancesInQueueOrder();
    void nextAtEndStopsWithoutRepeat();
    void nextAtEndWhenAlreadyStoppedIsNoOp();
    void nextAtEndWithRepeatAllWrapsToFirst();
    void nextWithRepeatOneStays();
    void nextBeforePlayAtDoesNothing();
    void nextOnEmptyQueueDoesNothing();

    void previousGoesBack();
    void previousAtStartWithoutRepeatStays();
    void previousAtStartWithRepeatAllWrapsToLast();
    void previousIgnoresRepeatOne();

    void shuffleFollowsStableHashOrder();
    void shuffleIsDeterministicAcrossPlayers();
    void shuffleKeepsQueueOrder();
    void setShuffleKeepsCurrentTrack();
    void setShuffleOffRestoresQueueOrder();
    void setShuffleSameValueIsNoOp();

    void setRepeatModeChangesAndEmits();
    void setRepeatModeIgnoresInvalidValue();

    void crossfadeDefaultsToFourSeconds();
    void crossfadeClampsToRange();

    void positionClampsToDuration();
    void positionWithoutDurationStaysZero();
    void positionNoOpAtSameValue();
    void durationClampsPosition();
    void negativeDurationBecomesZero();

    void playingToggles();

    void queueEditKeepsCurrentTrackById();
    void queueEditRemovingCurrentStopsPlayback();
    void queueEditToEmptyClearsCurrent();

    void clearDropsPlaybackButKeepsPreferences();
    void clearOnEmptyPlayerIsNoOp();

    void trackChangedOnlyOnRealIdChange();
    void changedOnlyOnRealChanges();
};

void MusicPlayerTest::startsEmptyWithDefaults()
{
    const MusicPlayer player;

    QVERIFY(player.queue().isEmpty());
    QCOMPARE(player.currentIndex(), -1);
    QCOMPARE(player.currentTrackId(), QString());
    QVERIFY(!player.shuffle());
    QCOMPARE(player.repeatMode(), RepeatMode::Off);
    QCOMPARE(player.crossfadeMs(), 4000);
    QCOMPARE(player.positionMs(), 0);
    QCOMPARE(player.durationMs(), 0);
    QVERIFY(!player.playing());
}

void MusicPlayerTest::setQueueDropsEmptyIdsAndKeepsFirstDuplicate()
{
    MusicPlayer player;
    QSignalSpy changed(&player, &MusicPlayer::changed);

    player.setQueue(queueOf({"a", "", "b", "a", "", "c", "b"}));

    QCOMPARE(player.queue(), queueOf({"a", "b", "c"}));
    QCOMPARE(changed.count(), 1);
}

void MusicPlayerTest::setQueueSameContentIsNoOp()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b"}));

    QSignalSpy changed(&player, &MusicPlayer::changed);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    player.setQueue(queueOf({"a", "b"}));
    player.setQueue(queueOf({"a", "", "b", "a"}));

    QCOMPARE(changed.count(), 0);
    QCOMPARE(trackChanged.count(), 0);
}

void MusicPlayerTest::setQueueWhileNothingCurrentSelectsNothing()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b", "c"}));

    QCOMPARE(player.currentIndex(), -1);
    QCOMPARE(player.currentTrackId(), QString());
    QVERIFY(!player.playing());
}

void MusicPlayerTest::queueIsACopy()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b"}));

    QVector<QString> copy = player.queue();
    copy.append(QStringLiteral("c"));

    QCOMPARE(player.queue(), queueOf({"a", "b"}));
}

void MusicPlayerTest::playAtSelectsTrackAndStartsIt()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b", "c"}));
    QSignalSpy changed(&player, &MusicPlayer::changed);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    player.playAt(1);

    QCOMPARE(player.currentIndex(), 1);
    QCOMPARE(player.currentTrackId(), QStringLiteral("b"));
    QVERIFY(player.playing());
    QCOMPARE(player.positionMs(), 0);
    QCOMPARE(player.durationMs(), 0);
    QCOMPARE(trackChanged.count(), 1);
    QCOMPARE(trackIdOf(trackChanged), QStringLiteral("b"));
    QCOMPARE(changed.count(), 1);
}

void MusicPlayerTest::playAtIgnoresOutOfRangeIndex()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b"}));
    QSignalSpy changed(&player, &MusicPlayer::changed);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    player.playAt(-1);
    player.playAt(2);
    player.playAt(99);

    QCOMPARE(player.currentIndex(), -1);
    QCOMPARE(player.currentTrackId(), QString());
    QCOMPARE(changed.count(), 0);
    QCOMPARE(trackChanged.count(), 0);
}

void MusicPlayerTest::playAtOnEmptyQueueIsIgnored()
{
    MusicPlayer player;
    QSignalSpy changed(&player, &MusicPlayer::changed);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    player.playAt(0);

    QCOMPARE(player.currentIndex(), -1);
    QVERIFY(!player.playing());
    QCOMPARE(changed.count(), 0);
    QCOMPARE(trackChanged.count(), 0);
}

void MusicPlayerTest::playAtSameTrackRewindsWithoutTrackChange()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b", "c"}));
    player.playAt(1);
    player.setDurationMs(5000);
    player.setPositionMs(2500);
    QSignalSpy changed(&player, &MusicPlayer::changed);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    player.playAt(1);

    QCOMPARE(player.currentIndex(), 1);
    QCOMPARE(player.positionMs(), 0);
    QCOMPARE(player.durationMs(), 5000);
    QVERIFY(player.playing());
    QCOMPARE(trackChanged.count(), 0);
    QCOMPARE(changed.count(), 1);
}

void MusicPlayerTest::playAtSameTrackAtStartIsNoOp()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b"}));
    player.playAt(0);
    QSignalSpy changed(&player, &MusicPlayer::changed);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    player.playAt(0);

    QCOMPARE(player.currentIndex(), 0);
    QVERIFY(player.playing());
    QCOMPARE(changed.count(), 0);
    QCOMPARE(trackChanged.count(), 0);
}

void MusicPlayerTest::playAtAnotherTrackResetsDurationAndPosition()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b"}));
    player.playAt(0);
    player.setDurationMs(5000);
    player.setPositionMs(4000);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    player.playAt(1);

    QCOMPARE(player.currentTrackId(), QStringLiteral("b"));
    QCOMPARE(player.durationMs(), 0);
    QCOMPARE(player.positionMs(), 0);
    QCOMPARE(trackChanged.count(), 1);
    QCOMPARE(trackIdOf(trackChanged), QStringLiteral("b"));
}

void MusicPlayerTest::nextAdvancesInQueueOrder()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b", "c"}));
    player.playAt(0);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    QVERIFY(player.next());
    QCOMPARE(player.currentIndex(), 1);
    QCOMPARE(player.currentTrackId(), QStringLiteral("b"));

    QVERIFY(player.next());
    QCOMPARE(player.currentIndex(), 2);
    QCOMPARE(player.currentTrackId(), QStringLiteral("c"));

    QCOMPARE(trackChanged.count(), 2);
    QCOMPARE(trackIdOf(trackChanged, 0), QStringLiteral("b"));
    QCOMPARE(trackIdOf(trackChanged, 1), QStringLiteral("c"));
}

void MusicPlayerTest::nextAtEndStopsWithoutRepeat()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b", "c"}));
    player.playAt(2);
    QSignalSpy changed(&player, &MusicPlayer::changed);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    QVERIFY(!player.next());

    QCOMPARE(player.currentIndex(), 2);
    QCOMPARE(player.currentTrackId(), QStringLiteral("c"));
    QVERIFY(!player.playing());
    QCOMPARE(changed.count(), 1);
    QCOMPARE(trackChanged.count(), 0);
}

void MusicPlayerTest::nextAtEndWhenAlreadyStoppedIsNoOp()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b"}));
    player.playAt(1);
    player.next();
    QVERIFY(!player.playing());
    QSignalSpy changed(&player, &MusicPlayer::changed);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    QVERIFY(!player.next());

    QCOMPARE(player.currentIndex(), 1);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(trackChanged.count(), 0);
}

void MusicPlayerTest::nextAtEndWithRepeatAllWrapsToFirst()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b", "c"}));
    player.setRepeatMode(RepeatMode::All);
    player.playAt(2);
    QSignalSpy changed(&player, &MusicPlayer::changed);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    QVERIFY(player.next());

    QCOMPARE(player.currentIndex(), 0);
    QCOMPARE(player.currentTrackId(), QStringLiteral("a"));
    QVERIFY(player.playing());
    QCOMPARE(trackChanged.count(), 1);
    QCOMPARE(trackIdOf(trackChanged), QStringLiteral("a"));
    QCOMPARE(changed.count(), 1);
}

void MusicPlayerTest::nextWithRepeatOneStays()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b", "c"}));
    player.setRepeatMode(RepeatMode::One);
    player.playAt(1);
    player.setDurationMs(5000);
    player.setPositionMs(2000);
    QSignalSpy changed(&player, &MusicPlayer::changed);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    QVERIFY(player.next());

    QCOMPARE(player.currentIndex(), 1);
    QCOMPARE(player.currentTrackId(), QStringLiteral("b"));
    QCOMPARE(player.positionMs(), 2000);
    QVERIFY(player.playing());
    QCOMPARE(changed.count(), 0);
    QCOMPARE(trackChanged.count(), 0);
}

void MusicPlayerTest::nextBeforePlayAtDoesNothing()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b"}));
    QSignalSpy changed(&player, &MusicPlayer::changed);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    QVERIFY(!player.next());

    QCOMPARE(player.currentIndex(), -1);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(trackChanged.count(), 0);
}

void MusicPlayerTest::nextOnEmptyQueueDoesNothing()
{
    MusicPlayer player;
    QSignalSpy changed(&player, &MusicPlayer::changed);

    QVERIFY(!player.next());

    QCOMPARE(changed.count(), 0);
}

void MusicPlayerTest::previousGoesBack()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b", "c"}));
    player.playAt(2);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    QVERIFY(player.previous());

    QCOMPARE(player.currentIndex(), 1);
    QCOMPARE(player.currentTrackId(), QStringLiteral("b"));
    QCOMPARE(trackChanged.count(), 1);
    QCOMPARE(trackIdOf(trackChanged), QStringLiteral("b"));
}

void MusicPlayerTest::previousAtStartWithoutRepeatStays()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b", "c"}));
    player.playAt(0);
    QSignalSpy changed(&player, &MusicPlayer::changed);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    QVERIFY(!player.previous());

    QCOMPARE(player.currentIndex(), 0);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(trackChanged.count(), 0);
}

void MusicPlayerTest::previousAtStartWithRepeatAllWrapsToLast()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b", "c"}));
    player.setRepeatMode(RepeatMode::All);
    player.playAt(0);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    QVERIFY(player.previous());

    QCOMPARE(player.currentIndex(), 2);
    QCOMPARE(player.currentTrackId(), QStringLiteral("c"));
    QCOMPARE(trackChanged.count(), 1);
    QCOMPARE(trackIdOf(trackChanged), QStringLiteral("c"));
}

void MusicPlayerTest::previousIgnoresRepeatOne()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b", "c"}));
    player.setRepeatMode(RepeatMode::One);
    player.playAt(0);
    QSignalSpy changed(&player, &MusicPlayer::changed);

    QVERIFY(!player.previous());

    QCOMPARE(player.currentIndex(), 0);
    QCOMPARE(changed.count(), 0);
}

void MusicPlayerTest::shuffleFollowsStableHashOrder()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b", "c", "d", "e"}));
    player.setShuffle(true);
    player.playAt(0);
    player.setRepeatMode(RepeatMode::All);

    // The order is derived from the ids with a fixed hash, never from a random
    // number generator: from a it runs c, b, e, d and wraps back to a.
    QVector<QString> visited;
    for (int step = 0; step < 5; ++step) {
        QVERIFY(player.next());
        visited.append(player.currentTrackId());
    }

    QCOMPARE(visited, queueOf({"c", "b", "e", "d", "a"}));
    QCOMPARE(player.currentTrackId(), QStringLiteral("a"));
}

void MusicPlayerTest::shuffleIsDeterministicAcrossPlayers()
{
    const auto sequence = [](MusicPlayer& player) {
        player.setQueue(queueOf({"a", "b", "c", "d", "e"}));
        player.setShuffle(true);
        player.playAt(0);
        player.setRepeatMode(RepeatMode::All);
        QVector<QString> visited;
        for (int step = 0; step < 5; ++step) {
            player.next();
            visited.append(player.currentTrackId());
        }
        return visited;
    };

    MusicPlayer first;
    MusicPlayer second;

    QCOMPARE(sequence(first), sequence(second));
}

void MusicPlayerTest::shuffleKeepsQueueOrder()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b", "c", "d", "e"}));

    player.setShuffle(true);

    QCOMPARE(player.queue(), queueOf({"a", "b", "c", "d", "e"}));
}

void MusicPlayerTest::setShuffleKeepsCurrentTrack()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b", "c"}));
    player.playAt(2);
    player.setDurationMs(5000);
    player.setPositionMs(1200);
    QSignalSpy changed(&player, &MusicPlayer::changed);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    player.setShuffle(true);

    QVERIFY(player.shuffle());
    QCOMPARE(player.currentIndex(), 2);
    QCOMPARE(player.currentTrackId(), QStringLiteral("c"));
    QCOMPARE(player.positionMs(), 1200);
    QCOMPARE(player.durationMs(), 5000);
    QVERIFY(player.playing());
    QCOMPARE(trackChanged.count(), 0);
    QCOMPARE(changed.count(), 1);
}

void MusicPlayerTest::setShuffleOffRestoresQueueOrder()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b", "c"}));
    player.setShuffle(true);
    player.playAt(0);

    player.setShuffle(false);

    QVERIFY(!player.shuffle());
    QVERIFY(player.next());
    QCOMPARE(player.currentIndex(), 1);
    QCOMPARE(player.currentTrackId(), QStringLiteral("b"));
}

void MusicPlayerTest::setShuffleSameValueIsNoOp()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b"}));
    QSignalSpy changed(&player, &MusicPlayer::changed);

    player.setShuffle(false);
    QCOMPARE(changed.count(), 0);

    player.setShuffle(true);
    QCOMPARE(changed.count(), 1);

    player.setShuffle(true);
    QCOMPARE(changed.count(), 1);
}

void MusicPlayerTest::setRepeatModeChangesAndEmits()
{
    MusicPlayer player;
    QSignalSpy changed(&player, &MusicPlayer::changed);

    player.setRepeatMode(RepeatMode::All);
    QCOMPARE(player.repeatMode(), RepeatMode::All);
    QCOMPARE(changed.count(), 1);

    player.setRepeatMode(RepeatMode::One);
    QCOMPARE(player.repeatMode(), RepeatMode::One);
    QCOMPARE(changed.count(), 2);

    player.setRepeatMode(RepeatMode::One);
    QCOMPARE(changed.count(), 2);
}

void MusicPlayerTest::setRepeatModeIgnoresInvalidValue()
{
    MusicPlayer player;
    QSignalSpy changed(&player, &MusicPlayer::changed);

    player.setRepeatMode(static_cast<RepeatMode>(42));

    QCOMPARE(player.repeatMode(), RepeatMode::Off);
    QCOMPARE(changed.count(), 0);
}

void MusicPlayerTest::crossfadeDefaultsToFourSeconds()
{
    const MusicPlayer player;

    QCOMPARE(player.crossfadeMs(), 4000);
}

void MusicPlayerTest::crossfadeClampsToRange()
{
    MusicPlayer player;
    QSignalSpy changed(&player, &MusicPlayer::changed);

    player.setCrossfadeMs(-1);
    QCOMPARE(player.crossfadeMs(), 0);
    QCOMPARE(changed.count(), 1);

    player.setCrossfadeMs(20000);
    QCOMPARE(player.crossfadeMs(), 12000);
    QCOMPARE(changed.count(), 2);

    player.setCrossfadeMs(12000);
    QCOMPARE(changed.count(), 2);

    player.setCrossfadeMs(6000);
    QCOMPARE(player.crossfadeMs(), 6000);
    QCOMPARE(changed.count(), 3);
}

void MusicPlayerTest::positionClampsToDuration()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a"}));
    player.playAt(0);
    player.setDurationMs(10000);

    player.setPositionMs(4000);
    QCOMPARE(player.positionMs(), 4000);

    player.setPositionMs(20000);
    QCOMPARE(player.positionMs(), 10000);

    player.setPositionMs(-500);
    QCOMPARE(player.positionMs(), 0);
}

void MusicPlayerTest::positionWithoutDurationStaysZero()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a"}));
    player.playAt(0);
    QSignalSpy changed(&player, &MusicPlayer::changed);

    player.setPositionMs(500);

    QCOMPARE(player.positionMs(), 0);
    QCOMPARE(changed.count(), 0);
}

void MusicPlayerTest::positionNoOpAtSameValue()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a"}));
    player.playAt(0);
    player.setDurationMs(10000);
    player.setPositionMs(3000);
    QSignalSpy changed(&player, &MusicPlayer::changed);

    player.setPositionMs(3000);
    player.setPositionMs(12000);

    QCOMPARE(player.positionMs(), 10000);
    QCOMPARE(changed.count(), 1);
}

void MusicPlayerTest::durationClampsPosition()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a"}));
    player.playAt(0);
    player.setDurationMs(10000);
    player.setPositionMs(8000);
    QSignalSpy changed(&player, &MusicPlayer::changed);

    player.setDurationMs(5000);

    QCOMPARE(player.durationMs(), 5000);
    QCOMPARE(player.positionMs(), 5000);
    QCOMPARE(changed.count(), 1);
}

void MusicPlayerTest::negativeDurationBecomesZero()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a"}));
    player.playAt(0);
    player.setDurationMs(5000);
    player.setPositionMs(4000);

    player.setDurationMs(-100);

    QCOMPARE(player.durationMs(), 0);
    QCOMPARE(player.positionMs(), 0);
}

void MusicPlayerTest::playingToggles()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a"}));
    player.playAt(0);
    QVERIFY(player.playing());
    QSignalSpy changed(&player, &MusicPlayer::changed);

    player.setPlaying(false);
    QVERIFY(!player.playing());
    QCOMPARE(changed.count(), 1);

    player.setPlaying(false);
    QCOMPARE(changed.count(), 1);

    player.setPlaying(true);
    QVERIFY(player.playing());
    QCOMPARE(changed.count(), 2);
}

void MusicPlayerTest::queueEditKeepsCurrentTrackById()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b", "c"}));
    player.playAt(1);
    player.setDurationMs(5000);
    player.setPositionMs(700);
    QSignalSpy changed(&player, &MusicPlayer::changed);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    player.setQueue(queueOf({"c", "b", "a"}));

    QCOMPARE(player.currentIndex(), 1);
    QCOMPARE(player.currentTrackId(), QStringLiteral("b"));
    QCOMPARE(player.positionMs(), 700);
    QCOMPARE(player.durationMs(), 5000);
    QVERIFY(player.playing());
    QCOMPARE(trackChanged.count(), 0);
    QCOMPARE(changed.count(), 1);
}

void MusicPlayerTest::queueEditRemovingCurrentStopsPlayback()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b", "c"}));
    player.playAt(1);
    player.setDurationMs(5000);
    player.setPositionMs(700);
    QSignalSpy changed(&player, &MusicPlayer::changed);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    player.setQueue(queueOf({"a", "c"}));

    QCOMPARE(player.currentIndex(), -1);
    QCOMPARE(player.currentTrackId(), QString());
    QVERIFY(!player.playing());
    QCOMPARE(player.positionMs(), 0);
    QCOMPARE(player.durationMs(), 0);
    QCOMPARE(trackChanged.count(), 1);
    QCOMPARE(trackIdOf(trackChanged), QString());
    QCOMPARE(changed.count(), 1);

    QVERIFY(!player.next());
    QCOMPARE(player.currentIndex(), -1);
}

void MusicPlayerTest::queueEditToEmptyClearsCurrent()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a"}));
    player.playAt(0);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    player.setQueue({});

    QVERIFY(player.queue().isEmpty());
    QCOMPARE(player.currentIndex(), -1);
    QVERIFY(!player.playing());
    QCOMPARE(trackChanged.count(), 1);
    QCOMPARE(trackIdOf(trackChanged), QString());
}

void MusicPlayerTest::clearDropsPlaybackButKeepsPreferences()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b", "c"}));
    player.playAt(0);
    player.setDurationMs(5000);
    player.setPositionMs(1000);
    player.setShuffle(true);
    player.setRepeatMode(RepeatMode::All);
    player.setCrossfadeMs(6000);
    QSignalSpy changed(&player, &MusicPlayer::changed);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    player.clear();

    QVERIFY(player.queue().isEmpty());
    QCOMPARE(player.currentIndex(), -1);
    QCOMPARE(player.currentTrackId(), QString());
    QVERIFY(!player.playing());
    QCOMPARE(player.positionMs(), 0);
    QCOMPARE(player.durationMs(), 0);
    QVERIFY(player.shuffle());
    QCOMPARE(player.repeatMode(), RepeatMode::All);
    QCOMPARE(player.crossfadeMs(), 6000);
    QCOMPARE(trackChanged.count(), 1);
    QCOMPARE(trackIdOf(trackChanged), QString());
    QCOMPARE(changed.count(), 1);
}

void MusicPlayerTest::clearOnEmptyPlayerIsNoOp()
{
    MusicPlayer player;
    QSignalSpy changed(&player, &MusicPlayer::changed);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    player.clear();

    QCOMPARE(changed.count(), 0);
    QCOMPARE(trackChanged.count(), 0);
}

void MusicPlayerTest::trackChangedOnlyOnRealIdChange()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b", "c"}));
    player.playAt(0);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    player.next();
    player.previous();
    player.next();
    player.playAt(1);

    QCOMPARE(trackChanged.count(), 3);
    QCOMPARE(trackIdOf(trackChanged, 0), QStringLiteral("b"));
    QCOMPARE(trackIdOf(trackChanged, 1), QStringLiteral("a"));
    QCOMPARE(trackIdOf(trackChanged, 2), QStringLiteral("b"));
}

void MusicPlayerTest::changedOnlyOnRealChanges()
{
    MusicPlayer player;
    player.setQueue(queueOf({"a", "b"}));
    player.playAt(0);
    QSignalSpy changed(&player, &MusicPlayer::changed);
    QSignalSpy trackChanged(&player, &MusicPlayer::trackChanged);

    player.setQueue(queueOf({"a", "b"}));
    player.playAt(0);
    player.setShuffle(false);
    player.setRepeatMode(RepeatMode::Off);
    player.setCrossfadeMs(4000);
    player.setPositionMs(0);
    player.setDurationMs(0);
    player.setPlaying(true);

    QCOMPARE(changed.count(), 0);
    QCOMPARE(trackChanged.count(), 0);
}

QTEST_GUILESS_MAIN(MusicPlayerTest)
#include "tst_music_player.moc"
