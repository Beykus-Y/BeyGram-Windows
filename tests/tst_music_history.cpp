#include "telegram/music/MusicHistory.h"

#include <QSignalSpy>
#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

MusicPlayEvent play(const QString& trackId, qint64 playedAt, int positionMs = 0,
                    bool completed = false)
{
    return MusicPlayEvent{trackId, playedAt, positionMs, completed};
}

QVector<qint64> timesOf(const QVector<MusicPlayEvent>& events)
{
    QVector<qint64> times;
    times.reserve(events.size());
    for (const MusicPlayEvent& event : events)
        times.append(event.playedAt);
    return times;
}

QVector<QString> idsOf(const QVector<MusicPlayEvent>& events)
{
    QVector<QString> ids;
    ids.reserve(events.size());
    for (const MusicPlayEvent& event : events)
        ids.append(event.trackId);
    return ids;
}

} // namespace

class MusicHistoryTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();

    void recordStoresEvent();
    void recordTrimsTrackId();
    void recordIgnoresBlankTrackId();
    void recordIgnoresNonPositivePlayedAt();
    void recordClampsNegativePosition();
    void recordKeepsCompletedFlag();

    void recordKeepsNewestFirst();
    void recordOrdersOutOfOrderByTime();
    void recordKeepsArrivalOrderForEqualTimes();
    void recordCapsAtMaxEntries();
    void recordDefaultCapIsTwoHundred();
    void recordWithZeroCapKeepsNothing();
    void recordWithZeroCapClearsExisting();
    void recordWithNegativeCapClearsExisting();
    void recordOlderThanFullCapChangesNothing();
    void recordSmallerCapTruncatesEvenWhenEventIsOldest();

    void recordEmitsChangedForEveryStoredEvent();
    void recordIgnoredEventsEmitNothing();

    void recentlyPlayedIsUniqueNewestFirst();
    void recentlyPlayedRespectsLimit();
    void recentlyPlayedNonPositiveLimitIsEmpty();
    void recentlyPlayedAfterClearIsEmpty();

    void playCountCountsEveryEvent();
    void playCountTrimsQuery();
    void playCountIsExact();
    void playCountBlankIsZero();
    void playCountUnknownIsZero();

    void clearDropsEvents();
    void clearEmitsChanged();
    void clearOnEmptyIsNoOp();
    void clearTwiceEmitsOnce();

    void eventsReturnsCopy();
    void queriesDoNotEmitChanged();
};

void MusicHistoryTest::startsEmpty()
{
    MusicHistory history;

    QVERIFY(history.events().isEmpty());
    QVERIFY(history.recentlyPlayed().isEmpty());
    QCOMPARE(history.playCount(QStringLiteral("track-1")), 0);
}

void MusicHistoryTest::recordStoresEvent()
{
    MusicHistory history;
    history.record(play(QStringLiteral("track-1"), 1000, 5000, true));

    const QVector<MusicPlayEvent> events = history.events();
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.at(0).trackId, QStringLiteral("track-1"));
    QCOMPARE(events.at(0).playedAt, qint64(1000));
    QCOMPARE(events.at(0).positionMs, 5000);
    QVERIFY(events.at(0).completed);
}

void MusicHistoryTest::recordTrimsTrackId()
{
    MusicHistory history;
    history.record(play(QStringLiteral("  track-1 \t"), 1000));

    QCOMPARE(history.events().at(0).trackId, QStringLiteral("track-1"));
    QCOMPARE(history.playCount(QStringLiteral(" track-1 ")), 1);
}

void MusicHistoryTest::recordIgnoresBlankTrackId()
{
    MusicHistory history;
    QSignalSpy spy(&history, &MusicHistory::changed);

    history.record(play(QString(), 1000));
    history.record(play(QStringLiteral("   "), 2000));
    history.record(play(QStringLiteral("\t\n"), 3000));

    QVERIFY(history.events().isEmpty());
    QCOMPARE(spy.count(), 0);
}

