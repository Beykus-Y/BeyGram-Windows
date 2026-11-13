#include "ui/MusicSearchFacade.h"

#include <QByteArray>
#include <QMetaObject>
#include <QMetaProperty>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtTest>

#include <initializer_list>

using beygram::ui::MusicSearchFacade;

namespace {

QStringList strings(std::initializer_list<QString> items)
{
    return QStringList(items);
}

/// Three tracks: two by a1, two on alb2, one without a duration.
void loadSampleLibrary(MusicSearchFacade& facade)
{
    facade.setLibrary(
        strings({QStringLiteral("t1"), QStringLiteral("t2"), QStringLiteral("t3")}),
        strings({QStringLiteral("Midnight City"), QStringLiteral("Northern Lights"),
                 QStringLiteral("Another Song")}),
        strings({QStringLiteral("a1"), QStringLiteral("a2"), QStringLiteral("a1")}),
        strings({QStringLiteral("alb1"), QStringLiteral("alb2"), QStringLiteral("alb2")}),
        strings({QStringLiteral("245"), QStringLiteral("0"), QStringLiteral("61")}));
}

} // namespace

class MusicSearchFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();

    void searchMatchesTitleAndReportsTheTrackId();
    void searchMatchesArtistAndAlbumIds();
    void searchTrimsAndIgnoresCase();
    void searchRecordsHistoryNewestFirstAndDeduped();
    void searchRecordsHistoryWithoutALibrary();
    void searchCapsHistoryAtTwenty();
    void blankSearchClearsResultsWithoutRecording();
    void clearDropsResultsAndLoadingButKeepsHistory();
    void clearOnEmptyStateIsSilent();

    void setLibraryPairsListsByIndex();
    void setLibrarySkipsEmptyAndRepeatedIds();
    void setLibraryParsesDurations();
    void setLibraryRerunsTheActiveQuery();
    void setLibraryRepaintsWhenOnlyTheResultIdChanges();
    void setLibraryWithoutAQueryKeepsResultsEmpty();

    void resultIdAtFollowsTheVisibleRows();
    void playResultAnnouncesOnlyPlayableRows();

    void addProviderLinkStoresSupportedLinksOnce();
    void addProviderLinkIgnoresMalformedAddresses();
    void providerLinksReturnsACopy();

    void setWavesPairsListsAndFallsBackToTheId();
    void setWavesSignalsOnlyOnARealChange();
    void wavesReturnsACopy();

    void loadingTogglesAndSignalsOnlyOnChange();
    void changedFollowsVisibleState();
    void resultsAndHistoryAreCopies();

    void propertiesNotifyThroughChanged();
};

void MusicSearchFacadeTest::startsEmpty()
{
    const MusicSearchFacade facade;

    QVERIFY(facade.query().isEmpty());
    QVERIFY(facade.results().isEmpty());
    QVERIFY(facade.history().isEmpty());
    QVERIFY(!facade.loading());
    QVERIFY(facade.waves().isEmpty());
    QVERIFY(facade.providerLinks().isEmpty());
    QVERIFY(facade.resultIdAt(0).isEmpty());
}

void MusicSearchFacadeTest::searchMatchesTitleAndReportsTheTrackId()
{
    MusicSearchFacade facade;
    loadSampleLibrary(facade);
    QSignalSpy changed(&facade, &MusicSearchFacade::changed);

    facade.search(QStringLiteral("midnight"));

    QCOMPARE(facade.query(), QStringLiteral("midnight"));
    QCOMPARE(facade.results(),
             strings({QStringLiteral("Midnight City \u2014 a1 \u00B7 4:05")}));
    QCOMPARE(facade.resultIdAt(0), QStringLiteral("t1"));
    QCOMPARE(facade.history(), strings({QStringLiteral("midnight")}));
    QCOMPARE(changed.count(), 1);
}

