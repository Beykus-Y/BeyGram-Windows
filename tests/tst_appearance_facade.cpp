#include "ui/AppearanceFacade.h"

#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

using namespace beygram::ui;

namespace {

QStringList sampleCodes()
{
    return {QStringLiteral("en"), QStringLiteral("de"), QStringLiteral("pt-br")};
}

QStringList sampleNames()
{
    return {QStringLiteral("English"), QStringLiteral("German"),
            QStringLiteral("Portuguese (Brazil)")};
}

void loadSampleLanguages(AppearanceFacade& facade)
{
    facade.setLanguages(sampleCodes(), sampleNames());
}

} // namespace

class AppearanceFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsAtDefaults();

    void accessibilityTogglesForward();
    void accessibilityTogglesAreIndependent();
    void repeatedToggleIsSilent();
    void textScaleClampsToRange();
    void clampedTextScaleIsSilent();

    void valueWithNoKindSelectsSolid();
    void backgroundKindKeepsTheStoredValue();
    void backgroundKindNoneClearsTheValue();
    void unknownBackgroundKindIsIgnored();
    void blankBackgroundValueIsIgnored();
    void backgroundValueIsTrimmed();
    void backgroundKindNeedsAValue();
    void backgroundKindIgnoresCaseAndSpacing();
    void backgroundChangedOnlyOnRealChange();

    void dimmerClampsToRange();
    void blurClampsToRange();
    void dimmerAndBlurRepeatIsSilent();

    void languagesListNamesInOrder();
    void languagesDropBlankCodes();
    void languagesFallBackToTheCodeForABlankName();
    void languagesLowercaseAndDedupeCodes();
    void selectLanguageEmitsTheStoredCode();
    void selectLanguageIgnoresOutOfRange();
    void selectLanguageNegativeOneClears();
    void languageSelectionSurvivesAReload();

    void changedCoversEveryModel();
    void resetRestoresDefaults();
    void resetKeepsTheOfferedLanguages();
    void resetOnDefaultsIsSilent();
};

void AppearanceFacadeTest::startsAtDefaults()
{
    AppearanceFacade facade;
    QSignalSpy changed(&facade, &AppearanceFacade::changed);

    QCOMPARE(facade.reduceMotion(), false);
    QCOMPARE(facade.highContrast(), false);
    QCOMPARE(facade.largeText(), false);
    QCOMPARE(facade.textScale(), 100);
    QCOMPARE(facade.backgroundKind(), QStringLiteral("none"));
    QVERIFY2(facade.backgroundValue().isEmpty(), "no background value by default");
    QCOMPARE(facade.dimmer(), 0);
    QCOMPARE(facade.blur(), 0);
    QVERIFY2(facade.languageNames().isEmpty(), "no languages are offered yet");
    QCOMPARE(facade.languageIndex(), -1);
    QCOMPARE(changed.count(), 0);
}

void AppearanceFacadeTest::accessibilityTogglesForward()
{
    AppearanceFacade facade;
    QSignalSpy changed(&facade, &AppearanceFacade::changed);

    facade.setReduceMotion(true);
    QCOMPARE(facade.reduceMotion(), true);
    QCOMPARE(changed.count(), 1);

    facade.setHighContrast(true);
    QCOMPARE(facade.highContrast(), true);
    QCOMPARE(changed.count(), 2);

    facade.setLargeText(true);
    QCOMPARE(facade.largeText(), true);
    QCOMPARE(changed.count(), 3);

    facade.setReduceMotion(false);
    QCOMPARE(facade.reduceMotion(), false);
    QCOMPARE(changed.count(), 4);
}

void AppearanceFacadeTest::accessibilityTogglesAreIndependent()
{
    AppearanceFacade facade;

    facade.setReduceMotion(true);
    QCOMPARE(facade.reduceMotion(), true);
    QCOMPARE(facade.highContrast(), false);
    QCOMPARE(facade.largeText(), false);
    QCOMPARE(facade.textScale(), 100);

    facade.setHighContrast(true);
    QCOMPARE(facade.largeText(), false);
    QCOMPARE(facade.textScale(), 100);

    facade.setLargeText(true);
    QCOMPARE(facade.textScale(), 100);

    facade.setTextScale(90);
    QCOMPARE(facade.largeText(), true);
    QCOMPARE(facade.textScale(), 90);
}

