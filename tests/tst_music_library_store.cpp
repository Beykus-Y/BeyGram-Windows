#include "telegram/music/MusicLibraryStore.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

MusicTrackModel track(const QString& id, const QString& title = {},
                      const QString& artistId = {}, const QString& albumId = {},
                      int durationSeconds = 0, int fileId = 0, const QString& localPath = {},
                      int trackNumber = 0)
{
    return MusicTrackModel{id, title, artistId, albumId, durationSeconds, fileId,
                           localPath, trackNumber};
}

MusicAlbum album(const QString& id, const QString& title = {}, const QString& artistId = {},
                 int year = 0, int coverFileId = 0)
{
    return MusicAlbum{id, title, artistId, year, coverFileId};
}

MusicArtist artist(const QString& id, const QString& name = {})
{
    return MusicArtist{id, name};
}

MusicPlaylist playlist(const QString& id, const QString& title = {},
                       const QVector<QString>& trackIds = {},
                       const QString& description = {}, qint64 createdAt = 0,
                       qint64 updatedAt = 0)
{
    return MusicPlaylist{id, title, description, trackIds, createdAt, updatedAt};
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

bool sameAlbum(const MusicAlbum& left, const MusicAlbum& right)
{
    return left.id == right.id
        && left.title == right.title
        && left.artistId == right.artistId
        && left.year == right.year
        && left.coverFileId == right.coverFileId;
}

bool sameArtist(const MusicArtist& left, const MusicArtist& right)
{
    return left.id == right.id && left.name == right.name;
}

bool samePlaylist(const MusicPlaylist& left, const MusicPlaylist& right)
{
    return left.id == right.id
        && left.title == right.title
        && left.description == right.description
        && left.trackIds == right.trackIds
        && left.createdAt == right.createdAt
        && left.updatedAt == right.updatedAt;
}

QString libraryPath(const QTemporaryDir& directory)
{
    return directory.filePath(QStringLiteral("music/library.json"));
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

} // namespace

class MusicLibraryStoreTest final : public QObject {
    Q_OBJECT

private slots:
    void missingFileLoadsEmpty();
    void pathIsKept();
    void roundTripsEverySection();
    void saveCreatesParentDirectory();
    void overwritesWithoutLeavingTemporaryFiles();
    void malformedRowsAreSkipped();
    void duplicateIdsKeepFirst();
    void playlistsKeepOnlyKnownTrackIds();
    void favoritesAreUniqueAndKnown();
    void numbersAreClamped();
    void missingOptionalFieldsUseDefaults();
    void unsupportedVersionLoadsEmpty();
    void acceptsAndroidSchemaVersionKey();
    void malformedDocumentLoadsEmpty();
    void saveNormalizesData();
    void saveFailsWhenTargetCannotBeCreated();
    void emptyPathIsInert();
};

void MusicLibraryStoreTest::missingFileLoadsEmpty()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    MusicLibraryStore store(libraryPath(directory));
    const MusicLibraryData data = store.load();

    QVERIFY(data.tracks.isEmpty());
    QVERIFY(data.albums.isEmpty());
    QVERIFY(data.artists.isEmpty());
    QVERIFY(data.playlists.isEmpty());
    QVERIFY(data.favorites.isEmpty());
}

void MusicLibraryStoreTest::pathIsKept()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = libraryPath(directory);
    QCOMPARE(MusicLibraryStore(path).path(), path);
}

void MusicLibraryStoreTest::roundTripsEverySection()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    MusicLibraryData data;
    data.tracks = {
        track(QStringLiteral("t1"), QStringLiteral("Song"), QStringLiteral("ar1"),
              QStringLiteral("al1"), 201, 77, QStringLiteral("/music/t1.ogg"), 3),
        track(QStringLiteral("t2")),
    };
    data.albums = {album(QStringLiteral("al1"), QStringLiteral("Album"),
                         QStringLiteral("ar1"), 1999, 55)};
    data.artists = {artist(QStringLiteral("ar1"), QStringLiteral("Artist"))};
    data.playlists = {playlist(QStringLiteral("p1"), QStringLiteral("Mix"),
                               {QStringLiteral("t1"), QStringLiteral("t2")},
                               QStringLiteral("Description"), 10, 20)};
    data.favorites = {QStringLiteral("t2"), QStringLiteral("t1")};

    MusicLibraryStore store(libraryPath(directory));
    QVERIFY(store.save(data));

    const MusicLibraryData loaded = store.load();
    QVERIFY(loaded == data);
}

void MusicLibraryStoreTest::saveCreatesParentDirectory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = directory.filePath(QStringLiteral("a/b/c/library.json"));
    QVERIFY(!QFileInfo::exists(path));

    MusicLibraryStore store(path);
    QVERIFY(store.save(MusicLibraryData{}));
    QVERIFY(QFileInfo::exists(path));
}