void MusicSearchFacadeTest::searchMatchesArtistAndAlbumIds()
{
    MusicSearchFacade facade;
    loadSampleLibrary(facade);

    facade.search(QStringLiteral("A1"));
    QCOMPARE(facade.results(),
             strings({QStringLiteral("Midnight City \u2014 a1 \u00B7 4:05"),
                      QStringLiteral("Another Song \u2014 a1 \u00B7 1:01")}));

    facade.search(QStringLiteral("alb2"));
    QCOMPARE(facade.results(),
             strings({QStringLiteral("Northern Lights \u2014 a2"),
                      QStringLiteral("Another Song \u2014 a1 \u00B7 1:01")}));
}

void MusicSearchFacadeTest::searchTrimsAndIgnoresCase()
{
    MusicSearchFacade facade;
    loadSampleLibrary(facade);

    facade.search(QStringLiteral("  NORTHERN  "));

    QCOMPARE(facade.query(), QStringLiteral("NORTHERN"));
    QCOMPARE(facade.results(), strings({QStringLiteral("Northern Lights \u2014 a2")}));
    // The history keeps the casing the user submitted.
    QCOMPARE(facade.history(), strings({QStringLiteral("NORTHERN")}));
}

void MusicSearchFacadeTest::searchRecordsHistoryNewestFirstAndDeduped()
{
    MusicSearchFacade facade;

    facade.search(QStringLiteral("Rock"));
    facade.search(QStringLiteral("Jazz"));
    facade.search(QStringLiteral("rock"));

    QCOMPARE(facade.history(),
             strings({QStringLiteral("rock"), QStringLiteral("Jazz")}));
    QCOMPARE(facade.query(), QStringLiteral("rock"));
}

void MusicSearchFacadeTest::searchRecordsHistoryWithoutALibrary()
{
    MusicSearchFacade facade;

    facade.search(QStringLiteral("  lofi  "));

    QVERIFY(facade.results().isEmpty());
    QCOMPARE(facade.history(), strings({QStringLiteral("lofi")}));
}

void MusicSearchFacadeTest::searchCapsHistoryAtTwenty()
{
    MusicSearchFacade facade;

    for (int i = 0; i < 21; ++i)
        facade.search(QStringLiteral("query%1").arg(i));

    const QStringList history = facade.history();
    QCOMPARE(history.size(), 20);
    QCOMPARE(history.first(), QStringLiteral("query20"));
    QCOMPARE(history.last(), QStringLiteral("query1"));
}

void MusicSearchFacadeTest::blankSearchClearsResultsWithoutRecording()
{
    MusicSearchFacade facade;
    loadSampleLibrary(facade);
    facade.search(QStringLiteral("midnight"));
    facade.setLoading(true);

    facade.search(QStringLiteral("   "));

    QVERIFY(facade.query().isEmpty());
    QVERIFY(facade.results().isEmpty());
    QVERIFY(facade.resultIdAt(0).isEmpty());
    QVERIFY(!facade.loading());
    QCOMPARE(facade.history(), strings({QStringLiteral("midnight")}));
}

void MusicSearchFacadeTest::clearDropsResultsAndLoadingButKeepsHistory()
{
    MusicSearchFacade facade;
    loadSampleLibrary(facade);
    facade.search(QStringLiteral("midnight"));
    facade.setLoading(true);
    facade.setWaves(strings({QStringLiteral("w1")}), strings({QStringLiteral("Focus")}));
    facade.addProviderLink(QStringLiteral("https://open.spotify.com/track/abc123"));

    QSignalSpy changed(&facade, &MusicSearchFacade::changed);
    facade.clear();

    QVERIFY(facade.query().isEmpty());
    QVERIFY(facade.results().isEmpty());
    QVERIFY(!facade.loading());
    QCOMPARE(facade.history(), strings({QStringLiteral("midnight")}));
    // Clearing the search is not clearing the page's other collections.
    QCOMPARE(facade.waves(), strings({QStringLiteral("Focus")}));
    QCOMPARE(facade.providerLinks().size(), 1);
    QCOMPARE(changed.count(), 1);
}

