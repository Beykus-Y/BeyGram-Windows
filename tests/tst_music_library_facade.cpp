#include "ui/MusicLibraryFacade.h"

#include "telegram/music/MusicLibraryStore.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QtTest>

#include <initializer_list>

using namespace beygram::ui;
using namespace beygram::telegram;

namespace {

QString libraryPath(const QTemporaryDir& dir)
{
    return QDir(dir.path()).filePath(QStringLiteral("music-library.json"));
}

QStringList strings(std::initializer_list<const char*> values)
{
    QStringList result;
    for (const char* value : values)
        result.append(QString::fromLatin1(value));
    return result;
}

MusicLibraryData readLibrary(const QString& path)
{
    return MusicLibraryStore(path).load();
}

QVariantMap findRow(const QVariantList& rows, const QString& id)
{
    for (const QVariant& entry : rows) {
        const QVariantMap row = entry.toMap();
        if (row.value(QStringLiteral("id")).toString() == id)
            return row;
    }
    return {};
}

void addTrack(MusicLibraryFacade& facade, const QString& id, const QString& title,
              const QString& artistId = QStringLiteral("artist-1"),
              const QString& albumId = QStringLiteral("album-1"),
              int durationSeconds = 180, int fileId = 11)
{
    facade.addTrack(id, title, artistId, albumId, durationSeconds, fileId);
}

MusicLibraryData sampleLibrary()
{
    MusicLibraryData data;
    data.tracks = {
        MusicTrackModel{QStringLiteral("t1"), QStringLiteral("First"),
                        QStringLiteral("ar1"), QStringLiteral("al1"), 65, 7, {}, 1},
        MusicTrackModel{QStringLiteral("t2"), QStringLiteral("Second"),
                        QStringLiteral("ar2"), {}, 3725, 8, {}, 0},
    };
    data.playlists = {
        MusicPlaylist{QStringLiteral("p1"), QStringLiteral("Road trip"), {},
                      {QStringLiteral("t1")}, 1000, 2000},
    };
    data.favorites = {QStringLiteral("t2")};
    return data;
}

} // namespace

class MusicLibraryFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();

    void loadReadsTheStoredLibrary();
    void loadOfAMissingFileIsEmpty();
    void loadOfTheSameLibraryIsSilent();

    void addTrackPersistsAndAnnounces();
    void addTrackTrimsIdsAndClampsNumbers();
    void addTrackRejectsBlankAndRepeatedIds();
    void addTrackWithoutAPathStaysInMemory();

    void removeTrackDropsPlaylistAndFavoriteReferences();
    void removeUnknownTrackIsSilent();

    void toggleFavoriteAddsAndRemoves();
    void toggleFavoriteIgnoresUnknownTracks();

    void createPlaylistAddsACard();
    void createPlaylistRejectsInvalidTitlesAndRepeatedIds();
    void deletePlaylistKeepsTracksAndFavorites();

    void addTrackToPlaylistAppendsOnce();
    void addTrackToPlaylistIgnoresUnknownIds();

    void searchMatchesTitleArtistAndAlbum();
    void searchIsCaseInsensitiveAndTrims();
    void searchKeepsStoredOrderAndReturnsIds();
    void searchTracksCarriesRowFields();

    void setQueryStoresTheTextVerbatim();
    void queryIsKeptByLoad();

    void clearEmptiesEverythingAndSaves();
    void clearOnEmptyIsSilent();

    void reloadRestoresTheSavedLibrary();
};

void MusicLibraryFacadeTest::startsEmpty()
{
    MusicLibraryFacade facade;
    QSignalSpy changed(&facade, &MusicLibraryFacade::changed);

    QCOMPARE(facade.trackCount(), 0);
    QCOMPARE(facade.playlistCount(), 0);
    QVERIFY2(facade.trackTitles().isEmpty(), "no track titles by default");
    QVERIFY2(facade.trackIds().isEmpty(), "no track ids by default");
    QVERIFY2(facade.playlistTitles().isEmpty(), "no playlist titles by default");
    QVERIFY2(facade.query().isEmpty(), "no query by default");
    QVERIFY2(facade.tracks().isEmpty(), "no track rows by default");
    QVERIFY2(facade.playlists().isEmpty(), "no playlist cards by default");
    QVERIFY2(facade.favoriteIds().isEmpty(), "no favorites by default");
    QVERIFY2(facade.search(QString()).isEmpty(), "nothing to search");
    QCOMPARE(facade.playlistTrackCount(QStringLiteral("p1")), 0);
    QCOMPARE(changed.count(), 0);
}

