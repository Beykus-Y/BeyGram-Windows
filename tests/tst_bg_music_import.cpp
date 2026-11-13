#include "telegram/music/BgMusicImport.h"

#include "telegram/music/BgMusic.h"

#include <QRegularExpression>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

constexpr qint64 kTwoMebibytes = 2LL * 1024 * 1024;

BgMusicPlaylist playlistWithTitle(const QString& title)
{
    BgMusicPlaylist playlist;
    playlist.id = QStringLiteral("playlist-1");
    playlist.title = title;
    playlist.entries = {
        BgMusicEntry{QStringLiteral("track-1"), QStringLiteral("Song"),
                     QStringLiteral("Artist"), 30, 0},
    };
    return playlist;
}

QString validJson()
{
    return bgMusicToJson(playlistWithTitle(QStringLiteral("Mix")));
}

QString validM3u()
{
    return QStringLiteral("#EXTM3U\n#EXTINF:30,Song - Artist\ntrack-1\n");
}

bool isSafeExportName(const QString& name)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9._-]+\\.json$"));
    return pattern.match(name).hasMatch();
}

} // namespace

class BgMusicImportTest final : public QObject {
    Q_OBJECT

private slots:
    void acceptsJsonPlaylist();
    void acceptsM3uPlaylist();
    void acceptsM3u8Playlist();
    void acceptsUpperCaseExtension();
    void rejectsEmptyContent();
    void rejectsZeroSize();
    void rejectsExactlyTwoMebibytes();
    void acceptsJustUnderTwoMebibytes();
    void rejectsUnknownExtension();
    void rejectsMalformedJson();
    void rejectsJsonWithoutEntries();
    void rejectsM3uWithoutEntries();
    void reportsPlaylistIdentity();

    void exportUsesSanitizedTitle();
    void exportReplacesUnsafeCharacters();
    void exportFallsBackForBlankTitle();
    void exportStripsSurroundingDots();
    void exportCapsLongTitles();
    void exportAlwaysEndsWithJson();
};

void BgMusicImportTest::acceptsJsonPlaylist()
{
    const QString content = validJson();
    const ImportDecision decision = planBgMusicImport(
        QStringLiteral("mix.json"), content.toUtf8().size(), content);

    QVERIFY(decision.accepted);
    QVERIFY(decision.reason.isEmpty());
    QCOMPARE(decision.playlistId, QStringLiteral("playlist-1"));
    QCOMPARE(decision.entryCount, 1);
}

void BgMusicImportTest::acceptsM3uPlaylist()
{
    const QString content = validM3u();
    const ImportDecision decision = planBgMusicImport(
        QStringLiteral("mix.m3u"), content.toUtf8().size(), content);

    QVERIFY(decision.accepted);
    QCOMPARE(decision.entryCount, 1);
}

void BgMusicImportTest::acceptsM3u8Playlist()
{
    const QString content = validM3u();
    const ImportDecision decision = planBgMusicImport(
        QStringLiteral("mix.m3u8"), content.toUtf8().size(), content);

    QVERIFY(decision.accepted);
    QCOMPARE(decision.entryCount, 1);
}

void BgMusicImportTest::acceptsUpperCaseExtension()
{
    const QString content = validJson();
    const ImportDecision decision = planBgMusicImport(
        QStringLiteral("MIX.JSON"), content.toUtf8().size(), content);

    QVERIFY(decision.accepted);
}

void BgMusicImportTest::rejectsEmptyContent()
{
    const ImportDecision decision =
        planBgMusicImport(QStringLiteral("mix.json"), 12, QStringLiteral("  \n\t "));

    QVERIFY(!decision.accepted);
    QCOMPARE(decision.reason, QStringLiteral("empty"));
    QCOMPARE(decision.entryCount, 0);
}

void BgMusicImportTest::rejectsZeroSize()
{
    const QString content = validJson();
    const ImportDecision decision =
        planBgMusicImport(QStringLiteral("mix.json"), 0, content);

    QVERIFY(!decision.accepted);
    QCOMPARE(decision.reason, QStringLiteral("empty"));
}

void BgMusicImportTest::rejectsExactlyTwoMebibytes()
{
    const QString content = validJson();
    const ImportDecision decision =
        planBgMusicImport(QStringLiteral("mix.json"), kTwoMebibytes, content);

    QVERIFY(!decision.accepted);
    QCOMPARE(decision.reason, QStringLiteral("too-large"));
}

