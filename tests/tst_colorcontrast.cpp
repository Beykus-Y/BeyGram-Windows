#include "core/ColorContrast.h"

#include <QtTest>

using namespace beygram::color;

class ColorContrastTest final : public QObject {
    Q_OBJECT

private slots:
    void blackOnWhiteIsMaximum();
    void identicalColorsHaveNoContrast();
    void beyGramAccentReadsOnBackground();
    void readableOnFallsBackWhenPreferredFails();
    void readableOnUsesWhiteOrBlackWhenBothFail();
    void mixInterpolatesChannels();
};

void ColorContrastTest::blackOnWhiteIsMaximum()
{
    QCOMPARE(contrastRatio(Qt::black, Qt::white), 21.0);
    QCOMPARE(contrastRatio(Qt::white, Qt::black), 21.0);
}

void ColorContrastTest::identicalColorsHaveNoContrast()
{
    QCOMPARE(contrastRatio(QColor::fromRgb(0x123456), QColor::fromRgb(0x123456)), 1.0);
}

void ColorContrastTest::beyGramAccentReadsOnBackground()
{
    // Values from the Android Warm light tokens.
    const QColor background = QColor::fromRgb(0xFAF8F5);
    const QColor text = QColor::fromRgb(0x1E1B18);
    const QColor accent = QColor::fromRgb(0x6A4FDC);
    QVERIFY(isReadableText(text, background));
    QVERIFY(contrastRatio(accent, background) >= MinimumAccentContrast);
}

void ColorContrastTest::readableOnFallsBackWhenPreferredFails()
{
    const QColor background = QColor::fromRgb(0xFAF8F5);
    const QColor unreadable = QColor::fromRgb(0xFFFFFF);
    const QColor fallback = QColor::fromRgb(0x1E1B18);
    QCOMPARE(readableOn(background, unreadable, fallback), fallback);
}

void ColorContrastTest::readableOnUsesWhiteOrBlackWhenBothFail()
{
    const QColor background = QColor::fromRgb(0x000000);
    const QColor white = readableOn(background, QColor::fromRgb(0x010101), QColor::fromRgb(0x020202));
    QCOMPARE(white, QColor(Qt::white));

    const QColor lightBackground = QColor::fromRgb(0xFFFFFF);
    const QColor black = readableOn(lightBackground, QColor::fromRgb(0xFEFEFE), QColor::fromRgb(0xFDFDFD));
    QCOMPARE(black, QColor(Qt::black));
}

void ColorContrastTest::mixInterpolatesChannels()
{
    const QColor result = mix(Qt::black, Qt::white, 0.5);
    QCOMPARE(result.red(), 128);
    QCOMPARE(result.green(), 128);
    QCOMPARE(result.blue(), 128);
}

QTEST_MAIN(ColorContrastTest)
#include "tst_colorcontrast.moc"
