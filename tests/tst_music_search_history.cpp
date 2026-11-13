#include "telegram/music/MusicSearchHistory.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <QtTest>

#include <initializer_list>

using namespace beygram::telegram;

namespace {

QStringList queries(std::initializer_list<QString> items)
{
    return QStringList(items);
}

MiniPlayerState playerState(bool hasTrack, bool playing, bool sheetOpen, bool fullScreen)
{
    MiniPlayerState state;
    state.hasTrack = hasTrack;
    state.playing = playing;
    state.sheetOpen = sheetOpen;
    state.fullScreen = fullScreen;
    return state;
}

} // namespace

class MusicSearchHistoryTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();

    void addTrimsAndIgnoresBlank();
    void addMovesExistingQueryToFrontIgnoringCase();
    void addCapsEntriesAndDropsTheOldest();
    void addUsesTwentyEntriesByDefault();
    void addWithNonPositiveCapKeepsNothing();
    void addSignalsOnlyWhenTheListChanges();

    void entriesReturnsACopy();

    void matchingUsesCaseInsensitivePrefix();
    void matchingBlankPrefixReturnsEveryEntry();

    void removeIgnoresCaseAndReportsWhetherItRemoved();
    void removeSignalsOnlyWhenSomethingWasDropped();

    void clearSignalsOnlyWhenSomethingWasDropped();

    void miniPlayerVisibleNeedsTrackAndNoOverlay();
    void miniPlayerExpandedFollowsPlaybackAndSheet();
};

void MusicSearchHistoryTest::startsEmpty()
{
    MusicSearchHistory history;
    QVERIFY(history.entries().isEmpty());
    QVERIFY(history.matching(QStringLiteral("a")).isEmpty());
}

void MusicSearchHistoryTest::addTrimsAndIgnoresBlank()
{
    MusicSearchHistory history;
    QSignalSpy spy(&history, &MusicSearchHistory::changed);

    history.add(QString());
    history.add(QStringLiteral("   "));
    history.add(QStringLiteral("\t\n"));

    QCOMPARE(spy.count(), 0);
    QVERIFY(history.entries().isEmpty());

    history.add(QStringLiteral("  lofi beats  "));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(history.entries(), queries({QStringLiteral("lofi beats")}));
}

void MusicSearchHistoryTest::addMovesExistingQueryToFrontIgnoringCase()
{
    MusicSearchHistory history;
    history.add(QStringLiteral("Rock"));
    history.add(QStringLiteral("Jazz"));
    history.add(QStringLiteral("Pop"));

    QCOMPARE(history.entries(),
             queries({QStringLiteral("Pop"), QStringLiteral("Jazz"), QStringLiteral("Rock")}));

    history.add(QStringLiteral("  jAzZ "));

    QCOMPARE(history.entries(),
             queries({QStringLiteral("jAzZ"), QStringLiteral("Pop"), QStringLiteral("Rock")}));
}

void MusicSearchHistoryTest::addCapsEntriesAndDropsTheOldest()
{
    MusicSearchHistory history;
    for (int i = 1; i <= 5; ++i)
        history.add(QStringLiteral("q%1").arg(i), 3);

    QCOMPARE(history.entries(),
             queries({QStringLiteral("q5"), QStringLiteral("q4"), QStringLiteral("q3")}));
}

void MusicSearchHistoryTest::addUsesTwentyEntriesByDefault()
{
    MusicSearchHistory history;
    for (int i = 1; i <= 25; ++i)
        history.add(QStringLiteral("q%1").arg(i));

    const QStringList stored = history.entries();
    QCOMPARE(int(stored.size()), 20);
    QCOMPARE(stored.first(), QStringLiteral("q25"));
    QCOMPARE(stored.last(), QStringLiteral("q6"));
}

void MusicSearchHistoryTest::addWithNonPositiveCapKeepsNothing()
{
    MusicSearchHistory history;
    history.add(QStringLiteral("one"));
    history.add(QStringLiteral("two"));

    QSignalSpy spy(&history, &MusicSearchHistory::changed);

    history.add(QStringLiteral("three"), 0);
    QCOMPARE(spy.count(), 1);
    QVERIFY(history.entries().isEmpty());

    history.add(QStringLiteral("four"), -5);
    QCOMPARE(spy.count(), 1);
    QVERIFY(history.entries().isEmpty());
}

