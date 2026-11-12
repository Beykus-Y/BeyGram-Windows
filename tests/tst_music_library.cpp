#include "telegram/MusicLibrary.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

LibraryTrack track(qint64 id, const QString& title = {}, const QString& performer = {},
                   const QString& album = {}, int durationSeconds = 0, int fileId = 0)
{
    return LibraryTrack{id, title, performer, album, durationSeconds, fileId};
}

QVector<qint64> idsOf(const QVector<LibraryTrack>& tracks)
{
    QVector<qint64> ids;
    ids.reserve(tracks.size());
    for (const LibraryTrack& item : tracks)
        ids.append(item.id);
    return ids;
}

} // namespace

class MusicLibraryTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();

    void setTracksDropsNonPositiveIds();
    void setTracksDedupesByIdKeepingFirst();
    void setTracksKeepsIncomingOrder();
    void setTracksClampsNegativeDuration();
    void setTracksKeepsZeroDurationAndFileId();
    void setTracksReplacesPreviousTracks();

    void searchEmptyQueryReturnsAll();
    void searchBlankQueryReturnsAll();
    void searchMatchesTitleCaseInsensitively();
    void searchMatchesPerformerCaseInsensitively();
    void searchMatchesAlbumCaseInsensitively();
    void searchMatchesSubstringsAndTrimsQuery();
    void searchKeepsStoredOrder();
    void searchWithoutMatchIsEmpty();

    void performersAreUniqueSortedAndSkipEmpty();
    void performersKeepFirstSpelling();
    void performersOfEmptyLibraryAreEmpty();
    void albumsAreUniqueSortedAndSkipEmpty();
    void albumsOfEmptyLibraryAreEmpty();

    void byPerformerMatchesCaseInsensitively();
    void byPerformerIsExact();
    void byPerformerKeepsStoredOrder();
    void byPerformerEmptyMatchesNothing();
    void byPerformerWithoutMatchIsEmpty();

    void clearDropsTracks();
    void clearOnEmptyIsNoOp();

    void emitsChangedOnRealSetTracks();
    void emitsChangedWhenAnyFieldChanges();
    void doesNotEmitChangedForIdenticalSetTracks();
    void doesNotEmitChangedWhenOnlyDroppedTracksArrive();
    void doesNotEmitChangedWhenNormalizationLeavesTracksEqual();
    void emitsChangedOnlyOncePerClear();
    void queriesDoNotEmitChanged();
    void returnsIndependentCopies();
};

void MusicLibraryTest::startsEmpty()
{
    MusicLibrary library;

    QCOMPARE(library.count(), 0);
    QVERIFY(library.tracks().isEmpty());
    QVERIFY(library.search(QString()).isEmpty());
    QVERIFY(library.search(QStringLiteral("anything")).isEmpty());
    QVERIFY(library.performers().isEmpty());
    QVERIFY(library.albums().isEmpty());
    QVERIFY(library.byPerformer(QStringLiteral("anyone")).isEmpty());
}

void MusicLibraryTest::setTracksDropsNonPositiveIds()
{
    MusicLibrary library;
    library.setTracks({track(0, QStringLiteral("Zero")),
                       track(-4, QStringLiteral("Negative")),
                       track(7, QStringLiteral("Kept"))});

    QCOMPARE(library.count(), 1);
    QCOMPARE(library.tracks().at(0).id, 7);
    QCOMPARE(library.tracks().at(0).title, QStringLiteral("Kept"));
}

void MusicLibraryTest::setTracksDedupesByIdKeepingFirst()
{
    MusicLibrary library;
    library.setTracks({track(5, QStringLiteral("First"), QStringLiteral("A"),
                            QStringLiteral("Album One"), 10, 100),
                       track(5, QStringLiteral("Second"), QStringLiteral("B"),
                            QStringLiteral("Album Two"), 20, 200)});

    QCOMPARE(library.count(), 1);

    const LibraryTrack kept = library.tracks().at(0);
    QCOMPARE(kept.id, 5);
    QCOMPARE(kept.title, QStringLiteral("First"));
    QCOMPARE(kept.performer, QStringLiteral("A"));
    QCOMPARE(kept.album, QStringLiteral("Album One"));
    QCOMPARE(kept.durationSeconds, 10);
    QCOMPARE(kept.fileId, 100);
}