void MusicHistoryTest::recordIgnoresNonPositivePlayedAt()
{
    MusicHistory history;
    QSignalSpy spy(&history, &MusicHistory::changed);

    history.record(play(QStringLiteral("track-1"), 0));
    history.record(play(QStringLiteral("track-2"), -1));

    QVERIFY(history.events().isEmpty());
    QCOMPARE(spy.count(), 0);
}

void MusicHistoryTest::recordClampsNegativePosition()
{
    MusicHistory history;
    history.record(play(QStringLiteral("track-1"), 1000, -500));

    QCOMPARE(history.events().at(0).positionMs, 0);
}

void MusicHistoryTest::recordKeepsCompletedFlag()
{
    MusicHistory history;
    history.record(play(QStringLiteral("track-1"), 1000, 0, false));
    history.record(play(QStringLiteral("track-2"), 2000, 0, true));

    QCOMPARE(history.events().size(), 2);
    QVERIFY(!history.events().at(1).completed);
    QVERIFY(history.events().at(0).completed);
}

void MusicHistoryTest::recordKeepsNewestFirst()
{
    MusicHistory history;
    history.record(play(QStringLiteral("a"), 100));
    history.record(play(QStringLiteral("b"), 200));
    history.record(play(QStringLiteral("c"), 300));

    QCOMPARE(timesOf(history.events()), QVector<qint64>({300, 200, 100}));
    QCOMPARE(idsOf(history.events()),
             QVector<QString>({QStringLiteral("c"), QStringLiteral("b"), QStringLiteral("a")}));
}

void MusicHistoryTest::recordOrdersOutOfOrderByTime()
{
    MusicHistory history;
    history.record(play(QStringLiteral("newest"), 300));
    history.record(play(QStringLiteral("oldest"), 100));
    history.record(play(QStringLiteral("middle"), 200));

    QCOMPARE(timesOf(history.events()), QVector<qint64>({300, 200, 100}));
}

void MusicHistoryTest::recordKeepsArrivalOrderForEqualTimes()
{
    MusicHistory history;
    history.record(play(QStringLiteral("first"), 100));
    history.record(play(QStringLiteral("second"), 100));

    QCOMPARE(idsOf(history.events()),
             QVector<QString>({QStringLiteral("second"), QStringLiteral("first")}));
}

void MusicHistoryTest::recordCapsAtMaxEntries()
{
    MusicHistory history;
    history.record(play(QStringLiteral("a"), 100), 2);
    history.record(play(QStringLiteral("b"), 200), 2);
    history.record(play(QStringLiteral("c"), 300), 2);

    QCOMPARE(timesOf(history.events()), QVector<qint64>({300, 200}));
    QCOMPARE(idsOf(history.events()),
             QVector<QString>({QStringLiteral("c"), QStringLiteral("b")}));
}

void MusicHistoryTest::recordDefaultCapIsTwoHundred()
{
    MusicHistory history;
    for (int i = 1; i <= 201; ++i)
        history.record(play(QStringLiteral("track-%1").arg(i), i));

    const QVector<MusicPlayEvent> events = history.events();
    QCOMPARE(events.size(), 200);
    QCOMPARE(events.first().playedAt, qint64(201));
    QCOMPARE(events.last().playedAt, qint64(2));
    QCOMPARE(events.first().trackId, QStringLiteral("track-201"));
}

void MusicHistoryTest::recordWithZeroCapKeepsNothing()
{
    MusicHistory history;
    QSignalSpy spy(&history, &MusicHistory::changed);

    history.record(play(QStringLiteral("track-1"), 1000), 0);

    QVERIFY(history.events().isEmpty());
    QCOMPARE(spy.count(), 0);
}

void MusicHistoryTest::recordWithZeroCapClearsExisting()
{
    MusicHistory history;
    history.record(play(QStringLiteral("track-1"), 1000));
    QSignalSpy spy(&history, &MusicHistory::changed);

    history.record(play(QStringLiteral("track-2"), 2000), 0);

    QVERIFY(history.events().isEmpty());
    QCOMPARE(spy.count(), 1);
}