void AppearanceFacadeTest::repeatedToggleIsSilent()
{
    AppearanceFacade facade;
    facade.setReduceMotion(true);

    QSignalSpy changed(&facade, &AppearanceFacade::changed);
    facade.setReduceMotion(true);
    facade.setReduceMotion(true);

    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.reduceMotion(), true);
}

void AppearanceFacadeTest::textScaleClampsToRange()
{
    AppearanceFacade facade;
    QSignalSpy changed(&facade, &AppearanceFacade::changed);

    facade.setTextScale(120);
    QCOMPARE(facade.textScale(), 120);
    QCOMPARE(changed.count(), 1);

    facade.setTextScale(0);
    QCOMPARE(facade.textScale(), 85);
    QCOMPARE(changed.count(), 2);

    facade.setTextScale(-40);
    QCOMPARE(facade.textScale(), 85);
    QCOMPARE(changed.count(), 2);

    facade.setTextScale(1000);
    QCOMPARE(facade.textScale(), 150);
    QCOMPARE(changed.count(), 3);

    facade.setTextScale(150);
    QCOMPARE(facade.textScale(), 150);
    QCOMPARE(changed.count(), 3);
}

void AppearanceFacadeTest::clampedTextScaleIsSilent()
{
    AppearanceFacade facade;
    facade.setTextScale(200);
    QCOMPARE(facade.textScale(), 150);

    QSignalSpy changed(&facade, &AppearanceFacade::changed);
    facade.setTextScale(151);
    facade.setTextScale(9999);

    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.textScale(), 150);
}

void AppearanceFacadeTest::valueWithNoKindSelectsSolid()
{
    AppearanceFacade facade;
    QSignalSpy background(&facade, &AppearanceFacade::backgroundChanged);

    facade.setBackgroundValue(QStringLiteral("#1b2836"));

    QCOMPARE(facade.backgroundKind(), QStringLiteral("solid"));
    QCOMPARE(facade.backgroundValue(), QStringLiteral("#1b2836"));
    QCOMPARE(background.count(), 1);
    QCOMPARE(background.at(0).at(0).toString(), QStringLiteral("solid"));
    QCOMPARE(background.at(0).at(1).toString(), QStringLiteral("#1b2836"));
}

void AppearanceFacadeTest::backgroundKindKeepsTheStoredValue()
{
    AppearanceFacade facade;
    facade.setBackgroundValue(QStringLiteral("#1b2836"));

    QSignalSpy background(&facade, &AppearanceFacade::backgroundChanged);
    facade.setBackgroundKind(QStringLiteral("gradient"));

    QCOMPARE(facade.backgroundKind(), QStringLiteral("gradient"));
    QCOMPARE(facade.backgroundValue(), QStringLiteral("#1b2836"));
    QCOMPARE(background.count(), 1);
    QCOMPARE(background.at(0).at(0).toString(), QStringLiteral("gradient"));
    QCOMPARE(background.at(0).at(1).toString(), QStringLiteral("#1b2836"));
}

void AppearanceFacadeTest::backgroundKindNoneClearsTheValue()
{
    AppearanceFacade facade;
    facade.setBackgroundValue(QStringLiteral("file:///bg.jpg"));
    facade.setBackgroundKind(QStringLiteral("wallpaper"));
    QCOMPARE(facade.backgroundKind(), QStringLiteral("wallpaper"));
    QCOMPARE(facade.backgroundValue(), QStringLiteral("file:///bg.jpg"));

    QSignalSpy background(&facade, &AppearanceFacade::backgroundChanged);
    facade.setBackgroundKind(QStringLiteral("none"));

    QCOMPARE(facade.backgroundKind(), QStringLiteral("none"));
    QVERIFY2(facade.backgroundValue().isEmpty(),
             "selecting none never keeps a value behind");
    QCOMPARE(background.count(), 1);
    QCOMPARE(background.at(0).at(0).toString(), QStringLiteral("none"));
    QVERIFY(background.at(0).at(1).toString().isEmpty());
}