void MusicLibraryTest::setTracksKeepsIncomingOrder()
{
    MusicLibrary library;
    library.setTracks({track(9), track(2), track(30)});

    QCOMPARE(idsOf(library.tracks()), (QVector<qint64>{9, 2, 30}));
}

void MusicLibraryTest::setTracksClampsNegativeDuration()
{
    MusicLibrary library;
    library.setTracks({track(1, QStringLiteral("Song"), {}, {}, -12)});

    QCOMPARE(library.count(), 1);
    QCOMPARE(library.tracks().at(0).durationSeconds, 0);
}

void MusicLibraryTest::setTracksKeepsZeroDurationAndFileId()
{
    MusicLibrary library;
    library.setTracks({track(1, QStringLiteral("Song"))});

    QCOMPARE(library.count(), 1);
    QCOMPARE(library.tracks().at(0).durationSeconds, 0);
    QCOMPARE(library.tracks().at(0).fileId, 0);
}

void MusicLibraryTest::setTracksReplacesPreviousTracks()
{
    MusicLibrary library;
    library.setTracks({track(1), track(2)});
    library.setTracks({track(3)});

    QCOMPARE(library.count(), 1);
    QCOMPARE(library.tracks().at(0).id, 3);
}

void MusicLibraryTest::searchEmptyQueryReturnsAll()
{
    MusicLibrary library;
    library.setTracks({track(1, QStringLiteral("One")),
                       track(2, QStringLiteral("Two"))});

    const QVector<LibraryTrack> all = library.search(QString());

    QCOMPARE(all.size(), 2);
    QCOMPARE(idsOf(all), (QVector<qint64>{1, 2}));
}

void MusicLibraryTest::searchBlankQueryReturnsAll()
{
    MusicLibrary library;
    library.setTracks({track(1, QStringLiteral("One")),
                       track(2, QStringLiteral("Two"))});

    QCOMPARE(library.search(QStringLiteral("   ")).size(), 2);
    QCOMPARE(library.search(QStringLiteral("\t\n")).size(), 2);
}

void MusicLibraryTest::searchMatchesTitleCaseInsensitively()
{
    MusicLibrary library;
    library.setTracks({track(1, QStringLiteral("Get Lucky"), QStringLiteral("Daft Punk"),
                            QStringLiteral("Random Access Memories")),
                       track(2, QStringLiteral("Around the World"), QStringLiteral("Daft Punk"),
                            QStringLiteral("Homework"))});

    QCOMPARE(idsOf(library.search(QStringLiteral("lucky"))), (QVector<qint64>{1}));
    QCOMPARE(idsOf(library.search(QStringLiteral("LUCKY"))), (QVector<qint64>{1}));
}

void MusicLibraryTest::searchMatchesPerformerCaseInsensitively()
{
    MusicLibrary library;
    library.setTracks({track(1, QStringLiteral("One"), QStringLiteral("Daft Punk")),
                       track(2, QStringLiteral("Two"), QStringLiteral("daft punk")),
                       track(3, QStringLiteral("Three"), QStringLiteral("Other"))});

    QCOMPARE(idsOf(library.search(QStringLiteral("DAFT PUNK"))),
             (QVector<qint64>{1, 2}));
}

void MusicLibraryTest::searchMatchesAlbumCaseInsensitively()
{
    MusicLibrary library;
    library.setTracks({track(1, QStringLiteral("One"), QStringLiteral("A"),
                            QStringLiteral("Random Access Memories")),
                       track(2, QStringLiteral("Two"), QStringLiteral("B"),
                            QStringLiteral("Homework"))});

    QCOMPARE(idsOf(library.search(QStringLiteral("homework"))),
             (QVector<qint64>{2}));
    QCOMPARE(idsOf(library.search(QStringLiteral("RANDOM ACCESS"))),
             (QVector<qint64>{1}));
}

