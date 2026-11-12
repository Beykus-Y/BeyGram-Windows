#include "telegram/CountryCodeModel.h"

#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

using namespace beygram::telegram;

namespace {

CountryRow row(const QString& iso,
               const QString& name,
               const QString& dialCode,
               const QString& flag = {})
{
    return CountryRow{iso, name, dialCode, flag};
}

QVector<CountryRow> sampleRows()
{
    return {
        row(QStringLiteral("de"), QStringLiteral("Germany"), QStringLiteral("+49"), QStringLiteral("🇩🇪")),
        row(QStringLiteral("us"), QStringLiteral("United States"), QStringLiteral("+1"), QStringLiteral("🇺🇸")),
        row(QStringLiteral("gb"), QStringLiteral("United Kingdom"), QStringLiteral("+44"), QStringLiteral("🇬🇧")),
    };
}

QStringList isosOf(const QVector<CountryRow>& rows)
{
    QStringList isos;
    isos.reserve(rows.size());
    for (const CountryRow& entry : rows)
        isos.append(entry.iso);
    return isos;
}

bool isEmptyRow(const CountryRow& entry)
{
    return entry.iso.isEmpty() && entry.name.isEmpty() && entry.dialCode.isEmpty()
        && entry.flag.isEmpty();
}

} // namespace

class CountryCodeModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void keepsIncomingOrder();
    void dropsRowsWithoutIso();
    void dedupesByIsoKeepingFirst();
    void uppercasesAndTrimsIso();
    void keepsDialCodeDigitsOnly();
    void filteredMatchesName();
    void filteredMatchesIso();
    void filteredMatchesDialCode();
    void filterIgnoresCase();
    void filterTrimsQuery();
    void filterOfEmptyQueryReturnsAll();
    void filterOfUnknownQueryReturnsEmpty();
    void byIsoFindsRow();
    void byIsoIgnoresCaseAndSurroundingSpace();
    void byIsoUnknownReturnsEmptyRow();
    void byDialCodeFindsFirstMatch();
    void byDialCodeIgnoresPunctuation();
    void byDialCodeUnknownReturnsEmptyRow();
    void selectStoresKnownIso();
    void selectIgnoresUnknownIso();
    void selectEmptyClearsSelection();
    void selectOfCurrentIsoEmitsNothing();
    void clearResetsRowsAndSelection();
    void setCountriesEmitsOnlyOnRealChanges();
    void clearEmitsOnlyOnce();
    void countriesReturnsIndependentCopy();
    void selectionSurvivesSetCountries();
};

void CountryCodeModelTest::startsEmpty()
{
    CountryCodeModel model;
    QCOMPARE(model.count(), 0);
    QVERIFY(model.countries().isEmpty());
    QVERIFY(model.filtered(QString()).isEmpty());
    QVERIFY(model.selectedIso().isEmpty());
    QVERIFY(isEmptyRow(model.byIso(QStringLiteral("DE"))));
    QVERIFY(isEmptyRow(model.byDialCode(QStringLiteral("49"))));
}

void CountryCodeModelTest::keepsIncomingOrder()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());

    QCOMPARE(model.count(), 3);
    QCOMPARE(isosOf(model.countries()),
             QStringList({QStringLiteral("DE"), QStringLiteral("US"), QStringLiteral("GB")}));
}

void CountryCodeModelTest::dropsRowsWithoutIso()
{
    CountryCodeModel model;
    model.setCountries({
        row(QString(), QStringLiteral("Nowhere"), QStringLiteral("999")),
        row(QStringLiteral("   "), QStringLiteral("Blank"), QStringLiteral("998")),
        row(QStringLiteral("fr"), QStringLiteral("France"), QStringLiteral("33")),
    });

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.countries().at(0).iso, QStringLiteral("FR"));
}

void CountryCodeModelTest::dedupesByIsoKeepingFirst()
{
    CountryCodeModel model;
    model.setCountries({
        row(QStringLiteral("de"), QStringLiteral("Germany"), QStringLiteral("49")),
        row(QStringLiteral("fr"), QStringLiteral("France"), QStringLiteral("33")),
        row(QStringLiteral("DE"), QStringLiteral("Deutschland"), QStringLiteral("0049")),
        row(QStringLiteral(" de "), QStringLiteral("Allemagne"), QStringLiteral("49")),
    });

    QCOMPARE(model.count(), 2);
    QCOMPARE(isosOf(model.countries()),
             QStringList({QStringLiteral("DE"), QStringLiteral("FR")}));
    QCOMPARE(model.countries().at(0).name, QStringLiteral("Germany"));
    QCOMPARE(model.countries().at(0).dialCode, QStringLiteral("49"));
}

