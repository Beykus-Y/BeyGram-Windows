#include "telegram/music/MusicLibraryEdits.h"

#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

MusicTrackModel track(const QString& id, const QString& title = {})
{
    return MusicTrackModel{id, title};
}

MusicAlbum album(const QString& id, const QString& title = {})
{
    return MusicAlbum{id, title};
}

MusicArtist artist(const QString& id, const QString& name = {})
{
    return MusicArtist{id, name};
}

MusicPlaylist playlist(const QString& id, const QString& title = {},
                       const QVector<QString>& trackIds = {},
                       const QString& description = {})
{
    return MusicPlaylist{id, title, description, trackIds};
}

bool sameTrack(const MusicTrackModel& left, const MusicTrackModel& right)
{
    return left.id == right.id
        && left.title == right.title
        && left.artistId == right.artistId
        && left.albumId == right.albumId
        && left.durationSeconds == right.durationSeconds
        && left.fileId == right.fileId
        && left.localPath == right.localPath
        && left.trackNumber == right.trackNumber;
}

bool sameTracks(const QVector<MusicTrackModel>& left, const QVector<MusicTrackModel>& right)
{
    if (left.size() != right.size())
        return false;
    for (int i = 0; i < left.size(); ++i) {
        if (!sameTrack(left.at(i), right.at(i)))
            return false;
    }
    return true;
}

MusicLibraryData sampleLibrary()
{
    MusicLibraryData data;
    data.tracks = {track(QStringLiteral("t1"), QStringLiteral("One")),
                   track(QStringLiteral("t2"), QStringLiteral("Two")),
                   track(QStringLiteral("t3"), QStringLiteral("Three"))};
    data.albums = {album(QStringLiteral("al1"), QStringLiteral("Album"))};
    data.artists = {artist(QStringLiteral("ar1"), QStringLiteral("Artist"))};
    data.playlists = {
        playlist(QStringLiteral("p1"), QStringLiteral("Mix"),
                 {QStringLiteral("t1"), QStringLiteral("t2")}),
        playlist(QStringLiteral("p2"), QStringLiteral("Other"), {QStringLiteral("t3")}),
    };
    data.favorites = {QStringLiteral("t1")};
    return data;
}

} // namespace

class MusicLibraryEditsTest final : public QObject {
    Q_OBJECT

private slots:
    void addTrackAddsTrimmedId();
    void addTrackRejectsEmptyId();
    void addTrackRejectsDuplicateKeepingFirst();

    void removeTrackRemovesTrackAndReferences();
    void removeTrackRejectsUnknown();

    void toggleFavoriteAddsAndRemoves();
    void toggleFavoriteRemovesDuplicates();
    void toggleFavoriteRejectsUnknownTrack();

    void createPlaylistAddsNormalizedPlaylist();
    void createPlaylistRejectsInvalidPlaylist();
    void createPlaylistAcceptsEightyCodePoints();

    void addTrackToPlaylistAppends();
    void addTrackToPlaylistRejectsInvalid();

    void removeTrackFromPlaylistRemoves();
    void removeTrackFromPlaylistRejectsMissing();

    void renamePlaylistTrimsAndStores();
    void renamePlaylistRejectsInvalid();

    void deletePlaylistRemovesOnlyIt();
    void deletePlaylistRejectsUnknown();
};

void MusicLibraryEditsTest::addTrackAddsTrimmedId()
{
    MusicLibraryData data;
    MusicTrackModel input = track(QStringLiteral(" t1 "), QStringLiteral("Song"));
    input.artistId = QStringLiteral(" ar1 ");
    input.albumId = QStringLiteral(" al1 ");

    QVERIFY(addTrack(data, input));
    QCOMPARE(data.tracks.size(), 1);
    QCOMPARE(data.tracks.first().id, QStringLiteral("t1"));
    QCOMPARE(data.tracks.first().title, QStringLiteral("Song"));
    QCOMPARE(data.tracks.first().artistId, QStringLiteral("ar1"));
    QCOMPARE(data.tracks.first().albumId, QStringLiteral("al1"));
}

void MusicLibraryEditsTest::addTrackRejectsEmptyId()
{
    MusicLibraryData data;

    QVERIFY(!addTrack(data, track(QStringLiteral("   "), QStringLiteral("Song"))));
    QVERIFY(!addTrack(data, track(QString(), QStringLiteral("Song"))));
    QVERIFY(data.tracks.isEmpty());
}

void MusicLibraryEditsTest::addTrackRejectsDuplicateKeepingFirst()
{
    MusicLibraryData data;
    QVERIFY(addTrack(data, track(QStringLiteral("t1"), QStringLiteral("First"))));

    QVERIFY(!addTrack(data, track(QStringLiteral("t1"), QStringLiteral("Second"))));
    QVERIFY(!addTrack(data, track(QStringLiteral(" t1 "), QStringLiteral("Third"))));

    QCOMPARE(data.tracks.size(), 1);
    QCOMPARE(data.tracks.first().title, QStringLiteral("First"));
}