void MusicSearchFacadeTest::clearOnEmptyStateIsSilent()
{
    MusicSearchFacade facade;
    QSignalSpy changed(&facade, &MusicSearchFacade::changed);

    facade.clear();

    QCOMPARE(changed.count(), 0);
}

void MusicSearchFacadeTest::setLibraryPairsListsByIndex()
{
    MusicSearchFacade facade;

    facade.setLibrary(strings({QStringLiteral("t1"), QStringLiteral("t2")}),
                      strings({QStringLiteral("One")}),
                      strings({QStringLiteral("a1"), QStringLiteral("a2")}),
                      strings({QStringLiteral("alb1")}),
                      strings({QStringLiteral("10")}));

    facade.search(QStringLiteral("One"));
    QCOMPARE(facade.results(), strings({QStringLiteral("One \u2014 a1 \u00B7 0:10")}));

    // The second track had no title, album or duration; the artist survived.
    facade.search(QStringLiteral("a2"));
    QCOMPARE(facade.results(), strings({QStringLiteral("t2 \u2014 a2")}));
}

void MusicSearchFacadeTest::setLibrarySkipsEmptyAndRepeatedIds()
{
    MusicSearchFacade facade;

    facade.setLibrary(
        strings({QStringLiteral("  t1  "), QString(), QStringLiteral("t1"),
                 QStringLiteral("t2")}),
        strings({QStringLiteral("One"), QStringLiteral("Gap"), QStringLiteral("Duplicate"),
                 QStringLiteral("Two")}),
        strings({QStringLiteral("a1"), QStringLiteral("a2"), QStringLiteral("a3"),
                 QStringLiteral("a4")}),
        strings({QStringLiteral("alb1"), QStringLiteral("alb2"), QStringLiteral("alb3"),
                 QStringLiteral("alb4")}),
        strings({QStringLiteral("10"), QStringLiteral("20"), QStringLiteral("30"),
                 QStringLiteral("40")}));

    facade.search(QStringLiteral("One"));
    QCOMPARE(facade.resultIdAt(0), QStringLiteral("t1"));

    // The empty id was dropped and the first t1 entry won.
    facade.search(QStringLiteral("Gap"));
    QVERIFY(facade.results().isEmpty());
    facade.search(QStringLiteral("Duplicate"));
    QVERIFY(facade.results().isEmpty());

    facade.search(QStringLiteral("Two"));
    QCOMPARE(facade.resultIdAt(0), QStringLiteral("t2"));
}

void MusicSearchFacadeTest::setLibraryParsesDurations()
{
    MusicSearchFacade facade;

    facade.setLibrary(strings({QStringLiteral("a"), QStringLiteral("b"),
                               QStringLiteral("c"), QStringLiteral("d"),
                               QStringLiteral("e")}),
                      strings({QStringLiteral("A"), QStringLiteral("B"),
                               QStringLiteral("C"), QStringLiteral("D"),
                               QStringLiteral("E")}),
                      {}, {}, strings({QStringLiteral("245"), QStringLiteral("abc"),
                                      QStringLiteral("-5"), QStringLiteral("3601"),
                                      QStringLiteral("999999999999")}));

    facade.search(QStringLiteral("A"));
    QCOMPARE(facade.results(), strings({QStringLiteral("A \u00B7 4:05")}));
    facade.search(QStringLiteral("B"));
    QCOMPARE(facade.results(), strings({QStringLiteral("B")}));
    facade.search(QStringLiteral("C"));
    QCOMPARE(facade.results(), strings({QStringLiteral("C")}));
    facade.search(QStringLiteral("D"));
    QCOMPARE(facade.results(), strings({QStringLiteral("D \u00B7 1:00:01")}));
    // A value beyond int is clamped to INT_MAX rather than wrapping.
    facade.search(QStringLiteral("E"));
    QCOMPARE(facade.results(), strings({QStringLiteral("E \u00B7 596523:14:07")}));
}