void MusicSearchHistoryTest::addSignalsOnlyWhenTheListChanges()
{
    MusicSearchHistory history;
    QSignalSpy spy(&history, &MusicSearchHistory::changed);

    history.add(QStringLiteral("Rock"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(history.entries(), queries({QStringLiteral("Rock")}));

    history.add(QStringLiteral("Rock"));
    QCOMPARE(spy.count(), 1);

    history.add(QStringLiteral("rock"));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(history.entries(), queries({QStringLiteral("rock")}));
}

void MusicSearchHistoryTest::entriesReturnsACopy()
{
    MusicSearchHistory history;
    history.add(QStringLiteral("Rock"));

    QStringList copy = history.entries();
    copy.append(QStringLiteral("Jazz"));

    QCOMPARE(history.entries(), queries({QStringLiteral("Rock")}));
}

void MusicSearchHistoryTest::matchingUsesCaseInsensitivePrefix()
{
    MusicSearchHistory history;
    history.add(QStringLiteral("Rock"));
    history.add(QStringLiteral("R&B"));
    history.add(QStringLiteral("Jazz"));
    history.add(QStringLiteral("rock ballad"));

    QCOMPARE(history.matching(QStringLiteral("ro")),
             queries({QStringLiteral("rock ballad"), QStringLiteral("Rock")}));
    QCOMPARE(history.matching(QStringLiteral("RO")),
             queries({QStringLiteral("rock ballad"), QStringLiteral("Rock")}));
    QCOMPARE(history.matching(QStringLiteral("  ja  ")), queries({QStringLiteral("Jazz")}));
    QCOMPARE(history.matching(QStringLiteral("r")),
             queries({QStringLiteral("rock ballad"), QStringLiteral("R&B"), QStringLiteral("Rock")}));
    QVERIFY(history.matching(QStringLiteral("zzz")).isEmpty());
}

void MusicSearchHistoryTest::matchingBlankPrefixReturnsEveryEntry()
{
    MusicSearchHistory history;
    history.add(QStringLiteral("Rock"));
    history.add(QStringLiteral("Jazz"));

    QCOMPARE(history.entries(), queries({QStringLiteral("Jazz"), QStringLiteral("Rock")}));
    QCOMPARE(history.matching(QString()), history.entries());
    QCOMPARE(history.matching(QStringLiteral("   ")), history.entries());
}

void MusicSearchHistoryTest::removeIgnoresCaseAndReportsWhetherItRemoved()
{
    MusicSearchHistory history;
    history.add(QStringLiteral("Rock"));
    history.add(QStringLiteral("Jazz"));

    QSignalSpy spy(&history, &MusicSearchHistory::changed);

    QVERIFY(history.remove(QStringLiteral("  jazz ")));
    QCOMPARE(history.entries(), queries({QStringLiteral("Rock")}));
    QCOMPARE(spy.count(), 1);

    QVERIFY(!history.remove(QStringLiteral("jazz")));
    QVERIFY(!history.remove(QString()));
    QVERIFY(!history.remove(QStringLiteral("   ")));
    QCOMPARE(spy.count(), 1);

    QVERIFY(history.remove(QStringLiteral("ROCK")));
    QVERIFY(history.entries().isEmpty());
    QCOMPARE(spy.count(), 2);
}

void MusicSearchHistoryTest::removeSignalsOnlyWhenSomethingWasDropped()
{
    MusicSearchHistory history;
    QSignalSpy spy(&history, &MusicSearchHistory::changed);

    QVERIFY(!history.remove(QStringLiteral("missing")));
    QCOMPARE(spy.count(), 0);

    history.add(QStringLiteral("Rock"));
    QCOMPARE(spy.count(), 1);

    QVERIFY(!history.remove(QStringLiteral("Jazz")));
    QCOMPARE(spy.count(), 1);

    QVERIFY(history.remove(QStringLiteral("rock")));
    QCOMPARE(spy.count(), 2);
}

void MusicSearchHistoryTest::clearSignalsOnlyWhenSomethingWasDropped()
{
    MusicSearchHistory history;
    QSignalSpy spy(&history, &MusicSearchHistory::changed);

    history.clear();
    QCOMPARE(spy.count(), 0);

    history.add(QStringLiteral("Rock"));
    QCOMPARE(spy.count(), 1);

    history.clear();
    QCOMPARE(spy.count(), 2);
    QVERIFY(history.entries().isEmpty());

    history.clear();
    QCOMPARE(spy.count(), 2);
}

void MusicSearchHistoryTest::miniPlayerVisibleNeedsTrackAndNoOverlay()
{
    QVERIFY(!miniPlayerVisible(playerState(false, false, false, false)));
    QVERIFY(!miniPlayerVisible(playerState(false, true, false, false)));
    QVERIFY(miniPlayerVisible(playerState(true, false, false, false)));
    QVERIFY(miniPlayerVisible(playerState(true, true, false, false)));
    QVERIFY(!miniPlayerVisible(playerState(true, true, true, false)));
    QVERIFY(!miniPlayerVisible(playerState(true, true, false, true)));
    QVERIFY(!miniPlayerVisible(playerState(true, true, true, true)));
}

void MusicSearchHistoryTest::miniPlayerExpandedFollowsPlaybackAndSheet()
{
    QVERIFY(!miniPlayerExpanded(playerState(true, false, false, false)));
    QVERIFY(miniPlayerExpanded(playerState(true, true, false, false)));
    QVERIFY(!miniPlayerExpanded(playerState(true, true, true, false)));
    QVERIFY(!miniPlayerExpanded(playerState(true, false, true, false)));
    // Expansion describes the controls, not visibility: a full-screen player
    // hides the bar without changing what it would show.
    QVERIFY(miniPlayerExpanded(playerState(true, true, false, true)));
}

QTEST_GUILESS_MAIN(MusicSearchHistoryTest)
#include "tst_music_search_history.moc"