void MusicHistoryTest::recordWithNegativeCapClearsExisting()
{
    MusicHistory history;
    history.record(play(QStringLiteral("track-1"), 1000));
    QSignalSpy spy(&history, &MusicHistory::changed);

    history.record(play(QStringLiteral("track-2"), 2000), -5);

    QVERIFY(history.events().isEmpty());
    QCOMPARE(spy.count(), 1);
}

void MusicHistoryTest::recordOlderThanFullCapChangesNothing()
{
    MusicHistory history;
    history.record(play(QStringLiteral("a"), 200), 2);
    history.record(play(QStringLiteral("b"), 100), 2);
    QSignalSpy spy(&history, &MusicHistory::changed);

    history.record(play(QStringLiteral("c"), 50), 2);

    QCOMPARE(timesOf(history.events()), QVector<qint64>({200, 100}));
    QCOMPARE(spy.count(), 0);
}

void MusicHistoryTest::recordSmallerCapTruncatesEvenWhenEventIsOldest()
{
    MusicHistory history;
    history.record(play(QStringLiteral("a"), 300));
    history.record(play(QStringLiteral("b"), 200));
    history.record(play(QStringLiteral("c"), 100));
    QCOMPARE(history.events().size(), 3);
    QSignalSpy spy(&history, &MusicHistory::changed);

    history.record(play(QStringLiteral("d"), 50), 2);

    QCOMPARE(timesOf(history.events()), QVector<qint64>({300, 200}));
    QCOMPARE(spy.count(), 1);
}

void MusicHistoryTest::recordEmitsChangedForEveryStoredEvent()
{
    MusicHistory history;
    QSignalSpy spy(&history, &MusicHistory::changed);

    history.record(play(QStringLiteral("a"), 100));
    history.record(play(QStringLiteral("b"), 200));
    history.record(play(QStringLiteral("a"), 300));

    QCOMPARE(spy.count(), 3);
}

void MusicHistoryTest::recordIgnoredEventsEmitNothing()
{
    MusicHistory history;
    QSignalSpy spy(&history, &MusicHistory::changed);

    history.record(play(QStringLiteral("  "), 100));
    history.record(play(QStringLiteral("track-1"), 0));
    history.record(play(QStringLiteral("track-1"), -10));

    QCOMPARE(spy.count(), 0);
    QVERIFY(history.events().isEmpty());
}

void MusicHistoryTest::recentlyPlayedIsUniqueNewestFirst()
{
    MusicHistory history;
    history.record(play(QStringLiteral("a"), 100));
    history.record(play(QStringLiteral("b"), 200));
    history.record(play(QStringLiteral("a"), 300));

    QCOMPARE(history.recentlyPlayed(),
             QVector<QString>({QStringLiteral("a"), QStringLiteral("b")}));
    QCOMPARE(history.playCount(QStringLiteral("a")), 2);
    QCOMPARE(history.playCount(QStringLiteral("b")), 1);
}

void MusicHistoryTest::recentlyPlayedRespectsLimit()
{
    MusicHistory history;
    history.record(play(QStringLiteral("a"), 100));
    history.record(play(QStringLiteral("b"), 200));
    history.record(play(QStringLiteral("c"), 300));

    QCOMPARE(history.recentlyPlayed(2),
             QVector<QString>({QStringLiteral("c"), QStringLiteral("b")}));
    QCOMPARE(history.recentlyPlayed(10).size(), 3);
}

void MusicHistoryTest::recentlyPlayedNonPositiveLimitIsEmpty()
{
    MusicHistory history;
    history.record(play(QStringLiteral("a"), 100));

    QVERIFY(history.recentlyPlayed(0).isEmpty());
    QVERIFY(history.recentlyPlayed(-1).isEmpty());
}

