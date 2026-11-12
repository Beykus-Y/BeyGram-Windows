#include "telegram/TranslationLanguageModel.h"

#include <QSignalSpy>
#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

LanguageRow language(const QString& code, const QString& name,
                     const QString& nativeName = QString())
{
    LanguageRow row;
    row.code = code;
    row.name = name;
    row.nativeName = nativeName;
    return row;
}

QStringList codes(const QVector<LanguageRow>& rows)
{
    QStringList list;
    list.reserve(rows.size());
    for (const LanguageRow& row : rows)
        list.append(row.code);
    return list;
}

QVector<LanguageRow> sampleLanguages()
{
    return {language(QStringLiteral("en"), QStringLiteral("English"),
                     QStringLiteral("English")),
            language(QStringLiteral("de"), QStringLiteral("German"),
                     QStringLiteral("Deutsch")),
            language(QStringLiteral("pt-br"), QStringLiteral("Brazilian Portuguese"),
                     QStringLiteral("Português (Brasil)"))};
}

} // namespace

class TranslationLanguageModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setLanguagesStoresRows();
    void setLanguagesDropsRowsWithoutCode();
    void setLanguagesLowercasesCodes();
    void setLanguagesDeduplicatesKeepingFirst();
    void setLanguagesPreservesOrder();
    void setLanguagesEmitsOnlyOnChange();
    void setLanguagesKeepsSelection();
    void countFollowsLanguages();
    void filteredEmptyQueryReturnsAll();
    void filteredMatchesCodeCaseInsensitively();
    void filteredMatchesNameCaseInsensitively();
    void filteredIgnoresNativeName();
    void filteredReturnsNothingWhenNothingMatches();
    void selectStoresLowercasedCode();
    void selectIgnoresUnknownCode();
    void selectEmptyClearsSelection();
    void selectEmitsOnlyOnChange();
    void selectedNameFollowsSelection();
    void selectedNameIsEmptyWhenCodeNotStored();
    void autoTranslateDefaultsOff();
    void setAutoTranslateEmitsOnlyOnChange();
    void clearDropsEverything();
    void clearIsNoOpWhenEmpty();
    void emitsChangedOnlyOnRealChanges();
};

void TranslationLanguageModelTest::startsEmpty()
{
    TranslationLanguageModel model;

    QVERIFY(model.languages().isEmpty());
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.selectedCode(), QString());
    QCOMPARE(model.selectedName(), QString());
    QVERIFY(!model.autoTranslate());
    QVERIFY(model.filtered(QString()).isEmpty());
}

void TranslationLanguageModelTest::setLanguagesStoresRows()
{
    TranslationLanguageModel model;
    model.setLanguages(sampleLanguages());

    QCOMPARE(model.count(), 3);
    QCOMPARE(codes(model.languages()),
             QStringList({QStringLiteral("en"), QStringLiteral("de"),
                          QStringLiteral("pt-br")}));
    QCOMPARE(model.languages().at(0).name, QStringLiteral("English"));
    QCOMPARE(model.languages().at(2).nativeName,
             QStringLiteral("Português (Brasil)"));
}

void TranslationLanguageModelTest::setLanguagesDropsRowsWithoutCode()
{
    TranslationLanguageModel model;
    model.setLanguages({language(QString(), QStringLiteral("Nameless")),
                        language(QStringLiteral("   "), QStringLiteral("Blank")),
                        language(QStringLiteral("en"), QStringLiteral("English")),
                        language(QString(), QStringLiteral("Nameless again"))});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.languages().at(0).code, QStringLiteral("en"));
    QVERIFY(model.languages().at(0).name == QStringLiteral("English"));
}

void TranslationLanguageModelTest::setLanguagesLowercasesCodes()
{
    TranslationLanguageModel model;
    model.setLanguages({language(QStringLiteral("EN"), QStringLiteral("English")),
                        language(QStringLiteral(" Pt-BR "),
                                 QStringLiteral("Brazilian Portuguese"))});

    QCOMPARE(codes(model.languages()),
             QStringList({QStringLiteral("en"), QStringLiteral("pt-br")}));
}

void TranslationLanguageModelTest::setLanguagesDeduplicatesKeepingFirst()
{
    TranslationLanguageModel model;
    model.setLanguages({language(QStringLiteral("en"), QStringLiteral("English")),
                        language(QStringLiteral("EN"), QStringLiteral("English again")),
                        language(QStringLiteral("de"), QStringLiteral("German")),
                        language(QStringLiteral("En"), QStringLiteral("Third English"))});

    QCOMPARE(model.count(), 2);
    QCOMPARE(codes(model.languages()),
             QStringList({QStringLiteral("en"), QStringLiteral("de")}));
    QCOMPARE(model.languages().at(0).name, QStringLiteral("English"));
}