void MusicLibraryFacadeTest::loadReadsTheStoredLibrary()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = libraryPath(dir);
    QVERIFY(MusicLibraryStore(path).save(sampleLibrary()));

    MusicLibraryFacade facade;
    facade.setStoragePath(path);
    QSignalSpy changed(&facade, &MusicLibraryFacade::changed);
    facade.load();

    QCOMPARE(changed.count(), 1);
    QCOMPARE(facade.trackCount(), 2);
    QCOMPARE(facade.trackIds(), strings({"t1", "t2"}));
    QCOMPARE(facade.trackTitles(), strings({"First", "Second"}));
    QCOMPARE(facade.playlistCount(), 1);
    QCOMPARE(facade.playlistTitles(), strings({"Road trip"}));
    QCOMPARE(facade.favoriteIds(), strings({"t2"}));
    QVERIFY(facade.isFavorite(QStringLiteral("t2")));
    QVERIFY(!facade.isFavorite(QStringLiteral("t1")));
    QCOMPARE(facade.playlistTrackCount(QStringLiteral("p1")), 1);

    const QVariantMap row = findRow(facade.tracks(), QStringLiteral("t2"));
    QVERIFY(!row.isEmpty());
    QCOMPARE(row.value(QStringLiteral("title")).toString(), QStringLiteral("Second"));
    QCOMPARE(row.value(QStringLiteral("durationSeconds")).toInt(), 3725);
    QCOMPARE(row.value(QStringLiteral("duration")).toString(),
             QStringLiteral("1:02:05"));
    QCOMPARE(row.value(QStringLiteral("favorite")).toBool(), true);
}

void MusicLibraryFacadeTest::loadOfAMissingFileIsEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    MusicLibraryFacade facade;
    facade.setStoragePath(QDir(dir.path()).filePath(QStringLiteral("missing.json")));
    QSignalSpy changed(&facade, &MusicLibraryFacade::changed);
    facade.load();

    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.trackCount(), 0);
    QCOMPARE(facade.playlistCount(), 0);
}

void MusicLibraryFacadeTest::loadOfTheSameLibraryIsSilent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = libraryPath(dir);
    QVERIFY(MusicLibraryStore(path).save(sampleLibrary()));

    MusicLibraryFacade facade;
    facade.setStoragePath(path);
    facade.load();

    QSignalSpy changed(&facade, &MusicLibraryFacade::changed);
    facade.load();

    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.trackCount(), 2);
}

void MusicLibraryFacadeTest::addTrackPersistsAndAnnounces()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = libraryPath(dir);

    MusicLibraryFacade facade;
    facade.setStoragePath(path);
    facade.load();
    QSignalSpy changed(&facade, &MusicLibraryFacade::changed);

    facade.addTrack(QStringLiteral(" t1 "), QStringLiteral("Midnight City"),
                    QStringLiteral(" m83 "), QStringLiteral(" hurry-up "), 245, 42);

    QCOMPARE(changed.count(), 1);
    QCOMPARE(facade.trackCount(), 1);
    QCOMPARE(facade.trackIds(), strings({"t1"}));
    QCOMPARE(facade.trackTitles(), strings({"Midnight City"}));

    const MusicLibraryData stored = readLibrary(path);
    QCOMPARE(stored.tracks.size(), 1);
    QCOMPARE(stored.tracks.at(0).id, QStringLiteral("t1"));
    QCOMPARE(stored.tracks.at(0).title, QStringLiteral("Midnight City"));
    QCOMPARE(stored.tracks.at(0).artistId, QStringLiteral("m83"));
    QCOMPARE(stored.tracks.at(0).albumId, QStringLiteral("hurry-up"));
    QCOMPARE(stored.tracks.at(0).durationSeconds, 245);
    QCOMPARE(stored.tracks.at(0).fileId, 42);
}

