#include "ui/TranslationFacade.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtTest>

using beygram::ui::TranslationFacade;

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

void loadSampleLanguages(TranslationFacade& facade)
{
    facade.setLanguages(sampleCodes(), sampleNames());
}

} // namespace

class TranslationFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();

    void setLanguagesExposesNamesInOrder();
    void setLanguagesDropsRowsWithoutCode();
    void setLanguagesFallsBackToTheCodeWhenTheNameIsMissing();
    void setLanguagesKeepsTheSelection();

    void selectLanguageSetsTargetAndIndex();
    void selectLanguageNormalisesTheCode();
    void selectLanguageIgnoresInvalidIndex();
    void selectLanguageIgnoresTheCurrentLanguage();

    void autoTranslateDefaultsToOffAndStoresTheFlag();

    void requestWithoutLanguageIsIgnored();
    void requestEmitsTranslateRequested();
    void requestIgnoresInvalidIds();
    void repeatedRequestDoesNotRepeatTheChange();

    void setTranslationStoresTheText();
    void setTranslationAcceptsLanguageCase();
    void setTranslationRejectsTheWrongLanguage();
    void setTranslationWithoutRequestStoresTheText();
    void setTranslationRejectsInvalidIdAndEmptyText();
    void translationForRejectsInvalidIds();

    void switchingLanguageAbandonsPendingRequests();
    void switchingLanguageKeepsCachedTranslations();

    void clearDropsTranslationsAndLoadingButKeepsSettings();
    void clearIsSilentWhenThereIsNothingToDrop();

    void changedIsEmittedOnlyForRealChanges();
};

void TranslationFacadeTest::startsEmpty()
{
    const TranslationFacade facade;

    QVERIFY2(facade.targetLanguage().isEmpty(), "translation is off by default");
    QVERIFY2(facade.languageNames().isEmpty(), "no languages are offered yet");
    QCOMPARE(facade.languageIndex(), -1);
    QVERIFY2(!facade.autoTranslate(), "auto-translate defaults to off");
    QVERIFY2(!facade.loading(), "nothing is loading by default");
    QVERIFY2(facade.translationFor(QStringLiteral("1")).isEmpty(),
             "a fresh facade has no translations");
}

void TranslationFacadeTest::setLanguagesExposesNamesInOrder()
{
    TranslationFacade facade;
    loadSampleLanguages(facade);

    QCOMPARE(facade.languageNames(), sampleNames());
    QCOMPARE(facade.languageIndex(), -1);
    QVERIFY2(facade.targetLanguage().isEmpty(),
             "loading languages does not turn translation on");
}

void TranslationFacadeTest::setLanguagesDropsRowsWithoutCode()
{
    TranslationFacade facade;
    facade.setLanguages({QStringLiteral("en"), QStringLiteral("   "),
                         QStringLiteral("de")},
                        {QStringLiteral("English"), QStringLiteral("Blank"),
                         QStringLiteral("German")});

    QCOMPARE(facade.languageNames(),
             QStringList({QStringLiteral("English"), QStringLiteral("German")}));
}

void TranslationFacadeTest::setLanguagesFallsBackToTheCodeWhenTheNameIsMissing()
{
    TranslationFacade facade;
    facade.setLanguages({QStringLiteral("en"), QStringLiteral("de")},
                        {QStringLiteral("English")});

    QCOMPARE(facade.languageNames(),
             QStringList({QStringLiteral("English"), QStringLiteral("de")}));

    facade.setLanguages({QStringLiteral("fr")}, {QStringLiteral("   ")});
    QCOMPARE(facade.languageNames(), QStringList({QStringLiteral("fr")}));
}

void TranslationFacadeTest::setLanguagesKeepsTheSelection()
{
    TranslationFacade facade;
    loadSampleLanguages(facade);
    facade.selectLanguage(1);

    loadSampleLanguages(facade);

    QCOMPARE(facade.targetLanguage(), QStringLiteral("de"));
    QCOMPARE(facade.languageIndex(), 1);
}