void MusicHistoryTest::recentlyPlayedAfterClearIsEmpty()
{
    MusicHistory history;
    history.record(play(QStringLiteral("a"), 100));
    history.clear();

    QVERIFY(history.recentlyPlayed().isEmpty());
}

void MusicHistoryTest::playCountCountsEveryEvent()
{
    MusicHistory history;
    history.record(play(QStringLiteral("a"), 100));
    history.record(play(QStringLiteral("b"), 200));
    history.record(play(QStringLiteral("a"), 300));
    history.record(play(QStringLiteral("a"), 400));

    QCOMPARE(history.playCount(QStringLiteral("a")), 3);
    QCOMPARE(history.playCount(QStringLiteral("b")), 1);
}

void MusicHistoryTest::playCountTrimsQuery()
{
    MusicHistory history;
    history.record(play(QStringLiteral("track-1"), 100));

    QCOMPARE(history.playCount(QStringLiteral("  track-1  ")), 1);
}

void MusicHistoryTest::playCountIsExact()
{
    MusicHistory history;
    history.record(play(QStringLiteral("Track-1"), 100));

    QCOMPARE(history.playCount(QStringLiteral("track-1")), 0);
}

void MusicHistoryTest::playCountBlankIsZero()
{
    MusicHistory history;
    history.record(play(QStringLiteral("track-1"), 100));

    QCOMPARE(history.playCount(QString()), 0);
    QCOMPARE(history.playCount(QStringLiteral("   ")), 0);
}

void MusicHistoryTest::playCountUnknownIsZero()
{
    MusicHistory history;
    history.record(play(QStringLiteral("track-1"), 100));

    QCOMPARE(history.playCount(QStringLiteral("track-2")), 0);
}

void MusicHistoryTest::clearDropsEvents()
{
    MusicHistory history;
    history.record(play(QStringLiteral("a"), 100));
    history.record(play(QStringLiteral("b"), 200));

    history.clear();

    QVERIFY(history.events().isEmpty());
    QVERIFY(history.recentlyPlayed().isEmpty());
    QCOMPARE(history.playCount(QStringLiteral("a")), 0);
}

void MusicHistoryTest::clearEmitsChanged()
{
    MusicHistory history;
    history.record(play(QStringLiteral("a"), 100));
    QSignalSpy spy(&history, &MusicHistory::changed);

    history.clear();

    QCOMPARE(spy.count(), 1);
}

void MusicHistoryTest::clearOnEmptyIsNoOp()
{
    MusicHistory history;
    QSignalSpy spy(&history, &MusicHistory::changed);

    history.clear();

    QCOMPARE(spy.count(), 0);
}

void MusicHistoryTest::clearTwiceEmitsOnce()
{
    MusicHistory history;
    history.record(play(QStringLiteral("a"), 100));
    QSignalSpy spy(&history, &MusicHistory::changed);

    history.clear();
    history.clear();

    QCOMPARE(spy.count(), 1);
}

void MusicHistoryTest::eventsReturnsCopy()
{
    MusicHistory history;
    history.record(play(QStringLiteral("a"), 100));

    QVector<MusicPlayEvent> copy = history.events();
    copy[0].trackId = QStringLiteral("changed");
    copy[0].playedAt = 999;

    QCOMPARE(history.events().at(0).trackId, QStringLiteral("a"));
    QCOMPARE(history.events().at(0).playedAt, qint64(100));
}

void MusicHistoryTest::queriesDoNotEmitChanged()
{
    MusicHistory history;
    history.record(play(QStringLiteral("a"), 100));
    QSignalSpy spy(&history, &MusicHistory::changed);

    QVERIFY(!history.events().isEmpty());
    QVERIFY(!history.recentlyPlayed().isEmpty());
    QCOMPARE(history.playCount(QStringLiteral("a")), 1);

    QCOMPARE(spy.count(), 0);
}

QTEST_GUILESS_MAIN(MusicHistoryTest)
#include "tst_music_history.moc"
