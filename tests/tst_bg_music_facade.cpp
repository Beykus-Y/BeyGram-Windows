#include "ui/BgMusicFacade.h"

#include "telegram/music/BgMusic.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTest>

using namespace beygram::ui;
using namespace beygram::telegram;

namespace {

BgMusicEntry entry(const QString& trackId)
{
    return BgMusicEntry{trackId, QStringLiteral("Title ") + trackId,
                        QStringLiteral("Artist"), 60, 0};
}

BgMusicPlaylist playlist(const QString& id, const QString& title,
                         const QStringList& trackIds)
{
    BgMusicPlaylist value;
    value.id = id;
    value.title = title;
    for (const QString& trackId : trackIds)
        value.entries.append(entry(trackId));
    return value;
}

QString jsonFor(const QString& id, const QString& title, const QStringList& trackIds)
{
    return bgMusicToJson(playlist(id, title, trackIds));
}

QString m3uFor(const QStringList& trackIds)
{
    QString text = QStringLiteral("#EXTM3U\n");
    for (const QString& trackId : trackIds) {
        text += QStringLiteral("#EXTINF:60,Title %1 - Artist\n%2\n")
                    .arg(trackId, trackId);
    }
    return text;
}

} // namespace

class BgMusicFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();

    void setPlaylistsPublishesIdsAndTitles();
    void setPlaylistsPairsListsAndCutsToTheShorterOne();
    void setPlaylistsDropsEmptyIds();
    void setPlaylistsKeepsFirstDuplicate();
    void setPlaylistsKeepsSelectionWhileItsIdRemains();
    void setPlaylistsClearsSelectionWhenItsIdDisappears();
    void setPlaylistsOfSameContentIsSilent();
    void setPlaylistsEmitsChangedOncePerRealChange();
    void setPlaylistsEmptyClearsEverything();
    void playlistListsAreCopies();

    void selectSetsTheSelectedId();
    void selectUnknownIdIsIgnored();
    void selectEmptyClearsTheSelection();
    void selectSameIdIsSilent();

    void entryCountFollowsTheSelection();

    void importJsonAddsAndSelectsPlaylist();
    void importUpperCaseJsonExtensionIsAccepted();
    void importM3uAddsAndSelectsPlaylist();
    void importM3u8IsAccepted();
    void importJsonWithoutIdGeneratesUniqueIds();
    void generatedIdSkipsAnExistingOne();
    void importWithoutTitleUsesTheFileName();

    void importRejectsEmptyContent();
    void importRejectsUnknownExtension();
    void importChoosesTheParserByExtension();
    void importRejectsTooLargeContent();
    void importRejectsContentWithoutEntries();
    void importFailureLeavesStateUntouched();

    void importReplacesPlaylistWithTheSameId();
    void importSelectsTheImportedPlaylist();
    void reimportingIdenticalContentIsSilent();

    void exportJsonReturnsTheSelectedPlaylist();
    void exportJsonIsEmptyWithoutSelection();
    void exportM3uReturnsTheSelectedPlaylist();
    void exportM3uIsEmptyWithoutSelection();
    void exportFollowsTheSelection();

    void setPlaylistsPreservesImportedEntries();
    void setPlaylistsDropsEntriesOfRemovedPlaylists();

    void clearForgetsEverything();
    void clearOnEmptyIsSilent();
};

void BgMusicFacadeTest::startsEmpty()
{
    BgMusicFacade facade;

    QVERIFY(facade.playlistIds().isEmpty());
    QVERIFY(facade.playlistTitles().isEmpty());
    QCOMPARE(facade.entryCount(), 0);
    QCOMPARE(facade.selectedId(), QString());
    QCOMPARE(facade.exportJson(), QString());
    QCOMPARE(facade.exportM3u(), QString());
}