void AppearanceFacadeTest::unknownBackgroundKindIsIgnored()
{
    AppearanceFacade facade;
    facade.setBackgroundValue(QStringLiteral("#101010"));

    QSignalSpy background(&facade, &AppearanceFacade::backgroundChanged);
    facade.setBackgroundKind(QStringLiteral("blurple"));

    QCOMPARE(facade.backgroundKind(), QStringLiteral("solid"));
    QCOMPARE(facade.backgroundValue(), QStringLiteral("#101010"));
    QCOMPARE(background.count(), 0);
}

void AppearanceFacadeTest::blankBackgroundValueIsIgnored()
{
    AppearanceFacade facade;
    facade.setBackgroundValue(QStringLiteral("#101010"));

    QSignalSpy background(&facade, &AppearanceFacade::backgroundChanged);
    facade.setBackgroundValue(QStringLiteral("   "));

    QCOMPARE(facade.backgroundKind(), QStringLiteral("solid"));
    QCOMPARE(facade.backgroundValue(), QStringLiteral("#101010"));
    QCOMPARE(background.count(), 0);
}

void AppearanceFacadeTest::backgroundValueIsTrimmed()
{
    AppearanceFacade facade;
    facade.setBackgroundValue(QStringLiteral("  #1b2836  "));

    QCOMPARE(facade.backgroundValue(), QStringLiteral("#1b2836"));
}

void AppearanceFacadeTest::backgroundKindNeedsAValue()
{
    AppearanceFacade facade;

    // Without a value a non-none kind cannot be painted, so it is rejected.
    QSignalSpy background(&facade, &AppearanceFacade::backgroundChanged);
    facade.setBackgroundKind(QStringLiteral("solid"));

    QCOMPARE(facade.backgroundKind(), QStringLiteral("none"));
    QVERIFY(facade.backgroundValue().isEmpty());
    QCOMPARE(background.count(), 0);

    facade.setBackgroundValue(QStringLiteral("#202020"));
    QCOMPARE(facade.backgroundKind(), QStringLiteral("solid"));
}

void AppearanceFacadeTest::backgroundKindIgnoresCaseAndSpacing()
{
    AppearanceFacade facade;
    facade.setBackgroundValue(QStringLiteral("#202020"));

    facade.setBackgroundKind(QStringLiteral("  WALLPAPER  "));
    QCOMPARE(facade.backgroundKind(), QStringLiteral("wallpaper"));

    facade.setBackgroundKind(QStringLiteral("Pattern"));
    QCOMPARE(facade.backgroundKind(), QStringLiteral("pattern"));
    QCOMPARE(facade.backgroundValue(), QStringLiteral("#202020"));
}

void AppearanceFacadeTest::backgroundChangedOnlyOnRealChange()
{
    AppearanceFacade facade;
    facade.setBackgroundValue(QStringLiteral("#1b2836"));

    QSignalSpy background(&facade, &AppearanceFacade::backgroundChanged);
    QSignalSpy changed(&facade, &AppearanceFacade::changed);

    facade.setBackgroundValue(QStringLiteral("#1b2836"));
    facade.setBackgroundKind(QStringLiteral("solid"));

    QCOMPARE(background.count(), 0);
    QCOMPARE(changed.count(), 0);
}

void AppearanceFacadeTest::dimmerClampsToRange()
{
    AppearanceFacade facade;
    QSignalSpy changed(&facade, &AppearanceFacade::changed);

    facade.setDimmer(40);
    QCOMPARE(facade.dimmer(), 40);
    QCOMPARE(changed.count(), 1);

    facade.setDimmer(-5);
    QCOMPARE(facade.dimmer(), 0);
    QCOMPARE(changed.count(), 2);

    facade.setDimmer(250);
    QCOMPARE(facade.dimmer(), 100);
    QCOMPARE(changed.count(), 3);

    facade.setDimmer(100);
    QCOMPARE(facade.dimmer(), 100);
    QCOMPARE(changed.count(), 3);
}

void AppearanceFacadeTest::blurClampsToRange()
{
    AppearanceFacade facade;
    QSignalSpy changed(&facade, &AppearanceFacade::changed);

    facade.setBlur(30);
    QCOMPARE(facade.blur(), 30);
    QCOMPARE(changed.count(), 1);

    facade.setBlur(-1);
    QCOMPARE(facade.blur(), 0);
    QCOMPARE(changed.count(), 2);

    facade.setBlur(101);
    QCOMPARE(facade.blur(), 100);
    QCOMPARE(changed.count(), 3);
}

