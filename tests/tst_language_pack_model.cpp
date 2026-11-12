#include "telegram/LanguagePackModel.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTest>

#include <algorithm>

using namespace beygram::telegram;

namespace {

LanguageRow row(const QString& code, const QString& name,
                const QString& nativeName = {}, int percentTranslated = 100,
                bool beta = false)
{
    LanguageRow language;
    language.code = code;
    language.name = name;
    language.nativeName = nativeName;
    language.beta = beta;
    language.percentTranslated = percentTranslated;
    return language;
}

QStringList codesOf(const QVector<LanguageRow>& languages)
{
    QStringList codes;
    codes.reserve(languages.size());
    for (const LanguageRow& language : languages)
        codes.append(language.code);
    return codes;
}

QStringList namesOf(const QVector<LanguageRow>& languages)
{
    QStringList names;
    names.reserve(languages.size());
    for (const LanguageRow& language : languages)
        names.append(language.name);
    return names;
}

QVector<LanguageRow> sampleLanguages()
{
    return {row(QStringLiteral("en"), QStringLiteral("English"),
                QStringLiteral("English")),
            row(QStringLiteral("de"), QStringLiteral("German"),
                QStringLiteral("Deutsch")),
            row(QStringLiteral("pt-br"), QStringLiteral("Portuguese (Brazil)"),
                QStringLiteral("Portugues (Brasil)"), 87, true)};
}

} // namespace

class LanguagePackModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void startsWithoutSelection();

    void setLanguagesStoresRowsInOrder();
    void setLanguagesDropsRowsWithoutCode();
    void setLanguagesDedupesByCodeKeepingFirst();
    void setLanguagesLowercasesCodesAndKeepsDashes();
    void setLanguagesTreatsCaseVariantsAsOneCode();
    void setLanguagesClampsPercentTranslated();
    void setLanguagesEmitsChangedOnlyForRealChange();
    void setLanguagesKeepsTheSelection();
    void languagesReturnsACopy();

    void filteredMatchesCode();
    void filteredMatchesName();
    void filteredMatchesNativeName();
    void filteredIgnoresCase();
    void filteredKeepsStoredOrder();
    void filteredWithEmptyQueryReturnsEverything();
    void filteredOfUnknownQueryIsEmpty();

    void selectStoresKnownCode();
    void selectIgnoresUnknownCode();
    void selectIgnoresCodeCase();
    void selectWithEmptyCodeClearsSelection();
    void selectEmitsChangedOnlyForRealChange();

    void selectedNameReportsTheRowName();
    void selectedNameIsEmptyWithoutSelection();
    void selectedNameIsEmptyForACodeNoLongerStored();

    void clearDropsLanguagesAndSelection();
    void clearEmitsChangedOnlyWhenSomethingIsDropped();
};

void LanguagePackModelTest::startsEmpty()
{
    const LanguagePackModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY2(model.languages().isEmpty(), "a fresh model stores no languages");
    QVERIFY2(model.filtered(QStringLiteral("en")).isEmpty(),
             "a fresh model has nothing to filter");
}

void LanguagePackModelTest::startsWithoutSelection()
{
    const LanguagePackModel model;

    QVERIFY2(model.selectedCode().isEmpty(), "nothing is selected by default");
    QVERIFY2(model.selectedName().isEmpty(), "no selection has no name");
}

void LanguagePackModelTest::setLanguagesStoresRowsInOrder()
{
    LanguagePackModel model;
    model.setLanguages(sampleLanguages());

    QCOMPARE(model.count(), 3);
    QCOMPARE(codesOf(model.languages()),
             QStringList({QStringLiteral("en"), QStringLiteral("de"),
                          QStringLiteral("pt-br")}));
    QCOMPARE(model.languages().at(1).nativeName, QStringLiteral("Deutsch"));
    QVERIFY2(model.languages().at(2).beta, "the beta flag is kept");
    QCOMPARE(model.languages().at(2).percentTranslated, 87);
}

void LanguagePackModelTest::setLanguagesDropsRowsWithoutCode()
{
    LanguagePackModel model;
    model.setLanguages({row(QString(), QStringLiteral("Nameless")),
                        row(QStringLiteral("en"), QStringLiteral("English")),
                        row(QString(), QStringLiteral("Also nameless"))});

    QCOMPARE(model.count(), 1);
    QCOMPARE(codesOf(model.languages()), QStringList({QStringLiteral("en")}));
}

void LanguagePackModelTest::setLanguagesDedupesByCodeKeepingFirst()
{
    LanguagePackModel model;
    model.setLanguages({row(QStringLiteral("en"), QStringLiteral("English")),
                        row(QStringLiteral("de"), QStringLiteral("German")),
                        row(QStringLiteral("en"), QStringLiteral("First wins"),
                            QStringLiteral("Ignored"))});

    QCOMPARE(model.count(), 2);
    QCOMPARE(namesOf(model.languages()),
             QStringList({QStringLiteral("English"),
                          QStringLiteral("German")}));
}

