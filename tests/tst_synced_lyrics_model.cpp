#include "telegram/SyncedLyricsModel.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QVector<int> startsOf(const QVector<LyricLine>& lines)
{
    QVector<int> starts;
    starts.reserve(lines.size());
    for (const LyricLine& line : lines)
        starts.append(line.startMs);
    return starts;
}

QStringList textsOf(const QVector<LyricLine>& lines)
{
    QStringList texts;
    texts.reserve(lines.size());
    for (const LyricLine& line : lines)
        texts.append(line.text);
    return texts;
}

} // namespace

class SyncedLyricsModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();

    void parsesBasicLines();
    void parsesEveryFractionWidth();
    void parsesSeveralTimestampsOnOneLine();
    void parsesCrlfAndLoneCr();
    void sortsLinesByStartTime();
    void dropsLinesWithoutText();
    void ignoresMetadataAndMalformedLines();
    void rejectsSecondsAtSixty();
    void acceptsThreeDigitMinutes();
    void mergesSameStartKeepingFirst();
    void rejectsOversizedInput();

    void appliesPositiveOffset();
    void appliesNegativeOffset();
    void clampsOffsetResultToZero();
    void offsetTagIsCaseInsensitiveAndGlobal();

    void currentIndexIsMinusOneBeforeFirstLine();
    void currentIndexAtLineBoundaries();
    void currentIndexWithoutLyricsIsMinusOne();
    void currentTextFollowsCurrentIndex();

    void setPositionClampsNegative();
    void setLyricsReplacesPreviousLines();
    void setLyricsKeepsPlaybackPosition();
    void setLyricsWithNoTimestampsClearsLines();

    void hasLyricsReflectsStoredLines();
    void clearDropsLines();
    void clearOnEmptyIsNoOp();

    void emitsChangedOnRealSetLyrics();
    void doesNotEmitChangedForEquivalentLyrics();
    void emitsChangedOncePerSetLyrics();
    void emitsChangedOnPositionChange();
    void doesNotEmitChangedForSamePosition();
    void doesNotEmitChangedWhenNegativeClampsToZero();
    void returnsIndependentCopies();
};

void SyncedLyricsModelTest::startsEmpty()
{
    SyncedLyricsModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.lines().isEmpty());
    QVERIFY(!model.hasLyrics());
    QCOMPARE(model.currentIndex(), -1);
    QVERIFY(model.currentText().isEmpty());
}

void SyncedLyricsModelTest::parsesBasicLines()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:05.00]Second\n"
                                   "[00:01.25]First"));

    QCOMPARE(model.count(), 2);
    QCOMPARE(startsOf(model.lines()), (QVector<int>{1250, 5000}));
    QCOMPARE(textsOf(model.lines()),
             QStringList({QStringLiteral("First"), QStringLiteral("Second")}));
}

void SyncedLyricsModelTest::parsesEveryFractionWidth()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:01]A\n"
                                   "[00:02.5]B\n"
                                   "[00:03.25]C\n"
                                   "[00:04.125]D\n"
                                   "[00:05:5]E\n"
                                   "[0:06.05]F\n"
                                   "[000:07.001]G"));

    QCOMPARE(startsOf(model.lines()),
             (QVector<int>{1000, 2500, 3250, 4125, 5500, 6050, 7001}));
    QCOMPARE(textsOf(model.lines()),
             QStringList({QStringLiteral("A"), QStringLiteral("B"),
                          QStringLiteral("C"), QStringLiteral("D"),
                          QStringLiteral("E"), QStringLiteral("F"),
                          QStringLiteral("G")}));
}

void SyncedLyricsModelTest::parsesSeveralTimestampsOnOneLine()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:30.00][00:10.00][00:20.00]Chorus"));

    QCOMPARE(model.count(), 3);
    QCOMPARE(startsOf(model.lines()), (QVector<int>{10000, 20000, 30000}));
    QCOMPARE(textsOf(model.lines()),
             QStringList({QStringLiteral("Chorus"), QStringLiteral("Chorus"),
                          QStringLiteral("Chorus")}));
}