void MusicLibraryStoreTest::overwritesWithoutLeavingTemporaryFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    MusicLibraryData first;
    first.tracks = {track(QStringLiteral("t1"), QStringLiteral("First"))};
    MusicLibraryData second;
    second.tracks = {track(QStringLiteral("t2"), QStringLiteral("Second"))};

    const QString path = libraryPath(directory);
    MusicLibraryStore store(path);
    QVERIFY(store.save(first));
    QVERIFY(store.save(second));

    // QSaveFile writes a temporary file and renames it over the target; after a
    // successful commit only the target may remain.
    const QDir parent = QFileInfo(path).absoluteDir();
    QCOMPARE(parent.entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot),
             QStringList{QStringLiteral("library.json")});

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    QCOMPARE(error.error, QJsonParseError::NoError);
    QVERIFY(document.isObject());
    QCOMPARE(document.object().value(QStringLiteral("version")).toInt(),
             MusicLibraryStore::kSchemaVersion);

    const MusicLibraryData loaded = store.load();
    QCOMPARE(loaded.tracks.size(), 1);
    QCOMPARE(loaded.tracks.first().id, QStringLiteral("t2"));
    QCOMPARE(loaded.tracks.first().title, QStringLiteral("Second"));
}

void MusicLibraryStoreTest::malformedRowsAreSkipped()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QByteArray payload = R"json({
        "version": 1,
        "tracks": [
            {"id": "t1", "title": "Good", "artistId": "ar1", "albumId": "al1",
             "durationSeconds": 100, "fileId": 5, "localPath": "/music/t1.ogg",
             "trackNumber": 3},
            {"id": "t1", "title": "Duplicate"},
            42,
            {"title": "No id"},
            {"id": "   ", "title": "Blank id"},
            {"id": "t2", "title": 123, "durationSeconds": -5}
        ],
        "albums": [
            {"id": "al1", "title": "Album"},
            {"id": "al1", "title": "Duplicate"},
            {"title": "No id"},
            "not an object"
        ],
        "artists": [
            {"id": "ar1", "name": "Artist"},
            {"id": ""}
        ],
        "playlists": [
            {"id": "p1", "title": "Mix", "trackIds": ["t1", "missing", "t1", " t2 "]},
            {"id": "p1", "title": "Duplicate"},
            {"id": "p2", "title": "   "},
            {"title": "No id"}
        ],
        "favorites": ["t1", "t1", "missing", " t2 "]
    })json";

    const QString path = libraryPath(directory);
    QVERIFY(writeFile(path, payload));

    MusicLibraryStore store(path);
    const MusicLibraryData data = store.load();

    QCOMPARE(data.tracks.size(), 2);
    QCOMPARE(data.tracks.at(0).id, QStringLiteral("t1"));
    QCOMPARE(data.tracks.at(0).title, QStringLiteral("Good"));
    QCOMPARE(data.tracks.at(0).artistId, QStringLiteral("ar1"));
    QCOMPARE(data.tracks.at(0).albumId, QStringLiteral("al1"));
    QCOMPARE(data.tracks.at(0).fileId, 5);
    QCOMPARE(data.tracks.at(0).localPath, QStringLiteral("/music/t1.ogg"));
    QCOMPARE(data.tracks.at(0).trackNumber, 3);
    QCOMPARE(data.tracks.at(1).id, QStringLiteral("t2"));
    QCOMPARE(data.tracks.at(1).title, QString());
    QCOMPARE(data.tracks.at(1).durationSeconds, 0);

    QCOMPARE(data.albums.size(), 1);
    QCOMPARE(data.albums.first().id, QStringLiteral("al1"));
    QCOMPARE(data.albums.first().title, QStringLiteral("Album"));

    QCOMPARE(data.artists.size(), 1);
    QCOMPARE(data.artists.first().id, QStringLiteral("ar1"));

    QCOMPARE(data.playlists.size(), 1);
    QCOMPARE(data.playlists.first().id, QStringLiteral("p1"));
    QCOMPARE(data.playlists.first().trackIds,
             (QVector<QString>{QStringLiteral("t1"), QStringLiteral("t2")}));

    QCOMPARE(data.favorites,
             (QVector<QString>{QStringLiteral("t1"), QStringLiteral("t2")}));
}