void LanguagePackModelTest::setLanguagesLowercasesCodesAndKeepsDashes()
{
    LanguagePackModel model;
    model.setLanguages({row(QStringLiteral("PT-BR"),
                            QStringLiteral("Portuguese (Brazil)"))});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.languages().at(0).code, QStringLiteral("pt-br"));
}

void LanguagePackModelTest::setLanguagesTreatsCaseVariantsAsOneCode()
{
    LanguagePackModel model;
    model.setLanguages({row(QStringLiteral("EN"), QStringLiteral("English")),
                        row(QStringLiteral("en"), QStringLiteral("Ignored"))});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.languages().at(0).code, QStringLiteral("en"));
    QCOMPARE(model.languages().at(0).name, QStringLiteral("English"));
}

void LanguagePackModelTest::setLanguagesClampsPercentTranslated()
{
    LanguagePackModel model;
    model.setLanguages({row(QStringLiteral("en"), QStringLiteral("English"),
                            QString(), -20),
                        row(QStringLiteral("de"), QStringLiteral("German"),
                            QString(), 250),
                        row(QStringLiteral("fr"), QStringLiteral("French"),
                            QString(), 42)});

    QCOMPARE(model.count(), 3);
    QCOMPARE(model.languages().at(0).percentTranslated, 0);
    QCOMPARE(model.languages().at(1).percentTranslated, 100);
    QCOMPARE(model.languages().at(2).percentTranslated, 42);
}

void LanguagePackModelTest::setLanguagesEmitsChangedOnlyForRealChange()
{
    LanguagePackModel model;
    QSignalSpy spy(&model, &LanguagePackModel::changed);

    model.setLanguages(sampleLanguages());
    QCOMPARE(spy.count(), 1);

    model.setLanguages(sampleLanguages());
    QCOMPARE(spy.count(), 1);

    // Same rows in another order is a different list.
    QVector<LanguageRow> reordered = sampleLanguages();
    std::reverse(reordered.begin(), reordered.end());
    model.setLanguages(reordered);
    QCOMPARE(spy.count(), 2);

    // A changed percentage is a real change too.
    QVector<LanguageRow> edited = reordered;
    edited[0].percentTranslated = 10;
    model.setLanguages(edited);
    QCOMPARE(spy.count(), 3);

    // Clearing back to nothing and clearing again.
    model.clear();
    QCOMPARE(spy.count(), 4);
    model.clear();
    QCOMPARE(spy.count(), 4);
}

void LanguagePackModelTest::setLanguagesKeepsTheSelection()
{
    LanguagePackModel model;
    model.setLanguages(sampleLanguages());
    model.select(QStringLiteral("de"));

    model.setLanguages(sampleLanguages());

    QCOMPARE(model.selectedCode(), QStringLiteral("de"));
    QCOMPARE(model.selectedName(), QStringLiteral("German"));
}

void LanguagePackModelTest::languagesReturnsACopy()
{
    LanguagePackModel model;
    model.setLanguages(sampleLanguages());

    QVector<LanguageRow> languages = model.languages();
    languages[0].name = QStringLiteral("Edited");

    QCOMPARE(model.languages().at(0).name, QStringLiteral("English"));
}

void LanguagePackModelTest::filteredMatchesCode()
{
    LanguagePackModel model;
    model.setLanguages(sampleLanguages());

    QCOMPARE(codesOf(model.filtered(QStringLiteral("pt"))),
             QStringList({QStringLiteral("pt-br")}));
}

void LanguagePackModelTest::filteredMatchesName()
{
    LanguagePackModel model;
    model.setLanguages(sampleLanguages());

    QCOMPARE(codesOf(model.filtered(QStringLiteral("German"))),
             QStringList({QStringLiteral("de")}));
}

void LanguagePackModelTest::filteredMatchesNativeName()
{
    LanguagePackModel model;
    model.setLanguages(sampleLanguages());

    QCOMPARE(codesOf(model.filtered(QStringLiteral("Deutsch"))),
             QStringList({QStringLiteral("de")}));
}

void LanguagePackModelTest::filteredIgnoresCase()
{
    LanguagePackModel model;
    model.setLanguages(sampleLanguages());

    QCOMPARE(codesOf(model.filtered(QStringLiteral("ENGLISH"))),
             QStringList({QStringLiteral("en")}));
    QCOMPARE(codesOf(model.filtered(QStringLiteral("deutsch"))),
             QStringList({QStringLiteral("de")}));
    QCOMPARE(codesOf(model.filtered(QStringLiteral("PT-BR"))),
             QStringList({QStringLiteral("pt-br")}));
}

void LanguagePackModelTest::filteredKeepsStoredOrder()
{
    LanguagePackModel model;
    model.setLanguages({row(QStringLiteral("en"), QStringLiteral("English")),
                        row(QStringLiteral("de"), QStringLiteral("German")),
                        row(QStringLiteral("es"), QStringLiteral("Spanish"))});

    // Every row carries an "n" in its code or one of its labels.
    QCOMPARE(codesOf(model.filtered(QStringLiteral("n"))),
             QStringList({QStringLiteral("en"), QStringLiteral("de"),
                          QStringLiteral("es")}));
}

