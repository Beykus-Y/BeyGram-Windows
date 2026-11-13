#include "telegram/music/MusicModels.h"

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject fileObject(int id, const QString& uniqueId, const QString& localPath)
{
    QJsonObject remote;
    if (!uniqueId.isEmpty())
        remote.insert(QStringLiteral("unique_id"), uniqueId);

    QJsonObject local;
    if (!localPath.isEmpty())
        local.insert(QStringLiteral("path"), localPath);

    QJsonObject file{
        {QStringLiteral("id"), id},
    };
    if (!remote.isEmpty())
        file.insert(QStringLiteral("remote"), remote);
    if (!local.isEmpty())
        file.insert(QStringLiteral("local"), local);
    return file;
}

} // namespace

class MusicModelsTest final : public QObject {
    Q_OBJECT

private slots:
    void audioParsesBareTrack();
    void audioParsesMessageWrapper();
    void audioParsesContentWrapper();
    void audioPrefersMessageIdOverFileUniqueId();
    void audioPrefersExplicitArtistAndAlbumIds();
    void audioFallsBackToFileUniqueId();
    void audioTrimsIds();
    void audioKeepsEmptyIdForCallerToDrop();
    void audioClampsNegativeDurationAndTrackNumber();
    void audioKeepsDefaultsForMissingFields();
    void audioRejectsOutOfRangeFileId();
    void audioReadsFileIdAndLocalPathAliases();

    void albumYear_data();
    void albumYear();

    void dedupedTrackIdsKeepFirstPosition();
    void dedupedTrackIdsTrimAndDropEmpty();
    void dedupedTrackIdsOfEmptyListAreEmpty();

    void durationText_data();
    void durationText();
};

void MusicModelsTest::audioParsesBareTrack()
{
    const QJsonObject audio{
        {QStringLiteral("@type"), QStringLiteral("audio")},
        {QStringLiteral("title"), QStringLiteral("Get Lucky")},
        {QStringLiteral("performer"), QStringLiteral("Daft Punk")},
        {QStringLiteral("album"), QStringLiteral("Random Access Memories")},
        {QStringLiteral("duration"), 369},
        {QStringLiteral("track_number"), 1},
        {QStringLiteral("audio"),
         fileObject(4242, QStringLiteral("unique-42"),
                    QStringLiteral("C:/music/get-lucky.mp3"))},
    };

    const MusicTrackModel track = musicTrackFromAudio(audio);

    QCOMPARE(track.id, QStringLiteral("unique-42"));
    QCOMPARE(track.title, QStringLiteral("Get Lucky"));
    QCOMPARE(track.artistId, QStringLiteral("Daft Punk"));
    QCOMPARE(track.albumId, QStringLiteral("Random Access Memories"));
    QCOMPARE(track.durationSeconds, 369);
    QCOMPARE(track.fileId, 4242);
    QCOMPARE(track.localPath, QStringLiteral("C:/music/get-lucky.mp3"));
    QCOMPARE(track.trackNumber, 1);
}

void MusicModelsTest::audioParsesMessageWrapper()
{
    const QJsonObject message{
        {QStringLiteral("@type"), QStringLiteral("message")},
        {QStringLiteral("id"), 77},
        {QStringLiteral("content"),
         QJsonObject{
             {QStringLiteral("@type"), QStringLiteral("messageAudio")},
             {QStringLiteral("audio"),
              QJsonObject{
                  {QStringLiteral("title"), QStringLiteral("Song")},
                  {QStringLiteral("performer"), QStringLiteral("Band")},
                  {QStringLiteral("duration"), 200},
              }},
         }},
    };

    const MusicTrackModel track = musicTrackFromAudio(message);

    QCOMPARE(track.id, QStringLiteral("77"));
    QCOMPARE(track.title, QStringLiteral("Song"));
    QCOMPARE(track.artistId, QStringLiteral("Band"));
    QCOMPARE(track.durationSeconds, 200);
    QCOMPARE(track.fileId, 0);
}

void MusicModelsTest::audioParsesContentWrapper()
{
    const QJsonObject content{
        {QStringLiteral("@type"), QStringLiteral("messageAudio")},
        {QStringLiteral("audio"),
         QJsonObject{
             {QStringLiteral("title"), QStringLiteral("Song")},
             {QStringLiteral("album"), QStringLiteral("Album")},
             {QStringLiteral("duration"), 90},
         }},
    };

    const MusicTrackModel track = musicTrackFromAudio(content);

    QVERIFY(track.id.isEmpty());
    QCOMPARE(track.title, QStringLiteral("Song"));
    QCOMPARE(track.albumId, QStringLiteral("Album"));
    QCOMPARE(track.durationSeconds, 90);
}

