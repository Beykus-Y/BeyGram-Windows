#include "telegram/BackgroundMusic.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

MusicTrack track(qint64 id, const QString& title = {}, const QString& performer = {},
                 int durationSeconds = 0, int fileId = 0, const QString& localPath = {})
{
    MusicTrack result;
    result.id = id;
    result.title = title;
    result.performer = performer;
    result.durationSeconds = durationSeconds;
    result.fileId = fileId;
    result.localPath = localPath;
    return result;
}

QVector<qint64> idsOf(const QVector<MusicTrack>& tracks)
{
    QVector<qint64> ids;
    ids.reserve(tracks.size());
    for (const MusicTrack& item : tracks)
        ids.append(item.id);
    return ids;
}

QJsonObject jsonObject(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

} // namespace

class BackgroundMusicTest final : public QObject {
    Q_OBJECT

private slots:
    void searchRequestTrimsQuery();
    void searchRequestRejectsEmptyQuery();
    void searchRequestClampsLimit();
    void playlistRequestCarriesId();

    void formatDurationUnderAnHour();
    void formatDurationWithHours();
    void formatDurationNegative();

    void parsesPlainTracks();
    void parsesNestedPlaylistTracks();
    void parsesTdlibAudioMessage();
    void parsesPerformerAliases();
    void dropsTracksWithoutId();
    void dedupesTracksKeepingFirst();
    void clampsNegativeDurations();
    void ignoresMalformedResponses();

    void queueStartsEmpty();
    void queueCleansTracks();
    void queuePlaysValidIndexes();
    void queueNextStopsAtEnd();
    void queueNextWrapsWithRepeatAll();
    void queuePreviousStopsAtStart();
    void queuePreviousWrapsAfterRepeatAll();
    void queueClearDropsEverything();
    void queueChangedOnlyOnRealChanges();
    void queueSetTracksResetsCurrent();
    void queueReturnsIndependentCopies();
};

void BackgroundMusicTest::searchRequestTrimsQuery()
{
    const QJsonObject request = searchMusicRequest(QStringLiteral("  daft punk  "), 20);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("searchMusic"));
    QCOMPARE(request.value(QStringLiteral("query")).toString(),
             QStringLiteral("daft punk"));
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 20);
    QCOMPARE(request.size(), qsizetype(3));
}

void BackgroundMusicTest::searchRequestRejectsEmptyQuery()
{
    QVERIFY(searchMusicRequest(QString(), 10).isEmpty());
    QVERIFY(searchMusicRequest(QStringLiteral("   "), 10).isEmpty());
    QVERIFY(searchMusicRequest(QStringLiteral("\n\t"), 10).isEmpty());
}

void BackgroundMusicTest::searchRequestClampsLimit()
{
    const auto limitOf = [](int limit) {
        return searchMusicRequest(QStringLiteral("q"), limit)
            .value(QStringLiteral("limit"))
            .toInt();
    };

    QCOMPARE(limitOf(0), 1);
    QCOMPARE(limitOf(-25), 1);
    QCOMPARE(limitOf(1), 1);
    QCOMPARE(limitOf(50), 50);
    QCOMPARE(limitOf(100), 100);
    QCOMPARE(limitOf(101), 100);
    QCOMPARE(limitOf(100000), 100);
}

void BackgroundMusicTest::playlistRequestCarriesId()
{
    const QJsonObject request = getMusicPlaylistRequest(4242);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getMusicPlaylist"));
    QCOMPARE(request.value(QStringLiteral("playlist_id")).toVariant().toLongLong(),
             qint64(4242));
    QCOMPARE(request.size(), qsizetype(2));
}

void BackgroundMusicTest::formatDurationUnderAnHour()
{
    QCOMPARE(formatDuration(0), QStringLiteral("0:00"));
    QCOMPARE(formatDuration(5), QStringLiteral("0:05"));
    QCOMPARE(formatDuration(59), QStringLiteral("0:59"));
    QCOMPARE(formatDuration(60), QStringLiteral("1:00"));
    QCOMPARE(formatDuration(61), QStringLiteral("1:01"));
    QCOMPARE(formatDuration(600), QStringLiteral("10:00"));
    QCOMPARE(formatDuration(3599), QStringLiteral("59:59"));
}

void BackgroundMusicTest::formatDurationWithHours()
{
    QCOMPARE(formatDuration(3600), QStringLiteral("1:00:00"));
    QCOMPARE(formatDuration(3661), QStringLiteral("1:01:01"));
    QCOMPARE(formatDuration(36000), QStringLiteral("10:00:00"));
    QCOMPARE(formatDuration(86399), QStringLiteral("23:59:59"));
}