void MusicLibraryFacadeTest::addTrackTrimsIdsAndClampsNumbers()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = libraryPath(dir);

    MusicLibraryFacade facade;
    facade.setStoragePath(path);
    facade.addTrack(QStringLiteral(" t1 "), QStringLiteral("Song"),
                    QStringLiteral(" ar "), QStringLiteral(" al "), -12, -5);

    QCOMPARE(facade.trackCount(), 1);
    const QVariantMap row = findRow(facade.tracks(), QStringLiteral("t1"));
    QVERIFY(!row.isEmpty());
    QCOMPARE(row.value(QStringLiteral("artistId")).toString(), QStringLiteral("ar"));
    QCOMPARE(row.value(QStringLiteral("albumId")).toString(), QStringLiteral("al"));
    QCOMPARE(row.value(QStringLiteral("durationSeconds")).toInt(), 0);
    QCOMPARE(row.value(QStringLiteral("duration")).toString(),
             QStringLiteral("0:00"));

    const MusicLibraryData stored = readLibrary(path);
    QCOMPARE(stored.tracks.size(), 1);
    QCOMPARE(stored.tracks.at(0).id, QStringLiteral("t1"));
    QCOMPARE(stored.tracks.at(0).durationSeconds, 0);
    QCOMPARE(stored.tracks.at(0).fileId, 0);
}

void MusicLibraryFacadeTest::addTrackRejectsBlankAndRepeatedIds()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = libraryPath(dir);

    MusicLibraryFacade facade;
    facade.setStoragePath(path);
    QSignalSpy changed(&facade, &MusicLibraryFacade::changed);

    addTrack(facade, QStringLiteral("   "), QStringLiteral("No id"));
    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.trackCount(), 0);
    QVERIFY2(!QFile::exists(path), "a refused edit writes no file");

    addTrack(facade, QStringLiteral("t1"), QStringLiteral("First"));
    QCOMPARE(changed.count(), 1);

    addTrack(facade, QStringLiteral(" t1 "), QStringLiteral("Second"));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(facade.trackCount(), 1);
    QCOMPARE(facade.trackTitles(), strings({"First"}));
}

void MusicLibraryFacadeTest::addTrackWithoutAPathStaysInMemory()
{
    MusicLibraryFacade facade;
    QSignalSpy changed(&facade, &MusicLibraryFacade::changed);

    addTrack(facade, QStringLiteral("t1"), QStringLiteral("Song"));

    QCOMPARE(changed.count(), 1);
    QCOMPARE(facade.trackCount(), 1);
    QCOMPARE(facade.search(QString()).size(), 1);
}

void MusicLibraryFacadeTest::removeTrackDropsPlaylistAndFavoriteReferences()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = libraryPath(dir);

    MusicLibraryFacade facade;
    facade.setStoragePath(path);
    addTrack(facade, QStringLiteral("t1"), QStringLiteral("First"));
    addTrack(facade, QStringLiteral("t2"), QStringLiteral("Second"));
    facade.toggleFavorite(QStringLiteral("t1"));
    facade.createPlaylist(QStringLiteral("p1"), QStringLiteral("Mix"));
    facade.addTrackToPlaylist(QStringLiteral("p1"), QStringLiteral("t1"));
    facade.addTrackToPlaylist(QStringLiteral("p1"), QStringLiteral("t2"));

    QSignalSpy changed(&facade, &MusicLibraryFacade::changed);
    facade.removeTrack(QStringLiteral(" t1 "));

    QCOMPARE(changed.count(), 1);
    QCOMPARE(facade.trackIds(), strings({"t2"}));
    QVERIFY2(facade.favoriteIds().isEmpty(), "the favorite went with the track");
    QCOMPARE(facade.playlistTrackCount(QStringLiteral("p1")), 1);

    const MusicLibraryData stored = readLibrary(path);
    QCOMPARE(stored.tracks.size(), 1);
    QVERIFY2(stored.favorites.isEmpty(), "no dangling favorite is stored");
    QCOMPARE(stored.playlists.size(), 1);
    QCOMPARE(stored.playlists.at(0).trackIds,
             (QVector<QString>{QStringLiteral("t2")}));
}

