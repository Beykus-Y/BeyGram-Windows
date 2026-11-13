#include "telegram/music/BgMusic.h"

#include <QChar>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

BgMusicEntry makeEntry(const QString& trackId, const QString& title = {},
                       const QString& artist = {}, int durationSeconds = 0,
                       int startMs = 0)
{
    return BgMusicEntry{trackId, title, artist, durationSeconds, startMs};
}

BgMusicPlaylist makePlaylist()
{
    BgMusicPlaylist playlist;
    playlist.id = QStringLiteral("playlist-1");
    playlist.title = QStringLiteral("Road Trip");
    playlist.updatedAt = 1700000000123LL;
    playlist.entries = {
        makeEntry(QStringLiteral("track-1"), QStringLiteral("Opening"), QStringLiteral("Band"), 125, 1500),
        makeEntry(QStringLiteral("track-2"), QStringLiteral("Solo"), {}, 0, 0),
    };
    return playlist;
}

QJsonObject parseObject(const QString& json)
{
    return QJsonDocument::fromJson(json.toUtf8()).object();
}

} // namespace

class BgMusicTest final : public QObject {
    Q_OBJECT

private slots:
    void jsonRoundTripPreservesPlaylist();
    void jsonHasVersionedShape();
    void jsonClampsNegativeValues();
    void jsonDropsEntriesWithoutTrackId();
    void jsonDedupesTrackIdsKeepingFirst();
    void jsonRejectsMalformedDocuments();
    void jsonRejectsFutureVersion();
    void jsonAcceptsMissingVersion();
    void jsonSkipsMalformedEntries();
    void jsonAcceptsSnakeCaseAliases();

    void m3uWritesExtinfTitleArtistAndStart();
    void m3uRoundTripKeepsEntries();
    void m3uSkipsMalformedDirectives();
    void m3uDedupesTrackIdsKeepingFirst();
    void m3uClampsNegativeValues();
    void m3uHandlesBomAndCrlf();
    void m3uStartAfterTrackAppliesToThatTrack();
    void m3uAcceptsPlainTrackWithoutMetadata();
    void m3uEmptyInputIsEmpty();
    void m3uTitleWithoutArtistHasNoSeparator();
    void m3uKeepsEmptyTitleWithArtist();
};

void BgMusicTest::jsonRoundTripPreservesPlaylist()
{
    const BgMusicPlaylist original = makePlaylist();
    const BgMusicPlaylist parsed = bgMusicFromJson(bgMusicToJson(original));

    QCOMPARE(parsed.id, original.id);
    QCOMPARE(parsed.title, original.title);
    QCOMPARE(parsed.updatedAt, original.updatedAt);
    QCOMPARE(parsed.entries.size(), original.entries.size());
    for (int i = 0; i < original.entries.size(); ++i) {
        QCOMPARE(parsed.entries.at(i).trackId, original.entries.at(i).trackId);
        QCOMPARE(parsed.entries.at(i).title, original.entries.at(i).title);
        QCOMPARE(parsed.entries.at(i).artist, original.entries.at(i).artist);
        QCOMPARE(parsed.entries.at(i).durationSeconds,
                 original.entries.at(i).durationSeconds);
        QCOMPARE(parsed.entries.at(i).startMs, original.entries.at(i).startMs);
    }
}

void BgMusicTest::jsonHasVersionedShape()
{
    const BgMusicPlaylist playlist = makePlaylist();
    const QJsonObject root = parseObject(bgMusicToJson(playlist));

    QCOMPARE(root.value(QStringLiteral("version")).toInt(), 1);
    QCOMPARE(root.value(QStringLiteral("id")).toString(), playlist.id);
    QCOMPARE(root.value(QStringLiteral("title")).toString(), playlist.title);
    QCOMPARE(static_cast<qint64>(root.value(QStringLiteral("updatedAt")).toDouble()),
             playlist.updatedAt);

    const QJsonArray entries = root.value(QStringLiteral("entries")).toArray();
    QCOMPARE(entries.size(), 2);
    const QJsonObject first = entries.at(0).toObject();
    QCOMPARE(first.value(QStringLiteral("trackId")).toString(), QStringLiteral("track-1"));
    QCOMPARE(first.value(QStringLiteral("title")).toString(), QStringLiteral("Opening"));
    QCOMPARE(first.value(QStringLiteral("artist")).toString(), QStringLiteral("Band"));
    QCOMPARE(first.value(QStringLiteral("durationSeconds")).toInt(), 125);
    QCOMPARE(first.value(QStringLiteral("startMs")).toInt(), 1500);
}