void MusicModelsTest::audioPrefersMessageIdOverFileUniqueId()
{
    const QJsonObject message{
        {QStringLiteral("@type"), QStringLiteral("message")},
        {QStringLiteral("id"), 88},
        {QStringLiteral("content"),
         QJsonObject{
             {QStringLiteral("@type"), QStringLiteral("messageAudio")},
             {QStringLiteral("audio"),
              QJsonObject{
                  {QStringLiteral("title"), QStringLiteral("Song")},
                  {QStringLiteral("audio"),
                   fileObject(555, QStringLiteral("nested-unique"), QString())},
              }},
         }},
    };

    const MusicTrackModel track = musicTrackFromAudio(message);

    QCOMPARE(track.id, QStringLiteral("88"));
    QCOMPARE(track.fileId, 555);
}

void MusicModelsTest::audioPrefersExplicitArtistAndAlbumIds()
{
    const QJsonObject audio{
        {QStringLiteral("title"), QStringLiteral("Song")},
        {QStringLiteral("performer"), QStringLiteral("Tag Artist")},
        {QStringLiteral("album"), QStringLiteral("Tag Album")},
        {QStringLiteral("artist_id"), QStringLiteral("  artist-9  ")},
        {QStringLiteral("album_id"), QStringLiteral("  album-3  ")},
    };

    const MusicTrackModel track = musicTrackFromAudio(audio);

    QCOMPARE(track.artistId, QStringLiteral("artist-9"));
    QCOMPARE(track.albumId, QStringLiteral("album-3"));
}

void MusicModelsTest::audioFallsBackToFileUniqueId()
{
    const QJsonObject audio{
        {QStringLiteral("title"), QStringLiteral("Song")},
        {QStringLiteral("file"),
         fileObject(0, QStringLiteral("file-unique"), QString())},
    };

    const MusicTrackModel track = musicTrackFromAudio(audio);

    QCOMPARE(track.id, QStringLiteral("file-unique"));
}

void MusicModelsTest::audioTrimsIds()
{
    const QJsonObject audio{
        {QStringLiteral("id"), QStringLiteral("  track-1  ")},
        {QStringLiteral("title"), QStringLiteral("Song")},
        {QStringLiteral("artist_id"), QStringLiteral("  artist-9  ")},
        {QStringLiteral("album_id"), QStringLiteral("  album-3  ")},
    };

    const MusicTrackModel track = musicTrackFromAudio(audio);

    QCOMPARE(track.id, QStringLiteral("track-1"));
    QCOMPARE(track.artistId, QStringLiteral("artist-9"));
    QCOMPARE(track.albumId, QStringLiteral("album-3"));
}

void MusicModelsTest::audioKeepsEmptyIdForCallerToDrop()
{
    const QJsonObject audio{
        {QStringLiteral("title"), QStringLiteral("Song")},
    };

    const MusicTrackModel track = musicTrackFromAudio(audio);

    QVERIFY(track.id.isEmpty());
    QCOMPARE(track.title, QStringLiteral("Song"));
}

void MusicModelsTest::audioClampsNegativeDurationAndTrackNumber()
{
    const QJsonObject audio{
        {QStringLiteral("title"), QStringLiteral("Song")},
        {QStringLiteral("duration"), -5},
        {QStringLiteral("track_number"), -2},
    };

    const MusicTrackModel track = musicTrackFromAudio(audio);

    QCOMPARE(track.durationSeconds, 0);
    QCOMPARE(track.trackNumber, 0);
}

void MusicModelsTest::audioKeepsDefaultsForMissingFields()
{
    const MusicTrackModel track = musicTrackFromAudio(QJsonObject{});

    QVERIFY(track.id.isEmpty());
    QVERIFY(track.title.isEmpty());
    QVERIFY(track.artistId.isEmpty());
    QVERIFY(track.albumId.isEmpty());
    QCOMPARE(track.durationSeconds, 0);
    QCOMPARE(track.fileId, 0);
    QVERIFY(track.localPath.isEmpty());
    QCOMPARE(track.trackNumber, 0);
}

void MusicModelsTest::audioRejectsOutOfRangeFileId()
{
    const QJsonObject audio{
        {QStringLiteral("title"), QStringLiteral("Song")},
        {QStringLiteral("audio"),
         QJsonObject{
             {QStringLiteral("id"), 5000000000LL},
         }},
    };

    QCOMPARE(musicTrackFromAudio(audio).fileId, 0);
}