void SyncedLyricsModelTest::parsesCrlfAndLoneCr()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:02.00]Two\r\n"
                                   "[00:01.00]One\r"
                                   "[00:03.00]Three"));

    QCOMPARE(model.count(), 3);
    QCOMPARE(startsOf(model.lines()), (QVector<int>{1000, 2000, 3000}));
    QCOMPARE(textsOf(model.lines()),
             QStringList({QStringLiteral("One"), QStringLiteral("Two"),
                          QStringLiteral("Three")}));
}

void SyncedLyricsModelTest::sortsLinesByStartTime()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:05.00]Last\n"
                                   "[00:01.00]First\n"
                                   "[00:03.00]Middle"));

    QCOMPARE(startsOf(model.lines()), (QVector<int>{1000, 3000, 5000}));
    QCOMPARE(textsOf(model.lines()),
             QStringList({QStringLiteral("First"), QStringLiteral("Middle"),
                          QStringLiteral("Last")}));
}

void SyncedLyricsModelTest::dropsLinesWithoutText()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:01.00]\n"
                                   "[00:02.00]   \n"
                                   "[00:03.00]  Kept  "));

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.lines().at(0).startMs, 3000);
    QCOMPARE(model.lines().at(0).text, QStringLiteral("Kept"));
}

void SyncedLyricsModelTest::ignoresMetadataAndMalformedLines()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[ti:Title]\n"
                                   "[ar:Artist]\n"
                                   "[al:Album]\n"
                                   "plain text\n"
                                   "[xx:yy]Nope\n"
                                   "[00:01.00]Good\n"
                                   "[00:61.00]Bad seconds\n"
                                   "[00:02]Good too"));

    QCOMPARE(model.count(), 2);
    QCOMPARE(startsOf(model.lines()), (QVector<int>{1000, 2000}));
    QCOMPARE(textsOf(model.lines()),
             QStringList({QStringLiteral("Good"), QStringLiteral("Good too")}));
}

void SyncedLyricsModelTest::rejectsSecondsAtSixty()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:60.00]Sixty\n"
                                   "[00:59.99]Fifty nine"));

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.lines().at(0).startMs, 59990);
    QCOMPARE(model.lines().at(0).text, QStringLiteral("Fifty nine"));
}

void SyncedLyricsModelTest::acceptsThreeDigitMinutes()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[123:45.67]Long"));

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.lines().at(0).startMs, 123 * 60000 + 45000 + 670);
}

void SyncedLyricsModelTest::mergesSameStartKeepingFirst()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:10.00]Echo\n"
                                   "[00:10.00]Other\n"
                                   "[0:10.000]Third\n"
                                   "[00:10.00][00:10.00]Echo"));

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.lines().at(0).startMs, 10000);
    QCOMPARE(model.lines().at(0).text, QStringLiteral("Echo"));
}

void SyncedLyricsModelTest::rejectsOversizedInput()
{
    SyncedLyricsModel model;
    const QString oversized = QStringLiteral("[00:01.00]")
        + QString(256 * 1024, QLatin1Char('x'));

    QVERIFY(oversized.size() > 256 * 1024);

    model.setLyrics(oversized);

    QVERIFY(!model.hasLyrics());
    QCOMPARE(model.count(), 0);
}

void SyncedLyricsModelTest::appliesPositiveOffset()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[offset:500]\n"
                                   "[00:01.00]A\n"
                                   "[00:00.20]B"));

    QCOMPARE(startsOf(model.lines()), (QVector<int>{0, 500}));
    QCOMPARE(textsOf(model.lines()),
             QStringList({QStringLiteral("B"), QStringLiteral("A")}));
}

void SyncedLyricsModelTest::appliesNegativeOffset()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[offset:-250]\n"
                                   "[00:01.00]A"));

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.lines().at(0).startMs, 1250);
}