void BgMusicFacadeTest::setPlaylistsPublishesIdsAndTitles()
{
    BgMusicFacade facade;
    facade.setPlaylists({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")},
                        {QStringLiteral("Alpha"), QStringLiteral("Beta"),
                         QStringLiteral("Gamma")});

    QCOMPARE(facade.playlistIds(),
             QStringList({QStringLiteral("a"), QStringLiteral("b"),
                          QStringLiteral("c")}));
    QCOMPARE(facade.playlistTitles(),
             QStringList({QStringLiteral("Alpha"), QStringLiteral("Beta"),
                          QStringLiteral("Gamma")}));
    QCOMPARE(facade.entryCount(), 0);
    QCOMPARE(facade.selectedId(), QString());
}

void BgMusicFacadeTest::setPlaylistsPairsListsAndCutsToTheShorterOne()
{
    BgMusicFacade facade;

    // A title with no id at its offset belongs to no playlist.
    facade.setPlaylists({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")},
                        {QStringLiteral("Alpha"), QStringLiteral("Beta")});
    QCOMPARE(facade.playlistIds(),
             QStringList({QStringLiteral("a"), QStringLiteral("b")}));
    QCOMPARE(facade.playlistTitles(),
             QStringList({QStringLiteral("Alpha"), QStringLiteral("Beta")}));

    // An id with no title at its offset is not invented either.
    facade.setPlaylists({QStringLiteral("a")},
                        {QStringLiteral("Alpha"), QStringLiteral("Beta"),
                         QStringLiteral("Gamma")});
    QCOMPARE(facade.playlistIds(), QStringList({QStringLiteral("a")}));
    QCOMPARE(facade.playlistTitles(), QStringList({QStringLiteral("Alpha")}));
}

void BgMusicFacadeTest::setPlaylistsDropsEmptyIds()
{
    BgMusicFacade facade;
    facade.setPlaylists({QString(), QStringLiteral("a"), QString(), QStringLiteral("b")},
                        {QStringLiteral("Blank"), QStringLiteral("Alpha"),
                         QStringLiteral("Blank again"), QStringLiteral("Beta")});

    QCOMPARE(facade.playlistIds(),
             QStringList({QStringLiteral("a"), QStringLiteral("b")}));
    QCOMPARE(facade.playlistTitles(),
             QStringList({QStringLiteral("Alpha"), QStringLiteral("Beta")}));
}

void BgMusicFacadeTest::setPlaylistsKeepsFirstDuplicate()
{
    BgMusicFacade facade;
    facade.setPlaylists({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("a")},
                        {QStringLiteral("First"), QStringLiteral("Beta"),
                         QStringLiteral("Second")});

    QCOMPARE(facade.playlistIds(),
             QStringList({QStringLiteral("a"), QStringLiteral("b")}));
    QCOMPARE(facade.playlistTitles(),
             QStringList({QStringLiteral("First"), QStringLiteral("Beta")}));
}

void BgMusicFacadeTest::setPlaylistsKeepsSelectionWhileItsIdRemains()
{
    BgMusicFacade facade;
    facade.setPlaylists({QStringLiteral("a"), QStringLiteral("b")},
                        {QStringLiteral("Alpha"), QStringLiteral("Beta")});
    facade.select(QStringLiteral("b"));

    QSignalSpy spy(&facade, &BgMusicFacade::changed);

    // The rows move, but the selected playlist is still there.
    facade.setPlaylists({QStringLiteral("b"), QStringLiteral("a")},
                        {QStringLiteral("Beta"), QStringLiteral("Alpha")});
    QCOMPARE(facade.selectedId(), QStringLiteral("b"));
    QCOMPARE(spy.count(), 1);

    // A reload that changes nothing is silent.
    facade.setPlaylists({QStringLiteral("b"), QStringLiteral("a")},
                        {QStringLiteral("Beta"), QStringLiteral("Alpha")});
    QCOMPARE(spy.count(), 1);
}

void BgMusicFacadeTest::setPlaylistsClearsSelectionWhenItsIdDisappears()
{
    BgMusicFacade facade;
    facade.setPlaylists({QStringLiteral("a"), QStringLiteral("b")},
                        {QStringLiteral("Alpha"), QStringLiteral("Beta")});
    facade.select(QStringLiteral("b"));

    QSignalSpy spy(&facade, &BgMusicFacade::changed);
    facade.setPlaylists({QStringLiteral("a")}, {QStringLiteral("Alpha")});

    QCOMPARE(facade.selectedId(), QString());
    QCOMPARE(facade.entryCount(), 0);
    QCOMPARE(spy.count(), 1);
}

void BgMusicFacadeTest::setPlaylistsOfSameContentIsSilent()
{
    BgMusicFacade facade;
    facade.setPlaylists({QStringLiteral("a"), QStringLiteral("b")},
                        {QStringLiteral("Alpha"), QStringLiteral("Beta")});

    QSignalSpy spy(&facade, &BgMusicFacade::changed);
    facade.setPlaylists({QStringLiteral("a"), QStringLiteral("b")},
                        {QStringLiteral("Alpha"), QStringLiteral("Beta")});

    QCOMPARE(spy.count(), 0);
}

void BgMusicFacadeTest::setPlaylistsEmitsChangedOncePerRealChange()
{
    BgMusicFacade facade;

    QSignalSpy spy(&facade, &BgMusicFacade::changed);

    facade.setPlaylists({QStringLiteral("a")}, {QStringLiteral("Alpha")});
    QCOMPARE(spy.count(), 1);

    facade.setPlaylists({QStringLiteral("a")}, {QStringLiteral("Renamed")});
    QCOMPARE(spy.count(), 2);

    facade.setPlaylists({QStringLiteral("a"), QStringLiteral("b")},
                        {QStringLiteral("Renamed"), QStringLiteral("Beta")});
    QCOMPARE(spy.count(), 3);
}

void BgMusicFacadeTest::setPlaylistsEmptyClearsEverything()
{
    BgMusicFacade facade;
    facade.setPlaylists({QStringLiteral("a")}, {QStringLiteral("Alpha")});
    facade.select(QStringLiteral("a"));

    QSignalSpy spy(&facade, &BgMusicFacade::changed);
    facade.setPlaylists(QStringList(), QStringList());

    QVERIFY(facade.playlistIds().isEmpty());
    QVERIFY(facade.playlistTitles().isEmpty());
    QCOMPARE(facade.selectedId(), QString());
    QCOMPARE(facade.entryCount(), 0);
    QCOMPARE(spy.count(), 1);
}

void BgMusicFacadeTest::playlistListsAreCopies()
{
    BgMusicFacade facade;
    facade.setPlaylists({QStringLiteral("a")}, {QStringLiteral("Alpha")});

    QStringList ids = facade.playlistIds();
    ids.clear();
    QCOMPARE(facade.playlistIds().size(), 1);

    QStringList titles = facade.playlistTitles();
    titles.append(QStringLiteral("Extra"));
    QCOMPARE(facade.playlistTitles().size(), 1);
}

void BgMusicFacadeTest::selectSetsTheSelectedId()
{
    BgMusicFacade facade;
    facade.setPlaylists({QStringLiteral("a"), QStringLiteral("b")},
                        {QStringLiteral("Alpha"), QStringLiteral("Beta")});

    QSignalSpy spy(&facade, &BgMusicFacade::changed);
    facade.select(QStringLiteral("b"));

    QCOMPARE(facade.selectedId(), QStringLiteral("b"));
    QCOMPARE(spy.count(), 1);
}

void BgMusicFacadeTest::selectUnknownIdIsIgnored()
{
    BgMusicFacade facade;
    facade.setPlaylists({QStringLiteral("a"), QStringLiteral("b")},
                        {QStringLiteral("Alpha"), QStringLiteral("Beta")});
    facade.select(QStringLiteral("a"));

    QSignalSpy spy(&facade, &BgMusicFacade::changed);

    // A stale id and a case-mismatched id both name no visible row.
    facade.select(QStringLiteral("missing"));
    facade.select(QStringLiteral("A"));

    QCOMPARE(facade.selectedId(), QStringLiteral("a"));
    QCOMPARE(spy.count(), 0);
}

void BgMusicFacadeTest::selectEmptyClearsTheSelection()
{
    BgMusicFacade facade;
    facade.setPlaylists({QStringLiteral("a")}, {QStringLiteral("Alpha")});
    facade.select(QStringLiteral("a"));

    QSignalSpy spy(&facade, &BgMusicFacade::changed);
    facade.select(QString());

    QCOMPARE(facade.selectedId(), QString());
    QCOMPARE(spy.count(), 1);

    facade.select(QString());
    QCOMPARE(spy.count(), 1);
}

void BgMusicFacadeTest::selectSameIdIsSilent()
{
    BgMusicFacade facade;
    facade.setPlaylists({QStringLiteral("a")}, {QStringLiteral("Alpha")});
    facade.select(QStringLiteral("a"));

    QSignalSpy spy(&facade, &BgMusicFacade::changed);
    facade.select(QStringLiteral("a"));

    QCOMPARE(spy.count(), 0);
}

void BgMusicFacadeTest::entryCountFollowsTheSelection()
{
    BgMusicFacade facade;
    QVERIFY(facade.importFromText(QStringLiteral("one.json"),
                                  jsonFor(QStringLiteral("p1"), QStringLiteral("One"),
                                          {QStringLiteral("a")})));
    QVERIFY(facade.importFromText(QStringLiteral("two.json"),
                                  jsonFor(QStringLiteral("p2"), QStringLiteral("Two"),
                                          {QStringLiteral("b"), QStringLiteral("c"),
                                           QStringLiteral("d")})));

    // Import selects the newest playlist.
    QCOMPARE(facade.selectedId(), QStringLiteral("p2"));
    QCOMPARE(facade.entryCount(), 3);

    facade.select(QStringLiteral("p1"));
    QCOMPARE(facade.entryCount(), 1);

    facade.select(QString());
    QCOMPARE(facade.entryCount(), 0);
}

void BgMusicFacadeTest::importJsonAddsAndSelectsPlaylist()
{
    BgMusicFacade facade;
    QSignalSpy changed(&facade, &BgMusicFacade::changed);
    QSignalSpy failed(&facade, &BgMusicFacade::importFailed);

    QVERIFY(facade.importFromText(
        QStringLiteral("road-trip.json"),
        jsonFor(QStringLiteral("mix-1"), QStringLiteral("Road Trip"),
                {QStringLiteral("t1"), QStringLiteral("t2")})));

    QCOMPARE(facade.playlistIds(), QStringList({QStringLiteral("mix-1")}));
    QCOMPARE(facade.playlistTitles(), QStringList({QStringLiteral("Road Trip")}));
    QCOMPARE(facade.selectedId(), QStringLiteral("mix-1"));
    QCOMPARE(facade.entryCount(), 2);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(failed.count(), 0);
}

void BgMusicFacadeTest::importUpperCaseJsonExtensionIsAccepted()
{
    BgMusicFacade facade;

    QVERIFY(facade.importFromText(
        QStringLiteral("MIX.JSON"),
        jsonFor(QStringLiteral("p1"), QStringLiteral("Mix"),
                {QStringLiteral("a")})));

    QCOMPARE(facade.playlistIds(), QStringList({QStringLiteral("p1")}));
    QCOMPARE(facade.entryCount(), 1);
}

void BgMusicFacadeTest::importM3uAddsAndSelectsPlaylist()
{
    BgMusicFacade facade;

    QVERIFY(facade.importFromText(
        QStringLiteral("Road Trip.m3u"),
        m3uFor({QStringLiteral("t1"), QStringLiteral("t2")})));

    // M3U carries no title, so the file's base name labels the row.
    QCOMPARE(facade.playlistTitles(), QStringList({QStringLiteral("Road Trip")}));
    QCOMPARE(facade.entryCount(), 2);
    QVERIFY(!facade.selectedId().isEmpty());

    const BgMusicPlaylist exported = bgMusicFromM3u(facade.exportM3u());
    QCOMPARE(exported.entries.size(), 2);
    QCOMPARE(exported.entries.at(0).trackId, QStringLiteral("t1"));
    QCOMPARE(exported.entries.at(1).trackId, QStringLiteral("t2"));
}

void BgMusicFacadeTest::importM3u8IsAccepted()
{
    BgMusicFacade facade;

    QVERIFY(facade.importFromText(
        QStringLiteral("mix.m3u8"),
        m3uFor({QStringLiteral("t1")})));

    QCOMPARE(facade.playlistTitles(), QStringList({QStringLiteral("mix")}));
    QCOMPARE(facade.entryCount(), 1);
}

void BgMusicFacadeTest::importJsonWithoutIdGeneratesUniqueIds()
{
    BgMusicFacade facade;

    QVERIFY(facade.importFromText(
        QStringLiteral("first.json"),
        jsonFor(QString(), QStringLiteral("First"), {QStringLiteral("a")})));
    QCOMPARE(facade.selectedId(), QStringLiteral("import-1"));

    QVERIFY(facade.importFromText(
        QStringLiteral("second.json"),
        jsonFor(QString(), QStringLiteral("Second"), {QStringLiteral("b")})));
    QCOMPARE(facade.selectedId(), QStringLiteral("import-2"));

    QCOMPARE(facade.playlistIds(),
             QStringList({QStringLiteral("import-1"), QStringLiteral("import-2")}));
    QCOMPARE(facade.entryCount(), 1);
}

void BgMusicFacadeTest::generatedIdSkipsAnExistingOne()
{
    BgMusicFacade facade;
    facade.setPlaylists({QStringLiteral("import-1")}, {QStringLiteral("Taken")});

    QVERIFY(facade.importFromText(
        QStringLiteral("mix.json"),
        jsonFor(QString(), QStringLiteral("Mix"), {QStringLiteral("a")})));

    QCOMPARE(facade.selectedId(), QStringLiteral("import-2"));
    QCOMPARE(facade.playlistIds(),
             QStringList({QStringLiteral("import-1"), QStringLiteral("import-2")}));
}

void BgMusicFacadeTest::importWithoutTitleUsesTheFileName()
{
    BgMusicFacade facade;

    QVERIFY(facade.importFromText(
        QStringLiteral("My Mix.json"),
        jsonFor(QStringLiteral("p1"), QString(), {QStringLiteral("a")})));

    QCOMPARE(facade.playlistTitles(), QStringList({QStringLiteral("My Mix")}));
}

void BgMusicFacadeTest::importRejectsEmptyContent()
{
    BgMusicFacade facade;
    QSignalSpy changed(&facade, &BgMusicFacade::changed);
    QSignalSpy failed(&facade, &BgMusicFacade::importFailed);

    QVERIFY(!facade.importFromText(QStringLiteral("mix.json"),
                                   QStringLiteral("  \n\t ")));

    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("empty"));
    QCOMPARE(changed.count(), 0);
    QVERIFY(facade.playlistIds().isEmpty());
}

void BgMusicFacadeTest::importRejectsUnknownExtension()
{
    BgMusicFacade facade;
    QSignalSpy failed(&facade, &BgMusicFacade::importFailed);

    QVERIFY(!facade.importFromText(
        QStringLiteral("mix.txt"),
        jsonFor(QStringLiteral("p1"), QStringLiteral("Mix"),
                {QStringLiteral("a")})));

    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.at(0).at(0).toString(),
             QStringLiteral("unsupported-extension"));
    QVERIFY(facade.playlistIds().isEmpty());
}