void MusicLibraryTest::searchMatchesSubstringsAndTrimsQuery()
{
    MusicLibrary library;
    library.setTracks({track(1, QStringLiteral("Get Lucky"), QStringLiteral("Daft Punk"),
                            QStringLiteral("Random Access Memories")),
                       track(2, QStringLiteral("Around the World"), QStringLiteral("Daft Punk"),
                            QStringLiteral("Homework"))});

    // Title substring, album substring and performer substring all match.
    QCOMPARE(idsOf(library.search(QStringLiteral("  world  "))),
             (QVector<qint64>{2}));
    QCOMPARE(idsOf(library.search(QStringLiteral("cess"))),
             (QVector<qint64>{1}));
    QCOMPARE(idsOf(library.search(QStringLiteral("ft pu"))),
             (QVector<qint64>{1, 2}));
}

void MusicLibraryTest::searchKeepsStoredOrder()
{
    MusicLibrary library;
    library.setTracks({track(30, QStringLiteral("Alpha")),
                       track(2, QStringLiteral("Alpha")),
                       track(9, QStringLiteral("Alpha"))});

    QCOMPARE(idsOf(library.search(QStringLiteral("alpha"))),
             (QVector<qint64>{30, 2, 9}));
}

void MusicLibraryTest::searchWithoutMatchIsEmpty()
{
    MusicLibrary library;
    library.setTracks({track(1, QStringLiteral("One"), QStringLiteral("A"),
                            QStringLiteral("First"))});

    QVERIFY(library.search(QStringLiteral("zzz")).isEmpty());
}

void MusicLibraryTest::performersAreUniqueSortedAndSkipEmpty()
{
    MusicLibrary library;
    library.setTracks({track(1, {}, QStringLiteral("zeta")),
                       track(2, {}, QString()),
                       track(3, {}, QStringLiteral("Alpha")),
                       track(4, {}, QStringLiteral("beta")),
                       track(5, {}, QStringLiteral("ZETA")),
                       track(6, {}, QStringLiteral("alpha"))});

    QCOMPARE(library.performers(),
             (QStringList{QStringLiteral("Alpha"), QStringLiteral("beta"),
                          QStringLiteral("zeta")}));
}

void MusicLibraryTest::performersKeepFirstSpelling()
{
    MusicLibrary library;
    library.setTracks({track(1, {}, QStringLiteral("Daft Punk")),
                       track(2, {}, QStringLiteral("daft punk")),
                       track(3, {}, QStringLiteral("DAFT PUNK"))});

    QCOMPARE(library.performers(), QStringList{QStringLiteral("Daft Punk")});
}

void MusicLibraryTest::performersOfEmptyLibraryAreEmpty()
{
    MusicLibrary library;

    QVERIFY(library.performers().isEmpty());
}

void MusicLibraryTest::albumsAreUniqueSortedAndSkipEmpty()
{
    MusicLibrary library;
    library.setTracks({track(1, {}, {}, QStringLiteral("Zebra")),
                       track(2, {}, {}, QString()),
                       track(3, {}, {}, QStringLiteral("apple")),
                       track(4, {}, {}, QStringLiteral("Banana")),
                       track(5, {}, {}, QStringLiteral("zebra"))});

    QCOMPARE(library.albums(),
             (QStringList{QStringLiteral("apple"), QStringLiteral("Banana"),
                          QStringLiteral("Zebra")}));
}

void MusicLibraryTest::albumsOfEmptyLibraryAreEmpty()
{
    MusicLibrary library;

    QVERIFY(library.albums().isEmpty());
}

void MusicLibraryTest::byPerformerMatchesCaseInsensitively()
{
    MusicLibrary library;
    library.setTracks({track(1, QStringLiteral("One"), QStringLiteral("Daft Punk")),
                       track(2, QStringLiteral("Two"), QStringLiteral("daft punk")),
                       track(3, QStringLiteral("Three"), QStringLiteral("Other"))});

    const QVector<LibraryTrack> matches =
        library.byPerformer(QStringLiteral("DAFT PUNK"));

    QCOMPARE(matches.size(), 2);
    QCOMPARE(idsOf(matches), (QVector<qint64>{1, 2}));
}