void MusicSearchFacadeTest::setLibraryRerunsTheActiveQuery()
{
    MusicSearchFacade facade;
    loadSampleLibrary(facade);
    facade.search(QStringLiteral("Midnight"));

    QSignalSpy changed(&facade, &MusicSearchFacade::changed);
    facade.setLibrary(strings({QStringLiteral("t9")}),
                      strings({QStringLiteral("Midnight Run")}),
                      strings({QStringLiteral("a9")}), {},
                      strings({QStringLiteral("120")}));

    QCOMPARE(facade.query(), QStringLiteral("Midnight"));
    QCOMPARE(facade.results(),
             strings({QStringLiteral("Midnight Run \u2014 a9 \u00B7 2:00")}));
    QCOMPARE(facade.resultIdAt(0), QStringLiteral("t9"));
    QCOMPARE(facade.history(), strings({QStringLiteral("Midnight")}));
    QCOMPARE(changed.count(), 1);
}

void MusicSearchFacadeTest::setLibraryRepaintsWhenOnlyTheResultIdChanges()
{
    MusicSearchFacade facade;
    facade.setLibrary(strings({QStringLiteral("t1")}),
                      strings({QStringLiteral("Same")}),
                      strings({QStringLiteral("a1")}),
                      strings({QStringLiteral("alb1")}),
                      strings({QStringLiteral("10")}));
    facade.search(QStringLiteral("Same"));

    QSignalSpy changed(&facade, &MusicSearchFacade::changed);
    facade.setLibrary(strings({QStringLiteral("t2")}),
                      strings({QStringLiteral("Same")}),
                      strings({QStringLiteral("a1")}),
                      strings({QStringLiteral("alb1")}),
                      strings({QStringLiteral("10")}));

    // The visible label did not change, but the row now plays another track.
    QCOMPARE(facade.results(), strings({QStringLiteral("Same \u2014 a1 \u00B7 0:10")}));
    QCOMPARE(facade.resultIdAt(0), QStringLiteral("t2"));
    QCOMPARE(changed.count(), 1);
}

void MusicSearchFacadeTest::setLibraryWithoutAQueryKeepsResultsEmpty()
{
    MusicSearchFacade facade;
    loadSampleLibrary(facade);

    facade.setLibrary(strings({QStringLiteral("t9")}),
                      strings({QStringLiteral("Midnight Run")}), {}, {}, {});

    QVERIFY(facade.query().isEmpty());
    QVERIFY(facade.results().isEmpty());
}

void MusicSearchFacadeTest::resultIdAtFollowsTheVisibleRows()
{
    MusicSearchFacade facade;
    loadSampleLibrary(facade);
    facade.search(QStringLiteral("Song"));

    QCOMPARE(facade.results().size(), 1);
    QCOMPARE(facade.resultIdAt(0), QStringLiteral("t3"));
    QVERIFY(facade.resultIdAt(-1).isEmpty());
    QVERIFY(facade.resultIdAt(1).isEmpty());
    QVERIFY(facade.resultIdAt(99).isEmpty());
}

void MusicSearchFacadeTest::playResultAnnouncesOnlyPlayableRows()
{
    MusicSearchFacade facade;
    loadSampleLibrary(facade);
    facade.search(QStringLiteral("Song"));

    QSignalSpy play(&facade, &MusicSearchFacade::playRequested);

    facade.playResult(0);
    QCOMPARE(play.count(), 1);
    QCOMPARE(play.at(0).at(0).toString(), QStringLiteral("t3"));

    facade.playResult(-1);
    facade.playResult(1);
    QCOMPARE(play.count(), 1);

    facade.clear();
    facade.playResult(0);
    QCOMPARE(play.count(), 1);
}