void BackgroundMusicTest::formatDurationNegative()
{
    QCOMPARE(formatDuration(-1), QStringLiteral("0:00"));
    QCOMPARE(formatDuration(-3600), QStringLiteral("0:00"));
}

void BackgroundMusicTest::parsesPlainTracks()
{
    const QJsonObject response = jsonObject(R"JSON({
        "tracks": [
            {
                "id": 7,
                "title": "Get Lucky",
                "performer": "Daft Punk",
                "durationSeconds": 369,
                "fileId": 900,
                "localPath": "C:/music/get-lucky.mp3"
            },
            {
                "id": "8",
                "title": "Second",
                "artist": "Other",
                "duration": -5,
                "file": {
                    "id": "901",
                    "local": {"path": "C:/music/second.mp3"}
                }
            }
        ]
    })JSON");

    const QVector<MusicTrack> tracks = musicTracksFromResponse(response);

    QCOMPARE(tracks.size(), qsizetype(2));
    QCOMPARE(tracks.at(0).id, qint64(7));
    QCOMPARE(tracks.at(0).title, QStringLiteral("Get Lucky"));
    QCOMPARE(tracks.at(0).performer, QStringLiteral("Daft Punk"));
    QCOMPARE(tracks.at(0).durationSeconds, 369);
    QCOMPARE(tracks.at(0).fileId, 900);
    QCOMPARE(tracks.at(0).localPath, QStringLiteral("C:/music/get-lucky.mp3"));

    QCOMPARE(tracks.at(1).id, qint64(8));
    QCOMPARE(tracks.at(1).title, QStringLiteral("Second"));
    QCOMPARE(tracks.at(1).performer, QStringLiteral("Other"));
    QCOMPARE(tracks.at(1).durationSeconds, 0);
    QCOMPARE(tracks.at(1).fileId, 901);
    QCOMPARE(tracks.at(1).localPath, QStringLiteral("C:/music/second.mp3"));
}

void BackgroundMusicTest::parsesNestedPlaylistTracks()
{
    const QJsonObject response = jsonObject(R"JSON({
        "playlist": {
            "id": 12,
            "title": "Road trip",
            "tracks": [
                {"id": 1, "title": "One", "performer": "A", "duration": 10},
                {"id": 2, "title": "Two", "performer": "B", "duration": 20}
            ]
        }
    })JSON");

    const QVector<MusicTrack> tracks = musicTracksFromResponse(response);

    QCOMPARE(idsOf(tracks), (QVector<qint64>{1, 2}));
    QCOMPARE(tracks.at(1).title, QStringLiteral("Two"));
    QCOMPARE(tracks.at(1).durationSeconds, 20);
}

void BackgroundMusicTest::parsesTdlibAudioMessage()
{
    const QJsonObject response = jsonObject(R"JSON({
        "messages": [
            {
                "id": 55,
                "chat_id": -100123,
                "content": {
                    "@type": "messageAudio",
                    "audio": {
                        "duration": 210,
                        "title": "Song",
                        "performer": "Band",
                        "audio": {
                            "id": 777,
                            "local": {"path": "C:/tdlib/song.ogg"}
                        }
                    }
                }
            }
        ]
    })JSON");

    const QVector<MusicTrack> tracks = musicTracksFromResponse(response);

    QCOMPARE(tracks.size(), qsizetype(1));
    QCOMPARE(tracks.at(0).id, qint64(55));
    QCOMPARE(tracks.at(0).title, QStringLiteral("Song"));
    QCOMPARE(tracks.at(0).performer, QStringLiteral("Band"));
    QCOMPARE(tracks.at(0).durationSeconds, 210);
    QCOMPARE(tracks.at(0).fileId, 777);
    QCOMPARE(tracks.at(0).localPath, QStringLiteral("C:/tdlib/song.ogg"));
}

void BackgroundMusicTest::parsesPerformerAliases()
{
    const QJsonObject response = jsonObject(R"JSON({
        "tracks": [
            {"id": 1, "title": "One", "artist": "Solo"},
            {"id": 2, "title": "Two", "artists": ["First", "Second"]}
        ]
    })JSON");

    const QVector<MusicTrack> tracks = musicTracksFromResponse(response);

    QCOMPARE(tracks.size(), qsizetype(2));
    QCOMPARE(tracks.at(0).performer, QStringLiteral("Solo"));
    QCOMPARE(tracks.at(1).performer, QStringLiteral("First, Second"));
}

void BackgroundMusicTest::dropsTracksWithoutId()
{
    const QJsonObject response = jsonObject(R"JSON({
        "tracks": [
            {"title": "No id"},
            {"id": 0, "title": "Zero"},
            {"id": -4, "title": "Negative"},
            {"id": 3, "title": "Kept"}
        ]
    })JSON");

    const QVector<MusicTrack> tracks = musicTracksFromResponse(response);

    QCOMPARE(tracks.size(), qsizetype(1));
    QCOMPARE(tracks.at(0).id, qint64(3));
    QCOMPARE(tracks.at(0).title, QStringLiteral("Kept"));
}