void MusicLibraryFacadeTest::removeUnknownTrackIsSilent()
{
    MusicLibraryFacade facade;
    addTrack(facade, QStringLiteral("t1"), QStringLiteral("First"));
    QSignalSpy changed(&facade, &MusicLibraryFacade::changed);

    facade.removeTrack(QStringLiteral("nope"));
    facade.removeTrack(QStringLiteral("  "));

    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.trackCount(), 1);
}

void MusicLibraryFacadeTest::toggleFavoriteAddsAndRemoves()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = libraryPath(dir);

    MusicLibraryFacade facade;
    facade.setStoragePath(path);
    addTrack(facade, QStringLiteral("t1"), QStringLiteral("First"));
    QSignalSpy changed(&facade, &MusicLibraryFacade::changed);

    facade.toggleFavorite(QStringLiteral(" t1 "));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(facade.favoriteIds(), strings({"t1"}));
    QVERIFY(facade.isFavorite(QStringLiteral("t1")));
    QCOMPARE(readLibrary(path).favorites, (QVector<QString>{QStringLiteral("t1")}));

    facade.toggleFavorite(QStringLiteral("t1"));
    QCOMPARE(changed.count(), 2);
    QVERIFY(facade.favoriteIds().isEmpty());
    QVERIFY(!facade.isFavorite(QStringLiteral("t1")));
    QVERIFY(readLibrary(path).favorites.isEmpty());
}

void MusicLibraryFacadeTest::toggleFavoriteIgnoresUnknownTracks()
{
    MusicLibraryFacade facade;
    addTrack(facade, QStringLiteral("t1"), QStringLiteral("First"));
    QSignalSpy changed(&facade, &MusicLibraryFacade::changed);

    facade.toggleFavorite(QStringLiteral("nope"));
    facade.toggleFavorite(QString());

    QCOMPARE(changed.count(), 0);
    QVERIFY(facade.favoriteIds().isEmpty());
}

void MusicLibraryFacadeTest::createPlaylistAddsACard()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = libraryPath(dir);

    MusicLibraryFacade facade;
    facade.setStoragePath(path);
    addTrack(facade, QStringLiteral("t1"), QStringLiteral("First"));
    QSignalSpy changed(&facade, &MusicLibraryFacade::changed);

    facade.createPlaylist(QStringLiteral(" p1 "), QStringLiteral(" Road trip "));

    QCOMPARE(changed.count(), 1);
    QCOMPARE(facade.playlistCount(), 1);
    QCOMPARE(facade.playlistTitles(), strings({"Road trip"}));
    QCOMPARE(facade.playlistTrackCount(QStringLiteral("p1")), 0);

    const QVariantList cards = facade.playlists();
    QCOMPARE(cards.size(), 1);
    const QVariantMap card = cards.at(0).toMap();
    QCOMPARE(card.value(QStringLiteral("id")).toString(), QStringLiteral("p1"));
    QCOMPARE(card.value(QStringLiteral("title")).toString(),
             QStringLiteral("Road trip"));
    QCOMPARE(card.value(QStringLiteral("trackCount")).toInt(), 0);
    QVERIFY(card.value(QStringLiteral("createdAt")).toLongLong() > 0);

    const MusicLibraryData stored = readLibrary(path);
    QCOMPARE(stored.playlists.size(), 1);
    QCOMPARE(stored.playlists.at(0).id, QStringLiteral("p1"));
    QCOMPARE(stored.playlists.at(0).title, QStringLiteral("Road trip"));
    QVERIFY(stored.playlists.at(0).createdAt > 0);
    QCOMPARE(stored.playlists.at(0).updatedAt,
             stored.playlists.at(0).createdAt);
}