void CountryCodeModelTest::uppercasesAndTrimsIso()
{
    CountryCodeModel model;
    model.setCountries({row(QStringLiteral("  gb  "), QStringLiteral("United Kingdom"),
                            QStringLiteral("44"))});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.countries().at(0).iso, QStringLiteral("GB"));
}

void CountryCodeModelTest::keepsDialCodeDigitsOnly()
{
    CountryCodeModel model;
    model.setCountries({
        row(QStringLiteral("us"), QStringLiteral("United States"), QStringLiteral("+1 (202)")),
        row(QStringLiteral("xx"), QStringLiteral("No digits"), QStringLiteral("n/a")),
    });

    QCOMPARE(model.countries().at(0).dialCode, QStringLiteral("1202"));
    QCOMPARE(model.countries().at(1).dialCode, QString());
}

void CountryCodeModelTest::filteredMatchesName()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());

    const QVector<CountryRow> matches = model.filtered(QStringLiteral("kingdom"));
    QCOMPARE(isosOf(matches), QStringList({QStringLiteral("GB")}));
}

void CountryCodeModelTest::filteredMatchesIso()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());

    QCOMPARE(isosOf(model.filtered(QStringLiteral("us"))),
             QStringList({QStringLiteral("US")}));
    QCOMPARE(isosOf(model.filtered(QStringLiteral("gb"))),
             QStringList({QStringLiteral("GB")}));
}

void CountryCodeModelTest::filteredMatchesDialCode()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());

    QCOMPARE(isosOf(model.filtered(QStringLiteral("44"))),
             QStringList({QStringLiteral("GB")}));
}

void CountryCodeModelTest::filterIgnoresCase()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());

    QCOMPARE(isosOf(model.filtered(QStringLiteral("GERMANY"))),
             QStringList({QStringLiteral("DE")}));
    QCOMPARE(isosOf(model.filtered(QStringLiteral("dE"))),
             QStringList({QStringLiteral("DE")}));
}

void CountryCodeModelTest::filterTrimsQuery()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());

    QCOMPARE(isosOf(model.filtered(QStringLiteral("  france  "))), QStringList());
    QCOMPARE(isosOf(model.filtered(QStringLiteral("  germany  "))),
             QStringList({QStringLiteral("DE")}));
}

void CountryCodeModelTest::filterOfEmptyQueryReturnsAll()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());

    QCOMPARE(model.filtered(QString()).size(), 3);
    QCOMPARE(model.filtered(QStringLiteral("   ")).size(), 3);
}

void CountryCodeModelTest::filterOfUnknownQueryReturnsEmpty()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());

    QVERIFY(model.filtered(QStringLiteral("atlantis")).isEmpty());
}

void CountryCodeModelTest::byIsoFindsRow()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());

    const CountryRow germany = model.byIso(QStringLiteral("DE"));
    QCOMPARE(germany.name, QStringLiteral("Germany"));
    QCOMPARE(germany.dialCode, QStringLiteral("49"));
    QCOMPARE(germany.flag, QStringLiteral("🇩🇪"));
}

void CountryCodeModelTest::byIsoIgnoresCaseAndSurroundingSpace()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());

    QCOMPARE(model.byIso(QStringLiteral("  de  ")).iso, QStringLiteral("DE"));
    QCOMPARE(model.byIso(QStringLiteral("uS")).iso, QStringLiteral("US"));
}

void CountryCodeModelTest::byIsoUnknownReturnsEmptyRow()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());

    QVERIFY(isEmptyRow(model.byIso(QStringLiteral("ZZ"))));
    QVERIFY(isEmptyRow(model.byIso(QString())));
}

void CountryCodeModelTest::byDialCodeFindsFirstMatch()
{
    CountryCodeModel model;
    model.setCountries({
        row(QStringLiteral("us"), QStringLiteral("United States"), QStringLiteral("1")),
        row(QStringLiteral("ca"), QStringLiteral("Canada"), QStringLiteral("1")),
    });

    const CountryRow match = model.byDialCode(QStringLiteral("1"));
    QCOMPARE(match.iso, QStringLiteral("US"));
    QCOMPARE(match.name, QStringLiteral("United States"));
}

void CountryCodeModelTest::byDialCodeIgnoresPunctuation()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());

    QCOMPARE(model.byDialCode(QStringLiteral("+49")).iso, QStringLiteral("DE"));
    QCOMPARE(model.byDialCode(QStringLiteral(" 44 ")).iso, QStringLiteral("GB"));
}