void TranslationLanguageModelTest::setLanguagesPreservesOrder()
{
    TranslationLanguageModel model;
    model.setLanguages({language(QStringLiteral("zz"), QStringLiteral("Zulu")),
                        language(QStringLiteral("aa"), QStringLiteral("Aar")),
                        language(QStringLiteral("mm"), QStringLiteral("Mmm"))});

    QCOMPARE(codes(model.languages()),
             QStringList({QStringLiteral("zz"), QStringLiteral("aa"),
                          QStringLiteral("mm")}));
}

void TranslationLanguageModelTest::setLanguagesEmitsOnlyOnChange()
{
    TranslationLanguageModel model;
    QSignalSpy spy(&model, &TranslationLanguageModel::changed);

    model.setLanguages(sampleLanguages());                 // 1
    QCOMPARE(spy.count(), 1);

    model.setLanguages(sampleLanguages());                 // no-op
    QCOMPARE(spy.count(), 1);

    // Same codes in a different spelling normalise to the same rows.
    model.setLanguages({language(QStringLiteral("EN"), QStringLiteral("English"),
                                 QStringLiteral("English")),
                        language(QStringLiteral("DE"), QStringLiteral("German"),
                                 QStringLiteral("Deutsch")),
                        language(QStringLiteral("PT-BR"),
                                 QStringLiteral("Brazilian Portuguese"),
                                 QStringLiteral("Português (Brasil)"))});
    QCOMPARE(spy.count(), 1);

    model.setLanguages({language(QStringLiteral("en"), QStringLiteral("English"),
                                 QStringLiteral("English"))}); // 2
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.count(), 1);
}

void TranslationLanguageModelTest::setLanguagesKeepsSelection()
{
    TranslationLanguageModel model;
    model.setLanguages(sampleLanguages());
    model.select(QStringLiteral("de"));

    model.setLanguages(sampleLanguages());
    QCOMPARE(model.selectedCode(), QStringLiteral("de"));
    QCOMPARE(model.selectedName(), QStringLiteral("German"));

    // The selection survives even when a refresh drops the row; only the name
    // becomes unavailable.
    model.setLanguages({language(QStringLiteral("en"), QStringLiteral("English"))});
    QCOMPARE(model.selectedCode(), QStringLiteral("de"));
    QCOMPARE(model.selectedName(), QString());
}

void TranslationLanguageModelTest::countFollowsLanguages()
{
    TranslationLanguageModel model;
    QCOMPARE(model.count(), 0);

    model.setLanguages(sampleLanguages());
    QCOMPARE(model.count(), 3);

    model.setLanguages({language(QStringLiteral("en"), QStringLiteral("English"))});
    QCOMPARE(model.count(), 1);

    model.clear();
    QCOMPARE(model.count(), 0);
}

void TranslationLanguageModelTest::filteredEmptyQueryReturnsAll()
{
    TranslationLanguageModel model;
    model.setLanguages(sampleLanguages());

    QCOMPARE(model.filtered(QString()).size(), 3);
    QCOMPARE(codes(model.filtered(QString())), codes(model.languages()));
}

void TranslationLanguageModelTest::filteredMatchesCodeCaseInsensitively()
{
    TranslationLanguageModel model;
    model.setLanguages(sampleLanguages());

    QCOMPARE(codes(model.filtered(QStringLiteral("PT"))),
             QStringList({QStringLiteral("pt-br")}));
    QCOMPARE(codes(model.filtered(QStringLiteral("pt-br"))),
             QStringList({QStringLiteral("pt-br")}));
    QCOMPARE(codes(model.filtered(QStringLiteral("en"))),
             QStringList({QStringLiteral("en")}));
}

void TranslationLanguageModelTest::filteredMatchesNameCaseInsensitively()
{
    TranslationLanguageModel model;
    model.setLanguages(sampleLanguages());

    QCOMPARE(codes(model.filtered(QStringLiteral("german"))),
             QStringList({QStringLiteral("de")}));
    QCOMPARE(codes(model.filtered(QStringLiteral("PORTUGUESE"))),
             QStringList({QStringLiteral("pt-br")}));
}

void TranslationLanguageModelTest::filteredIgnoresNativeName()
{
    TranslationLanguageModel model;
    model.setLanguages(sampleLanguages());

    // nativeName is not a search field here; the picker searches the code and
    // the English label only.
    QVERIFY(model.filtered(QStringLiteral("Deutsch")).isEmpty());
}

void TranslationLanguageModelTest::filteredReturnsNothingWhenNothingMatches()
{
    TranslationLanguageModel model;
    model.setLanguages(sampleLanguages());

    QVERIFY(model.filtered(QStringLiteral("klingon")).isEmpty());
}

void TranslationLanguageModelTest::selectStoresLowercasedCode()
{
    TranslationLanguageModel model;
    model.setLanguages(sampleLanguages());

    model.select(QStringLiteral("DE"));
    QCOMPARE(model.selectedCode(), QStringLiteral("de"));

    model.select(QStringLiteral(" Pt-BR "));
    QCOMPARE(model.selectedCode(), QStringLiteral("pt-br"));
}