void BgMusicFacadeTest::importChoosesTheParserByExtension()
{
    BgMusicFacade facade;
    QSignalSpy failed(&facade, &BgMusicFacade::importFailed);

    // M3U text behind a .json name is not an M3U import: the extension picks
    // the parser, exactly like planBgMusicImport().
    QVERIFY(!facade.importFromText(QStringLiteral("mix.json"),
                                   m3uFor({QStringLiteral("t1")})));

    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("no-entries"));
    QVERIFY(facade.playlistIds().isEmpty());
}

void BgMusicFacadeTest::importRejectsTooLargeContent()
{
    BgMusicFacade facade;
    QSignalSpy failed(&facade, &BgMusicFacade::importFailed);

    // Valid JSON followed by trailing whitespace: only the size rule rejects it.
    QString content = jsonFor(QStringLiteral("p1"), QStringLiteral("Mix"),
                              {QStringLiteral("a")});
    content += QString(2 * 1024 * 1024, QLatin1Char(' '));

    QVERIFY(!facade.importFromText(QStringLiteral("mix.json"), content));

    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("too-large"));
    QVERIFY(facade.playlistIds().isEmpty());
}

void BgMusicFacadeTest::importRejectsContentWithoutEntries()
{
    BgMusicFacade facade;
    QSignalSpy changed(&facade, &BgMusicFacade::changed);
    QSignalSpy failed(&facade, &BgMusicFacade::importFailed);

    const QString content = QStringLiteral(
        R"({"version": 1, "id": "p1", "title": "Empty", "entries": []})");

    QVERIFY(!facade.importFromText(QStringLiteral("empty.json"), content));

    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("no-entries"));
    QCOMPARE(changed.count(), 0);
    QVERIFY(facade.playlistIds().isEmpty());
}