void MusicLibraryFacadeTest::createPlaylistRejectsInvalidTitlesAndRepeatedIds()
{
    MusicLibraryFacade facade;
    QSignalSpy changed(&facade, &MusicLibraryFacade::changed);

    facade.createPlaylist(QStringLiteral("p1"), QStringLiteral("   "));
    facade.createPlaylist(QString(), QStringLiteral("No id"));
    facade.createPlaylist(QStringLiteral("p2"), QString(81, QLatin1Char('x')));
    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.playlistCount(), 0);

    facade.createPlaylist(QStringLiteral("p1"), QStringLiteral("Mix"));
    QCOMPARE(changed.count(), 1);

    facade.createPlaylist(QStringLiteral(" p1 "), QStringLiteral("Other"));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(facade.playlistCount(), 1);
    QCOMPARE(facade.playlistTitles(), strings({"Mix"}));
}

void MusicLibraryFacadeTest::deletePlaylistKeepsTracksAndFavorites()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = libraryPath(dir);

    MusicLibraryFacade facade;
    facade.setStoragePath(path);
    addTrack(facade, QStringLiteral("t1"), QStringLiteral("First"));
    facade.toggleFavorite(QStringLiteral("t1"));
    facade.createPlaylist(QStringLiteral("p1"), QStringLiteral("Mix"));
    facade.addTrackToPlaylist(QStringLiteral("p1"), QStringLiteral("t1"));

    QSignalSpy changed(&facade, &MusicLibraryFacade::changed);
    facade.deletePlaylist(QStringLiteral(" p1 "));

    QCOMPARE(changed.count(), 1);
    QCOMPARE(facade.playlistCount(), 0);
    QCOMPARE(facade.trackCount(), 1);
    QCOMPARE(facade.favoriteIds(), strings({"t1"}));

    const MusicLibraryData stored = readLibrary(path);
    QVERIFY(stored.playlists.isEmpty());
    QCOMPARE(stored.tracks.size(), 1);
    QCOMPARE(stored.favorites, (QVector<QString>{QStringLiteral("t1")}));
}

void MusicLibraryFacadeTest::addTrackToPlaylistAppendsOnce()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = libraryPath(dir);

    MusicLibraryFacade facade;
    facade.setStoragePath(path);
    addTrack(facade, QStringLiteral("t1"), QStringLiteral("First"));
    addTrack(facade, QStringLiteral("t2"), QStringLiteral("Second"));
    facade.createPlaylist(QStringLiteral("p1"), QStringLiteral("Mix"));
    QSignalSpy changed(&facade, &MusicLibraryFacade::changed);

    facade.addTrackToPlaylist(QStringLiteral("p1"), QStringLiteral("t1"));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(facade.playlistTrackCount(QStringLiteral("p1")), 1);

    facade.addTrackToPlaylist(QStringLiteral(" p1 "), QStringLiteral(" t2 "));
    QCOMPARE(changed.count(), 2);
    QCOMPARE(facade.playlistTrackCount(QStringLiteral("p1")), 2);

    facade.addTrackToPlaylist(QStringLiteral("p1"), QStringLiteral("t1"));
    QCOMPARE(changed.count(), 2);
    QCOMPARE(facade.playlistTrackCount(QStringLiteral("p1")), 2);

    const MusicLibraryData stored = readLibrary(path);
    QCOMPARE(stored.playlists.size(), 1);
    QCOMPARE(stored.playlists.at(0).trackIds,
             (QVector<QString>{QStringLiteral("t1"), QStringLiteral("t2")}));
    QVERIFY(stored.playlists.at(0).updatedAt >= stored.playlists.at(0).createdAt);
}

void MusicLibraryFacadeTest::addTrackToPlaylistIgnoresUnknownIds()
{
    MusicLibraryFacade facade;
    addTrack(facade, QStringLiteral("t1"), QStringLiteral("First"));
    facade.createPlaylist(QStringLiteral("p1"), QStringLiteral("Mix"));
    QSignalSpy changed(&facade, &MusicLibraryFacade::changed);

    facade.addTrackToPlaylist(QStringLiteral("nope"), QStringLiteral("t1"));
    facade.addTrackToPlaylist(QStringLiteral("p1"), QStringLiteral("nope"));
    facade.addTrackToPlaylist(QString(), QStringLiteral("t1"));

    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.playlistTrackCount(QStringLiteral("p1")), 0);
}