void TranslationFacadeTest::selectLanguageSetsTargetAndIndex()
{
    TranslationFacade facade;
    loadSampleLanguages(facade);
    QSignalSpy spy(&facade, &TranslationFacade::changed);

    facade.selectLanguage(2);

    QCOMPARE(facade.targetLanguage(), QStringLiteral("pt-br"));
    QCOMPARE(facade.languageIndex(), 2);
    QCOMPARE(spy.count(), 1);
}

void TranslationFacadeTest::selectLanguageNormalisesTheCode()
{
    TranslationFacade facade;
    facade.setLanguages({QStringLiteral("PT-BR")},
                        {QStringLiteral("Portuguese (Brazil)")});

    facade.selectLanguage(0);

    QCOMPARE(facade.targetLanguage(), QStringLiteral("pt-br"));
    QCOMPARE(facade.languageIndex(), 0);
}

void TranslationFacadeTest::selectLanguageIgnoresInvalidIndex()
{
    TranslationFacade facade;
    loadSampleLanguages(facade);
    QSignalSpy spy(&facade, &TranslationFacade::changed);

    facade.selectLanguage(-1);
    facade.selectLanguage(3);
    facade.selectLanguage(99);

    QVERIFY2(facade.targetLanguage().isEmpty(), "an unknown index selects nothing");
    QCOMPARE(facade.languageIndex(), -1);
    QCOMPARE(spy.count(), 0);
}

void TranslationFacadeTest::selectLanguageIgnoresTheCurrentLanguage()
{
    TranslationFacade facade;
    loadSampleLanguages(facade);
    facade.selectLanguage(1);

    QSignalSpy spy(&facade, &TranslationFacade::changed);
    facade.selectLanguage(1);

    QCOMPARE(facade.targetLanguage(), QStringLiteral("de"));
    QCOMPARE(spy.count(), 0);
}

void TranslationFacadeTest::autoTranslateDefaultsToOffAndStoresTheFlag()
{
    TranslationFacade facade;
    QVERIFY(!facade.autoTranslate());
    QSignalSpy spy(&facade, &TranslationFacade::changed);

    facade.setAutoTranslate(true);
    QVERIFY(facade.autoTranslate());
    QCOMPARE(spy.count(), 1);

    facade.setAutoTranslate(true);
    QCOMPARE(spy.count(), 1);

    facade.setAutoTranslate(false);
    QVERIFY(!facade.autoTranslate());
    QCOMPARE(spy.count(), 2);
}

void TranslationFacadeTest::requestWithoutLanguageIsIgnored()
{
    TranslationFacade facade;
    loadSampleLanguages(facade);
    QSignalSpy requestSpy(&facade, &TranslationFacade::translateRequested);
    QSignalSpy changeSpy(&facade, &TranslationFacade::changed);

    facade.requestTranslation(QStringLiteral("42"));

    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(changeSpy.count(), 0);
    QVERIFY2(!facade.loading(), "nothing may load while translation is off");
}

void TranslationFacadeTest::requestEmitsTranslateRequested()
{
    TranslationFacade facade;
    loadSampleLanguages(facade);
    facade.selectLanguage(1);

    QSignalSpy requestSpy(&facade, &TranslationFacade::translateRequested);
    QSignalSpy changeSpy(&facade, &TranslationFacade::changed);

    facade.requestTranslation(QStringLiteral("42"));

    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(requestSpy.at(0).at(0).toString(), QStringLiteral("42"));
    QCOMPARE(requestSpy.at(0).at(1).toString(), QStringLiteral("de"));
    QVERIFY(facade.loading());
    QCOMPARE(changeSpy.count(), 1);
}

