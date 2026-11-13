#include "telegram/music/SilenceEdges.h"

#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

constexpr int kRate = 8000;

QVector<qint16> msSamples(int ms, qint16 value, int sampleRate = kRate)
{
    return QVector<qint16>(ms * sampleRate / 1000, value);
}

/// Silence, one audible block and silence again, the shape of a normal track.
QVector<qint16> track(int leadingMs, int toneMs, int trailingMs, qint16 tone = 2000,
                      int sampleRate = kRate)
{
    QVector<qint16> samples = msSamples(leadingMs, 0, sampleRate);
    samples += msSamples(toneMs, tone, sampleRate);
    samples += msSamples(trailingMs, 0, sampleRate);
    return samples;
}

} // namespace

class SilenceEdgesTest final : public QObject {
    Q_OBJECT

private slots:
    void analyzeMeasuresBothEdges();
    void analyzeTreatsThresholdAsSilence();
    void analyzeTreatsNegativePeaksAsAudible();
    void analyzeIgnoresSilenceBelowMinimum();
    void analyzeReportsShortSilenceWhenMinimumIsZero();
    void analyzeMinimumBoundaryIsInclusive();
    void analyzeCapsEachEdgeAtFiveSeconds();
    void analyzeRejectsUnsupportedSampleRate();
    void analyzeAcceptsSampleRateBounds();
    void analyzeEmptyBufferIsUnanalyzed();
    void analyzeFullySilentBufferIsUnanalyzed();
    void analyzeMeasuresSingleAudibleSample();
    void analyzeClampsThreshold();
    void analyzeClampsMinimumSilence();
    void analyzeFloorsToWholeMilliseconds();

    void encodeWritesBothEdges();
    void encodeEmptyWhenNotAnalyzed();
    void encodeKeepsAnalyzedZeroes();
    void encodeClampsOutOfRangeValues();

    void decodeReadsBothEdges();
    void decodeTrimsSpaces();
    void decodeRejectsEmptyInput();
    void decodeRejectsWrongFieldCount();
    void decodeRejectsNonNumeric();
    void decodeRejectsOutOfRange();
    void decodeRoundTripsEncodedEdges();

    void trimStartRespectsCapAndEdges();
    void trimEndRespectsCapDurationAndEdges();
    void analyzedEdgesSurviveStorageAndDriveTrimming();
};

void SilenceEdgesTest::analyzeMeasuresBothEdges()
{
    const SilenceEdges edges = analyzeSilence(track(1000, 2000, 500), kRate);

    QVERIFY(edges.analyzed);
    QCOMPARE(edges.leadingMs, 1000);
    QCOMPARE(edges.trailingMs, 500);
}

void SilenceEdgesTest::analyzeTreatsThresholdAsSilence()
{
    QVector<qint16> samples = msSamples(500, 500);
    samples += msSamples(300, 501);
    samples += msSamples(250, -500);

    const SilenceEdges edges = analyzeSilence(samples, kRate, 500);

    QVERIFY(edges.analyzed);
    QCOMPARE(edges.leadingMs, 500);
    QCOMPARE(edges.trailingMs, 250);
}

void SilenceEdgesTest::analyzeTreatsNegativePeaksAsAudible()
{
    QVector<qint16> samples = msSamples(300, 0);
    samples += msSamples(100, -2000);
    samples += msSamples(200, 0);

    const SilenceEdges edges = analyzeSilence(samples, kRate, 500, 0);

    QVERIFY(edges.analyzed);
    QCOMPARE(edges.leadingMs, 300);
    QCOMPARE(edges.trailingMs, 200);
}

void SilenceEdgesTest::analyzeIgnoresSilenceBelowMinimum()
{
    const QVector<qint16> samples = track(200, 500, 100);

    const SilenceEdges withDefault = analyzeSilence(samples, kRate);
    QVERIFY(withDefault.analyzed);
    QCOMPARE(withDefault.leadingMs, 0);
    QCOMPARE(withDefault.trailingMs, 0);

    const SilenceEdges withLowMinimum = analyzeSilence(samples, kRate, 500, 100);
    QVERIFY(withLowMinimum.analyzed);
    QCOMPARE(withLowMinimum.leadingMs, 200);
    QCOMPARE(withLowMinimum.trailingMs, 100);

    const SilenceEdges withHighMinimum = analyzeSilence(samples, kRate, 500, 201);
    QVERIFY(withHighMinimum.analyzed);
    QCOMPARE(withHighMinimum.leadingMs, 0);
    QCOMPARE(withHighMinimum.trailingMs, 0);
}