void MusicLibraryTest::byPerformerIsExact()
{
    MusicLibrary library;
    library.setTracks({track(1, QStringLiteral("One"), QStringLiteral("Daft Punk"))});

    QCOMPARE(idsOf(library.byPerformer(QStringLiteral("Daft Punk"))),
             (QVector<qint64>{1}));
    QVERIFY(library.byPerformer(QStringLiteral("Daft")).isEmpty());
    QVERIFY(library.byPerformer(QStringLiteral("Daft Punk ")).isEmpty());
    QVERIFY(library.byPerformer(QStringLiteral("daft punk!")).isEmpty());
}

void MusicLibraryTest::byPerformerKeepsStoredOrder()
{
    MusicLibrary library;
    library.setTracks({track(4, {}, QStringLiteral("Band")),
                       track(2, {}, QStringLiteral("band")),
                       track(9, {}, QStringLiteral("BAND"))});

    QCOMPARE(idsOf(library.byPerformer(QStringLiteral("Band"))),
             (QVector<qint64>{4, 2, 9}));
}

void MusicLibraryTest::byPerformerEmptyMatchesNothing()
{
    MusicLibrary library;
    library.setTracks({track(1, QStringLiteral("Untagged")),
                       track(2, QStringLiteral("Named"), QStringLiteral("Someone"))});

    QVERIFY(library.byPerformer(QString()).isEmpty());
}

void MusicLibraryTest::byPerformerWithoutMatchIsEmpty()
{
    MusicLibrary library;
    library.setTracks({track(1, {}, QStringLiteral("Someone"))});

    QVERIFY(library.byPerformer(QStringLiteral("Nobody")).isEmpty());
}

void MusicLibraryTest::clearDropsTracks()
{
    MusicLibrary library;
    library.setTracks({track(1, QStringLiteral("One"), QStringLiteral("A")),
                       track(2, QStringLiteral("Two"), QStringLiteral("B"))});

    library.clear();

    QCOMPARE(library.count(), 0);
    QVERIFY(library.tracks().isEmpty());
    QVERIFY(library.search(QString()).isEmpty());
    QVERIFY(library.performers().isEmpty());
    QVERIFY(library.albums().isEmpty());
}

void MusicLibraryTest::clearOnEmptyIsNoOp()
{
    MusicLibrary library;
    QSignalSpy spy(&library, &MusicLibrary::changed);

    library.clear();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(library.count(), 0);
}

void MusicLibraryTest::emitsChangedOnRealSetTracks()
{
    MusicLibrary library;
    QSignalSpy spy(&library, &MusicLibrary::changed);

    library.setTracks({track(1, QStringLiteral("Song"))});

    QCOMPARE(spy.count(), 1);
}

void MusicLibraryTest::emitsChangedWhenAnyFieldChanges()
{
    MusicLibrary library;
    library.setTracks({track(1, QStringLiteral("Song"), QStringLiteral("Band"),
                            QStringLiteral("Album"), 10, 100)});
    QSignalSpy spy(&library, &MusicLibrary::changed);

    library.setTracks({track(1, QStringLiteral("Renamed"), QStringLiteral("Band"),
                            QStringLiteral("Album"), 10, 100)});
    QCOMPARE(spy.count(), 1);

    library.setTracks({track(1, QStringLiteral("Renamed"), QStringLiteral("Other"),
                            QStringLiteral("Album"), 10, 100)});
    QCOMPARE(spy.count(), 2);

    library.setTracks({track(1, QStringLiteral("Renamed"), QStringLiteral("Other"),
                            QStringLiteral("Other Album"), 10, 100)});
    QCOMPARE(spy.count(), 3);

    library.setTracks({track(1, QStringLiteral("Renamed"), QStringLiteral("Other"),
                            QStringLiteral("Other Album"), 11, 100)});
    QCOMPARE(spy.count(), 4);

    library.setTracks({track(1, QStringLiteral("Renamed"), QStringLiteral("Other"),
                            QStringLiteral("Other Album"), 11, 101)});
    QCOMPARE(spy.count(), 5);
}