void BackgroundMusicTest::dedupesTracksKeepingFirst()
{
    const QJsonObject response = jsonObject(R"JSON({
        "tracks": [
            {"id": 2, "title": "First", "performer": "A", "duration": 10},
            {"id": 2, "title": "Second", "performer": "B", "duration": 20},
            {"id": 1, "title": "Other"},
            {"id": 2, "title": "Third"}
        ]
    })JSON");

    const QVector<MusicTrack> tracks = musicTracksFromResponse(response);

    QCOMPARE(idsOf(tracks), (QVector<qint64>{2, 1}));
    QCOMPARE(tracks.at(0).title, QStringLiteral("First"));
    QCOMPARE(tracks.at(0).performer, QStringLiteral("A"));
    QCOMPARE(tracks.at(0).durationSeconds, 10);
}

void BackgroundMusicTest::clampsNegativeDurations()
{
    const QJsonObject response = jsonObject(R"JSON({
        "tracks": [
            {"id": 1, "duration": -1},
            {"id": 2, "durationSeconds": -3600},
            {"id": 3, "duration": 0},
            {"id": 4, "duration": 42}
        ]
    })JSON");

    const QVector<MusicTrack> tracks = musicTracksFromResponse(response);

    QCOMPARE(tracks.size(), qsizetype(4));
    QCOMPARE(tracks.at(0).durationSeconds, 0);
    QCOMPARE(tracks.at(1).durationSeconds, 0);
    QCOMPARE(tracks.at(2).durationSeconds, 0);
    QCOMPARE(tracks.at(3).durationSeconds, 42);
}

void BackgroundMusicTest::ignoresMalformedResponses()
{
    QVERIFY(musicTracksFromResponse(QJsonObject()).isEmpty());
    QVERIFY(musicTracksFromResponse(
                jsonObject(R"JSON({"tracks": "not an array"})JSON"))
                .isEmpty());
    QVERIFY(musicTracksFromResponse(
                jsonObject(R"JSON({"tracks": [null, 3, "text", {}, {"id": 0}]})JSON"))
                .isEmpty());
}

void BackgroundMusicTest::queueStartsEmpty()
{
    MusicQueue queue;

    QVERIFY(queue.tracks().isEmpty());
    QCOMPARE(queue.currentIndex(), -1);
    QCOMPARE(queue.current().id, qint64(0));
    QVERIFY(queue.current().title.isEmpty());
    QVERIFY(!queue.next());
    QVERIFY(!queue.previous());
}

void BackgroundMusicTest::queueCleansTracks()
{
    MusicQueue queue;
    queue.setTracks({track(3, QStringLiteral("Three")),
                     track(1, QStringLiteral("One")),
                     track(3, QStringLiteral("Duplicate")),
                     track(0, QStringLiteral("Zero")),
                     track(-2, QStringLiteral("Negative")),
                     track(2, QStringLiteral("Two"), {}, -9)});

    QCOMPARE(idsOf(queue.tracks()), (QVector<qint64>{3, 1, 2}));
    QCOMPARE(queue.tracks().at(0).title, QStringLiteral("Three"));
    QCOMPARE(queue.tracks().at(2).durationSeconds, 0);
    QCOMPARE(queue.currentIndex(), -1);
}

void BackgroundMusicTest::queuePlaysValidIndexes()
{
    MusicQueue queue;
    queue.setTracks({track(1, QStringLiteral("One")),
                     track(2, QStringLiteral("Two")),
                     track(3, QStringLiteral("Three"))});
    QSignalSpy spy(&queue, &MusicQueue::changed);

    queue.playAt(1);

    QCOMPARE(queue.currentIndex(), 1);
    QCOMPARE(queue.current().id, qint64(2));
    QCOMPARE(spy.count(), 1);

    queue.playAt(1);
    queue.playAt(-1);
    queue.playAt(3);
    queue.playAt(99);

    QCOMPARE(queue.currentIndex(), 1);
    QCOMPARE(spy.count(), 1);

    queue.playAt(0);
    QCOMPARE(queue.currentIndex(), 0);
    QCOMPARE(spy.count(), 2);
}

void BackgroundMusicTest::queueNextStopsAtEnd()
{
    MusicQueue queue;
    queue.setTracks({track(1), track(2), track(3)});
    QSignalSpy spy(&queue, &MusicQueue::changed);

    QVERIFY(queue.next());
    QCOMPARE(queue.currentIndex(), 0);
    QVERIFY(queue.next());
    QCOMPARE(queue.currentIndex(), 1);
    QVERIFY(queue.next());
    QCOMPARE(queue.currentIndex(), 2);
    QVERIFY(!queue.next());
    QCOMPARE(queue.currentIndex(), 2);
    QCOMPARE(spy.count(), 3);
}