void BgMusicFacadeTest::importFailureLeavesStateUntouched()
{
    BgMusicFacade facade;
    facade.setPlaylists({QStringLiteral("a")}, {QStringLiteral("Alpha")});
    facade.select(QStringLiteral("a"));

    QSignalSpy changed(&facade, &BgMusicFacade::changed);
    QSignalSpy failed(&facade, &BgMusicFacade::importFailed);

    QVERIFY(!facade.importFromText(QStringLiteral("broken.json"),
                                   QStringLiteral("{nope")));

    QCOMPARE(changed.count(), 0);
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("no-entries"));
    QCOMPARE(facade.selectedId(), QStringLiteral("a"));
    QCOMPARE(facade.playlistIds(), QStringList({QStringLiteral("a")}));
}

void BgMusicFacadeTest::importReplacesPlaylistWithTheSameId()
{
    BgMusicFacade facade;
    QVERIFY(facade.importFromText(
        QStringLiteral("one.json"),
        jsonFor(QStringLiteral("p1"), QStringLiteral("One"),
                {QStringLiteral("a")})));
    QCOMPARE(facade.entryCount(), 1);

    QSignalSpy changed(&facade, &BgMusicFacade::changed);
    QVERIFY(facade.importFromText(
        QStringLiteral("two.json"),
        jsonFor(QStringLiteral("p1"), QStringLiteral("One updated"),
                {QStringLiteral("a"), QStringLiteral("b")})));

    // The id is the identity: no second row, and the newest payload wins.
    QCOMPARE(facade.playlistIds(), QStringList({QStringLiteral("p1")}));
    QCOMPARE(facade.playlistTitles(), QStringList({QStringLiteral("One updated")}));
    QCOMPARE(facade.entryCount(), 2);
    QCOMPARE(facade.selectedId(), QStringLiteral("p1"));
    QCOMPARE(changed.count(), 1);
}

