#include "ui/MusicPlayerFacade.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>
#include <QtTest>

#include <initializer_list>
#include <limits>
#include <memory>

using namespace beygram::ui;

namespace {

QStringList list(std::initializer_list<const char*> values)
{
    QStringList result;
    result.reserve(static_cast<qsizetype>(values.size()));
    for (const char* value : values)
        result.append(QString::fromLatin1(value));
    return result;
}

QString firstArgument(const QSignalSpy& spy, int index)
{
    return spy.at(index).at(0).toString();
}

} // namespace

class MusicPlayerFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void startsEmpty();

    void setQueueKeepsCurrentEmptyUntilPlayed();
    void setQueueDropsEmptyIdsAndDuplicates();
    void setQueueDropsMissingCurrent();
    void setQueuePairsTitlesByIndex();

    void playPauseStartsFirstTrack();
    void playPauseTogglesWithoutRestarting();

    void nextAndPreviousWalkTheQueue();
    void nextFromIdleStartsFirstTrack();
    void previousFromIdleStartsLastTrack();
    void nextAtEndStopsPlayback();

    void seekClampsToDuration();
    void durationRejectsNegative();

    void speedClampsAndRounds();
    void speedIgnoresNan();
    void shuffleToggles();
    void repeatCycles();
    void hasNextFollowsQueueAndRepeat();

    void setTitleForUpdatesCurrentTrack();
    void setTitleForUnknownTrackIsSilent();

    void saveSessionWritesVersionedDocument();
    void saveAndRestoreRoundTrip();
    void restoreWithoutSessionIsNoOp();

    void changedOnlyOnObservableChanges();

private:
    [[nodiscard]] QString sessionPath() const;

    std::unique_ptr<QTemporaryDir> m_sessionDir;
};

void MusicPlayerFacadeTest::init()
{
    m_sessionDir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_sessionDir->isValid());
    QVERIFY(qputenv("BEYGRAM_MUSIC_SESSION_PATH", sessionPath().toUtf8()));
}

void MusicPlayerFacadeTest::cleanup()
{
    qunsetenv("BEYGRAM_MUSIC_SESSION_PATH");
    m_sessionDir.reset();
}

QString MusicPlayerFacadeTest::sessionPath() const
{
    if (!m_sessionDir)
        return QString();
    return m_sessionDir->filePath(QStringLiteral("music/session.json"));
}

void MusicPlayerFacadeTest::startsEmpty()
{
    const MusicPlayerFacade facade;

    QVERIFY(facade.title().isEmpty());
    QVERIFY(!facade.playing());
    QCOMPARE(facade.positionMs(), 0);
    QCOMPARE(facade.durationMs(), 0);
    QCOMPARE(facade.speed(), 1.0);
    QVERIFY(!facade.shuffle());
    QCOMPARE(facade.repeatMode(), QStringLiteral("off"));
    QVERIFY(!facade.hasNext());
}

void MusicPlayerFacadeTest::setQueueKeepsCurrentEmptyUntilPlayed()
{
    MusicPlayerFacade facade;
    QSignalSpy trackSpy(&facade, &MusicPlayerFacade::trackChanged);
    QSignalSpy changedSpy(&facade, &MusicPlayerFacade::changed);

    facade.setQueue(list({"a", "b"}), list({"A", "B"}));

    QVERIFY(facade.title().isEmpty());
    QVERIFY(!facade.playing());
    QCOMPARE(facade.positionMs(), 0);
    QCOMPARE(facade.durationMs(), 0);
    // Nothing is current yet, so next() would start the first entry.
    QVERIFY(facade.hasNext());
    QCOMPARE(trackSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 1);
}

void MusicPlayerFacadeTest::setQueueDropsEmptyIdsAndDuplicates()
{
    MusicPlayerFacade facade;

    facade.setQueue(list({"", "a", "a", "b"}), list({"", "First", "Second", "B"}));
    facade.playPause();

    QCOMPARE(facade.title(), QStringLiteral("First"));

    facade.next();
    QCOMPARE(facade.title(), QStringLiteral("B"));
}