void TranslationFacadeTest::requestIgnoresInvalidIds()
{
    TranslationFacade facade;
    loadSampleLanguages(facade);
    facade.selectLanguage(0);

    QSignalSpy requestSpy(&facade, &TranslationFacade::translateRequested);

    facade.requestTranslation(QStringLiteral("abc"));
    facade.requestTranslation(QStringLiteral("0"));
    facade.requestTranslation(QStringLiteral("-5"));
    facade.requestTranslation(QString());
    facade.requestTranslation(QStringLiteral("   "));

    QCOMPARE(requestSpy.count(), 0);
    QVERIFY(!facade.loading());
}

void TranslationFacadeTest::repeatedRequestDoesNotRepeatTheChange()
{
    TranslationFacade facade;
    loadSampleLanguages(facade);
    facade.selectLanguage(0);
    facade.requestTranslation(QStringLiteral("42"));

    QSignalSpy changeSpy(&facade, &TranslationFacade::changed);
    facade.requestTranslation(QStringLiteral("42"));

    QVERIFY(facade.loading());
    QCOMPARE(changeSpy.count(), 0);
}

void TranslationFacadeTest::setTranslationStoresTheText()
{
    TranslationFacade facade;
    loadSampleLanguages(facade);
    facade.selectLanguage(1);
    facade.requestTranslation(QStringLiteral("42"));

    QSignalSpy changeSpy(&facade, &TranslationFacade::changed);
    facade.setTranslation(QStringLiteral("42"), QStringLiteral("de"),
                          QStringLiteral("Hallo"));

    QCOMPARE(facade.translationFor(QStringLiteral("42")), QStringLiteral("Hallo"));
    QVERIFY2(!facade.loading(), "a result clears the loading state");
    QCOMPARE(changeSpy.count(), 1);
}

void TranslationFacadeTest::setTranslationAcceptsLanguageCase()
{
    TranslationFacade facade;
    loadSampleLanguages(facade);
    facade.selectLanguage(1);

    facade.setTranslation(QStringLiteral("42"), QStringLiteral("DE"),
                          QStringLiteral("Hallo"));

    QCOMPARE(facade.translationFor(QStringLiteral("42")), QStringLiteral("Hallo"));
}

void TranslationFacadeTest::setTranslationRejectsTheWrongLanguage()
{
    TranslationFacade facade;
    loadSampleLanguages(facade);
    facade.selectLanguage(1);
    facade.requestTranslation(QStringLiteral("42"));

    QSignalSpy changeSpy(&facade, &TranslationFacade::changed);
    facade.setTranslation(QStringLiteral("42"), QStringLiteral("en"),
                          QStringLiteral("Hello"));

    QVERIFY2(facade.translationFor(QStringLiteral("42")).isEmpty(),
             "a result for another language is dropped");
    QVERIFY2(facade.loading(),
             "a rejected result leaves the message waiting");
    QCOMPARE(changeSpy.count(), 0);
}

void TranslationFacadeTest::setTranslationWithoutRequestStoresTheText()
{
    TranslationFacade facade;
    loadSampleLanguages(facade);
    facade.selectLanguage(1);

    facade.setTranslation(QStringLiteral("7"), QStringLiteral("de"),
                          QStringLiteral("Hallo"));

    QCOMPARE(facade.translationFor(QStringLiteral("7")), QStringLiteral("Hallo"));
    QVERIFY(!facade.loading());
}

void TranslationFacadeTest::setTranslationRejectsInvalidIdAndEmptyText()
{
    TranslationFacade facade;
    loadSampleLanguages(facade);
    facade.selectLanguage(1);

    facade.setTranslation(QStringLiteral("abc"), QStringLiteral("de"),
                          QStringLiteral("Hallo"));
    QVERIFY2(facade.translationFor(QStringLiteral("abc")).isEmpty(),
             "an unparsable id stores nothing");

    facade.requestTranslation(QStringLiteral("9"));
    facade.setTranslation(QStringLiteral("9"), QStringLiteral("de"),
                          QStringLiteral("   "));
    QVERIFY2(facade.translationFor(QStringLiteral("9")).isEmpty(),
             "blank text is not a translation");
    QVERIFY2(facade.loading(), "a rejected result leaves the message waiting");
}