void MusicLibraryEditsTest::removeTrackRemovesTrackAndReferences()
{
    MusicLibraryData data = sampleLibrary();

    QVERIFY(removeTrack(data, QStringLiteral(" t1 ")));

    QCOMPARE(data.tracks.size(), 2);
    QCOMPARE(data.tracks.at(0).id, QStringLiteral("t2"));
    QCOMPARE(data.tracks.at(1).id, QStringLiteral("t3"));

    QCOMPARE(data.playlists.at(0).trackIds, (QVector<QString>{QStringLiteral("t2")}));
    QCOMPARE(data.playlists.at(1).trackIds, (QVector<QString>{QStringLiteral("t3")}));
    QVERIFY(data.favorites.isEmpty());
}

void MusicLibraryEditsTest::removeTrackRejectsUnknown()
{
    MusicLibraryData data = sampleLibrary();
    const MusicLibraryData before = data;

    QVERIFY(!removeTrack(data, QStringLiteral("missing")));
    QVERIFY(!removeTrack(data, QString()));
    QVERIFY(data == before);
}

void MusicLibraryEditsTest::toggleFavoriteAddsAndRemoves()
{
    MusicLibraryData data = sampleLibrary();

    QVERIFY(toggleFavorite(data, QStringLiteral(" t2 ")));
    QCOMPARE(data.favorites,
             (QVector<QString>{QStringLiteral("t1"), QStringLiteral("t2")}));

    QVERIFY(toggleFavorite(data, QStringLiteral("t1")));
    QCOMPARE(data.favorites, (QVector<QString>{QStringLiteral("t2")}));

    QVERIFY(toggleFavorite(data, QStringLiteral("t2")));
    QVERIFY(data.favorites.isEmpty());
}

void MusicLibraryEditsTest::toggleFavoriteRemovesDuplicates()
{
    MusicLibraryData data = sampleLibrary();
    data.favorites = {QStringLiteral("t1"), QStringLiteral(" t1 ")};

    QVERIFY(toggleFavorite(data, QStringLiteral("t1")));
    QVERIFY(data.favorites.isEmpty());
}

void MusicLibraryEditsTest::toggleFavoriteRejectsUnknownTrack()
{
    MusicLibraryData data = sampleLibrary();
    const MusicLibraryData before = data;

    QVERIFY(!toggleFavorite(data, QStringLiteral("missing")));
    QVERIFY(!toggleFavorite(data, QString()));
    QVERIFY(data == before);
}

void MusicLibraryEditsTest::createPlaylistAddsNormalizedPlaylist()
{
    MusicLibraryData data = sampleLibrary();

    MusicPlaylist input = playlist(
        QStringLiteral(" p3 "), QStringLiteral("  New Mix  "),
        {QStringLiteral("t1"), QStringLiteral("missing"), QStringLiteral("t1"),
         QStringLiteral(" t2 ")},
        QStringLiteral("Description"));
    input.createdAt = 100;
    input.updatedAt = 200;

    QVERIFY(createPlaylist(data, input));

    QCOMPARE(data.playlists.size(), 3);
    const MusicPlaylist& created = data.playlists.last();
    QCOMPARE(created.id, QStringLiteral("p3"));
    QCOMPARE(created.title, QStringLiteral("New Mix"));
    QCOMPARE(created.description, QStringLiteral("Description"));
    QCOMPARE(created.trackIds,
             (QVector<QString>{QStringLiteral("t1"), QStringLiteral("t2")}));
    QCOMPARE(created.createdAt, qint64(100));
    QCOMPARE(created.updatedAt, qint64(200));
}

void MusicLibraryEditsTest::createPlaylistRejectsInvalidPlaylist()
{
    MusicLibraryData data = sampleLibrary();
    const MusicLibraryData before = data;

    QVERIFY(!createPlaylist(data, playlist(QString(), QStringLiteral("Title"))));
    QVERIFY(!createPlaylist(data, playlist(QStringLiteral("   "), QStringLiteral("Title"))));
    QVERIFY(!createPlaylist(data, playlist(QStringLiteral("p3"), QStringLiteral("   "))));
    QVERIFY(!createPlaylist(
        data, playlist(QStringLiteral("p3"), QString(81, QLatin1Char('a')))));
    QVERIFY(!createPlaylist(data, playlist(QStringLiteral("p1"), QStringLiteral("Duplicate"))));

    QVERIFY(data == before);
}

void MusicLibraryEditsTest::createPlaylistAcceptsEightyCodePoints()
{
    MusicLibraryData data = sampleLibrary();

    // Each emoji is one code point but two UTF-16 units, so the limit must be
    // counted in code points.
    const QString emoji = QString::fromUtf8("\xF0\x9F\x98\x80");
    const QString title = emoji.repeated(80);

    QVERIFY(createPlaylist(data, playlist(QStringLiteral("p3"), title)));
    QCOMPARE(data.playlists.last().title, title);

    QVERIFY(!createPlaylist(data, playlist(QStringLiteral("p4"), emoji.repeated(81))));
    QCOMPARE(data.playlists.size(), 3);
}