void MusicModelsTest::audioReadsFileIdAndLocalPathAliases()
{
    const QJsonObject audio{
        {QStringLiteral("title"), QStringLiteral("Song")},
        {QStringLiteral("file_id"), QStringLiteral("321")},
        {QStringLiteral("file"),
         QJsonObject{
             {QStringLiteral("local"),
              QJsonObject{
                  {QStringLiteral("path"), QStringLiteral("D:/music/song.mp3")},
              }},
         }},
    };

    const MusicTrackModel track = musicTrackFromAudio(audio);

    QCOMPARE(track.fileId, 321);
    QCOMPARE(track.localPath, QStringLiteral("D:/music/song.mp3"));
}

void MusicModelsTest::albumYear_data()
{
    QTest::addColumn<int>("input");
    QTest::addColumn<int>("expected");

    QTest::newRow("unknown") << 0 << 0;
    QTest::newRow("lower bound") << 1000 << 1000;
    QTest::newRow("ordinary") << 1999 << 1999;
    QTest::newRow("upper bound") << 2100 << 2100;
    QTest::newRow("below range") << 999 << 0;
    QTest::newRow("above range") << 2101 << 0;
    QTest::newRow("negative") << -1 << 0;
    QTest::newRow("too small") << 123 << 0;
}

void MusicModelsTest::albumYear()
{
    QFETCH(int, input);
    QFETCH(int, expected);

    QCOMPARE(musicAlbumYear(input), expected);
}

void MusicModelsTest::dedupedTrackIdsKeepFirstPosition()
{
    QCOMPARE(musicDedupedTrackIds(
                 QVector<QString>{QStringLiteral("a"), QStringLiteral("b"),
                                  QStringLiteral("a"), QStringLiteral("c"),
                                  QStringLiteral("b")}),
             (QVector<QString>{QStringLiteral("a"), QStringLiteral("b"),
                               QStringLiteral("c")}));
    QCOMPARE(musicDedupedTrackIds(
                 QVector<QString>{QStringLiteral("b"), QStringLiteral("a"),
                                  QStringLiteral("b")}),
             (QVector<QString>{QStringLiteral("b"), QStringLiteral("a")}));
}

void MusicModelsTest::dedupedTrackIdsTrimAndDropEmpty()
{
    QCOMPARE(musicDedupedTrackIds(
                 QVector<QString>{QStringLiteral(" a "), QStringLiteral("a"),
                                  QString(), QStringLiteral("   "),
                                  QStringLiteral("\t"), QStringLiteral(" b ")}),
             (QVector<QString>{QStringLiteral("a"), QStringLiteral("b")}));
}

void MusicModelsTest::dedupedTrackIdsOfEmptyListAreEmpty()
{
    QVERIFY(musicDedupedTrackIds(QVector<QString>{}).isEmpty());
}

void MusicModelsTest::durationText_data()
{
    QTest::addColumn<int>("seconds");
    QTest::addColumn<QString>("expected");

    QTest::newRow("negative") << -1 << QStringLiteral("0:00");
    QTest::newRow("zero") << 0 << QStringLiteral("0:00");
    QTest::newRow("nine seconds") << 9 << QStringLiteral("0:09");
    QTest::newRow("fifty nine seconds") << 59 << QStringLiteral("0:59");
    QTest::newRow("one minute") << 60 << QStringLiteral("1:00");
    QTest::newRow("one minute one") << 61 << QStringLiteral("1:01");
    QTest::newRow("nine minutes") << 599 << QStringLiteral("9:59");
    QTest::newRow("ten minutes") << 600 << QStringLiteral("10:00");
    QTest::newRow("last minute") << 3599 << QStringLiteral("59:59");
    QTest::newRow("one hour") << 3600 << QStringLiteral("1:00:00");
    QTest::newRow("one hour one") << 3601 << QStringLiteral("1:00:01");
    QTest::newRow("one hour one minute one") << 3661 << QStringLiteral("1:01:01");
    QTest::newRow("last second") << 86399 << QStringLiteral("23:59:59");
    QTest::newRow("one day") << 86400 << QStringLiteral("24:00:00");
    QTest::newRow("int max") << 2147483647 << QStringLiteral("596523:14:07");
}

void MusicModelsTest::durationText()
{
    QFETCH(int, seconds);
    QFETCH(QString, expected);

    QCOMPARE(musicDurationText(seconds), expected);
}

QTEST_GUILESS_MAIN(MusicModelsTest)
#include "tst_music_models.moc"