void SilenceEdgesTest::analyzeReportsShortSilenceWhenMinimumIsZero()
{
    const SilenceEdges edges = analyzeSilence(track(50, 100, 30), kRate, 500, 0);

    QVERIFY(edges.analyzed);
    QCOMPARE(edges.leadingMs, 50);
    QCOMPARE(edges.trailingMs, 30);
}

void SilenceEdgesTest::analyzeMinimumBoundaryIsInclusive()
{
    const SilenceEdges edges = analyzeSilence(track(250, 500, 249), kRate);

    QVERIFY(edges.analyzed);
    QCOMPARE(edges.leadingMs, 250);
    QCOMPARE(edges.trailingMs, 0);
}

void SilenceEdgesTest::analyzeCapsEachEdgeAtFiveSeconds()
{
    const SilenceEdges edges = analyzeSilence(track(6000, 100, 7000), kRate);

    QVERIFY(edges.analyzed);
    QCOMPARE(edges.leadingMs, 5000);
    QCOMPARE(edges.trailingMs, 5000);
}

void SilenceEdgesTest::analyzeRejectsUnsupportedSampleRate()
{
    const QVector<qint16> samples = track(500, 500, 500);
    const QVector<int> rates = {0, -8000, 7999, 192001};

    for (int rate : rates) {
        const SilenceEdges edges = analyzeSilence(samples, rate);
        QVERIFY(!edges.analyzed);
        QCOMPARE(edges.leadingMs, 0);
        QCOMPARE(edges.trailingMs, 0);
    }
}

void SilenceEdgesTest::analyzeAcceptsSampleRateBounds()
{
    const QVector<int> rates = {8000, 192000};

    for (int rate : rates) {
        const SilenceEdges edges = analyzeSilence(track(500, 500, 500, 2000, rate), rate);
        QVERIFY(edges.analyzed);
        QCOMPARE(edges.leadingMs, 500);
        QCOMPARE(edges.trailingMs, 500);
    }
}

void SilenceEdgesTest::analyzeEmptyBufferIsUnanalyzed()
{
    const SilenceEdges edges = analyzeSilence(QVector<qint16>(), kRate);

    QVERIFY(!edges.analyzed);
    QCOMPARE(edges.leadingMs, 0);
    QCOMPARE(edges.trailingMs, 0);
}

void SilenceEdgesTest::analyzeFullySilentBufferIsUnanalyzed()
{
    const SilenceEdges edges = analyzeSilence(msSamples(1000, 0), kRate);

    QVERIFY(!edges.analyzed);
    QCOMPARE(edges.leadingMs, 0);
    QCOMPARE(edges.trailingMs, 0);
}

void SilenceEdgesTest::analyzeMeasuresSingleAudibleSample()
{
    QVector<qint16> samples = msSamples(400, 0);
    samples.append(2000);
    samples += msSamples(600, 0);

    const SilenceEdges edges = analyzeSilence(samples, kRate, 500, 0);

    QVERIFY(edges.analyzed);
    QCOMPARE(edges.leadingMs, 400);
    QCOMPARE(edges.trailingMs, 600);
}

void SilenceEdgesTest::analyzeClampsThreshold()
{
    // A negative threshold means "nothing is silent" and is clamped to 0, so
    // the zero samples still count as silence.
    const SilenceEdges negative = analyzeSilence(track(400, 400, 400, 1), kRate, -10);
    QVERIFY(negative.analyzed);
    QCOMPARE(negative.leadingMs, 400);
    QCOMPARE(negative.trailingMs, 400);

    // A threshold of 2 swallows the whole 2-amplitude tone, so there is no
    // audible sample left and the measurement is unusable.
    const QVector<qint16> twos = track(400, 400, 400, 2);
    QVERIFY(!analyzeSilence(twos, kRate, 2).analyzed);

    const SilenceEdges audible = analyzeSilence(twos, kRate, 1);
    QVERIFY(audible.analyzed);
    QCOMPARE(audible.leadingMs, 400);
    QCOMPARE(audible.trailingMs, 400);

    // An oversized threshold is clamped to the largest PCM16 amplitude.
    const QVector<qint16> fullScale = track(100, 100, 100, 32767);
    QVERIFY(!analyzeSilence(fullScale, kRate, 40000).analyzed);

    const SilenceEdges loud = analyzeSilence(fullScale, kRate, 32766, 0);
    QVERIFY(loud.analyzed);
    QCOMPARE(loud.leadingMs, 100);
    QCOMPARE(loud.trailingMs, 100);
}