void TranslationLanguageModelTest::selectIgnoresUnknownCode()
{
    TranslationLanguageModel model;
    model.setLanguages(sampleLanguages());
    model.select(QStringLiteral("de"));

    QSignalSpy spy(&model, &TranslationLanguageModel::changed);
    model.select(QStringLiteral("klingon"));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.selectedCode(), QStringLiteral("de"));
}

void TranslationLanguageModelTest::selectEmptyClearsSelection()
{
    TranslationLanguageModel model;
    model.setLanguages(sampleLanguages());
    model.select(QStringLiteral("de"));

    model.select(QString());
    QCOMPARE(model.selectedCode(), QString());
    QCOMPARE(model.selectedName(), QString());

    // A blank code normalises to empty and clears as well.
    model.select(QStringLiteral("en"));
    model.select(QStringLiteral("   "));
    QCOMPARE(model.selectedCode(), QString());
}

void TranslationLanguageModelTest::selectEmitsOnlyOnChange()
{
    TranslationLanguageModel model;
    model.setLanguages(sampleLanguages());
    QSignalSpy spy(&model, &TranslationLanguageModel::changed);

    model.select(QStringLiteral("en"));      // 1
    model.select(QStringLiteral("en"));      // no-op
    model.select(QStringLiteral(" EN "));    // normalises -> no-op
    model.select(QStringLiteral("de"));      // 2
    model.select(QString());                 // 3
    model.select(QString());                 // no-op

    QCOMPARE(spy.count(), 3);
}

void TranslationLanguageModelTest::selectedNameFollowsSelection()
{
    TranslationLanguageModel model;
    model.setLanguages(sampleLanguages());

    QCOMPARE(model.selectedName(), QString());

    model.select(QStringLiteral("pt-br"));
    QCOMPARE(model.selectedName(), QStringLiteral("Brazilian Portuguese"));

    model.select(QStringLiteral("en"));
    QCOMPARE(model.selectedName(), QStringLiteral("English"));
}

void TranslationLanguageModelTest::selectedNameIsEmptyWhenCodeNotStored()
{
    TranslationLanguageModel model;
    model.setLanguages(sampleLanguages());
    model.select(QStringLiteral("de"));

    model.setLanguages({language(QStringLiteral("en"), QStringLiteral("English"))});

    QCOMPARE(model.selectedCode(), QStringLiteral("de"));
    QCOMPARE(model.selectedName(), QString());
}

void TranslationLanguageModelTest::autoTranslateDefaultsOff()
{
    TranslationLanguageModel model;
    QVERIFY(!model.autoTranslate());

    model.setAutoTranslate(true);
    QVERIFY(model.autoTranslate());

    // The flag is independent of the selection and the row list.
    model.setLanguages(sampleLanguages());
    QVERIFY(model.autoTranslate());
}

void TranslationLanguageModelTest::setAutoTranslateEmitsOnlyOnChange()
{
    TranslationLanguageModel model;
    QSignalSpy spy(&model, &TranslationLanguageModel::changed);

    model.setAutoTranslate(false);   // no-op
    QCOMPARE(spy.count(), 0);

    model.setAutoTranslate(true);    // 1
    model.setAutoTranslate(true);    // no-op
    model.setAutoTranslate(false);   // 2

    QCOMPARE(spy.count(), 2);
    QVERIFY(!model.autoTranslate());
}

void TranslationLanguageModelTest::clearDropsEverything()
{
    TranslationLanguageModel model;
    model.setLanguages(sampleLanguages());
    model.select(QStringLiteral("de"));
    model.setAutoTranslate(true);

    model.clear();

    QVERIFY(model.languages().isEmpty());
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.selectedCode(), QString());
    QCOMPARE(model.selectedName(), QString());
    QVERIFY(!model.autoTranslate());
}

void TranslationLanguageModelTest::clearIsNoOpWhenEmpty()
{
    TranslationLanguageModel model;
    QSignalSpy spy(&model, &TranslationLanguageModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);

    // Rows alone are still something to drop.
    model.setLanguages(sampleLanguages());
    spy.clear();
    model.clear();
    QCOMPARE(spy.count(), 1);
    model.clear();
    QCOMPARE(spy.count(), 1);
}

void TranslationLanguageModelTest::emitsChangedOnlyOnRealChanges()
{
    TranslationLanguageModel model;
    QSignalSpy spy(&model, &TranslationLanguageModel::changed);

    model.setLanguages(sampleLanguages());  // 1
    model.select(QStringLiteral("en"));     // 2
    model.setAutoTranslate(true);           // 3
    model.select(QStringLiteral("en"));     // no-op
    model.setAutoTranslate(true);           // no-op
    model.setLanguages(sampleLanguages());  // no-op
    model.clear();                          // 4

    QCOMPARE(spy.count(), 4);
    QVERIFY(model.languages().isEmpty());
    QCOMPARE(model.selectedCode(), QString());
    QVERIFY(!model.autoTranslate());
}

QTEST_GUILESS_MAIN(TranslationLanguageModelTest)
#include "tst_translation_language_model.moc"