void AppearanceFacadeTest::dimmerAndBlurRepeatIsSilent()
{
    AppearanceFacade facade;
    facade.setDimmer(25);
    facade.setBlur(25);

    QSignalSpy changed(&facade, &AppearanceFacade::changed);
    facade.setDimmer(25);
    facade.setBlur(25);
    facade.setDimmer(300); // clamps to 100, a real change
    QCOMPARE(changed.count(), 1);

    QSignalSpy again(&facade, &AppearanceFacade::changed);
    facade.setDimmer(500); // clamps to the stored 100
    QCOMPARE(again.count(), 0);
}

void AppearanceFacadeTest::languagesListNamesInOrder()
{
    AppearanceFacade facade;
    QSignalSpy changed(&facade, &AppearanceFacade::changed);

    loadSampleLanguages(facade);

    QCOMPARE(facade.languageNames(), sampleNames());
    QCOMPARE(changed.count(), 1);
    QCOMPARE(facade.languageIndex(), -1);
}

void AppearanceFacadeTest::languagesDropBlankCodes()
{
    AppearanceFacade facade;

    facade.setLanguages({QString(), QStringLiteral("en"), QStringLiteral("   ")},
                        {QStringLiteral("Nameless"), QStringLiteral("English"),
                         QStringLiteral("Also nameless")});

    QCOMPARE(facade.languageNames(), QStringList({QStringLiteral("English")}));
}

void AppearanceFacadeTest::languagesFallBackToTheCodeForABlankName()
{
    AppearanceFacade facade;

    facade.setLanguages({QStringLiteral("en"), QStringLiteral("de")},
                        {QString(), QStringLiteral("German")});

    QCOMPARE(facade.languageNames(),
             QStringList({QStringLiteral("en"), QStringLiteral("German")}));
}

void AppearanceFacadeTest::languagesLowercaseAndDedupeCodes()
{
    AppearanceFacade facade;

    facade.setLanguages({QStringLiteral("EN"), QStringLiteral("en"),
                         QStringLiteral("PT-BR")},
                        {QStringLiteral("English"), QStringLiteral("Ignored"),
                         QStringLiteral("Portuguese (Brazil)")});

    QCOMPARE(facade.languageNames(),
             QStringList({QStringLiteral("English"),
                          QStringLiteral("Portuguese (Brazil)")}));

    facade.selectLanguage(1);
    QCOMPARE(facade.languageIndex(), 1);
}

void AppearanceFacadeTest::selectLanguageEmitsTheStoredCode()
{
    AppearanceFacade facade;
    loadSampleLanguages(facade);

    QSignalSpy selected(&facade, &AppearanceFacade::languageSelected);

    facade.selectLanguage(1);
    QCOMPARE(facade.languageIndex(), 1);
    QCOMPARE(selected.count(), 1);
    QCOMPARE(selected.at(0).at(0).toString(), QStringLiteral("de"));

    // Selecting the stored row again is not a change.
    facade.selectLanguage(1);
    QCOMPARE(selected.count(), 1);
    QCOMPARE(facade.languageIndex(), 1);

    facade.selectLanguage(2);
    QCOMPARE(facade.languageIndex(), 2);
    QCOMPARE(selected.count(), 2);
    QCOMPARE(selected.at(1).at(0).toString(), QStringLiteral("pt-br"));
}

void AppearanceFacadeTest::selectLanguageIgnoresOutOfRange()
{
    AppearanceFacade facade;
    loadSampleLanguages(facade);
    facade.selectLanguage(0);

    QSignalSpy selected(&facade, &AppearanceFacade::languageSelected);
    facade.selectLanguage(3);
    facade.selectLanguage(99);

    QCOMPARE(facade.languageIndex(), 0);
    QCOMPARE(selected.count(), 0);
}

void AppearanceFacadeTest::selectLanguageNegativeOneClears()
{
    AppearanceFacade facade;
    loadSampleLanguages(facade);
    facade.selectLanguage(1);
    QCOMPARE(facade.languageIndex(), 1);

    QSignalSpy selected(&facade, &AppearanceFacade::languageSelected);
    facade.selectLanguage(-1);

    QCOMPARE(facade.languageIndex(), -1);
    QCOMPARE(selected.count(), 0);

    // Clearing an already clear selection stays silent too.
    QSignalSpy changed(&facade, &AppearanceFacade::changed);
    facade.selectLanguage(-1);
    QCOMPARE(changed.count(), 0);
}

