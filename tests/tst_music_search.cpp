#include "telegram/music/MusicSearch.h"

#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

MusicTrackModel track(const QString& trackId, const QString& title,
                      const QString& artistId = {}, const QString& albumId = {})
{
    return MusicTrackModel{trackId, title, artistId, albumId};
}

QVector<MusicTrackModel> sampleTracks()
{
    return {
        track(QStringLiteral("t1"), QStringLiteral("Midnight City"), QStringLiteral("a1"),
              QStringLiteral("alb1")),
        track(QStringLiteral("t2"), QStringLiteral("Northern Lights"), QStringLiteral("a2"),
              QStringLiteral("alb2")),
        track(QStringLiteral("t3"), QStringLiteral("Another Song"), QStringLiteral("a1"),
              QStringLiteral("alb2")),
        track(QStringLiteral("t4"), QStringLiteral("Silent Track"), QStringLiteral("a3"),
              QStringLiteral("alb3")),
    };
}

QVector<QString> idsOf(const QVector<MusicTrackModel>& tracks)
{
    QVector<QString> ids;
    ids.reserve(tracks.size());
    for (const MusicTrackModel& item : tracks)
        ids.append(item.id);
    return ids;
}

QVector<MusicTrackModel> numberedTracks(int count)
{
    QVector<MusicTrackModel> tracks;
    tracks.reserve(count);
    for (int i = 1; i <= count; ++i)
        tracks.append(track(QStringLiteral("t%1").arg(i), QStringLiteral("Track %1").arg(i)));
    return tracks;
}

} // namespace

class MusicSearchTest final : public QObject {
    Q_OBJECT

private slots:
    void emptyQueryReturnsEveryTrackInOrder();
    void blankTextMatchesEveryTrack();
    void textMatchesTitleIgnoringCase();
    void textMatchesArtistIdIgnoringCase();
    void textMatchesAlbumIdIgnoringCase();
    void textMatchesSubstring();
    void textIsTrimmed();
    void textDoesNotMatchTrackId();
    void textWithoutMatchIsEmpty();

    void artistFilterKeepsOnlyThatArtist();
    void artistFilterIsExact();
    void artistFilterIsTrimmed();
    void albumFilterKeepsOnlyThatAlbum();
    void albumFilterIsExact();

    void filtersAreAnded();
    void allFiltersCombine();

    void favoritesOnlyKeepsListedTracks();
    void favoritesOnlyWithEmptyFavoritesIsEmpty();
    void favoritesAreIgnoredWithoutFlag();
    void favoriteDuplicatesDoNotDuplicate();
    void favoritesMatchExactTrackId();
    void emptyFavoriteIdDoesNotMatchEmptyTrackId();

    void limitZeroClampsToOne();
    void negativeLimitClampsToOne();
    void defaultLimitIsFifty();
    void limitAboveTwoHundredClampsToTwoHundred();
    void limitAppliesAfterFiltering();

    void orderIsPreserved();
    void emptyTrackListIsEmpty();
    void noMatchesIsEmpty();
    void resultIsIndependentCopy();
};

void MusicSearchTest::emptyQueryReturnsEveryTrackInOrder()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();

    const QVector<MusicTrackModel> matches = MusicSearchEngine::search(tracks, {}, MusicSearchQuery{});

    QCOMPARE(idsOf(matches),
             QVector<QString>({QStringLiteral("t1"), QStringLiteral("t2"), QStringLiteral("t3"),
                               QStringLiteral("t4")}));
}

void MusicSearchTest::blankTextMatchesEveryTrack()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.text = QStringLiteral("   ");

    QCOMPARE(MusicSearchEngine::search(tracks, {}, query).size(), 4);
}

void MusicSearchTest::textMatchesTitleIgnoringCase()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.text = QStringLiteral("MIDNIGHT");

    QCOMPARE(idsOf(MusicSearchEngine::search(tracks, {}, query)),
             QVector<QString>({QStringLiteral("t1")}));
}

void MusicSearchTest::textMatchesArtistIdIgnoringCase()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.text = QStringLiteral("A1");

    QCOMPARE(idsOf(MusicSearchEngine::search(tracks, {}, query)),
             QVector<QString>({QStringLiteral("t1"), QStringLiteral("t3")}));
}

