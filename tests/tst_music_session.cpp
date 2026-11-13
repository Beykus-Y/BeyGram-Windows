#include "telegram/music/MusicSession.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVector>
#include <QtTest>

#include <initializer_list>

using namespace beygram::telegram;

namespace {

constexpr qint64 kNow = 1'700'000'000;

QString sessionPath(const QTemporaryDir& directory)
{
    return directory.filePath(QStringLiteral("music/session.json"));
}

QVector<QString> ids(std::initializer_list<const char*> values)
{
    QVector<QString> result;
    result.reserve(static_cast<int>(values.size()));
    for (const char* value : values)
        result.append(QString::fromLatin1(value));
    return result;
}

MusicSessionState playableSession()
{
    MusicSessionState state;
    state.trackId = QStringLiteral("track-b");
    state.queue = ids({"track-a", "track-b", "track-c"});
    state.index = 1;
    state.positionMs = 42'000;
    state.durationMs = 180'000;
    state.playing = true;
    state.shuffle = true;
    state.repeatMode = 2;
    state.crossfadeMs = 6'000;
    state.savedAt = kNow;
    return state;
}

bool writeFile(const QString& path, const QByteArray& payload)
{
    const QDir parent = QFileInfo(path).absoluteDir();
    if (!parent.exists() && !QDir().mkpath(parent.absolutePath()))
        return false;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(payload) == payload.size();
}

QJsonObject readJson(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject{};
}

} // namespace

class MusicSessionTest final : public QObject {
    Q_OBJECT

private slots:
    void missingFileLoadsEmpty();
    void emptyPathIsInert();
    void pathIsKept();

    void roundTripsEveryField();
    void writesVersionedJson();
    void saveCreatesParentDirectory();
    void overwritesWithoutTemporaryFiles();

    void malformedDocumentLoadsEmpty();
    void unsupportedVersionLoadsEmpty();
    void missingFieldsUseDefaults();

    void queueDropsEmptyAndDuplicateIds();
    void indexIsClampedToQueue();
    void positionIsClampedToDuration();
    void negativeDurationBecomesZero();
    void repeatModeIsClamped();
    void crossfadeIsClamped();
    void negativeTimestampBecomesZero();
    void emptyQueueClearsCurrentTrack();

    void restoreKeepsFreshSession();
    void restoreRejectsExpiredSession();
    void restoreRejectsFutureTimestamp();
    void restoreRejectsMissingTimestamp();
    void restoreRejectsEmptyQueue();
    void restoreClampsIndexAndPosition();
    void restoreNormalizesSavedState();
    void restoreClearsPlayingWhenTrackMissing();
    void restoreKeepsPlayingWhenTrackPresent();
    void zeroMaxAgeOnlyAcceptsNow();
};

void MusicSessionTest::missingFileLoadsEmpty()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const MusicSessionStore store(sessionPath(directory));
    QVERIFY(!QFileInfo::exists(store.path()));
    QVERIFY((store.load() == MusicSessionState{}));
}

void MusicSessionTest::emptyPathIsInert()
{
    const MusicSessionStore store{QString()};
    QVERIFY(store.path().isEmpty());
    QVERIFY((store.load() == MusicSessionState{}));
    QVERIFY(!store.save(playableSession()));
}

void MusicSessionTest::pathIsKept()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = sessionPath(directory);
    const MusicSessionStore store(path);
    QCOMPARE(store.path(), path);
}

void MusicSessionTest::roundTripsEveryField()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const MusicSessionStore store(sessionPath(directory));

    const MusicSessionState expected = playableSession();
    QVERIFY(store.save(expected));
    QVERIFY(store.load() == expected);
}