void AppearanceFacadeTest::languageSelectionSurvivesAReload()
{
    AppearanceFacade facade;
    loadSampleLanguages(facade);
    facade.selectLanguage(2);
    QCOMPARE(facade.languageIndex(), 2);

    // A refresh that still offers the pack keeps the choice.
    loadSampleLanguages(facade);
    QCOMPARE(facade.languageIndex(), 2);

    // Dropping the pack hides the index but not the choice.
    facade.setLanguages({QStringLiteral("en"), QStringLiteral("de")},
                        {QStringLiteral("English"), QStringLiteral("German")});
    QCOMPARE(facade.languageIndex(), -1);

    // Bringing it back restores the same row.
    loadSampleLanguages(facade);
    QCOMPARE(facade.languageIndex(), 2);
}

void AppearanceFacadeTest::changedCoversEveryModel()
{
    AppearanceFacade facade;
    QSignalSpy changed(&facade, &AppearanceFacade::changed);

    facade.setReduceMotion(true);
    facade.setHighContrast(true);
    facade.setLargeText(true);
    facade.setTextScale(130);
    facade.setDimmer(10);
    facade.setBlur(10);
    facade.setBackgroundValue(QStringLiteral("#1b2836"));
    facade.setBackgroundKind(QStringLiteral("pattern"));
    loadSampleLanguages(facade);
    facade.selectLanguage(0);

    // Eight single-value writes, one language-list write and one selection.
    QCOMPARE(changed.count(), 10);
}

void AppearanceFacadeTest::resetRestoresDefaults()
{
    AppearanceFacade facade;
    facade.setReduceMotion(true);
    facade.setHighContrast(true);
    facade.setLargeText(true);
    facade.setTextScale(140);
    facade.setBackgroundValue(QStringLiteral("#1b2836"));
    facade.setBackgroundKind(QStringLiteral("wallpaper"));
    facade.setDimmer(60);
    facade.setBlur(35);
    loadSampleLanguages(facade);
    facade.selectLanguage(1);

    QSignalSpy changed(&facade, &AppearanceFacade::changed);
    QSignalSpy background(&facade, &AppearanceFacade::backgroundChanged);

    facade.reset();

    QVERIFY2(changed.count() >= 1, "a reset that restored something announces it");
    QCOMPARE(facade.reduceMotion(), false);
    QCOMPARE(facade.highContrast(), false);
    QCOMPARE(facade.largeText(), false);
    QCOMPARE(facade.textScale(), 100);
    QCOMPARE(facade.backgroundKind(), QStringLiteral("none"));
    QVERIFY2(facade.backgroundValue().isEmpty(), "reset clears the background value");
    QCOMPARE(facade.dimmer(), 0);
    QCOMPARE(facade.blur(), 0);
    QCOMPARE(facade.languageIndex(), -1);
    QCOMPARE(background.count(), 1);
    QCOMPARE(background.at(0).at(0).toString(), QStringLiteral("none"));
    QVERIFY(background.at(0).at(1).toString().isEmpty());
}

void AppearanceFacadeTest::resetKeepsTheOfferedLanguages()
{
    AppearanceFacade facade;
    loadSampleLanguages(facade);
    facade.selectLanguage(1);

    facade.reset();

    QCOMPARE(facade.languageNames(), sampleNames());
    QCOMPARE(facade.languageIndex(), -1);
}

void AppearanceFacadeTest::resetOnDefaultsIsSilent()
{
    AppearanceFacade facade;
    QSignalSpy changed(&facade, &AppearanceFacade::changed);
    QSignalSpy background(&facade, &AppearanceFacade::backgroundChanged);

    facade.reset();
    facade.reset();

    QCOMPARE(changed.count(), 0);
    QCOMPARE(background.count(), 0);
    QCOMPARE(facade.backgroundKind(), QStringLiteral("none"));
}

QTEST_GUILESS_MAIN(AppearanceFacadeTest)
#include "tst_appearance_facade.moc"