void BgMusicFacadeTest::importSelectsTheImportedPlaylist()
{
    BgMusicFacade facade;
    facade.setPlaylists({QStringLiteral("a"), QStringLiteral("b")},
                        {QStringLiteral("Alpha"), QStringLiteral("Beta")});
    facade.select(QStringLiteral("a"));

    QVERIFY(facade.importFromText(
        QStringLiteral("new.json"),
        jsonFor(QStringLiteral("p1"), QStringLiteral("New"),
                {QStringLiteral("track")})));

    QCOMPARE(facade.selectedId(), QStringLiteral("p1"));
    QCOMPARE(facade.entryCount(), 1);
}

void BgMusicFacadeTest::reimportingIdenticalContentIsSilent()
{
    BgMusicFacade facade;
    const QString content = jsonFor(QStringLiteral("p1"), QStringLiteral("One"),
                                    {QStringLiteral("a")});
    QVERIFY(facade.importFromText(QStringLiteral("one.json"), content));

    QSignalSpy changed(&facade, &BgMusicFacade::changed);
    QVERIFY(facade.importFromText(QStringLiteral("one.json"), content));

    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.entryCount(), 1);
    QCOMPARE(facade.selectedId(), QStringLiteral("p1"));
}

void BgMusicFacadeTest::exportJsonReturnsTheSelectedPlaylist()
{
    BgMusicFacade facade;
    const BgMusicPlaylist original = playlist(
        QStringLiteral("p1"), QStringLiteral("Road Trip"),
        {QStringLiteral("t1"), QStringLiteral("t2")});

    QVERIFY(facade.importFromText(QStringLiteral("road-trip.json"),
                                  bgMusicToJson(original)));

    const BgMusicPlaylist exported = bgMusicFromJson(facade.exportJson());
    QCOMPARE(exported.id, original.id);
    QCOMPARE(exported.title, original.title);
    QCOMPARE(exported.updatedAt, original.updatedAt);
    QVERIFY(exported.entries == original.entries);
}