void MusicLibraryEditsTest::addTrackToPlaylistAppends()
{
    MusicLibraryData data = sampleLibrary();

    QVERIFY(addTrackToPlaylist(data, QStringLiteral(" p1 "), QStringLiteral(" t3 ")));
    QCOMPARE(data.playlists.at(0).trackIds,
             (QVector<QString>{QStringLiteral("t1"), QStringLiteral("t2"),
                               QStringLiteral("t3")}));
    QCOMPARE(data.playlists.at(1).trackIds, (QVector<QString>{QStringLiteral("t3")}));
}

void MusicLibraryEditsTest::addTrackToPlaylistRejectsInvalid()
{
    MusicLibraryData data = sampleLibrary();
    const MusicLibraryData before = data;

    // Already in the playlist.
    QVERIFY(!addTrackToPlaylist(data, QStringLiteral("p1"), QStringLiteral("t1")));
    // Unknown track.
    QVERIFY(!addTrackToPlaylist(data, QStringLiteral("p1"), QStringLiteral("missing")));
    // Unknown or blank playlist.
    QVERIFY(!addTrackToPlaylist(data, QStringLiteral("missing"), QStringLiteral("t3")));
    QVERIFY(!addTrackToPlaylist(data, QString(), QStringLiteral("t3")));
    // Blank track id.
    QVERIFY(!addTrackToPlaylist(data, QStringLiteral("p1"), QStringLiteral("   ")));

    QVERIFY(data == before);
}

void MusicLibraryEditsTest::removeTrackFromPlaylistRemoves()
{
    MusicLibraryData data = sampleLibrary();

    QVERIFY(removeTrackFromPlaylist(data, QStringLiteral(" p1 "), QStringLiteral(" t1 ")));
    QCOMPARE(data.playlists.at(0).trackIds, (QVector<QString>{QStringLiteral("t2")}));
    QCOMPARE(data.playlists.at(1).trackIds, (QVector<QString>{QStringLiteral("t3")}));
}

void MusicLibraryEditsTest::removeTrackFromPlaylistRejectsMissing()
{
    MusicLibraryData data = sampleLibrary();
    const MusicLibraryData before = data;

    QVERIFY(!removeTrackFromPlaylist(data, QStringLiteral("missing"),
                                     QStringLiteral("t1")));
    QVERIFY(!removeTrackFromPlaylist(data, QStringLiteral("p1"),
                                     QStringLiteral("t3")));
    QVERIFY(!removeTrackFromPlaylist(data, QStringLiteral("p1"), QString()));

    QVERIFY(data == before);
}

void MusicLibraryEditsTest::renamePlaylistTrimsAndStores()
{
    MusicLibraryData data = sampleLibrary();

    QVERIFY(renamePlaylist(data, QStringLiteral(" p1 "), QStringLiteral("  New Name  ")));
    QCOMPARE(data.playlists.at(0).title, QStringLiteral("New Name"));
    QCOMPARE(data.playlists.at(1).title, QStringLiteral("Other"));
}

void MusicLibraryEditsTest::renamePlaylistRejectsInvalid()
{
    MusicLibraryData data = sampleLibrary();
    const MusicLibraryData before = data;

    QVERIFY(!renamePlaylist(data, QStringLiteral("p1"), QStringLiteral("   ")));
    QVERIFY(!renamePlaylist(data, QStringLiteral("p1"), QString(81, QLatin1Char('a'))));
    QVERIFY(!renamePlaylist(data, QStringLiteral("missing"), QStringLiteral("New")));
    // Same title after trimming is not a change.
    QVERIFY(!renamePlaylist(data, QStringLiteral("p1"), QStringLiteral(" Mix ")));

    QVERIFY(data == before);
}

void MusicLibraryEditsTest::deletePlaylistRemovesOnlyIt()
{
    MusicLibraryData data = sampleLibrary();
    const QVector<MusicTrackModel> tracksBefore = data.tracks;
    const QVector<QString> favoritesBefore = data.favorites;

    QVERIFY(deletePlaylist(data, QStringLiteral(" p1 ")));

    QCOMPARE(data.playlists.size(), 1);
    QCOMPARE(data.playlists.first().id, QStringLiteral("p2"));
    QVERIFY(sameTracks(data.tracks, tracksBefore));
    QCOMPARE(data.favorites, favoritesBefore);
    QCOMPARE(data.albums.size(), 1);
    QCOMPARE(data.artists.size(), 1);
}

void MusicLibraryEditsTest::deletePlaylistRejectsUnknown()
{
    MusicLibraryData data = sampleLibrary();
    const MusicLibraryData before = data;

    QVERIFY(!deletePlaylist(data, QStringLiteral("missing")));
    QVERIFY(!deletePlaylist(data, QString()));
    QVERIFY(data == before);
}

QTEST_GUILESS_MAIN(MusicLibraryEditsTest)

#include "tst_music_library_edits.moc"