void SilenceEdgesTest::analyzeClampsMinimumSilence()
{
    // 9000 ms is clamped to the 5000 ms cap, which is still longer than the
    // 4000 ms runs here, so both edges are reported as zero.
    const SilenceEdges cappedMinimum =
        analyzeSilence(track(4000, 500, 4000), kRate, 500, 9000);
    QVERIFY(cappedMinimum.analyzed);
    QCOMPARE(cappedMinimum.leadingMs, 0);
    QCOMPARE(cappedMinimum.trailingMs, 0);

    // A negative minimum is clamped to zero, so even a 10 ms run is reported.
    const SilenceEdges zeroMinimum =
        analyzeSilence(track(10, 500, 10), kRate, 500, -100);
    QVERIFY(zeroMinimum.analyzed);
    QCOMPARE(zeroMinimum.leadingMs, 10);
    QCOMPARE(zeroMinimum.trailingMs, 10);
}

void SilenceEdgesTest::analyzeFloorsToWholeMilliseconds()
{
    QVector<qint16> samples = QVector<qint16>(11, 0);
    samples += msSamples(1000, 2000);

    const SilenceEdges edges = analyzeSilence(samples, kRate, 500, 0);

    QVERIFY(edges.analyzed);
    QCOMPARE(edges.leadingMs, 1);
    QCOMPARE(edges.trailingMs, 0);
}

void SilenceEdgesTest::encodeWritesBothEdges()
{
    QCOMPARE(encodeSilenceEdges(SilenceEdges{120, 340, true}), QStringLiteral("120,340"));
}

void SilenceEdgesTest::encodeEmptyWhenNotAnalyzed()
{
    QCOMPARE(encodeSilenceEdges(SilenceEdges{}), QString());
    QCOMPARE(encodeSilenceEdges(SilenceEdges{100, 200, false}), QString());
}

void SilenceEdgesTest::encodeKeepsAnalyzedZeroes()
{
    QCOMPARE(encodeSilenceEdges(SilenceEdges{0, 0, true}), QStringLiteral("0,0"));
}

void SilenceEdgesTest::encodeClampsOutOfRangeValues()
{
    QCOMPARE(encodeSilenceEdges(SilenceEdges{-5, 6000, true}), QStringLiteral("0,5000"));
    QCOMPARE(encodeSilenceEdges(SilenceEdges{5000, 5000, true}), QStringLiteral("5000,5000"));
}

void SilenceEdgesTest::decodeReadsBothEdges()
{
    const SilenceEdges edges = decodeSilenceEdges(QStringLiteral("120,340"));

    QVERIFY(edges.analyzed);
    QCOMPARE(edges.leadingMs, 120);
    QCOMPARE(edges.trailingMs, 340);

    const SilenceEdges bounds = decodeSilenceEdges(QStringLiteral("0,5000"));
    QVERIFY(bounds.analyzed);
    QCOMPARE(bounds.leadingMs, 0);
    QCOMPARE(bounds.trailingMs, 5000);
}

void SilenceEdgesTest::decodeTrimsSpaces()
{
    const SilenceEdges edges = decodeSilenceEdges(QStringLiteral(" 120 , 340 "));

    QVERIFY(edges.analyzed);
    QCOMPARE(edges.leadingMs, 120);
    QCOMPARE(edges.trailingMs, 340);
}

void SilenceEdgesTest::decodeRejectsEmptyInput()
{
    QVERIFY(!decodeSilenceEdges(QString()).analyzed);
    QVERIFY(!decodeSilenceEdges(QStringLiteral("   ")).analyzed);
}