void MusicLibraryStoreTest::duplicateIdsKeepFirst()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QByteArray payload = R"json({
        "version": 1,
        "tracks": [{"id": "t1", "title": "First"}, {"id": "t1", "title": "Second"}],
        "albums": [{"id": "al1", "title": "First"}, {"id": "al1", "title": "Second"}],
        "artists": [{"id": "ar1", "name": "First"}, {"id": "ar1", "name": "Second"}],
        "playlists": [
            {"id": "p1", "title": "First"},
            {"id": "p1", "title": "Second"}
        ],
        "favorites": []
    })json";

    const QString path = libraryPath(directory);
    QVERIFY(writeFile(path, payload));

    MusicLibraryStore store(path);
    const MusicLibraryData data = store.load();

    QCOMPARE(data.tracks.size(), 1);
    QCOMPARE(data.tracks.first().title, QStringLiteral("First"));
    QCOMPARE(data.albums.size(), 1);
    QCOMPARE(data.albums.first().title, QStringLiteral("First"));
    QCOMPARE(data.artists.size(), 1);
    QCOMPARE(data.artists.first().name, QStringLiteral("First"));
    QCOMPARE(data.playlists.size(), 1);
    QCOMPARE(data.playlists.first().title, QStringLiteral("First"));
}

void MusicLibraryStoreTest::playlistsKeepOnlyKnownTrackIds()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QByteArray payload = R"json({
        "version": 1,
        "tracks": [{"id": "t1"}, {"id": "t2"}],
        "playlists": [{
            "id": "p1",
            "title": "Mix",
            "trackIds": ["t2", "missing", "t1", "t2", " t2 ", ""]
        }],
        "favorites": []
    })json";

    const QString path = libraryPath(directory);
    QVERIFY(writeFile(path, payload));

    MusicLibraryStore store(path);
    const MusicLibraryData data = store.load();

    QCOMPARE(data.playlists.size(), 1);
    QCOMPARE(data.playlists.first().trackIds,
             (QVector<QString>{QStringLiteral("t2"), QStringLiteral("t1")}));
}

void MusicLibraryStoreTest::favoritesAreUniqueAndKnown()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QByteArray payload = R"json({
        "version": 1,
        "tracks": [{"id": "t1"}, {"id": "t2"}],
        "favorites": ["t2", "t1", "t2", "missing", " t1 ", ""]
    })json";

    const QString path = libraryPath(directory);
    QVERIFY(writeFile(path, payload));

    MusicLibraryStore store(path);
    const MusicLibraryData data = store.load();

    QCOMPARE(data.favorites,
             (QVector<QString>{QStringLiteral("t2"), QStringLiteral("t1")}));
}

void MusicLibraryStoreTest::numbersAreClamped()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QByteArray payload = R"json({
        "version": 1,
        "tracks": [{"id": "t1", "durationSeconds": -5, "fileId": -1, "trackNumber": -9}],
        "albums": [{"id": "al1", "year": -2, "coverFileId": -3}],
        "playlists": [{"id": "p1", "title": "Mix", "createdAt": -3, "updatedAt": -4}],
        "favorites": []
    })json";

    const QString path = libraryPath(directory);
    QVERIFY(writeFile(path, payload));

    MusicLibraryStore store(path);
    const MusicLibraryData data = store.load();

    QCOMPARE(data.tracks.first().durationSeconds, 0);
    QCOMPARE(data.tracks.first().fileId, 0);
    QCOMPARE(data.tracks.first().trackNumber, 0);
    QCOMPARE(data.albums.first().year, 0);
    QCOMPARE(data.albums.first().coverFileId, 0);
    QCOMPARE(data.playlists.first().createdAt, qint64(0));
    QCOMPARE(data.playlists.first().updatedAt, qint64(0));
}

void MusicLibraryStoreTest::missingOptionalFieldsUseDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QByteArray payload = R"json({
        "version": 1,
        "tracks": [{"id": "t1"}],
        "albums": [{"id": "al1"}],
        "artists": [{"id": "ar1"}],
        "playlists": [{"id": "p1", "title": "Mix"}]
    })json";

    const QString path = libraryPath(directory);
    QVERIFY(writeFile(path, payload));

    MusicLibraryStore store(path);
    const MusicLibraryData data = store.load();

    QVERIFY(sameTrack(data.tracks.first(), track(QStringLiteral("t1"))));
    QVERIFY(sameAlbum(data.albums.first(), album(QStringLiteral("al1"))));
    QVERIFY(sameArtist(data.artists.first(), artist(QStringLiteral("ar1"))));
    QVERIFY(samePlaylist(data.playlists.first(),
                         playlist(QStringLiteral("p1"), QStringLiteral("Mix"))));
    QVERIFY(data.favorites.isEmpty());
}

void MusicLibraryStoreTest::unsupportedVersionLoadsEmpty()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = libraryPath(directory);
    QVERIFY(writeFile(path, R"json({"version": 2, "tracks": [{"id": "t1"}]})json"));

    MusicLibraryStore store(path);
    QVERIFY(store.load().tracks.isEmpty());
}