void MusicSessionTest::writesVersionedJson()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const MusicSessionStore store(sessionPath(directory));

    QVERIFY(store.save(playableSession()));

    const QJsonObject root = readJson(store.path());
    QVERIFY(!root.isEmpty());
    QCOMPARE(root.value(QStringLiteral("version")).toInt(), MusicSessionStore::kSchemaVersion);
    QCOMPARE(root.value(QStringLiteral("trackId")).toString(), QStringLiteral("track-b"));
    QCOMPARE(static_cast<int>(root.value(QStringLiteral("queue")).toArray().size()), 3);
    QCOMPARE(root.value(QStringLiteral("savedAt")).toInteger(), kNow);
}

void MusicSessionTest::saveCreatesParentDirectory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const MusicSessionStore store(sessionPath(directory));

    QVERIFY(store.save(playableSession()));
    QVERIFY(QFileInfo::exists(store.path()));
    QVERIFY(QFileInfo(store.path()).absoluteDir().exists());
}

void MusicSessionTest::overwritesWithoutTemporaryFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const MusicSessionStore store(sessionPath(directory));

    MusicSessionState first = playableSession();
    first.trackId = QStringLiteral("track-a");
    first.index = 0;
    QVERIFY(store.save(first));

    MusicSessionState second = playableSession();
    second.trackId = QStringLiteral("track-c");
    second.index = 2;
    second.playing = false;
    QVERIFY(store.save(second));

    QVERIFY(store.load() == second);
    const QStringList entries = QFileInfo(store.path()).absoluteDir().entryList(
        QDir::Files | QDir::Hidden | QDir::System);
    QCOMPARE(entries, QStringList{QStringLiteral("session.json")});
}

void MusicSessionTest::malformedDocumentLoadsEmpty()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const MusicSessionStore store(sessionPath(directory));

    QVERIFY(writeFile(store.path(), QByteArrayLiteral("{\"version\":1,")));
    QVERIFY((store.load() == MusicSessionState{}));

    QVERIFY(writeFile(store.path(), QByteArrayLiteral("[1,2,3]")));
    QVERIFY((store.load() == MusicSessionState{}));

    QVERIFY(writeFile(store.path(), QByteArray()));
    QVERIFY((store.load() == MusicSessionState{}));
}

void MusicSessionTest::unsupportedVersionLoadsEmpty()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const MusicSessionStore store(sessionPath(directory));

    QVERIFY(writeFile(store.path(),
                      QByteArrayLiteral("{\"version\":2,\"queue\":[\"a\"],\"index\":0}")));
    QVERIFY((store.load() == MusicSessionState{}));

    // A document without a version is not guessed at either.
    QVERIFY(writeFile(store.path(), QByteArrayLiteral("{\"queue\":[\"a\"]}")));
    QVERIFY((store.load() == MusicSessionState{}));

    QVERIFY(writeFile(store.path(),
                      QByteArrayLiteral("{\"version\":\"1\",\"queue\":[\"a\"]}")));
    QVERIFY((store.load() == MusicSessionState{}));
}

void MusicSessionTest::missingFieldsUseDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const MusicSessionStore store(sessionPath(directory));

    QVERIFY(writeFile(store.path(),
                      QByteArrayLiteral("{\"version\":1,\"queue\":[\"a\"]}")));

    const MusicSessionState loaded = store.load();
    QVERIFY(loaded.trackId.isEmpty());
    QVERIFY((loaded.queue == ids({"a"})));
    QCOMPARE(loaded.index, 0);
    QCOMPARE(loaded.positionMs, 0);
    QCOMPARE(loaded.durationMs, 0);
    QVERIFY(!loaded.playing);
    QVERIFY(!loaded.shuffle);
    QCOMPARE(loaded.repeatMode, 0);
    QCOMPARE(loaded.crossfadeMs, 4000);
    QCOMPARE(loaded.savedAt, qint64(0));
}