void BgMusicTest::jsonClampsNegativeValues()
{
    BgMusicPlaylist playlist;
    playlist.entries = {makeEntry(QStringLiteral("track-1"), QStringLiteral("Song"),
                                  QStringLiteral("Artist"), -10, -20)};

    const QJsonArray entries =
        parseObject(bgMusicToJson(playlist)).value(QStringLiteral("entries")).toArray();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.at(0).toObject().value(QStringLiteral("durationSeconds")).toInt(), 0);
    QCOMPARE(entries.at(0).toObject().value(QStringLiteral("startMs")).toInt(), 0);

    const BgMusicPlaylist parsed = bgMusicFromJson(QStringLiteral(R"({
        "version": 1,
        "entries": [{
            "trackId": "track-1",
            "durationSeconds": -10,
            "startMs": -20
        }]
    })"));
    QCOMPARE(parsed.entries.size(), 1);
    QCOMPARE(parsed.entries.at(0).durationSeconds, 0);
    QCOMPARE(parsed.entries.at(0).startMs, 0);
}

void BgMusicTest::jsonDropsEntriesWithoutTrackId()
{
    BgMusicPlaylist playlist;
    playlist.entries = {
        makeEntry(QString(), QStringLiteral("No id")),
        makeEntry(QStringLiteral("   "), QStringLiteral("Blank id")),
        makeEntry(QStringLiteral("track-1"), QStringLiteral("Kept")),
    };

    const BgMusicPlaylist parsed = bgMusicFromJson(bgMusicToJson(playlist));
    QCOMPARE(parsed.entries.size(), 1);
    QCOMPARE(parsed.entries.at(0).trackId, QStringLiteral("track-1"));
}

void BgMusicTest::jsonDedupesTrackIdsKeepingFirst()
{
    BgMusicPlaylist playlist;
    playlist.entries = {
        makeEntry(QStringLiteral("track-1"), QStringLiteral("First"), QStringLiteral("A"), 10),
        makeEntry(QStringLiteral("track-1"), QStringLiteral("Second"), QStringLiteral("B"), 20),
        makeEntry(QStringLiteral("track-2"), QStringLiteral("Third"), QStringLiteral("C"), 30),
    };

    const BgMusicPlaylist parsed = bgMusicFromJson(bgMusicToJson(playlist));
    QCOMPARE(parsed.entries.size(), 2);
    QCOMPARE(parsed.entries.at(0).title, QStringLiteral("First"));
    QCOMPARE(parsed.entries.at(0).durationSeconds, 10);
    QCOMPARE(parsed.entries.at(1).trackId, QStringLiteral("track-2"));
}

void BgMusicTest::jsonRejectsMalformedDocuments()
{
    QVERIFY(bgMusicFromJson(QStringLiteral("{not json")).entries.isEmpty());
    QVERIFY(bgMusicFromJson(QStringLiteral("[]")).entries.isEmpty());
    QVERIFY(bgMusicFromJson(QString()).entries.isEmpty());
    QVERIFY(bgMusicFromJson(QStringLiteral(R"({"version": 1, "entries": "no"})")).entries.isEmpty());
}

void BgMusicTest::jsonRejectsFutureVersion()
{
    const BgMusicPlaylist parsed = bgMusicFromJson(QStringLiteral(R"({
        "version": 2,
        "id": "future",
        "entries": [{"trackId": "track-1"}]
    })"));
    QVERIFY(parsed.entries.isEmpty());
    QVERIFY(parsed.id.isEmpty());
}

void BgMusicTest::jsonAcceptsMissingVersion()
{
    const BgMusicPlaylist parsed = bgMusicFromJson(QStringLiteral(R"({
        "id": "legacy",
        "entries": [{"trackId": "track-1", "title": "Song"}]
    })"));
    QCOMPARE(parsed.id, QStringLiteral("legacy"));
    QCOMPARE(parsed.entries.size(), 1);
    QCOMPARE(parsed.entries.at(0).title, QStringLiteral("Song"));
}