void MusicLibraryStoreTest::acceptsAndroidSchemaVersionKey()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = libraryPath(directory);
    QVERIFY(writeFile(path, R"json({"schemaVersion": 1, "tracks": [{"id": "t1"}]})json"));

    MusicLibraryStore store(path);
    const MusicLibraryData data = store.load();
    QCOMPARE(data.tracks.size(), 1);
    QCOMPARE(data.tracks.first().id, QStringLiteral("t1"));
}

void MusicLibraryStoreTest::malformedDocumentLoadsEmpty()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = libraryPath(directory);
    MusicLibraryStore store(path);

    const QVector<QByteArray> payloads{
        QByteArrayLiteral("not json"),
        QByteArrayLiteral("[]"),
        QByteArrayLiteral(R"json({"tracks": [{"id": "t1"}]})json"),
        QByteArrayLiteral(R"json({"version": "1", "tracks": [{"id": "t1"}]})json"),
        QByteArrayLiteral(R"json({"version": 0, "tracks": [{"id": "t1"}]})json"),
    };

    for (const QByteArray& payload : payloads) {
        QVERIFY(writeFile(path, payload));
        QVERIFY(store.load().tracks.isEmpty());
    }
}

void MusicLibraryStoreTest::saveNormalizesData()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    MusicLibraryData data;
    data.tracks = {
        track(QStringLiteral(" t1 "), QStringLiteral("First")),
        track(QStringLiteral("t1"), QStringLiteral("Second")),
        track(QStringLiteral("   "), QStringLiteral("No id")),
        track(QStringLiteral("t2")),
    };
    data.albums = {album(QStringLiteral(" a1 ")), album(QStringLiteral("a1"))};
    data.artists = {artist(QStringLiteral(" ar1 ")), artist(QStringLiteral("ar1"))};
    data.playlists = {
        playlist(QStringLiteral(" p1 "), QStringLiteral("  Mix  "),
                 {QStringLiteral("t1"), QStringLiteral("unknown"), QStringLiteral("t1"),
                  QStringLiteral(" t2 ")}),
        playlist(QStringLiteral("p1"), QStringLiteral("Duplicate")),
        playlist(QStringLiteral("p2"), QStringLiteral("   ")),
    };
    data.favorites = {QStringLiteral("t2"), QStringLiteral("t2"),
                      QStringLiteral("unknown"), QStringLiteral(" t1 ")};

    const QString path = libraryPath(directory);
    MusicLibraryStore store(path);
    QVERIFY(store.save(data));

    const MusicLibraryData loaded = store.load();

    QCOMPARE(loaded.tracks.size(), 2);
    QCOMPARE(loaded.tracks.at(0).id, QStringLiteral("t1"));
    QCOMPARE(loaded.tracks.at(0).title, QStringLiteral("First"));
    QCOMPARE(loaded.tracks.at(1).id, QStringLiteral("t2"));

    QCOMPARE(loaded.albums.size(), 1);
    QCOMPARE(loaded.albums.first().id, QStringLiteral("a1"));
    QCOMPARE(loaded.artists.size(), 1);
    QCOMPARE(loaded.artists.first().id, QStringLiteral("ar1"));

    QCOMPARE(loaded.playlists.size(), 1);
    QCOMPARE(loaded.playlists.first().id, QStringLiteral("p1"));
    QCOMPARE(loaded.playlists.first().title, QStringLiteral("Mix"));
    QCOMPARE(loaded.playlists.first().trackIds,
             (QVector<QString>{QStringLiteral("t1"), QStringLiteral("t2")}));

    QCOMPARE(loaded.favorites,
             (QVector<QString>{QStringLiteral("t2"), QStringLiteral("t1")}));
}

void MusicLibraryStoreTest::saveFailsWhenTargetCannotBeCreated()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    // A regular file stands where the library's parent directory would go.
    const QString blocker = directory.filePath(QStringLiteral("blocker"));
    QFile file(blocker);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("x"), qint64(1));
    file.close();

    MusicLibraryStore store(QDir(blocker).filePath(QStringLiteral("library.json")));
    QVERIFY(!store.save(MusicLibraryData{}));
    QVERIFY(!QFileInfo::exists(store.path()));
}

void MusicLibraryStoreTest::emptyPathIsInert()
{
    MusicLibraryStore store{QString()};
    QVERIFY(store.path().isEmpty());
    QVERIFY((store.load() == MusicLibraryData{}));
    QVERIFY(!store.save(MusicLibraryData{}));
}

QTEST_GUILESS_MAIN(MusicLibraryStoreTest)

#include "tst_music_library_store.moc"