void MusicSessionTest::queueDropsEmptyAndDuplicateIds()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const MusicSessionStore store(sessionPath(directory));

    MusicSessionState state;
    state.trackId = QStringLiteral(" b ");
    state.queue = {QStringLiteral("a"), QString(), QStringLiteral("   "), QStringLiteral("a"),
                   QStringLiteral(" b "), QStringLiteral("b"), QStringLiteral("c")};
    state.index = 1;

    QVERIFY(store.save(state));
    const MusicSessionState loaded = store.load();
    QVERIFY((loaded.queue == ids({"a", "b", "c"})));
    QCOMPARE(loaded.trackId, QStringLiteral("b"));
    QCOMPARE(loaded.index, 1);
}

void MusicSessionTest::indexIsClampedToQueue()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const MusicSessionStore store(sessionPath(directory));

    MusicSessionState state = playableSession();
    state.index = 99;
    QVERIFY(store.save(state));
    QCOMPARE(store.load().index, 2);

    state.index = -7;
    QVERIFY(store.save(state));
    QCOMPARE(store.load().index, 0);
}

void MusicSessionTest::positionIsClampedToDuration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const MusicSessionStore store(sessionPath(directory));

    MusicSessionState state = playableSession();
    state.durationMs = 1'000;
    state.positionMs = 5'000;
    QVERIFY(store.save(state));
    QCOMPARE(store.load().positionMs, 1'000);

    state.positionMs = -50;
    QVERIFY(store.save(state));
    QCOMPARE(store.load().positionMs, 0);
}

void MusicSessionTest::negativeDurationBecomesZero()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const MusicSessionStore store(sessionPath(directory));

    MusicSessionState state = playableSession();
    state.durationMs = -100;
    state.positionMs = 50;
    QVERIFY(store.save(state));

    const MusicSessionState loaded = store.load();
    QCOMPARE(loaded.durationMs, 0);
    QCOMPARE(loaded.positionMs, 0);
}

void MusicSessionTest::repeatModeIsClamped()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const MusicSessionStore store(sessionPath(directory));

    MusicSessionState state = playableSession();

    state.repeatMode = 9;
    QVERIFY(store.save(state));
    QCOMPARE(store.load().repeatMode, 2);

    state.repeatMode = -2;
    QVERIFY(store.save(state));
    QCOMPARE(store.load().repeatMode, 0);

    state.repeatMode = 1;
    QVERIFY(store.save(state));
    QCOMPARE(store.load().repeatMode, 1);
}

void MusicSessionTest::crossfadeIsClamped()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const MusicSessionStore store(sessionPath(directory));

    MusicSessionState state = playableSession();

    state.crossfadeMs = 99'999;
    QVERIFY(store.save(state));
    QCOMPARE(store.load().crossfadeMs, 12'000);

    state.crossfadeMs = -1;
    QVERIFY(store.save(state));
    QCOMPARE(store.load().crossfadeMs, 0);

    state.crossfadeMs = 12'000;
    QVERIFY(store.save(state));
    QCOMPARE(store.load().crossfadeMs, 12'000);
}

void MusicSessionTest::negativeTimestampBecomesZero()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const MusicSessionStore store(sessionPath(directory));

    MusicSessionState state = playableSession();
    state.savedAt = -10;
    QVERIFY(store.save(state));
    QCOMPARE(store.load().savedAt, qint64(0));
}

void MusicSessionTest::emptyQueueClearsCurrentTrack()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const MusicSessionStore store(sessionPath(directory));

    MusicSessionState state = playableSession();
    state.queue = {QString(), QStringLiteral("   ")};
    state.trackId = QStringLiteral("track-a");
    state.index = 3;
    state.positionMs = 500;
    state.durationMs = 1'000;
    state.playing = true;

    QVERIFY(store.save(state));
    const MusicSessionState loaded = store.load();
    QVERIFY(loaded.queue.isEmpty());
    QVERIFY(loaded.trackId.isEmpty());
    QCOMPARE(loaded.index, -1);
    QCOMPARE(loaded.positionMs, 0);
    QCOMPARE(loaded.durationMs, 1'000);
    QVERIFY(!loaded.playing);
}