void BgMusicFacadeTest::exportJsonIsEmptyWithoutSelection()
{
    BgMusicFacade facade;
    facade.setPlaylists({QStringLiteral("a")}, {QStringLiteral("Alpha")});

    QCOMPARE(facade.exportJson(), QString());

    // A selected playlist with no known entries still exports a valid document.
    facade.select(QStringLiteral("a"));
    QVERIFY(!facade.exportJson().isEmpty());
    QCOMPARE(bgMusicFromJson(facade.exportJson()).entries.size(), 0);
}

void BgMusicFacadeTest::exportM3uReturnsTheSelectedPlaylist()
{
    BgMusicFacade facade;
    BgMusicPlaylist original = playlist(
        QStringLiteral("p1"), QStringLiteral("Mix"),
        {QStringLiteral("track-a"), QStringLiteral("track-b")});
    original.entries[0].startMs = 1500;

    QVERIFY(facade.importFromText(QStringLiteral("mix.json"),
                                  bgMusicToJson(original)));

    const QString m3u = facade.exportM3u();
    QVERIFY(m3u.startsWith(QStringLiteral("#EXTM3U")));
    QVERIFY(m3u.contains(QStringLiteral("#EXTINF:60,Title track-a - Artist")));
    QVERIFY(m3u.contains(QStringLiteral("#EXTBGSTART:1500")));

    const QStringList lines = m3u.split(QLatin1Char('\n'));
    QVERIFY(lines.contains(QStringLiteral("track-a")));
    QVERIFY(lines.contains(QStringLiteral("track-b")));

    const BgMusicPlaylist roundTrip = bgMusicFromM3u(m3u);
    QCOMPARE(roundTrip.entries.size(), 2);
    QCOMPARE(roundTrip.entries.at(0).startMs, 1500);
    QCOMPARE(roundTrip.entries.at(1).startMs, 0);
}