void BgMusicTest::jsonSkipsMalformedEntries()
{
    const BgMusicPlaylist parsed = bgMusicFromJson(QStringLiteral(R"({
        "version": 1,
        "id": "playlist-1",
        "title": "Mix",
        "updatedAt": 5,
        "entries": [
            42,
            {"title": "No id"},
            {"trackId": "a", "title": "A", "durationSeconds": -5, "startMs": -2},
            {"trackId": "a", "title": "Duplicate"},
            {"trackId": "b", "title": "B", "durationSeconds": 7, "startMs": 3}
        ]
    })"));

    QCOMPARE(parsed.id, QStringLiteral("playlist-1"));
    QCOMPARE(parsed.title, QStringLiteral("Mix"));
    QCOMPARE(parsed.updatedAt, qint64(5));
    QCOMPARE(parsed.entries.size(), 2);
    QCOMPARE(parsed.entries.at(0).trackId, QStringLiteral("a"));
    QCOMPARE(parsed.entries.at(0).title, QStringLiteral("A"));
    QCOMPARE(parsed.entries.at(0).durationSeconds, 0);
    QCOMPARE(parsed.entries.at(0).startMs, 0);
    QCOMPARE(parsed.entries.at(1).trackId, QStringLiteral("b"));
    QCOMPARE(parsed.entries.at(1).durationSeconds, 7);
    QCOMPARE(parsed.entries.at(1).startMs, 3);
}

void BgMusicTest::jsonAcceptsSnakeCaseAliases()
{
    const BgMusicPlaylist parsed = bgMusicFromJson(QStringLiteral(R"({
        "version": 1,
        "playlist_id": "playlist-1",
        "updated_at": 9,
        "tracks": [{
            "id": "track-1",
            "title": "Song",
            "performer": "Artist",
            "duration_sec": 4,
            "start_ms": 5
        }]
    })"));

    QCOMPARE(parsed.id, QStringLiteral("playlist-1"));
    QCOMPARE(parsed.updatedAt, qint64(9));
    QCOMPARE(parsed.entries.size(), 1);
    QCOMPARE(parsed.entries.at(0).trackId, QStringLiteral("track-1"));
    QCOMPARE(parsed.entries.at(0).artist, QStringLiteral("Artist"));
    QCOMPARE(parsed.entries.at(0).durationSeconds, 4);
    QCOMPARE(parsed.entries.at(0).startMs, 5);
}

void BgMusicTest::m3uWritesExtinfTitleArtistAndStart()
{
    const BgMusicPlaylist playlist = makePlaylist();
    const QString m3u = bgMusicToM3u(playlist);

    QCOMPARE(m3u, QStringLiteral("#EXTM3U\n"
                                 "#EXTINF:125,Opening - Band\n"
                                 "#EXTBGSTART:1500\n"
                                 "track-1\n"
                                 "#EXTINF:0,Solo\n"
                                 "track-2"));
}

void BgMusicTest::m3uRoundTripKeepsEntries()
{
    const BgMusicPlaylist original = makePlaylist();
    const BgMusicPlaylist parsed = bgMusicFromM3u(bgMusicToM3u(original));

    QCOMPARE(parsed.entries.size(), original.entries.size());
    for (int i = 0; i < original.entries.size(); ++i) {
        QCOMPARE(parsed.entries.at(i).trackId, original.entries.at(i).trackId);
        QCOMPARE(parsed.entries.at(i).title, original.entries.at(i).title);
        QCOMPARE(parsed.entries.at(i).artist, original.entries.at(i).artist);
        QCOMPARE(parsed.entries.at(i).durationSeconds,
                 original.entries.at(i).durationSeconds);
        QCOMPARE(parsed.entries.at(i).startMs, original.entries.at(i).startMs);
    }
}

void BgMusicTest::m3uSkipsMalformedDirectives()
{
    const BgMusicPlaylist parsed = bgMusicFromM3u(QStringLiteral(
        "#EXTM3U\n"
        "#EXTINF:not-a-number,Bad - Artist\n"
        "bad\n"
        "#EXTINF:12,Kept - Artist\n"
        "#EXTBGSTART:oops\n"
        "kept\n"
        "#EXTINF\n"
        "#EXTBGSTART\n"
        "#UNKNOWN:value\n"));

    QCOMPARE(parsed.entries.size(), 2);
    QCOMPARE(parsed.entries.at(0).trackId, QStringLiteral("bad"));
    QCOMPARE(parsed.entries.at(0).durationSeconds, 0);
    QCOMPARE(parsed.entries.at(0).title, QStringLiteral("Bad"));
    QCOMPARE(parsed.entries.at(0).artist, QStringLiteral("Artist"));
    QCOMPARE(parsed.entries.at(1).trackId, QStringLiteral("kept"));
    QCOMPARE(parsed.entries.at(1).durationSeconds, 12);
    QCOMPARE(parsed.entries.at(1).startMs, 0);
}