void MusicSessionTest::restoreKeepsFreshSession()
{
    const MusicSessionState saved = playableSession();
    QVERIFY((restoreMusicSession(saved, kNow + 3'600) == saved));
}

void MusicSessionTest::restoreRejectsExpiredSession()
{
    const MusicSessionState saved = playableSession();

    QVERIFY((restoreMusicSession(saved, kNow + 86'401) == MusicSessionState{}));
    QVERIFY((restoreMusicSession(saved, kNow + 86'400) == saved));
}

void MusicSessionTest::restoreRejectsFutureTimestamp()
{
    const MusicSessionState saved = playableSession();
    QVERIFY((restoreMusicSession(saved, kNow - 1) == MusicSessionState{}));
}

void MusicSessionTest::restoreRejectsMissingTimestamp()
{
    MusicSessionState saved = playableSession();

    saved.savedAt = 0;
    QVERIFY((restoreMusicSession(saved, kNow) == MusicSessionState{}));

    saved.savedAt = -10;
    QVERIFY((restoreMusicSession(saved, kNow) == MusicSessionState{}));
}

void MusicSessionTest::restoreRejectsEmptyQueue()
{
    MusicSessionState saved = playableSession();
    saved.queue.clear();
    saved.trackId.clear();
    saved.index = -1;

    QVERIFY((restoreMusicSession(saved, kNow) == MusicSessionState{}));
}

void MusicSessionTest::restoreClampsIndexAndPosition()
{
    MusicSessionState saved = playableSession();
    saved.index = 42;
    saved.positionMs = 999'999;
    saved.durationMs = 1'000;

    MusicSessionState restored = restoreMusicSession(saved, kNow);
    QCOMPARE(restored.index, 2);
    QCOMPARE(restored.positionMs, 1'000);

    saved.index = -4;
    restored = restoreMusicSession(saved, kNow);
    QCOMPARE(restored.index, 0);
}

void MusicSessionTest::restoreNormalizesSavedState()
{
    MusicSessionState saved = playableSession();
    saved.queue = {QStringLiteral("a"), QString(), QStringLiteral("a")};
    saved.trackId = QStringLiteral("a");
    saved.index = 9;
    saved.repeatMode = 9;
    saved.crossfadeMs = -1;

    const MusicSessionState restored = restoreMusicSession(saved, kNow);
    QVERIFY((restored.queue == ids({"a"})));
    QCOMPARE(restored.index, 0);
    QCOMPARE(restored.repeatMode, 2);
    QCOMPARE(restored.crossfadeMs, 0);
}

void MusicSessionTest::restoreClearsPlayingWhenTrackMissing()
{
    MusicSessionState saved = playableSession();
    saved.trackId = QStringLiteral("track-gone");
    saved.playing = true;

    MusicSessionState restored = restoreMusicSession(saved, kNow);
    QVERIFY(!restored.playing);
    QVERIFY(restored.queue == saved.queue);
    QCOMPARE(restored.index, 1);

    saved.trackId.clear();
    restored = restoreMusicSession(saved, kNow);
    QVERIFY(!restored.playing);
}

void MusicSessionTest::restoreKeepsPlayingWhenTrackPresent()
{
    const MusicSessionState saved = playableSession();
    QVERIFY(restoreMusicSession(saved, kNow).playing);
}

void MusicSessionTest::zeroMaxAgeOnlyAcceptsNow()
{
    const MusicSessionState saved = playableSession();

    QVERIFY((restoreMusicSession(saved, kNow, 0) == saved));
    QVERIFY((restoreMusicSession(saved, kNow + 1, 0) == MusicSessionState{}));

    // A negative age behaves like zero rather than accepting everything.
    QVERIFY((restoreMusicSession(saved, kNow, -5) == saved));
    QVERIFY((restoreMusicSession(saved, kNow + 1, -5) == MusicSessionState{}));
}

QTEST_GUILESS_MAIN(MusicSessionTest)

#include "tst_music_session.moc"
