#include "ui/LyricsFacade.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtTest>

using namespace beygram::ui;

namespace {

// Three lines deliberately out of order, with metadata tags that must be
// ignored: the facade exposes them sorted by start time.
const QString kThreeLines = QStringLiteral(
    "[ar:Artist]\n"
    "[ti:Title]\n"
    "[00:12.00]Second\n"
    "[00:05.50]First\n"
    "[00:20.00]Third\n");

} // namespace

class LyricsFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void parsesLrcIntoOrderedLines();
    void ignoresMetadataAndUntimedLines();
    void rejectsOversizedInput();
    void setLyricsReplacesPreviousLines();
    void setLyricsKeepsPlaybackPosition();
    void setLyricsWithoutTimestampsClearsLines();

    void currentIndexIsMinusOneBeforeFirstLine();
    void currentIndexAtLineBoundaries();
    void currentIndexWithoutLyricsIsMinusOne();
    void setPositionClampsNegative();

    void clearDropsLyrics();
    void clearKeepsPlaybackPosition();
    void clearOnEmptyIsNoOp();

    void hasLyricsReflectsStoredLines();

    void emitsChangedOnRealSetLyrics();
    void emitsChangedWhenLinesChangeButIndexStays();
    void doesNotEmitChangedForEquivalentLyrics();
    void emitsChangedWhenCurrentLineChanges();
    void doesNotEmitChangedWhileCurrentLineStays();
    void emitsChangedOncePerClear();
    void linesReturnsIndependentCopy();
};

void LyricsFacadeTest::startsEmpty()
{
    const LyricsFacade facade;

    QVERIFY(facade.lines().isEmpty());
    QCOMPARE(facade.currentIndex(), -1);
    QVERIFY(!facade.hasLyrics());
}

void LyricsFacadeTest::parsesLrcIntoOrderedLines()
{
    LyricsFacade facade;

    facade.setLyrics(kThreeLines);

    QCOMPARE(facade.lines(), (QStringList{QStringLiteral("First"),
                                          QStringLiteral("Second"),
                                          QStringLiteral("Third")}));
    QVERIFY(facade.hasLyrics());
    QCOMPARE(facade.currentIndex(), -1);
}

void LyricsFacadeTest::ignoresMetadataAndUntimedLines()
{
    LyricsFacade facade;

    facade.setLyrics(QStringLiteral(
        "[ar:Artist]\n"
        "plain words\n"
        "[00:01.00]\n"
        "[00:02.00]Kept\n"));

    QCOMPARE(facade.lines(), QStringList{QStringLiteral("Kept")});
    QCOMPARE(facade.currentIndex(), -1);
}

void LyricsFacadeTest::rejectsOversizedInput()
{
    LyricsFacade facade;

    // SyncedLyricsModel rejects anything over 256 KiB as a whole.
    facade.setLyrics(QStringLiteral("[00:01.00]")
                     + QString(300 * 1024, QLatin1Char('x')));

    QVERIFY(facade.lines().isEmpty());
    QVERIFY(!facade.hasLyrics());
}

void LyricsFacadeTest::setLyricsReplacesPreviousLines()
{
    LyricsFacade facade;
    facade.setLyrics(kThreeLines);

    facade.setLyrics(QStringLiteral("[00:01.00]Only line"));

    QCOMPARE(facade.lines(), QStringList{QStringLiteral("Only line")});
    QVERIFY(facade.hasLyrics());
}

void LyricsFacadeTest::setLyricsKeepsPlaybackPosition()
{
    LyricsFacade facade;
    facade.setLyrics(QStringLiteral("[00:00.00]A\n[00:05.00]B\n"));
    facade.setPosition(6000);
    QCOMPARE(facade.currentIndex(), 1);

    facade.setLyrics(QStringLiteral(
        "[00:00.00]A\n"
        "[00:03.00]B\n"
        "[00:06.00]C\n"
        "[00:09.00]D\n"));

    QCOMPARE(facade.lines().size(), 4);
    // The position stayed at 6000, which is the start of the third line.
    QCOMPARE(facade.currentIndex(), 2);
}

void LyricsFacadeTest::setLyricsWithoutTimestampsClearsLines()
{
    LyricsFacade facade;
    facade.setLyrics(kThreeLines);
    QSignalSpy changedSpy(&facade, &LyricsFacade::changed);

    facade.setLyrics(QStringLiteral("just words, no timestamps"));

    QVERIFY(facade.lines().isEmpty());
    QVERIFY(!facade.hasLyrics());
    QCOMPARE(facade.currentIndex(), -1);
    QCOMPARE(changedSpy.count(), 1);
}

void LyricsFacadeTest::currentIndexIsMinusOneBeforeFirstLine()
{
    LyricsFacade facade;
    facade.setLyrics(kThreeLines);

    facade.setPosition(0);
    QCOMPARE(facade.currentIndex(), -1);

    facade.setPosition(5499);
    QCOMPARE(facade.currentIndex(), -1);
}

void LyricsFacadeTest::currentIndexAtLineBoundaries()
{
    LyricsFacade facade;
    facade.setLyrics(kThreeLines);

    facade.setPosition(5500);
    QCOMPARE(facade.currentIndex(), 0);

    facade.setPosition(11999);
    QCOMPARE(facade.currentIndex(), 0);

    facade.setPosition(12000);
    QCOMPARE(facade.currentIndex(), 1);

    facade.setPosition(20000);
    QCOMPARE(facade.currentIndex(), 2);

    facade.setPosition(999999);
    QCOMPARE(facade.currentIndex(), 2);
}