void BgMusicFacadeTest::exportM3uIsEmptyWithoutSelection()
{
    BgMusicFacade facade;
    facade.setPlaylists({QStringLiteral("a")}, {QStringLiteral("Alpha")});

    QCOMPARE(facade.exportM3u(), QString());

    // An empty playlist still has the format header.
    facade.select(QStringLiteral("a"));
    QCOMPARE(facade.exportM3u(), QStringLiteral("#EXTM3U"));
}

void BgMusicFacadeTest::exportFollowsTheSelection()
{
    BgMusicFacade facade;
    QVERIFY(facade.importFromText(
        QStringLiteral("one.json"),
        jsonFor(QStringLiteral("p1"), QStringLiteral("One"),
                {QStringLiteral("track-a")})));
    QVERIFY(facade.importFromText(
        QStringLiteral("two.json"),
        jsonFor(QStringLiteral("p2"), QStringLiteral("Two"),
                {QStringLiteral("track-b")})));

    // The second import is selected.
    QVERIFY(facade.exportM3u().contains(QStringLiteral("track-b")));
    QVERIFY(!facade.exportM3u().contains(QStringLiteral("track-a")));

    facade.select(QStringLiteral("p1"));
    QVERIFY(facade.exportM3u().contains(QStringLiteral("track-a")));
    QVERIFY(!facade.exportM3u().contains(QStringLiteral("track-b")));
}