void BgMusicImportTest::acceptsJustUnderTwoMebibytes()
{
    const QString content = validJson();
    const ImportDecision decision =
        planBgMusicImport(QStringLiteral("mix.json"), kTwoMebibytes - 1, content);

    QVERIFY(decision.accepted);
}

void BgMusicImportTest::rejectsUnknownExtension()
{
    const QString content = validJson();
    const ImportDecision decision =
        planBgMusicImport(QStringLiteral("mix.txt"), content.toUtf8().size(), content);

    QVERIFY(!decision.accepted);
    QCOMPARE(decision.reason, QStringLiteral("unsupported-extension"));
}

void BgMusicImportTest::rejectsMalformedJson()
{
    const ImportDecision decision =
        planBgMusicImport(QStringLiteral("mix.json"), 6, QStringLiteral("{nope"));

    QVERIFY(!decision.accepted);
    QCOMPARE(decision.reason, QStringLiteral("no-entries"));
}

void BgMusicImportTest::rejectsJsonWithoutEntries()
{
    const QString content = QStringLiteral(R"({"version": 1, "entries": []})");
    const ImportDecision decision =
        planBgMusicImport(QStringLiteral("mix.json"), content.toUtf8().size(), content);

    QVERIFY(!decision.accepted);
    QCOMPARE(decision.reason, QStringLiteral("no-entries"));
}

void BgMusicImportTest::rejectsM3uWithoutEntries()
{
    const QString content = QStringLiteral("#EXTM3U\n#EXTINF:10,Title - Artist\n");
    const ImportDecision decision =
        planBgMusicImport(QStringLiteral("mix.m3u"), content.toUtf8().size(), content);

    QVERIFY(!decision.accepted);
    QCOMPARE(decision.reason, QStringLiteral("no-entries"));
}

void BgMusicImportTest::reportsPlaylistIdentity()
{
    const QString content = QStringLiteral(R"({
        "version": 1,
        "id": "mix-42",
        "title": "Mix",
        "entries": [
            {"trackId": "a"},
            {"trackId": "b"},
            {"trackId": "a"}
        ]
    })");
    const ImportDecision decision =
        planBgMusicImport(QStringLiteral("mix.json"), content.toUtf8().size(), content);

    QVERIFY(decision.accepted);
    QCOMPARE(decision.playlistId, QStringLiteral("mix-42"));
    QCOMPARE(decision.entryCount, 2);
}

void BgMusicImportTest::exportUsesSanitizedTitle()
{
    const QString name = exportBgMusicFileName(playlistWithTitle(QStringLiteral("Road Trip")));

    QCOMPARE(name, QStringLiteral("Road_Trip.json"));
    QVERIFY(isSafeExportName(name));
}

void BgMusicImportTest::exportReplacesUnsafeCharacters()
{
    const QString name =
        exportBgMusicFileName(playlistWithTitle(QStringLiteral("My Mix: 2024?")));

    QCOMPARE(name, QStringLiteral("My_Mix__2024_.json"));
    QVERIFY(isSafeExportName(name));
}

void BgMusicImportTest::exportFallsBackForBlankTitle()
{
    QCOMPARE(exportBgMusicFileName(playlistWithTitle(QString())),
             QStringLiteral("playlist.json"));
    QCOMPARE(exportBgMusicFileName(playlistWithTitle(QStringLiteral("   "))),
             QStringLiteral("playlist.json"));
}

void BgMusicImportTest::exportStripsSurroundingDots()
{
    const QString name = exportBgMusicFileName(playlistWithTitle(QStringLiteral("...Mix...")));

    QCOMPARE(name, QStringLiteral("Mix.json"));
}

void BgMusicImportTest::exportCapsLongTitles()
{
    const QString name = exportBgMusicFileName(playlistWithTitle(QString(100, QLatin1Char('a'))));

    QCOMPARE(name, QString(80, QLatin1Char('a')) + QStringLiteral(".json"));
    QVERIFY(isSafeExportName(name));
}

void BgMusicImportTest::exportAlwaysEndsWithJson()
{
    const QString name =
        exportBgMusicFileName(playlistWithTitle(QStringLiteral("Caf\u00E9 / Live")));

    QVERIFY(name.endsWith(QStringLiteral(".json")));
    QVERIFY(isSafeExportName(name));
    QCOMPARE(name, QStringLiteral("Caf____Live.json"));
}

QTEST_GUILESS_MAIN(BgMusicImportTest)
#include "tst_bg_music_import.moc"