void MusicSearchTest::textMatchesAlbumIdIgnoringCase()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.text = QStringLiteral("ALB2");

    QCOMPARE(idsOf(MusicSearchEngine::search(tracks, {}, query)),
             QVector<QString>({QStringLiteral("t2"), QStringLiteral("t3")}));
}

void MusicSearchTest::textMatchesSubstring()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.text = QStringLiteral("song");

    QCOMPARE(idsOf(MusicSearchEngine::search(tracks, {}, query)),
             QVector<QString>({QStringLiteral("t3")}));
}

void MusicSearchTest::textIsTrimmed()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.text = QStringLiteral("  midnight  ");

    QCOMPARE(idsOf(MusicSearchEngine::search(tracks, {}, query)),
             QVector<QString>({QStringLiteral("t1")}));
}

void MusicSearchTest::textDoesNotMatchTrackId()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.text = QStringLiteral("t1");

    QVERIFY(MusicSearchEngine::search(tracks, {}, query).isEmpty());
}

void MusicSearchTest::textWithoutMatchIsEmpty()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.text = QStringLiteral("nothing here");

    QVERIFY(MusicSearchEngine::search(tracks, {}, query).isEmpty());
}

void MusicSearchTest::artistFilterKeepsOnlyThatArtist()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.artistId = QStringLiteral("a1");

    QCOMPARE(idsOf(MusicSearchEngine::search(tracks, {}, query)),
             QVector<QString>({QStringLiteral("t1"), QStringLiteral("t3")}));
}

void MusicSearchTest::artistFilterIsExact()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.artistId = QStringLiteral("a");

    QVERIFY(MusicSearchEngine::search(tracks, {}, query).isEmpty());
}

void MusicSearchTest::artistFilterIsTrimmed()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.artistId = QStringLiteral("  a2 ");

    QCOMPARE(idsOf(MusicSearchEngine::search(tracks, {}, query)),
             QVector<QString>({QStringLiteral("t2")}));
}

void MusicSearchTest::albumFilterKeepsOnlyThatAlbum()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.albumId = QStringLiteral("alb2");

    QCOMPARE(idsOf(MusicSearchEngine::search(tracks, {}, query)),
             QVector<QString>({QStringLiteral("t2"), QStringLiteral("t3")}));
}

void MusicSearchTest::albumFilterIsExact()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.albumId = QStringLiteral("alb");

    QVERIFY(MusicSearchEngine::search(tracks, {}, query).isEmpty());
}

void MusicSearchTest::filtersAreAnded()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.text = QStringLiteral("song");
    query.artistId = QStringLiteral("a1");

    QCOMPARE(idsOf(MusicSearchEngine::search(tracks, {}, query)),
             QVector<QString>({QStringLiteral("t3")}));
}

void MusicSearchTest::allFiltersCombine()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.text = QStringLiteral("another");
    query.artistId = QStringLiteral("a1");
    query.albumId = QStringLiteral("alb2");
    query.favoritesOnly = true;

    const QVector<QString> favorites{QStringLiteral("t1"), QStringLiteral("t3")};

    QCOMPARE(idsOf(MusicSearchEngine::search(tracks, favorites, query)),
             QVector<QString>({QStringLiteral("t3")}));
}

void MusicSearchTest::favoritesOnlyKeepsListedTracks()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.favoritesOnly = true;

    const QVector<QString> favorites{QStringLiteral("t4"), QStringLiteral("t2")};

    QCOMPARE(idsOf(MusicSearchEngine::search(tracks, favorites, query)),
             QVector<QString>({QStringLiteral("t2"), QStringLiteral("t4")}));
}

void MusicSearchTest::favoritesOnlyWithEmptyFavoritesIsEmpty()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.favoritesOnly = true;

    QVERIFY(MusicSearchEngine::search(tracks, {}, query).isEmpty());
}

void MusicSearchTest::favoritesAreIgnoredWithoutFlag()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    const QVector<QString> favorites{QStringLiteral("t2")};

    QCOMPARE(MusicSearchEngine::search(tracks, favorites, query).size(), 4);
}

void MusicSearchTest::favoriteDuplicatesDoNotDuplicate()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.favoritesOnly = true;

    const QVector<QString> favorites{QStringLiteral("t2"), QStringLiteral("t2"),
                                     QStringLiteral("t2")};

    QCOMPARE(idsOf(MusicSearchEngine::search(tracks, favorites, query)),
             QVector<QString>({QStringLiteral("t2")}));
}