void MusicSearchFacadeTest::addProviderLinkStoresSupportedLinksOnce()
{
    MusicSearchFacade facade;
    QSignalSpy changed(&facade, &MusicSearchFacade::changed);

    const QString spotify =
        QStringLiteral("https://open.spotify.com/track/4uLU6hMCjMI75M1A2tKUQC?si=abc");
    facade.addProviderLink(spotify);

    QCOMPARE(facade.providerLinks(),
             strings({QStringLiteral("Spotify \u00B7 Track \u00B7 4uLU6hMCjMI75M1A2tKUQC")}));
    QCOMPARE(changed.count(), 1);

    // The same provider/kind/id is stored once.
    facade.addProviderLink(spotify);
    QCOMPARE(facade.providerLinks().size(), 1);
    QCOMPARE(changed.count(), 1);

    facade.addProviderLink(QStringLiteral("https://music.yandex.ru/album/42/track/789"));
    QCOMPARE(facade.providerLinks().size(), 2);
    QCOMPARE(facade.providerLinks().at(1),
             QStringLiteral("Yandex Music \u00B7 Track \u00B7 789"));
    QCOMPARE(changed.count(), 2);
}

void MusicSearchFacadeTest::addProviderLinkIgnoresMalformedAddresses()
{
    MusicSearchFacade facade;
    QSignalSpy changed(&facade, &MusicSearchFacade::changed);

    facade.addProviderLink(QString());
    facade.addProviderLink(QStringLiteral("   "));
    facade.addProviderLink(QStringLiteral("https://example.com/track/1"));
    facade.addProviderLink(QStringLiteral("not a link"));

    QVERIFY(facade.providerLinks().isEmpty());
    QCOMPARE(changed.count(), 0);
}

void MusicSearchFacadeTest::providerLinksReturnsACopy()
{
    MusicSearchFacade facade;
    facade.addProviderLink(QStringLiteral("https://open.spotify.com/track/abc123"));

    QStringList copy = facade.providerLinks();
    QCOMPARE(copy.size(), 1);
    copy.append(QStringLiteral("extra"));

    QCOMPARE(facade.providerLinks().size(), 1);
}

void MusicSearchFacadeTest::setWavesPairsListsAndFallsBackToTheId()
{
    MusicSearchFacade facade;
    QSignalSpy changed(&facade, &MusicSearchFacade::changed);

    facade.setWaves(strings({QStringLiteral("w1"), QString(), QStringLiteral("w2"),
                             QStringLiteral("w1")}),
                    strings({QStringLiteral("Focus"), QStringLiteral("Gap"),
                             QStringLiteral("   "), QStringLiteral("Duplicate")}));

    QCOMPARE(facade.waves(), strings({QStringLiteral("Focus"), QStringLiteral("w2")}));
    QCOMPARE(changed.count(), 1);
}

void MusicSearchFacadeTest::setWavesSignalsOnlyOnARealChange()
{
    MusicSearchFacade facade;
    facade.setWaves(strings({QStringLiteral("w1"), QStringLiteral("w2")}),
                    strings({QStringLiteral("Focus"), QString()}));

    QSignalSpy changed(&facade, &MusicSearchFacade::changed);

    // Trimming makes this the same visible list.
    facade.setWaves(strings({QStringLiteral(" w1 "), QStringLiteral("w2")}),
                    strings({QStringLiteral(" Focus "), QString()}));
    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.waves(), strings({QStringLiteral("Focus"), QStringLiteral("w2")}));

    facade.setWaves(strings({QStringLiteral("w3")}), strings({QStringLiteral("Chill")}));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(facade.waves(), strings({QStringLiteral("Chill")}));
}

void MusicSearchFacadeTest::wavesReturnsACopy()
{
    MusicSearchFacade facade;
    facade.setWaves(strings({QStringLiteral("w1")}), strings({QStringLiteral("Focus")}));

    QStringList copy = facade.waves();
    copy.append(QStringLiteral("extra"));

    QCOMPARE(facade.waves(), strings({QStringLiteral("Focus")}));
}