void LanguagePackModelTest::filteredWithEmptyQueryReturnsEverything()
{
    LanguagePackModel model;
    model.setLanguages(sampleLanguages());

    QCOMPARE(static_cast<int>(model.filtered(QString()).size()), model.count());
}

void LanguagePackModelTest::filteredOfUnknownQueryIsEmpty()
{
    LanguagePackModel model;
    model.setLanguages(sampleLanguages());

    QVERIFY2(model.filtered(QStringLiteral("klingon")).isEmpty(),
             "an unmatched query yields nothing");
}

void LanguagePackModelTest::selectStoresKnownCode()
{
    LanguagePackModel model;
    model.setLanguages(sampleLanguages());

    model.select(QStringLiteral("de"));

    QCOMPARE(model.selectedCode(), QStringLiteral("de"));
    QCOMPARE(model.selectedName(), QStringLiteral("German"));
}

void LanguagePackModelTest::selectIgnoresUnknownCode()
{
    LanguagePackModel model;
    model.setLanguages(sampleLanguages());
    model.select(QStringLiteral("de"));

    model.select(QStringLiteral("klingon"));

    QCOMPARE(model.selectedCode(), QStringLiteral("de"));
}

void LanguagePackModelTest::selectIgnoresCodeCase()
{
    LanguagePackModel model;
    model.setLanguages(sampleLanguages());

    model.select(QStringLiteral("PT-BR"));

    QCOMPARE(model.selectedCode(), QStringLiteral("pt-br"));
    QCOMPARE(model.selectedName(), QStringLiteral("Portuguese (Brazil)"));
}

void LanguagePackModelTest::selectWithEmptyCodeClearsSelection()
{
    LanguagePackModel model;
    model.setLanguages(sampleLanguages());
    model.select(QStringLiteral("en"));

    model.select(QString());

    QVERIFY2(model.selectedCode().isEmpty(), "an empty code clears the choice");
    QVERIFY2(model.selectedName().isEmpty(), "a cleared choice has no name");
}

void LanguagePackModelTest::selectEmitsChangedOnlyForRealChange()
{
    LanguagePackModel model;
    model.setLanguages(sampleLanguages());

    QSignalSpy spy(&model, &LanguagePackModel::changed);

    model.select(QStringLiteral("de"));
    QCOMPARE(spy.count(), 1);

    model.select(QStringLiteral("de"));
    QCOMPARE(spy.count(), 1);

    // A different spelling of the stored code is the same selection.
    model.select(QStringLiteral("DE"));
    QCOMPARE(spy.count(), 1);

    model.select(QStringLiteral("klingon"));
    QCOMPARE(spy.count(), 1);

    model.select(QString());
    QCOMPARE(spy.count(), 2);

    model.select(QString());
    QCOMPARE(spy.count(), 2);
}

void LanguagePackModelTest::selectedNameReportsTheRowName()
{
    LanguagePackModel model;
    model.setLanguages(sampleLanguages());

    model.select(QStringLiteral("en"));
    QCOMPARE(model.selectedName(), QStringLiteral("English"));

    model.select(QStringLiteral("pt-br"));
    QCOMPARE(model.selectedName(), QStringLiteral("Portuguese (Brazil)"));
}

void LanguagePackModelTest::selectedNameIsEmptyWithoutSelection()
{
    LanguagePackModel model;
    model.setLanguages(sampleLanguages());

    QVERIFY2(model.selectedName().isEmpty(), "no selection has no name");
}

void LanguagePackModelTest::selectedNameIsEmptyForACodeNoLongerStored()
{
    LanguagePackModel model;
    model.setLanguages(sampleLanguages());
    model.select(QStringLiteral("de"));

    model.setLanguages({row(QStringLiteral("en"), QStringLiteral("English"))});

    QCOMPARE(model.selectedCode(), QStringLiteral("de"));
    QVERIFY2(model.selectedName().isEmpty(),
             "a selection that is not stored has no name");
}

void LanguagePackModelTest::clearDropsLanguagesAndSelection()
{
    LanguagePackModel model;
    model.setLanguages(sampleLanguages());
    model.select(QStringLiteral("en"));

    model.clear();

    QCOMPARE(model.count(), 0);
    QVERIFY(model.languages().isEmpty());
    QVERIFY2(model.selectedCode().isEmpty(), "clear drops the selection");
    QVERIFY2(model.selectedName().isEmpty(), "clear leaves no selected name");
}

void LanguagePackModelTest::clearEmitsChangedOnlyWhenSomethingIsDropped()
{
    LanguagePackModel model;
    QSignalSpy spy(&model, &LanguagePackModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);

    model.setLanguages(sampleLanguages());
    QCOMPARE(spy.count(), 1);

    model.clear();
    QCOMPARE(spy.count(), 2);

    model.clear();
    QCOMPARE(spy.count(), 2);
}

QTEST_GUILESS_MAIN(LanguagePackModelTest)
#include "tst_language_pack_model.moc"