void MusicPlayerFacadeTest::setQueueDropsMissingCurrent()
{
    MusicPlayerFacade facade;
    facade.setQueue(list({"a", "b"}), list({"A", "B"}));
    facade.playPause();
    QCOMPARE(facade.title(), QStringLiteral("A"));

    QSignalSpy trackSpy(&facade, &MusicPlayerFacade::trackChanged);

    facade.setQueue(list({"b", "c"}), list({"B", "C"}));

    QVERIFY(facade.title().isEmpty());
    QVERIFY(!facade.playing());
    QCOMPARE(facade.positionMs(), 0);
    QCOMPARE(trackSpy.count(), 1);
    QCOMPARE(firstArgument(trackSpy, 0), QString());
    QVERIFY(facade.hasNext());

    facade.playPause();
    QCOMPARE(facade.title(), QStringLiteral("B"));
    QVERIFY(facade.playing());
}

void MusicPlayerFacadeTest::setQueuePairsTitlesByIndex()
{
    MusicPlayerFacade facade;

    facade.setQueue(QStringList{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")},
                    QStringList{QStringLiteral("A"), QString(), QStringLiteral("C")});
    facade.playPause();
    QCOMPARE(facade.title(), QStringLiteral("A"));

    facade.next();
    QVERIFY(facade.title().isEmpty());

    facade.setTitleFor(QStringLiteral("b"), QStringLiteral("Bee"));
    QCOMPARE(facade.title(), QStringLiteral("Bee"));

    facade.next();
    QCOMPARE(facade.title(), QStringLiteral("C"));
}

void MusicPlayerFacadeTest::playPauseStartsFirstTrack()
{
    MusicPlayerFacade facade;
    facade.setQueue(list({"a", "b"}), list({"A", "B"}));

    QSignalSpy trackSpy(&facade, &MusicPlayerFacade::trackChanged);
    facade.playPause();

    QVERIFY(facade.playing());
    QCOMPARE(facade.title(), QStringLiteral("A"));
    QCOMPARE(facade.positionMs(), 0);
    QCOMPARE(trackSpy.count(), 1);
    QCOMPARE(firstArgument(trackSpy, 0), QStringLiteral("a"));
}

void MusicPlayerFacadeTest::playPauseTogglesWithoutRestarting()
{
    MusicPlayerFacade facade;
    facade.setQueue(list({"a", "b"}), list({"A", "B"}));
    facade.playPause();
    facade.setDurationMs(60000);
    facade.seek(12000);

    facade.playPause();

    QVERIFY(!facade.playing());
    QCOMPARE(facade.title(), QStringLiteral("A"));
    QCOMPARE(facade.positionMs(), 12000);

    facade.playPause();

    QVERIFY(facade.playing());
    QCOMPARE(facade.positionMs(), 12000);
}

void MusicPlayerFacadeTest::nextAndPreviousWalkTheQueue()
{
    MusicPlayerFacade facade;
    facade.setQueue(list({"a", "b", "c"}), list({"A", "B", "C"}));
    facade.playPause();

    QSignalSpy trackSpy(&facade, &MusicPlayerFacade::trackChanged);

    facade.next();
    QVERIFY(facade.playing());
    QCOMPARE(facade.title(), QStringLiteral("B"));
    QCOMPARE(facade.positionMs(), 0);
    QCOMPARE(trackSpy.count(), 1);
    QCOMPARE(firstArgument(trackSpy, 0), QStringLiteral("b"));

    facade.next();
    QCOMPARE(facade.title(), QStringLiteral("C"));
    QCOMPARE(trackSpy.count(), 2);
    QCOMPARE(firstArgument(trackSpy, 1), QStringLiteral("c"));

    facade.previous();
    QCOMPARE(facade.title(), QStringLiteral("B"));
    QCOMPARE(trackSpy.count(), 3);
    QCOMPARE(firstArgument(trackSpy, 2), QStringLiteral("b"));
}

void MusicPlayerFacadeTest::nextFromIdleStartsFirstTrack()
{
    MusicPlayerFacade facade;
    facade.setQueue(list({"a", "b"}), list({"A", "B"}));

    QSignalSpy trackSpy(&facade, &MusicPlayerFacade::trackChanged);
    facade.next();

    QVERIFY(facade.playing());
    QCOMPARE(facade.title(), QStringLiteral("A"));
    QCOMPARE(trackSpy.count(), 1);
    QCOMPARE(firstArgument(trackSpy, 0), QStringLiteral("a"));
}

void MusicPlayerFacadeTest::previousFromIdleStartsLastTrack()
{
    MusicPlayerFacade facade;
    facade.setQueue(list({"a", "b", "c"}), list({"A", "B", "C"}));

    QSignalSpy trackSpy(&facade, &MusicPlayerFacade::trackChanged);
    facade.previous();

    QVERIFY(facade.playing());
    QCOMPARE(facade.title(), QStringLiteral("C"));
    QCOMPARE(trackSpy.count(), 1);
    QCOMPARE(firstArgument(trackSpy, 0), QStringLiteral("c"));
}

void MusicPlayerFacadeTest::nextAtEndStopsPlayback()
{
    MusicPlayerFacade facade;
    facade.setQueue(list({"a", "b"}), list({"A", "B"}));
    facade.playPause();
    facade.next();
    QCOMPARE(facade.title(), QStringLiteral("B"));

    QSignalSpy trackSpy(&facade, &MusicPlayerFacade::trackChanged);
    QSignalSpy changedSpy(&facade, &MusicPlayerFacade::changed);

    facade.next();

    QVERIFY(!facade.playing());
    QCOMPARE(facade.title(), QStringLiteral("B"));
    QVERIFY(!facade.hasNext());
    QCOMPARE(trackSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 1);
}

void MusicPlayerFacadeTest::seekClampsToDuration()
{
    MusicPlayerFacade facade;
    facade.setQueue(list({"a"}), list({"A"}));
    facade.playPause();
    facade.setDurationMs(10000);

    QCOMPARE(facade.durationMs(), 10000);

    facade.seek(2500);
    QCOMPARE(facade.positionMs(), 2500);

    facade.seek(99999);
    QCOMPARE(facade.positionMs(), 10000);

    facade.seek(-5);
    QCOMPARE(facade.positionMs(), 0);

    facade.setDurationMs(1000);
    QCOMPARE(facade.durationMs(), 1000);
    QCOMPARE(facade.positionMs(), 0);

    facade.seek(1500);
    QCOMPARE(facade.positionMs(), 1000);
}

void MusicPlayerFacadeTest::durationRejectsNegative()
{
    MusicPlayerFacade facade;

    facade.setDurationMs(-500);
    QCOMPARE(facade.durationMs(), 0);
}

void MusicPlayerFacadeTest::speedClampsAndRounds()
{
    MusicPlayerFacade facade;
    QSignalSpy changedSpy(&facade, &MusicPlayerFacade::changed);

    facade.setSpeed(1.237);
    QCOMPARE(facade.speed(), 1.24);
    QCOMPARE(changedSpy.count(), 1);

    facade.setSpeed(1.24);
    QCOMPARE(changedSpy.count(), 1);

    facade.setSpeed(0.1);
    QCOMPARE(facade.speed(), 0.5);
    QCOMPARE(changedSpy.count(), 2);

    facade.setSpeed(9.0);
    QCOMPARE(facade.speed(), 2.0);
    QCOMPARE(changedSpy.count(), 3);
}

void MusicPlayerFacadeTest::speedIgnoresNan()
{
    MusicPlayerFacade facade;
    facade.setSpeed(1.5);

    QSignalSpy changedSpy(&facade, &MusicPlayerFacade::changed);
    facade.setSpeed(std::numeric_limits<double>::quiet_NaN());

    QCOMPARE(facade.speed(), 1.5);
    QCOMPARE(changedSpy.count(), 0);
}

void MusicPlayerFacadeTest::shuffleToggles()
{
    MusicPlayerFacade facade;
    QSignalSpy changedSpy(&facade, &MusicPlayerFacade::changed);

    facade.setShuffle(true);
    QVERIFY(facade.shuffle());
    QCOMPARE(changedSpy.count(), 1);

    facade.setShuffle(true);
    QCOMPARE(changedSpy.count(), 1);

    facade.setShuffle(false);
    QVERIFY(!facade.shuffle());
    QCOMPARE(changedSpy.count(), 2);
}

void MusicPlayerFacadeTest::repeatCycles()
{
    MusicPlayerFacade facade;
    QSignalSpy changedSpy(&facade, &MusicPlayerFacade::changed);

    QCOMPARE(facade.repeatMode(), QStringLiteral("off"));

    facade.cycleRepeat();
    QCOMPARE(facade.repeatMode(), QStringLiteral("all"));
    QCOMPARE(changedSpy.count(), 1);

    facade.cycleRepeat();
    QCOMPARE(facade.repeatMode(), QStringLiteral("one"));
    QCOMPARE(changedSpy.count(), 2);

    facade.cycleRepeat();
    QCOMPARE(facade.repeatMode(), QStringLiteral("off"));
    QCOMPARE(changedSpy.count(), 3);
}

void MusicPlayerFacadeTest::hasNextFollowsQueueAndRepeat()
{
    MusicPlayerFacade facade;
    QVERIFY(!facade.hasNext());

    facade.setQueue(list({"a"}), list({"A"}));
    QVERIFY(facade.hasNext());

    facade.playPause();
    QVERIFY(!facade.hasNext());

    facade.cycleRepeat();
    QVERIFY(facade.hasNext());

    facade.cycleRepeat();
    QVERIFY(facade.hasNext());

    facade.cycleRepeat();
    QVERIFY(!facade.hasNext());
}

void MusicPlayerFacadeTest::setTitleForUpdatesCurrentTrack()
{
    MusicPlayerFacade facade;
    facade.setQueue(list({"a", "b"}), list({"A", "B"}));
    facade.playPause();

    QSignalSpy changedSpy(&facade, &MusicPlayerFacade::changed);

    facade.setTitleFor(QStringLiteral("a"), QStringLiteral("New A"));
    QCOMPARE(facade.title(), QStringLiteral("New A"));
    QCOMPARE(changedSpy.count(), 1);

    facade.setTitleFor(QStringLiteral("a"), QStringLiteral("New A"));
    QCOMPARE(changedSpy.count(), 1);

    // A title for a track that is not current is kept but changes no property.
    facade.setTitleFor(QStringLiteral("b"), QStringLiteral("New B"));
    QCOMPARE(changedSpy.count(), 1);

    facade.next();
    QCOMPARE(facade.title(), QStringLiteral("New B"));
}

void MusicPlayerFacadeTest::setTitleForUnknownTrackIsSilent()
{
    MusicPlayerFacade facade;
    facade.setQueue(list({"a"}), list({"A"}));
    facade.playPause();

    QSignalSpy changedSpy(&facade, &MusicPlayerFacade::changed);

    facade.setTitleFor(QString(), QStringLiteral("Ignored"));
    facade.setTitleFor(QStringLiteral("ghost"), QStringLiteral("Ghost"));

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(facade.title(), QStringLiteral("A"));
}

void MusicPlayerFacadeTest::saveSessionWritesVersionedDocument()
{
    MusicPlayerFacade facade;
    facade.setQueue(list({"a", "b"}), list({"A", "B"}));
    facade.playPause();
    facade.setDurationMs(60000);
    facade.seek(15000);

    facade.saveSession();

    QFile file(sessionPath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    QVERIFY(document.isObject());

    const QJsonObject root = document.object();
    QCOMPARE(root.value(QStringLiteral("version")).toInt(),
             beygram::telegram::MusicSessionStore::kSchemaVersion);
    QCOMPARE(root.value(QStringLiteral("trackId")).toString(), QStringLiteral("a"));
    QCOMPARE(root.value(QStringLiteral("index")).toInt(), 0);
    QCOMPARE(root.value(QStringLiteral("positionMs")).toInt(), 15000);
    QCOMPARE(root.value(QStringLiteral("durationMs")).toInt(), 60000);
    QVERIFY(root.value(QStringLiteral("playing")).toBool());
    QCOMPARE(static_cast<int>(root.value(QStringLiteral("queue")).toArray().size()), 2);
}

void MusicPlayerFacadeTest::saveAndRestoreRoundTrip()
{
    {
        MusicPlayerFacade first;
        first.setQueue(list({"a", "b", "c"}), list({"A", "B", "C"}));
        first.playPause();
        first.next();
        first.setDurationMs(180000);
        first.seek(42000);
        first.setSpeed(1.25);
        first.setShuffle(true);
        first.cycleRepeat();
        first.saveSession();
    }

    MusicPlayerFacade second;
    // Titles are not part of the session document; the caller re-registers
    // them and restoreSession() keeps the ones that still name a queued track.
    second.setTitleFor(QStringLiteral("b"), QStringLiteral("Restored B"));

    QSignalSpy trackSpy(&second, &MusicPlayerFacade::trackChanged);
    QSignalSpy changedSpy(&second, &MusicPlayerFacade::changed);

    second.restoreSession();

    QCOMPARE(second.title(), QStringLiteral("Restored B"));
    QVERIFY(second.playing());
    QCOMPARE(second.positionMs(), 42000);
    QCOMPARE(second.durationMs(), 180000);
    QVERIFY(second.shuffle());
    QCOMPARE(second.repeatMode(), QStringLiteral("all"));
    QVERIFY(second.hasNext());
    QCOMPARE(trackSpy.count(), 1);
    QCOMPARE(firstArgument(trackSpy, 0), QStringLiteral("b"));
    QCOMPARE(changedSpy.count(), 1);
}

void MusicPlayerFacadeTest::restoreWithoutSessionIsNoOp()
{
    MusicPlayerFacade facade;
    facade.setQueue(list({"a"}), list({"A"}));
    facade.playPause();
    QCOMPARE(facade.title(), QStringLiteral("A"));

    QSignalSpy trackSpy(&facade, &MusicPlayerFacade::trackChanged);
    QSignalSpy changedSpy(&facade, &MusicPlayerFacade::changed);

    facade.restoreSession();

    QVERIFY(facade.playing());
    QCOMPARE(facade.title(), QStringLiteral("A"));
    QCOMPARE(trackSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 0);
}

void MusicPlayerFacadeTest::changedOnlyOnObservableChanges()
{
    MusicPlayerFacade facade;
    facade.setQueue(list({"a", "b"}), list({"A", "B"}));
    facade.playPause();

    QSignalSpy changedSpy(&facade, &MusicPlayerFacade::changed);

    facade.setQueue(list({"a", "b"}), list({"A", "B"}));
    QCOMPARE(changedSpy.count(), 0);

    facade.setQueue(list({"a", "b"}), list({"A2", "B"}));
    QCOMPARE(changedSpy.count(), 1);

    facade.setSpeed(facade.speed());
    QCOMPARE(changedSpy.count(), 1);

    facade.setShuffle(false);
    QCOMPARE(changedSpy.count(), 1);

    facade.seek(0);
    QCOMPARE(changedSpy.count(), 1);

    facade.cycleRepeat();
    QCOMPARE(changedSpy.count(), 2);
}

QTEST_GUILESS_MAIN(MusicPlayerFacadeTest)

#include "tst_music_player_facade.moc"