void MusicSearchTest::favoritesMatchExactTrackId()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.favoritesOnly = true;

    const QVector<QString> favorites{QStringLiteral("T2")};

    QVERIFY(MusicSearchEngine::search(tracks, favorites, query).isEmpty());
}

void MusicSearchTest::emptyFavoriteIdDoesNotMatchEmptyTrackId()
{
    const QVector<MusicTrackModel> tracks{track(QString(), QStringLiteral("Untitled"))};
    MusicSearchQuery query;
    query.favoritesOnly = true;

    const QVector<QString> favorites{QString()};

    QVERIFY(MusicSearchEngine::search(tracks, favorites, query).isEmpty());
}

void MusicSearchTest::limitZeroClampsToOne()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.limit = 0;

    QCOMPARE(idsOf(MusicSearchEngine::search(tracks, {}, query)),
             QVector<QString>({QStringLiteral("t1")}));
}

void MusicSearchTest::negativeLimitClampsToOne()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.limit = -10;

    QCOMPARE(MusicSearchEngine::search(tracks, {}, query).size(), 1);
}

void MusicSearchTest::defaultLimitIsFifty()
{
    const QVector<MusicTrackModel> tracks = numberedTracks(60);

    const QVector<MusicTrackModel> matches = MusicSearchEngine::search(tracks, {}, MusicSearchQuery{});

    QCOMPARE(matches.size(), 50);
    QCOMPARE(matches.first().id, QStringLiteral("t1"));
    QCOMPARE(matches.last().id, QStringLiteral("t50"));
}

void MusicSearchTest::limitAboveTwoHundredClampsToTwoHundred()
{
    const QVector<MusicTrackModel> tracks = numberedTracks(250);
    MusicSearchQuery query;
    query.limit = 1000;

    const QVector<MusicTrackModel> matches = MusicSearchEngine::search(tracks, {}, query);

    QCOMPARE(matches.size(), 200);
    QCOMPARE(matches.last().id, QStringLiteral("t200"));
}

void MusicSearchTest::limitAppliesAfterFiltering()
{
    QVector<MusicTrackModel> tracks;
    for (int i = 1; i <= 6; ++i) {
        tracks.append(track(QStringLiteral("t%1").arg(i), QStringLiteral("Track %1").arg(i),
                            i % 2 == 0 ? QStringLiteral("even") : QStringLiteral("odd")));
    }

    MusicSearchQuery query;
    query.artistId = QStringLiteral("even");
    query.limit = 2;

    QCOMPARE(idsOf(MusicSearchEngine::search(tracks, {}, query)),
             QVector<QString>({QStringLiteral("t2"), QStringLiteral("t4")}));
}

void MusicSearchTest::orderIsPreserved()
{
    const QVector<MusicTrackModel> tracks{
        track(QStringLiteral("c"), QStringLiteral("C")),
        track(QStringLiteral("a"), QStringLiteral("A")),
        track(QStringLiteral("b"), QStringLiteral("B")),
    };

    QCOMPARE(idsOf(MusicSearchEngine::search(tracks, {}, MusicSearchQuery{})),
             QVector<QString>({QStringLiteral("c"), QStringLiteral("a"), QStringLiteral("b")}));
}

void MusicSearchTest::emptyTrackListIsEmpty()
{
    QVERIFY(MusicSearchEngine::search({}, {}, MusicSearchQuery{}).isEmpty());
}

void MusicSearchTest::noMatchesIsEmpty()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();
    MusicSearchQuery query;
    query.text = QStringLiteral("zzz");

    QVERIFY(MusicSearchEngine::search(tracks, {}, query).isEmpty());
}

void MusicSearchTest::resultIsIndependentCopy()
{
    const QVector<MusicTrackModel> tracks = sampleTracks();

    QVector<MusicTrackModel> matches = MusicSearchEngine::search(tracks, {}, MusicSearchQuery{});
    matches[0].title = QStringLiteral("Changed");

    QCOMPARE(tracks.at(0).title, QStringLiteral("Midnight City"));
}

QTEST_GUILESS_MAIN(MusicSearchTest)
#include "tst_music_search.moc"