void MusicLibraryFacadeTest::searchMatchesTitleArtistAndAlbum()
{
    MusicLibraryFacade facade;
    addTrack(facade, QStringLiteral("t1"), QStringLiteral("Midnight City"),
             QStringLiteral("M83"), QStringLiteral("Hurry Up, We're Dreaming"), 244);
    addTrack(facade, QStringLiteral("t2"), QStringLiteral("Intro"),
             QStringLiteral("The xx"), QStringLiteral("xx"), 127);
    addTrack(facade, QStringLiteral("t3"), QStringLiteral("Outro"),
             QStringLiteral("M83"), QString(), 200);

    QCOMPARE(facade.search(QStringLiteral("midnight")), strings({"t1"}));
    QCOMPARE(facade.search(QStringLiteral("m83")), strings({"t1", "t3"}));
    QCOMPARE(facade.search(QStringLiteral("hurry")), strings({"t1"}));
    QVERIFY(facade.search(QStringLiteral("nothing")).isEmpty());
    QCOMPARE(facade.search(QStringLiteral("")), strings({"t1", "t2", "t3"}));
}

void MusicLibraryFacadeTest::searchIsCaseInsensitiveAndTrims()
{
    MusicLibraryFacade facade;
    addTrack(facade, QStringLiteral("t1"), QStringLiteral("Midnight City"),
             QStringLiteral("M83"), QStringLiteral("Hurry Up"));
    addTrack(facade, QStringLiteral("t2"), QStringLiteral("Intro"),
             QStringLiteral("The xx"), QStringLiteral("xx"));

    QCOMPARE(facade.search(QStringLiteral("  MIDNIGHT  ")), strings({"t1"}));
    QCOMPARE(facade.search(QStringLiteral("the XX")), strings({"t2"}));
    QCOMPARE(facade.search(QStringLiteral("   ")), strings({"t1", "t2"}));
}

void MusicLibraryFacadeTest::searchKeepsStoredOrderAndReturnsIds()
{
    MusicLibraryFacade facade;
    addTrack(facade, QStringLiteral("t3"), QStringLiteral("Third"));
    addTrack(facade, QStringLiteral("t1"), QStringLiteral("First"));
    addTrack(facade, QStringLiteral("t2"), QStringLiteral("Second"));

    QCOMPARE(facade.search(QStringLiteral("artist-1")),
             strings({"t3", "t1", "t2"}));
    QCOMPARE(facade.trackIds(), strings({"t3", "t1", "t2"}));
}

void MusicLibraryFacadeTest::searchTracksCarriesRowFields()
{
    MusicLibraryFacade facade;
    addTrack(facade, QStringLiteral("t1"), QStringLiteral("Midnight City"),
             QStringLiteral("M83"), QStringLiteral("Hurry Up"), 185);
    facade.toggleFavorite(QStringLiteral("t1"));

    const QVariantList rows = facade.searchTracks(QStringLiteral("mid"));
    QCOMPARE(rows.size(), 1);
    const QVariantMap row = rows.at(0).toMap();
    QCOMPARE(row.value(QStringLiteral("id")).toString(), QStringLiteral("t1"));
    QCOMPARE(row.value(QStringLiteral("title")).toString(),
             QStringLiteral("Midnight City"));
    QCOMPARE(row.value(QStringLiteral("artistId")).toString(), QStringLiteral("M83"));
    QCOMPARE(row.value(QStringLiteral("albumId")).toString(), QStringLiteral("Hurry Up"));
    QCOMPARE(row.value(QStringLiteral("duration")).toString(),
             QStringLiteral("3:05"));
    QCOMPARE(row.value(QStringLiteral("favorite")).toBool(), true);
}