void MusicLibraryTest::doesNotEmitChangedForIdenticalSetTracks()
{
    MusicLibrary library;
    library.setTracks({track(1, QStringLiteral("Song"), QStringLiteral("Band"),
                            QStringLiteral("Album"), 12, 99)});
    QSignalSpy spy(&library, &MusicLibrary::changed);

    library.setTracks({track(1, QStringLiteral("Song"), QStringLiteral("Band"),
                            QStringLiteral("Album"), 12, 99)});

    QCOMPARE(spy.count(), 0);
}

void MusicLibraryTest::doesNotEmitChangedWhenOnlyDroppedTracksArrive()
{
    MusicLibrary library;
    QSignalSpy spy(&library, &MusicLibrary::changed);

    library.setTracks({track(0, QStringLiteral("Zero")),
                       track(-3, QStringLiteral("Negative"))});
    QCOMPARE(spy.count(), 0);
    QCOMPARE(library.count(), 0);

    library.setTracks({track(1, QStringLiteral("Song"))});
    QCOMPARE(spy.count(), 1);

    library.setTracks({track(1, QStringLiteral("Song")),
                       track(0, QStringLiteral("Dropped"))});
    QCOMPARE(spy.count(), 1);
}

void MusicLibraryTest::doesNotEmitChangedWhenNormalizationLeavesTracksEqual()
{
    MusicLibrary library;
    library.setTracks({track(1, QStringLiteral("Song"), {}, {}, -5)});
    QCOMPARE(library.tracks().at(0).durationSeconds, 0);

    QSignalSpy spy(&library, &MusicLibrary::changed);

    library.setTracks({track(1, QStringLiteral("Song"), {}, {}, 0)});
    QCOMPARE(spy.count(), 0);

    library.setTracks({track(1, QStringLiteral("Song"), {}, {}, -9)});
    QCOMPARE(spy.count(), 0);
}

void MusicLibraryTest::emitsChangedOnlyOncePerClear()
{
    MusicLibrary library;
    library.setTracks({track(1), track(2)});
    QSignalSpy spy(&library, &MusicLibrary::changed);

    library.clear();
    library.clear();

    QCOMPARE(spy.count(), 1);
}

void MusicLibraryTest::queriesDoNotEmitChanged()
{
    MusicLibrary library;
    library.setTracks({track(1, QStringLiteral("Song"), QStringLiteral("Band"),
                            QStringLiteral("Album"))});
    QSignalSpy spy(&library, &MusicLibrary::changed);

    QCOMPARE(library.search(QStringLiteral("song")).size(), 1);
    QCOMPARE(library.search(QString()).size(), 1);
    QCOMPARE(library.performers().size(), 1);
    QCOMPARE(library.albums().size(), 1);
    QCOMPARE(library.byPerformer(QStringLiteral("Band")).size(), 1);

    QCOMPARE(spy.count(), 0);
}

void MusicLibraryTest::returnsIndependentCopies()
{
    MusicLibrary library;
    library.setTracks({track(1, QStringLiteral("Song"), QStringLiteral("Band"),
                            QStringLiteral("Album"))});

    QVector<LibraryTrack> copy = library.tracks();
    copy[0].title = QStringLiteral("Changed");
    QCOMPARE(library.tracks().at(0).title, QStringLiteral("Song"));

    QVector<LibraryTrack> matches = library.search(QStringLiteral("song"));
    QCOMPARE(matches.size(), 1);
    matches[0].title = QStringLiteral("Changed");
    QCOMPARE(library.tracks().at(0).title, QStringLiteral("Song"));
}

QTEST_GUILESS_MAIN(MusicLibraryTest)
#include "tst_music_library.moc"