void CountryCodeModelTest::byDialCodeUnknownReturnsEmptyRow()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());

    QVERIFY(isEmptyRow(model.byDialCode(QStringLiteral("999"))));
    QVERIFY(isEmptyRow(model.byDialCode(QString())));
    QVERIFY(isEmptyRow(model.byDialCode(QStringLiteral("+"))));
}

void CountryCodeModelTest::selectStoresKnownIso()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());

    QSignalSpy spy(&model, &CountryCodeModel::changed);
    model.select(QStringLiteral("de"));

    QCOMPARE(model.selectedIso(), QStringLiteral("DE"));
    QCOMPARE(spy.count(), 1);

    model.select(QStringLiteral("  gb  "));
    QCOMPARE(model.selectedIso(), QStringLiteral("GB"));
    QCOMPARE(spy.count(), 2);
}

void CountryCodeModelTest::selectIgnoresUnknownIso()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());
    model.select(QStringLiteral("DE"));

    QSignalSpy spy(&model, &CountryCodeModel::changed);
    model.select(QStringLiteral("ZZ"));

    QCOMPARE(model.selectedIso(), QStringLiteral("DE"));
    QCOMPARE(spy.count(), 0);
}

void CountryCodeModelTest::selectEmptyClearsSelection()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());
    model.select(QStringLiteral("US"));

    QSignalSpy spy(&model, &CountryCodeModel::changed);
    model.select(QString());

    QVERIFY(model.selectedIso().isEmpty());
    QCOMPARE(spy.count(), 1);

    model.select(QStringLiteral("   "));
    QCOMPARE(spy.count(), 1);
}

void CountryCodeModelTest::selectOfCurrentIsoEmitsNothing()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());
    model.select(QStringLiteral("FR"));
    QVERIFY(model.selectedIso().isEmpty());

    model.select(QStringLiteral("DE"));
    QSignalSpy spy(&model, &CountryCodeModel::changed);
    model.select(QStringLiteral("de"));

    QCOMPARE(model.selectedIso(), QStringLiteral("DE"));
    QCOMPARE(spy.count(), 0);
}

void CountryCodeModelTest::clearResetsRowsAndSelection()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());
    model.select(QStringLiteral("GB"));

    model.clear();

    QCOMPARE(model.count(), 0);
    QVERIFY(model.countries().isEmpty());
    QVERIFY(model.selectedIso().isEmpty());
    QVERIFY(model.filtered(QStringLiteral("Germany")).isEmpty());
    QVERIFY(isEmptyRow(model.byIso(QStringLiteral("GB"))));
}

void CountryCodeModelTest::setCountriesEmitsOnlyOnRealChanges()
{
    CountryCodeModel model;
    QSignalSpy spy(&model, &CountryCodeModel::changed);

    model.setCountries(sampleRows());
    QCOMPARE(spy.count(), 1);

    model.setCountries(sampleRows());
    QCOMPARE(spy.count(), 1);

    model.setCountries({row(QStringLiteral("de"), QStringLiteral("Germany"),
                            QStringLiteral("49"))});
    QCOMPARE(spy.count(), 2);

    model.setCountries(QVector<CountryRow>());
    QCOMPARE(spy.count(), 3);
    QCOMPARE(model.count(), 0);

    model.setCountries(QVector<CountryRow>());
    QCOMPARE(spy.count(), 3);
}

void CountryCodeModelTest::clearEmitsOnlyOnce()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());

    QSignalSpy spy(&model, &CountryCodeModel::changed);
    model.clear();
    model.clear();

    QCOMPARE(spy.count(), 1);
}

void CountryCodeModelTest::countriesReturnsIndependentCopy()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());

    QVector<CountryRow> copy = model.countries();
    copy[0].name = QStringLiteral("Changed");

    QCOMPARE(model.countries().at(0).name, QStringLiteral("Germany"));
}

void CountryCodeModelTest::selectionSurvivesSetCountries()
{
    CountryCodeModel model;
    model.setCountries(sampleRows());
    model.select(QStringLiteral("US"));

    model.setCountries(sampleRows());

    QCOMPARE(model.selectedIso(), QStringLiteral("US"));
    QCOMPARE(model.byIso(model.selectedIso()).name, QStringLiteral("United States"));
}

QTEST_GUILESS_MAIN(CountryCodeModelTest)

#include "tst_country_code_model.moc"