void SyncedLyricsModelTest::clampsOffsetResultToZero()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[offset:5000]\n"
                                   "[00:01.00]A"));

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.lines().at(0).startMs, 0);
}

void SyncedLyricsModelTest::offsetTagIsCaseInsensitiveAndGlobal()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:05.00]Late\n"
                                   "[OFFSET:-1000]\n"
                                   "[00:01.00]Early"));

    // Android parity: startMs - offset, so a negative offset moves lines later.
    QCOMPARE(startsOf(model.lines()), (QVector<int>{2000, 6000}));
    QCOMPARE(textsOf(model.lines()),
             QStringList({QStringLiteral("Early"), QStringLiteral("Late")}));
}

void SyncedLyricsModelTest::currentIndexIsMinusOneBeforeFirstLine()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:01.00]One"));

    model.setPositionMs(999);

    QCOMPARE(model.currentIndex(), -1);
    QVERIFY(model.currentText().isEmpty());
}

void SyncedLyricsModelTest::currentIndexAtLineBoundaries()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:01.00]One\n"
                                   "[00:02.00]Two\n"
                                   "[00:03.00]Three"));

    model.setPositionMs(999);
    QCOMPARE(model.currentIndex(), -1);

    model.setPositionMs(1000);
    QCOMPARE(model.currentIndex(), 0);

    model.setPositionMs(1999);
    QCOMPARE(model.currentIndex(), 0);

    model.setPositionMs(2000);
    QCOMPARE(model.currentIndex(), 1);

    model.setPositionMs(2999);
    QCOMPARE(model.currentIndex(), 1);

    model.setPositionMs(3000);
    QCOMPARE(model.currentIndex(), 2);

    model.setPositionMs(60000);
    QCOMPARE(model.currentIndex(), 2);
}

void SyncedLyricsModelTest::currentIndexWithoutLyricsIsMinusOne()
{
    SyncedLyricsModel model;
    model.setPositionMs(5000);

    QCOMPARE(model.currentIndex(), -1);
    QVERIFY(model.currentText().isEmpty());
}

void SyncedLyricsModelTest::currentTextFollowsCurrentIndex()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:01.00]One\n"
                                   "[00:02.00]Two\n"
                                   "[00:03.00]Three"));

    model.setPositionMs(999);
    QVERIFY(model.currentText().isEmpty());

    model.setPositionMs(1000);
    QCOMPARE(model.currentText(), QStringLiteral("One"));

    model.setPositionMs(2500);
    QCOMPARE(model.currentText(), QStringLiteral("Two"));

    model.setPositionMs(3000);
    QCOMPARE(model.currentText(), QStringLiteral("Three"));
}

void SyncedLyricsModelTest::setPositionClampsNegative()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:00.00]Start\n"
                                   "[00:01.00]Next"));

    model.setPositionMs(-500);
    QCOMPARE(model.currentIndex(), 0);

    model.setPositionMs(1000);
    QCOMPARE(model.currentIndex(), 1);

    model.setPositionMs(-1);
    QCOMPARE(model.currentIndex(), 0);
}

void SyncedLyricsModelTest::setLyricsReplacesPreviousLines()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:01.00]Old one\n"
                                   "[00:02.00]Old two"));
    model.setLyrics(QStringLiteral("[00:05.00]New"));

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.lines().at(0).startMs, 5000);
    QCOMPARE(model.lines().at(0).text, QStringLiteral("New"));
}

void SyncedLyricsModelTest::setLyricsKeepsPlaybackPosition()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:01.00]One\n"
                                   "[00:02.00]Two"));
    model.setPositionMs(2000);
    QCOMPARE(model.currentIndex(), 1);

    model.setLyrics(QStringLiteral("[00:01.00]One\n"
                                   "[00:02.00]Two\n"
                                   "[00:03.00]Three"));

    QCOMPARE(model.count(), 3);
    QCOMPARE(model.currentIndex(), 1);
    QCOMPARE(model.currentText(), QStringLiteral("Two"));
}