void SilenceEdgesTest::decodeRejectsWrongFieldCount()
{
    const QVector<QString> values = {
        QStringLiteral("120"),
        QStringLiteral("120,340,0"),
        QStringLiteral("120,340,"),
        QStringLiteral(",340"),
    };

    for (const QString& value : values) {
        const SilenceEdges edges = decodeSilenceEdges(value);
        QVERIFY(!edges.analyzed);
        QCOMPARE(edges.leadingMs, 0);
        QCOMPARE(edges.trailingMs, 0);
    }
}

void SilenceEdgesTest::decodeRejectsNonNumeric()
{
    QVERIFY(!decodeSilenceEdges(QStringLiteral("abc,340")).analyzed);
    QVERIFY(!decodeSilenceEdges(QStringLiteral("120,3x0")).analyzed);
}

void SilenceEdgesTest::decodeRejectsOutOfRange()
{
    const QVector<QString> values = {
        QStringLiteral("-1,340"),
        QStringLiteral("120,-1"),
        QStringLiteral("5001,340"),
        QStringLiteral("120,9999"),
    };

    for (const QString& value : values)
        QVERIFY(!decodeSilenceEdges(value).analyzed);
}

void SilenceEdgesTest::decodeRoundTripsEncodedEdges()
{
    const SilenceEdges original{1234, 4321, true};
    const SilenceEdges decoded = decodeSilenceEdges(encodeSilenceEdges(original));

    QVERIFY(decoded.analyzed);
    QCOMPARE(decoded.leadingMs, original.leadingMs);
    QCOMPARE(decoded.trailingMs, original.trailingMs);

    QVERIFY(!decodeSilenceEdges(encodeSilenceEdges(SilenceEdges{})).analyzed);
}

void SilenceEdgesTest::trimStartRespectsCapAndEdges()
{
    const SilenceEdges edges{1200, 800, true};

    QCOMPARE(trimStartMs(edges, 5000), 1200);
    QCOMPARE(trimStartMs(edges, 700), 700);
    QCOMPARE(trimStartMs(edges, 0), 0);
    QCOMPARE(trimStartMs(edges, -50), 0);
    QCOMPARE(trimStartMs(edges, 9000), 1200);

    // Both the cap and the stored edge are limited to 5000 ms.
    QCOMPARE(trimStartMs(SilenceEdges{8000, 0, true}, 9000), 5000);
    QCOMPARE(trimStartMs(SilenceEdges{8000, 0, true}, 6000), 5000);

    QCOMPARE(trimStartMs(SilenceEdges{1200, 0, false}, 5000), 0);
    QCOMPARE(trimStartMs(SilenceEdges{-100, 0, true}, 5000), 0);
}

void SilenceEdgesTest::trimEndRespectsCapDurationAndEdges()
{
    const SilenceEdges edges{0, 1500, true};

    QCOMPARE(trimEndMs(edges, 60000, 5000), 1500);
    QCOMPARE(trimEndMs(edges, 60000, 700), 700);
    QCOMPARE(trimEndMs(edges, 900, 5000), 900);
    QCOMPARE(trimEndMs(edges, 0, 5000), 0);
    QCOMPARE(trimEndMs(edges, -100, 5000), 0);
    QCOMPARE(trimEndMs(edges, 60000, -1), 0);

    // Both the cap and the stored edge are limited to 5000 ms, and the result
    // still never runs past the track.
    QCOMPARE(trimEndMs(SilenceEdges{0, 8000, true}, 60000, 9000), 5000);
    QCOMPARE(trimEndMs(SilenceEdges{0, 8000, true}, 4000, 9000), 4000);

    QCOMPARE(trimEndMs(SilenceEdges{0, 1500, false}, 60000, 5000), 0);
    QCOMPARE(trimEndMs(SilenceEdges{0, -1500, true}, 60000, 5000), 0);
}

void SilenceEdgesTest::analyzedEdgesSurviveStorageAndDriveTrimming()
{
    const SilenceEdges measured = analyzeSilence(track(1200, 2000, 800), kRate);
    const SilenceEdges stored = decodeSilenceEdges(encodeSilenceEdges(measured));

    QVERIFY(stored.analyzed);
    QCOMPARE(stored.leadingMs, 1200);
    QCOMPARE(stored.trailingMs, 800);
    QCOMPARE(trimStartMs(stored, 5000), 1200);
    QCOMPARE(trimEndMs(stored, 4000, 5000), 800);
}

QTEST_GUILESS_MAIN(SilenceEdgesTest)
#include "tst_silence_edges.moc"