void BgMusicTest::m3uDedupesTrackIdsKeepingFirst()
{
    const BgMusicPlaylist parsed = bgMusicFromM3u(QStringLiteral(
        "#EXTM3U\n"
        "#EXTINF:10,First - A\n"
        "dup\n"
        "#EXTINF:20,Second - B\n"
        "dup\n"
        "#EXTINF:30,Third - C\n"
        "third\n"));

    QCOMPARE(parsed.entries.size(), 2);
    QCOMPARE(parsed.entries.at(0).trackId, QStringLiteral("dup"));
    QCOMPARE(parsed.entries.at(0).title, QStringLiteral("First"));
    QCOMPARE(parsed.entries.at(0).durationSeconds, 10);
    QCOMPARE(parsed.entries.at(1).trackId, QStringLiteral("third"));
}

void BgMusicTest::m3uClampsNegativeValues()
{
    const BgMusicPlaylist parsed = bgMusicFromM3u(QStringLiteral(
        "#EXTM3U\n"
        "#EXTINF:-10,Negative - Artist\n"
        "#EXTBGSTART:-50\n"
        "negative\n"));

    QCOMPARE(parsed.entries.size(), 1);
    QCOMPARE(parsed.entries.at(0).durationSeconds, 0);
    QCOMPARE(parsed.entries.at(0).startMs, 0);
}

void BgMusicTest::m3uHandlesBomAndCrlf()
{
    QString m3u;
    m3u.append(QChar(0xFEFF));
    m3u += QStringLiteral("#EXTM3U\r\n#EXTINF:9,One - Two\r\none\r\n");
    const BgMusicPlaylist parsed = bgMusicFromM3u(m3u);

    QCOMPARE(parsed.entries.size(), 1);
    QCOMPARE(parsed.entries.at(0).trackId, QStringLiteral("one"));
    QCOMPARE(parsed.entries.at(0).title, QStringLiteral("One"));
    QCOMPARE(parsed.entries.at(0).artist, QStringLiteral("Two"));
    QCOMPARE(parsed.entries.at(0).durationSeconds, 9);
}

void BgMusicTest::m3uStartAfterTrackAppliesToThatTrack()
{
    const BgMusicPlaylist parsed = bgMusicFromM3u(QStringLiteral(
        "#EXTM3U\n"
        "#EXTINF:10,One - A\n"
        "one\n"
        "#EXTBGSTART:2500\n"
        "#EXTINF:20,Two - B\n"
        "two\n"));

    QCOMPARE(parsed.entries.size(), 2);
    QCOMPARE(parsed.entries.at(0).startMs, 2500);
    QCOMPARE(parsed.entries.at(1).startMs, 0);
}

void BgMusicTest::m3uAcceptsPlainTrackWithoutMetadata()
{
    const BgMusicPlaylist parsed =
        bgMusicFromM3u(QStringLiteral("song-a\n\nsong-b\n"));

    QCOMPARE(parsed.entries.size(), 2);
    QCOMPARE(parsed.entries.at(0).trackId, QStringLiteral("song-a"));
    QCOMPARE(parsed.entries.at(0).title, QString());
    QCOMPARE(parsed.entries.at(0).durationSeconds, 0);
    QCOMPARE(parsed.entries.at(1).trackId, QStringLiteral("song-b"));
}

void BgMusicTest::m3uEmptyInputIsEmpty()
{
    QVERIFY(bgMusicFromM3u(QString()).entries.isEmpty());
    QVERIFY(bgMusicFromM3u(QStringLiteral("#EXTM3U\n\n")).entries.isEmpty());
}

void BgMusicTest::m3uTitleWithoutArtistHasNoSeparator()
{
    const BgMusicPlaylist parsed = bgMusicFromM3u(QStringLiteral(
        "#EXTM3U\n"
        "#EXTINF:7,Song With Dash\n"
        "track-1\n"));

    QCOMPARE(parsed.entries.size(), 1);
    QCOMPARE(parsed.entries.at(0).title, QStringLiteral("Song With Dash"));
    QCOMPARE(parsed.entries.at(0).artist, QString());
}

void BgMusicTest::m3uKeepsEmptyTitleWithArtist()
{
    const BgMusicPlaylist parsed = bgMusicFromM3u(QStringLiteral(
        "#EXTM3U\n"
        "#EXTINF:3, - Artist\n"
        "track-1\n"));

    QCOMPARE(parsed.entries.size(), 1);
    QCOMPARE(parsed.entries.at(0).title, QString());
    QCOMPARE(parsed.entries.at(0).artist, QStringLiteral("Artist"));

    BgMusicPlaylist playlist;
    playlist.entries = {makeEntry(QStringLiteral("track-1"), QString(), QStringLiteral("Artist"), 3)};
    QVERIFY(bgMusicToM3u(playlist).contains(QStringLiteral("#EXTINF:3, - Artist")));
}

QTEST_GUILESS_MAIN(BgMusicTest)
#include "tst_bg_music.moc"