void MusicLibraryFacadeTest::setQueryStoresTheTextVerbatim()
{
    MusicLibraryFacade facade;
    QSignalSpy changed(&facade, &MusicLibraryFacade::changed);

    facade.setQuery(QStringLiteral("  Midnight  "));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(facade.query(), QStringLiteral("  Midnight  "));

    facade.setQuery(QStringLiteral("  Midnight  "));
    QCOMPARE(changed.count(), 1);

    facade.setQuery(QStringLiteral("Midnight"));
    QCOMPARE(changed.count(), 2);
    QCOMPARE(facade.query(), QStringLiteral("Midnight"));
}

void MusicLibraryFacadeTest::queryIsKeptByLoad()
{
    MusicLibraryFacade facade;
    facade.setQuery(QStringLiteral("mix"));

    facade.load();

    QCOMPARE(facade.query(), QStringLiteral("mix"));
}

void MusicLibraryFacadeTest::clearEmptiesEverythingAndSaves()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = libraryPath(dir);

    MusicLibraryFacade facade;
    facade.setStoragePath(path);
    addTrack(facade, QStringLiteral("t1"), QStringLiteral("First"));
    facade.toggleFavorite(QStringLiteral("t1"));
    facade.createPlaylist(QStringLiteral("p1"), QStringLiteral("Mix"));
    facade.addTrackToPlaylist(QStringLiteral("p1"), QStringLiteral("t1"));
    facade.setQuery(QStringLiteral("mix"));

    QSignalSpy changed(&facade, &MusicLibraryFacade::changed);
    facade.clear();

    QCOMPARE(changed.count(), 1);
    QCOMPARE(facade.trackCount(), 0);
    QCOMPARE(facade.playlistCount(), 0);
    QVERIFY(facade.trackIds().isEmpty());
    QVERIFY(facade.trackTitles().isEmpty());
    QVERIFY(facade.playlistTitles().isEmpty());
    QVERIFY(facade.favoriteIds().isEmpty());
    QVERIFY(facade.query().isEmpty());
    QVERIFY(facade.tracks().isEmpty());
    QVERIFY(facade.playlists().isEmpty());
    QVERIFY(facade.search(QString()).isEmpty());

    const MusicLibraryData stored = readLibrary(path);
    QVERIFY(stored.tracks.isEmpty());
    QVERIFY(stored.playlists.isEmpty());
    QVERIFY(stored.favorites.isEmpty());
}

void MusicLibraryFacadeTest::clearOnEmptyIsSilent()
{
    MusicLibraryFacade facade;
    QSignalSpy changed(&facade, &MusicLibraryFacade::changed);

    facade.clear();

    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.trackCount(), 0);
    QCOMPARE(facade.playlistCount(), 0);
}

void MusicLibraryFacadeTest::reloadRestoresTheSavedLibrary()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = libraryPath(dir);

    MusicLibraryFacade first;
    first.setStoragePath(path);
    addTrack(first, QStringLiteral("t1"), QStringLiteral("First"),
             QStringLiteral("ar1"), QStringLiteral("al1"), 185, 5);
    addTrack(first, QStringLiteral("t2"), QStringLiteral("Second"),
             QStringLiteral("ar2"), QString(), 90, 6);
    first.toggleFavorite(QStringLiteral("t2"));
    first.createPlaylist(QStringLiteral("p1"), QStringLiteral("Road trip"));
    first.addTrackToPlaylist(QStringLiteral("p1"), QStringLiteral("t1"));

    MusicLibraryFacade second;
    second.setStoragePath(path);
    QSignalSpy changed(&second, &MusicLibraryFacade::changed);
    second.load();

    QCOMPARE(changed.count(), 1);
    QCOMPARE(second.trackIds(), first.trackIds());
    QCOMPARE(second.trackTitles(), first.trackTitles());
    QCOMPARE(second.playlistTitles(), first.playlistTitles());
    QCOMPARE(second.favoriteIds(), first.favoriteIds());
    QCOMPARE(second.playlistTrackCount(QStringLiteral("p1")), 1);
    QCOMPARE(second.tracks(), first.tracks());
    QCOMPARE(second.playlists(), first.playlists());
}

QTEST_GUILESS_MAIN(MusicLibraryFacadeTest)
#include "tst_music_library_facade.moc"