void TranslationFacadeTest::translationForRejectsInvalidIds()
{
    const TranslationFacade facade;

    QVERIFY(facade.translationFor(QStringLiteral("abc")).isEmpty());
    QVERIFY(facade.translationFor(QStringLiteral("0")).isEmpty());
    QVERIFY(facade.translationFor(QStringLiteral("-1")).isEmpty());
    QVERIFY(facade.translationFor(QString()).isEmpty());
}

void TranslationFacadeTest::switchingLanguageAbandonsPendingRequests()
{
    TranslationFacade facade;
    loadSampleLanguages(facade);
    facade.selectLanguage(0);
    facade.requestTranslation(QStringLiteral("42"));
    QVERIFY(facade.loading());

    facade.selectLanguage(1);

    QCOMPARE(facade.targetLanguage(), QStringLiteral("de"));
    QVERIFY2(!facade.loading(), "a language switch drops stale loading states");
    QVERIFY(facade.translationFor(QStringLiteral("42")).isEmpty());
}

void TranslationFacadeTest::switchingLanguageKeepsCachedTranslations()
{
    TranslationFacade facade;
    loadSampleLanguages(facade);
    facade.selectLanguage(1);
    facade.setTranslation(QStringLiteral("5"), QStringLiteral("de"),
                          QStringLiteral("Hallo"));

    facade.selectLanguage(0);

    QCOMPARE(facade.translationFor(QStringLiteral("5")), QStringLiteral("Hallo"));
}

void TranslationFacadeTest::clearDropsTranslationsAndLoadingButKeepsSettings()
{
    TranslationFacade facade;
    loadSampleLanguages(facade);
    facade.selectLanguage(1);
    facade.setAutoTranslate(true);
    facade.setTranslation(QStringLiteral("5"), QStringLiteral("de"),
                          QStringLiteral("Hallo"));
    facade.requestTranslation(QStringLiteral("6"));
    QVERIFY(facade.loading());

    facade.clear();

    QVERIFY2(facade.translationFor(QStringLiteral("5")).isEmpty(),
             "clear drops the stored text");
    QVERIFY2(!facade.loading(), "clear drops the loading states");
    QCOMPARE(facade.targetLanguage(), QStringLiteral("de"));
    QCOMPARE(facade.languageIndex(), 1);
    QVERIFY2(facade.autoTranslate(), "clear keeps the auto-translate setting");
    QCOMPARE(facade.languageNames(), sampleNames());
}

void TranslationFacadeTest::clearIsSilentWhenThereIsNothingToDrop()
{
    TranslationFacade facade;
    loadSampleLanguages(facade);
    facade.selectLanguage(0);

    QSignalSpy spy(&facade, &TranslationFacade::changed);
    facade.clear();

    QCOMPARE(spy.count(), 0);
}

void TranslationFacadeTest::changedIsEmittedOnlyForRealChanges()
{
    TranslationFacade facade;
    QSignalSpy spy(&facade, &TranslationFacade::changed);

    loadSampleLanguages(facade);
    QCOMPARE(spy.count(), 1);

    loadSampleLanguages(facade);
    QCOMPARE(spy.count(), 1);

    facade.setAutoTranslate(true);
    QCOMPARE(spy.count(), 2);

    facade.setAutoTranslate(true);
    QCOMPARE(spy.count(), 2);

    facade.selectLanguage(0);
    QCOMPARE(spy.count(), 3);

    facade.selectLanguage(0);
    QCOMPARE(spy.count(), 3);

    facade.setTranslation(QStringLiteral("3"), QStringLiteral("en"),
                          QStringLiteral("Hi"));
    QCOMPARE(spy.count(), 4);

    facade.setTranslation(QStringLiteral("3"), QStringLiteral("en"),
                          QStringLiteral("Hi"));
    QCOMPARE(spy.count(), 4);
}

QTEST_GUILESS_MAIN(TranslationFacadeTest)
#include "tst_translation_facade.moc"