void SyncedLyricsModelTest::setLyricsWithNoTimestampsClearsLines()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:01.00]One"));
    QSignalSpy spy(&model, &SyncedLyricsModel::changed);

    model.setLyrics(QStringLiteral("just words, no timestamps"));

    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.hasLyrics());
    QCOMPARE(model.count(), 0);
}

void SyncedLyricsModelTest::hasLyricsReflectsStoredLines()
{
    SyncedLyricsModel model;
    QVERIFY(!model.hasLyrics());

    model.setLyrics(QStringLiteral("[00:01.00]One"));
    QVERIFY(model.hasLyrics());

    model.clear();
    QVERIFY(!model.hasLyrics());
}

void SyncedLyricsModelTest::clearDropsLines()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:01.00]One\n"
                                   "[00:02.00]Two"));

    model.clear();

    QCOMPARE(model.count(), 0);
    QVERIFY(model.lines().isEmpty());
    QVERIFY(!model.hasLyrics());
    QCOMPARE(model.currentIndex(), -1);
    QVERIFY(model.currentText().isEmpty());
}

void SyncedLyricsModelTest::clearOnEmptyIsNoOp()
{
    SyncedLyricsModel model;
    QSignalSpy spy(&model, &SyncedLyricsModel::changed);

    model.clear();

    QCOMPARE(spy.count(), 0);
}

void SyncedLyricsModelTest::emitsChangedOnRealSetLyrics()
{
    SyncedLyricsModel model;
    QSignalSpy spy(&model, &SyncedLyricsModel::changed);

    model.setLyrics(QStringLiteral("[00:01.00]One"));

    QCOMPARE(spy.count(), 1);
}

void SyncedLyricsModelTest::doesNotEmitChangedForEquivalentLyrics()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:01.50]Same"));
    QSignalSpy spy(&model, &SyncedLyricsModel::changed);

    model.setLyrics(QStringLiteral("[0:01.500]Same"));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.lines().at(0).startMs, 1500);
}

void SyncedLyricsModelTest::emitsChangedOncePerSetLyrics()
{
    SyncedLyricsModel model;
    QSignalSpy spy(&model, &SyncedLyricsModel::changed);

    model.setLyrics(QStringLiteral("[00:03.00]Three\n"
                                   "[00:01.00]One\n"
                                   "[00:02.00]Two"));

    QCOMPARE(spy.count(), 1);
}

void SyncedLyricsModelTest::emitsChangedOnPositionChange()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:01.00]One\n"
                                   "[00:02.00]Two"));
    QSignalSpy spy(&model, &SyncedLyricsModel::changed);

    model.setPositionMs(1500);
    QCOMPARE(spy.count(), 1);

    model.setPositionMs(2500);
    QCOMPARE(spy.count(), 2);
}

void SyncedLyricsModelTest::doesNotEmitChangedForSamePosition()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:01.00]One"));
    model.setPositionMs(1500);
    QSignalSpy spy(&model, &SyncedLyricsModel::changed);

    model.setPositionMs(1500);

    QCOMPARE(spy.count(), 0);
}

void SyncedLyricsModelTest::doesNotEmitChangedWhenNegativeClampsToZero()
{
    SyncedLyricsModel model;
    QSignalSpy spy(&model, &SyncedLyricsModel::changed);

    model.setPositionMs(-100);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.currentIndex(), -1);
}

void SyncedLyricsModelTest::returnsIndependentCopies()
{
    SyncedLyricsModel model;
    model.setLyrics(QStringLiteral("[00:01.00]Original"));

    QVector<LyricLine> copy = model.lines();
    copy[0].text = QStringLiteral("Changed");

    QCOMPARE(model.lines().at(0).text, QStringLiteral("Original"));
}

QTEST_GUILESS_MAIN(SyncedLyricsModelTest)
#include "tst_synced_lyrics_model.moc"