void BackgroundMusicTest::queueNextWrapsWithRepeatAll()
{
    MusicQueue queue;
    queue.setTracks({track(1), track(2), track(3)});
    queue.playAt(2);
    QSignalSpy spy(&queue, &MusicQueue::changed);

    QVERIFY(queue.next(true));
    QCOMPARE(queue.currentIndex(), 0);
    QCOMPARE(spy.count(), 1);

    QVERIFY(queue.next());
    QCOMPARE(queue.currentIndex(), 1);
    QVERIFY(queue.next());
    QCOMPARE(queue.currentIndex(), 2);
    QVERIFY(!queue.next());
    QCOMPARE(queue.currentIndex(), 2);
}

void BackgroundMusicTest::queuePreviousStopsAtStart()
{
    MusicQueue queue;
    queue.setTracks({track(1), track(2), track(3)});
    queue.playAt(0);
    QSignalSpy spy(&queue, &MusicQueue::changed);

    QVERIFY(!queue.previous());
    QCOMPARE(queue.currentIndex(), 0);
    QCOMPARE(spy.count(), 0);

    queue.playAt(2);
    QVERIFY(queue.previous());
    QCOMPARE(queue.currentIndex(), 1);
    QVERIFY(queue.previous());
    QCOMPARE(queue.currentIndex(), 0);
    QVERIFY(!queue.previous());
    QCOMPARE(spy.count(), 3);
}

void BackgroundMusicTest::queuePreviousWrapsAfterRepeatAll()
{
    MusicQueue queue;
    queue.setTracks({track(1), track(2), track(3)});
    queue.playAt(2);
    QVERIFY(queue.next(true));
    QCOMPARE(queue.currentIndex(), 0);

    QSignalSpy spy(&queue, &MusicQueue::changed);
    QVERIFY(queue.previous());
    QCOMPARE(queue.currentIndex(), 2);
    QCOMPARE(spy.count(), 1);
}

void BackgroundMusicTest::queueClearDropsEverything()
{
    MusicQueue queue;
    queue.setTracks({track(1), track(2)});
    queue.playAt(1);
    QSignalSpy spy(&queue, &MusicQueue::changed);

    queue.clear();

    QVERIFY(queue.tracks().isEmpty());
    QCOMPARE(queue.currentIndex(), -1);
    QCOMPARE(spy.count(), 1);

    queue.clear();
    QCOMPARE(spy.count(), 1);
}

void BackgroundMusicTest::queueChangedOnlyOnRealChanges()
{
    MusicQueue queue;
    queue.setTracks({track(1, QStringLiteral("One")),
                     track(2, QStringLiteral("Two"))});
    QSignalSpy spy(&queue, &MusicQueue::changed);

    queue.setTracks({track(1, QStringLiteral("One")),
                     track(2, QStringLiteral("Two"))});
    QCOMPARE(spy.count(), 0);

    queue.playAt(1);
    QCOMPARE(spy.count(), 1);
    queue.playAt(1);
    QCOMPARE(spy.count(), 1);

    QVERIFY(!queue.next());
    QCOMPARE(spy.count(), 1);

    queue.playAt(0);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!queue.previous());
    QCOMPARE(spy.count(), 2);

    queue.clear();
    QCOMPARE(spy.count(), 3);
    queue.clear();
    QCOMPARE(spy.count(), 3);
}

void BackgroundMusicTest::queueSetTracksResetsCurrent()
{
    MusicQueue queue;
    const QVector<MusicTrack> tracks{track(1, QStringLiteral("One")),
                                     track(2, QStringLiteral("Two"))};
    queue.setTracks(tracks);
    queue.playAt(1);
    QSignalSpy spy(&queue, &MusicQueue::changed);

    queue.setTracks(tracks);

    QCOMPARE(queue.currentIndex(), -1);
    QCOMPARE(spy.count(), 1);

    queue.setTracks(tracks);
    QCOMPARE(spy.count(), 1);
}

void BackgroundMusicTest::queueReturnsIndependentCopies()
{
    MusicQueue queue;
    queue.setTracks({track(1, QStringLiteral("Song"), QStringLiteral("Band"))});

    QVector<MusicTrack> copy = queue.tracks();
    copy[0].title = QStringLiteral("Changed");

    QCOMPARE(queue.tracks().at(0).title, QStringLiteral("Song"));
}

QTEST_GUILESS_MAIN(BackgroundMusicTest)
#include "tst_background_music.moc"
