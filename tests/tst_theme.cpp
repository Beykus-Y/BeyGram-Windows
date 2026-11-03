#include "ui/ThemeController.h"

#include "core/ColorContrast.h"

#include <QtTest>

using beygram::ui::ThemeController;

class ThemeTest final : public QObject {
    Q_OBJECT

private slots:
    void warmLightMatchesAndroidTokens();
    void togglingSwitchesToDark();
    void amoledUsesPureBlack();
    void graphitePresetChangesAccent();
    void textReadsOnBothBubbles();
};

void ThemeTest::warmLightMatchesAndroidTokens()
{
    ThemeController theme;
    theme.setModeName(QStringLiteral("Light"));
    QVERIFY(!theme.darkMode());
    QCOMPARE(theme.bg(), QColor::fromRgb(0xFFFAF8F5));
    QCOMPARE(theme.accent(), QColor::fromRgb(0xFF6A4FDC));
    QCOMPARE(theme.bubbleOut(), QColor::fromRgb(0xFFE6DEFD));
}

void ThemeTest::togglingSwitchesToDark()
{
    ThemeController theme;
    theme.setModeName(QStringLiteral("Light"));
    theme.toggleDarkMode();
    QVERIFY(theme.darkMode());
    QCOMPARE(theme.modeName(), QStringLiteral("Dark"));
    QCOMPARE(theme.bg(), QColor::fromRgb(0xFF161417));
}

void ThemeTest::amoledUsesPureBlack()
{
    ThemeController theme;
    theme.setModeName(QStringLiteral("Amoled"));
    QCOMPARE(theme.bg(), QColor::fromRgb(0xFF000000));
    QVERIFY(theme.darkMode());
}

void ThemeTest::graphitePresetChangesAccent()
{
    ThemeController theme;
    theme.setModeName(QStringLiteral("Light"));
    theme.setPresetName(QStringLiteral("Graphite"));
    QCOMPARE(theme.presetName(), QStringLiteral("Graphite"));
    QCOMPARE(theme.accent(), QColor::fromRgb(0xFF3B5BDB));
}

void ThemeTest::textReadsOnBothBubbles()
{
    ThemeController theme;
    for (const QString& mode : {QStringLiteral("Light"), QStringLiteral("Dark")}) {
        theme.setModeName(mode);
        QVERIFY2(beygram::color::isReadableText(theme.text(), theme.bubbleOut()),
                 qPrintable(QStringLiteral("message text fails on bubble in %1").arg(mode)));
    }
}

QTEST_MAIN(ThemeTest)
#include "tst_theme.moc"