void LyricsFacadeTest::currentIndexWithoutLyricsIsMinusOne()
{
    LyricsFacade facade;

    facade.setPosition(5000);

    QCOMPARE(facade.currentIndex(), -1);
}

void LyricsFacadeTest::setPositionClampsNegative()
{
    LyricsFacade facade;
    facade.setLyrics(QStringLiteral("[00:01.00]First"));
    facade.setPosition(5000);
    QCOMPARE(facade.currentIndex(), 0);

    // Negative input clamps to 0, which is before the first line at 1000 ms.
    facade.setPosition(-250);
    QCOMPARE(facade.currentIndex(), -1);

    facade.setPosition(0);
    QCOMPARE(facade.currentIndex(), -1);
}

void LyricsFacadeTest::clearDropsLyrics()
{
    LyricsFacade facade;
    facade.setLyrics(kThreeLines);

    facade.clear();

    QVERIFY(facade.lines().isEmpty());
    QVERIFY(!facade.hasLyrics());
    QCOMPARE(facade.currentIndex(), -1);
}

void LyricsFacadeTest::clearKeepsPlaybackPosition()
{
    LyricsFacade facade;
    const QString lyrics = QStringLiteral(
        "[00:00.00]A\n"
        "[00:03.00]B\n"
        "[00:06.00]C\n");
    facade.setLyrics(lyrics);
    facade.setPosition(6000);
    QCOMPARE(facade.currentIndex(), 2);

    facade.clear();
    facade.setLyrics(lyrics);

    // The position survived clear(), so the third line is current again.
    QCOMPARE(facade.currentIndex(), 2);
}

void LyricsFacadeTest::clearOnEmptyIsNoOp()
{
    LyricsFacade facade;
    QSignalSpy changedSpy(&facade, &LyricsFacade::changed);

    facade.clear();

    QCOMPARE(changedSpy.count(), 0);
    QVERIFY(facade.lines().isEmpty());
    QVERIFY(!facade.hasLyrics());
    QCOMPARE(facade.currentIndex(), -1);
}

void LyricsFacadeTest::hasLyricsReflectsStoredLines()
{
    LyricsFacade facade;
    QVERIFY(!facade.hasLyrics());

    facade.setLyrics(kThreeLines);
    QVERIFY(facade.hasLyrics());

    facade.setLyrics(QStringLiteral("[ar:Only metadata]"));
    QVERIFY(!facade.hasLyrics());

    facade.setLyrics(kThreeLines);
    QVERIFY(facade.hasLyrics());

    facade.clear();
    QVERIFY(!facade.hasLyrics());
}

void LyricsFacadeTest::emitsChangedOnRealSetLyrics()
{
    LyricsFacade facade;
    QSignalSpy changedSpy(&facade, &LyricsFacade::changed);

    facade.setLyrics(kThreeLines);

    QCOMPARE(changedSpy.count(), 1);
}

void LyricsFacadeTest::emitsChangedWhenLinesChangeButIndexStays()
{
    LyricsFacade facade;
    facade.setLyrics(QStringLiteral("[00:00.00]A"));
    facade.setPosition(0);
    QCOMPARE(facade.currentIndex(), 0);

    QSignalSpy changedSpy(&facade, &LyricsFacade::changed);
    facade.setLyrics(QStringLiteral("[00:00.00]B"));

    QCOMPARE(facade.currentIndex(), 0);
    QCOMPARE(facade.lines(), QStringList{QStringLiteral("B")});
    QCOMPARE(changedSpy.count(), 1);
}

void LyricsFacadeTest::doesNotEmitChangedForEquivalentLyrics()
{
    LyricsFacade facade;
    facade.setLyrics(kThreeLines);
    QSignalSpy changedSpy(&facade, &LyricsFacade::changed);

    facade.setLyrics(kThreeLines);

    QCOMPARE(changedSpy.count(), 0);
}

void LyricsFacadeTest::emitsChangedWhenCurrentLineChanges()
{
    LyricsFacade facade;
    facade.setLyrics(kThreeLines);
    QSignalSpy changedSpy(&facade, &LyricsFacade::changed);

    facade.setPosition(5500);
    QCOMPARE(facade.currentIndex(), 0);
    QCOMPARE(changedSpy.count(), 1);

    facade.setPosition(12000);
    QCOMPARE(facade.currentIndex(), 1);
    QCOMPARE(changedSpy.count(), 2);
}

void LyricsFacadeTest::doesNotEmitChangedWhileCurrentLineStays()
{
    LyricsFacade facade;
    facade.setLyrics(kThreeLines);
    facade.setPosition(5500);
    QCOMPARE(facade.currentIndex(), 0);

    QSignalSpy changedSpy(&facade, &LyricsFacade::changed);
    facade.setPosition(6000);
    facade.setPosition(11999);
    facade.setLyrics(kThreeLines);

    QCOMPARE(facade.currentIndex(), 0);
    QCOMPARE(changedSpy.count(), 0);
}

void LyricsFacadeTest::emitsChangedOncePerClear()
{
    LyricsFacade facade;
    facade.setLyrics(kThreeLines);
    QSignalSpy changedSpy(&facade, &LyricsFacade::changed);

    facade.clear();
    QCOMPARE(changedSpy.count(), 1);

    facade.clear();
    QCOMPARE(changedSpy.count(), 1);
}

void LyricsFacadeTest::linesReturnsIndependentCopy()
{
    LyricsFacade facade;
    facade.setLyrics(kThreeLines);

    QStringList copy = facade.lines();
    copy[0] = QStringLiteral("Changed");

    QCOMPARE(facade.lines().at(0), QStringLiteral("First"));
}

QTEST_GUILESS_MAIN(LyricsFacadeTest)
#include "tst_lyrics_facade.moc"