void MusicSearchFacadeTest::loadingTogglesAndSignalsOnlyOnChange()
{
    MusicSearchFacade facade;
    QSignalSpy changed(&facade, &MusicSearchFacade::changed);

    facade.setLoading(true);
    QVERIFY(facade.loading());
    QCOMPARE(changed.count(), 1);

    facade.setLoading(true);
    QCOMPARE(changed.count(), 1);

    facade.setLoading(false);
    QVERIFY(!facade.loading());
    QCOMPARE(changed.count(), 2);
}

void MusicSearchFacadeTest::changedFollowsVisibleState()
{
    MusicSearchFacade facade;
    QSignalSpy changed(&facade, &MusicSearchFacade::changed);

    facade.search(QStringLiteral("rock"));
    QCOMPARE(changed.count(), 1);

    // Same query, same history, same empty result list.
    facade.search(QStringLiteral("  rock  "));
    QCOMPARE(changed.count(), 1);

    facade.search(QStringLiteral("jazz"));
    QCOMPARE(changed.count(), 2);

    facade.setLoading(true);
    QCOMPARE(changed.count(), 3);

    facade.setWaves(strings({QStringLiteral("w1")}), strings({QStringLiteral("Focus")}));
    QCOMPARE(changed.count(), 4);

    facade.addProviderLink(QStringLiteral("https://open.spotify.com/track/abc123"));
    QCOMPARE(changed.count(), 5);

    facade.clear();
    QCOMPARE(changed.count(), 6);

    // Already empty: nothing left to announce.
    facade.clear();
    QCOMPARE(changed.count(), 6);
}

void MusicSearchFacadeTest::resultsAndHistoryAreCopies()
{
    MusicSearchFacade facade;
    loadSampleLibrary(facade);
    facade.search(QStringLiteral("midnight"));

    QStringList results = facade.results();
    QStringList history = facade.history();
    results.clear();
    history.clear();

    QCOMPARE(facade.results().size(), 1);
    QCOMPARE(facade.history(), strings({QStringLiteral("midnight")}));
}

void MusicSearchFacadeTest::propertiesNotifyThroughChanged()
{
    MusicSearchFacade facade;
    const QMetaObject* meta = facade.metaObject();

    const QStringList names{QStringLiteral("query"), QStringLiteral("results"),
                            QStringLiteral("history"), QStringLiteral("loading")};
    for (const QString& name : names) {
        const int index = meta->indexOfProperty(name.toLatin1().constData());
        QVERIFY2(index >= 0, qPrintable(name));

        const QMetaProperty property = meta->property(index);
        QVERIFY2(property.hasNotifySignal(), qPrintable(name));
        QCOMPARE(QByteArray(property.notifySignal().name()), QByteArrayLiteral("changed"));
    }

    QVERIFY(meta->indexOfSignal("changed()") >= 0);
    QVERIFY(meta->indexOfSignal("playRequested(QString)") >= 0);

    QVERIFY(meta->indexOfMethod(
                "setLibrary(QStringList,QStringList,QStringList,QStringList,QStringList)")
            >= 0);
    QVERIFY(meta->indexOfMethod("search(QString)") >= 0);
    QVERIFY(meta->indexOfMethod("clear()") >= 0);
    QVERIFY(meta->indexOfMethod("addProviderLink(QString)") >= 0);
    QVERIFY(meta->indexOfMethod("waves()") >= 0);
    QVERIFY(meta->indexOfMethod("setWaves(QStringList,QStringList)") >= 0);
    QVERIFY(meta->indexOfMethod("resultIdAt(int)") >= 0);
    QVERIFY(meta->indexOfMethod("playResult(int)") >= 0);
    QVERIFY(meta->indexOfMethod("providerLinks()") >= 0);
    QVERIFY(meta->indexOfMethod("setLoading(bool)") >= 0);
}

QTEST_GUILESS_MAIN(MusicSearchFacadeTest)
#include "tst_music_search_facade.moc"