void BgMusicFacadeTest::setPlaylistsPreservesImportedEntries()
{
    BgMusicFacade facade;
    QVERIFY(facade.importFromText(
        QStringLiteral("mix.json"),
        jsonFor(QStringLiteral("p1"), QStringLiteral("Mix"),
                {QStringLiteral("a"), QStringLiteral("b")})));
    QCOMPARE(facade.selectedId(), QStringLiteral("p1"));
    QCOMPARE(facade.entryCount(), 2);

    QSignalSpy spy(&facade, &BgMusicFacade::changed);
    facade.setPlaylists({QStringLiteral("p1"), QStringLiteral("p2")},
                        {QStringLiteral("Renamed"), QStringLiteral("Second")});

    // The title is the caller's, the entries are still the imported ones.
    QCOMPARE(facade.playlistTitles(),
             QStringList({QStringLiteral("Renamed"), QStringLiteral("Second")}));
    QCOMPARE(facade.selectedId(), QStringLiteral("p1"));
    QCOMPARE(facade.entryCount(), 2);
    QCOMPARE(spy.count(), 1);

    // The newly published playlist has no known content until it is imported.
    facade.select(QStringLiteral("p2"));
    QCOMPARE(facade.entryCount(), 0);
}

void BgMusicFacadeTest::setPlaylistsDropsEntriesOfRemovedPlaylists()
{
    BgMusicFacade facade;
    QVERIFY(facade.importFromText(
        QStringLiteral("mix.json"),
        jsonFor(QStringLiteral("p1"), QStringLiteral("Mix"),
                {QStringLiteral("a")})));

    facade.setPlaylists({QStringLiteral("p2")}, {QStringLiteral("Second")});

    QCOMPARE(facade.playlistIds(), QStringList({QStringLiteral("p2")}));
    QCOMPARE(facade.selectedId(), QString());

    // The dropped id cannot be selected any more.
    facade.select(QStringLiteral("p1"));
    QCOMPARE(facade.selectedId(), QString());
    QCOMPARE(facade.exportJson(), QString());
}

void BgMusicFacadeTest::clearForgetsEverything()
{
    BgMusicFacade facade;
    QVERIFY(facade.importFromText(
        QStringLiteral("mix.json"),
        jsonFor(QStringLiteral("p1"), QStringLiteral("Mix"),
                {QStringLiteral("a")})));

    QSignalSpy spy(&facade, &BgMusicFacade::changed);
    facade.clear();

    QVERIFY(facade.playlistIds().isEmpty());
    QVERIFY(facade.playlistTitles().isEmpty());
    QCOMPARE(facade.selectedId(), QString());
    QCOMPARE(facade.entryCount(), 0);
    QCOMPARE(facade.exportJson(), QString());
    QCOMPARE(facade.exportM3u(), QString());
    QCOMPARE(spy.count(), 1);
}

void BgMusicFacadeTest::clearOnEmptyIsSilent()
{
    BgMusicFacade facade;
    QSignalSpy spy(&facade, &BgMusicFacade::changed);

    facade.clear();

    QCOMPARE(spy.count(), 0);
}

QTEST_GUILESS_MAIN(BgMusicFacadeTest)
#include "tst_bg_music_facade.moc"
