#include "telegram/PaymentMethodModel.h"

#include <QSignalSpy>
#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

PaymentMethodRow makeMethod(const QString& id, const QString& title,
                            const QString& subtitle = QStringLiteral("Visa ... 4242"),
                            const QString& kind = QStringLiteral("card"),
                            bool saved = false)
{
    PaymentMethodRow method;
    method.id = id;
    method.title = title;
    method.subtitle = subtitle;
    method.kind = kind;
    method.saved = saved;
    return method;
}

// Two saved cards and one Stars balance, so a test only has to change the row it
// is about.
QVector<PaymentMethodRow> sampleMethods()
{
    return {makeMethod(QStringLiteral("card-1"), QStringLiteral("Personal card"),
                       QStringLiteral("Visa ... 4242"), QStringLiteral("card"), true),
            makeMethod(QStringLiteral("stars"), QStringLiteral("Telegram Stars"),
                       QStringLiteral("812 stars"), QStringLiteral("stars")),
            makeMethod(QStringLiteral("card-2"), QStringLiteral("Work card"),
                       QStringLiteral("Mastercard ... 1111"), QStringLiteral("card"), true)};
}

} // namespace

class PaymentMethodModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setMethodsKeepsOrder();
    void setMethodsDropsEmptyIds();
    void setMethodsKeepsFirstDuplicate();
    void setMethodsEmitsChangedOnce();
    void setMethodsEmptyOnEmptyModelIsSilent();
    void setMethodsSameMethodsIsSilent();
    void setMethodsChangedFieldIsAChange();
    void setMethodsSavedFlipIsAChange();
    void setMethodsReorderedIsAChange();
    void setMethodsReplacesPrevious();
    void setMethodsKeepsSelectionWhenStillPresent();
    void setMethodsDropsSelectionWhenMethodRemoved();
    void methodsReturnsCopy();
    void countMatchesMethods();
    void savedMethodsFiltersSaved();
    void savedMethodsKeepsOrder();
    void savedMethodsEmptyWhenNoneSaved();
    void selectStoresKnownId();
    void selectEmitsChangedOnce();
    void selectUnknownIdIsIgnored();
    void selectUnknownIdOnEmptyModelIsSilent();
    void selectSameIdIsSilent();
    void selectEmptyClearsSelection();
    void selectEmptyWithoutSelectionIsSilent();
    void selectAfterReload();
    void selectedMethodReturnsRow();
    void selectedMethodDefaultWhenNoneSelected();
    void selectedMethodDefaultAfterClear();
    void clearForgetsMethodsAndSelection();
    void clearOnEmptyModelIsSilent();
    void modelIsUsableAfterClear();
    void changedOnlyOnRealChanges();
};

void PaymentMethodModelTest::startsEmpty()
{
    const PaymentMethodModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.methods().isEmpty());
    QVERIFY(model.savedMethods().isEmpty());
    QCOMPARE(model.selectedId(), QString());
    QVERIFY(model.selectedMethod().id.isEmpty());
}

void PaymentMethodModelTest::setMethodsKeepsOrder()
{
    PaymentMethodModel model;
    model.setMethods({makeMethod(QStringLiteral("card-2"), QStringLiteral("Work card")),
                      makeMethod(QStringLiteral("stars"), QStringLiteral("Telegram Stars")),
                      makeMethod(QStringLiteral("card-1"), QStringLiteral("Personal card"))});

    const QVector<PaymentMethodRow> methods = model.methods();
    QCOMPARE(int(methods.size()), 3);
    // The caller's order is the screen's order: nothing here sorts by id or kind.
    QCOMPARE(methods.at(0).id, QStringLiteral("card-2"));
    QCOMPARE(methods.at(1).id, QStringLiteral("stars"));
    QCOMPARE(methods.at(2).id, QStringLiteral("card-1"));
}

void PaymentMethodModelTest::setMethodsDropsEmptyIds()
{
    PaymentMethodModel model;
    model.setMethods({makeMethod(QString(), QStringLiteral("Nameless")),
                      makeMethod(QStringLiteral("card-1"), QStringLiteral("Personal card")),
                      makeMethod(QString(), QStringLiteral("Nameless again"))});

    // A row without an id can be neither selected nor paid with.
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.methods().at(0).id, QStringLiteral("card-1"));
}

void PaymentMethodModelTest::setMethodsKeepsFirstDuplicate()
{
    PaymentMethodModel model;
    model.setMethods({makeMethod(QStringLiteral("card-1"), QStringLiteral("First")),
                      makeMethod(QStringLiteral("card-1"), QStringLiteral("Second")),
                      makeMethod(QStringLiteral("stars"), QStringLiteral("Other"))});

    QCOMPARE(model.count(), 2);
    QCOMPARE(model.methods().at(0).title, QStringLiteral("First"));
    QCOMPARE(model.methods().at(1).id, QStringLiteral("stars"));
}

void PaymentMethodModelTest::setMethodsEmitsChangedOnce()
{
    PaymentMethodModel model;
    QSignalSpy spy(&model, &PaymentMethodModel::changed);

    model.setMethods({makeMethod(QStringLiteral("card-1"), QStringLiteral("Personal card"))});

    QCOMPARE(spy.count(), 1);
}

void PaymentMethodModelTest::setMethodsEmptyOnEmptyModelIsSilent()
{
    PaymentMethodModel model;
    QSignalSpy spy(&model, &PaymentMethodModel::changed);

    // Every row is unusable, so the normalized list is the one already stored.
    model.setMethods({makeMethod(QString(), QStringLiteral("Nameless"))});

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 0);
}

void PaymentMethodModelTest::setMethodsSameMethodsIsSilent()
{
    PaymentMethodModel model;
    const QVector<PaymentMethodRow> methods = sampleMethods();
    model.setMethods(methods);

    QSignalSpy spy(&model, &PaymentMethodModel::changed);
    model.setMethods(methods);

    // A refresh that brings the same rows must not repaint the list.
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 3);
}

void PaymentMethodModelTest::setMethodsChangedFieldIsAChange()
{
    PaymentMethodModel model;
    model.setMethods({makeMethod(QStringLiteral("card-1"), QStringLiteral("Personal card"))});

    // The provider re-issues the card, so the masked number moves.
    QSignalSpy spy(&model, &PaymentMethodModel::changed);
    model.setMethods({makeMethod(QStringLiteral("card-1"), QStringLiteral("Personal card"),
                                 QStringLiteral("Visa ... 9999"))});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.methods().at(0).subtitle, QStringLiteral("Visa ... 9999"));
}

void PaymentMethodModelTest::setMethodsSavedFlipIsAChange()
{
    PaymentMethodModel model;
    model.setMethods({makeMethod(QStringLiteral("card-1"), QStringLiteral("Personal card"),
                                 QStringLiteral("Visa ... 4242"), QStringLiteral("card"),
                                 false)});

    // saved decides whether savedMethods() offers the row, so it is display data
    // even though the id and text did not move.
    QSignalSpy spy(&model, &PaymentMethodModel::changed);
    model.setMethods({makeMethod(QStringLiteral("card-1"), QStringLiteral("Personal card"),
                                 QStringLiteral("Visa ... 4242"), QStringLiteral("card"),
                                 true)});

    QCOMPARE(spy.count(), 1);
    QVERIFY(model.methods().at(0).saved);
    QCOMPARE(int(model.savedMethods().size()), 1);
}

void PaymentMethodModelTest::setMethodsReorderedIsAChange()
{
    PaymentMethodModel model;
    model.setMethods({makeMethod(QStringLiteral("card-1"), QStringLiteral("Personal card")),
                      makeMethod(QStringLiteral("stars"), QStringLiteral("Telegram Stars"))});

    QSignalSpy spy(&model, &PaymentMethodModel::changed);
    model.setMethods({makeMethod(QStringLiteral("stars"), QStringLiteral("Telegram Stars")),
                      makeMethod(QStringLiteral("card-1"), QStringLiteral("Personal card"))});

    // Same rows, different display order: the screen would look different.
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.methods().at(0).id, QStringLiteral("stars"));
}

void PaymentMethodModelTest::setMethodsReplacesPrevious()
{
    PaymentMethodModel model;
    model.setMethods(sampleMethods());
    model.setMethods({makeMethod(QStringLiteral("stars"), QStringLiteral("Telegram Stars"))});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.methods().at(0).id, QStringLiteral("stars"));
}

void PaymentMethodModelTest::setMethodsKeepsSelectionWhenStillPresent()
{
    PaymentMethodModel model;
    model.setMethods(sampleMethods());
    model.select(QStringLiteral("card-2"));

    // A reload may rewrite the saved card's masked number; the buyer's choice
    // must not jump back to nothing for it.
    QSignalSpy spy(&model, &PaymentMethodModel::changed);
    model.setMethods({makeMethod(QStringLiteral("stars"), QStringLiteral("Telegram Stars")),
                      makeMethod(QStringLiteral("card-2"), QStringLiteral("Work card"),
                                 QStringLiteral("Mastercard ... 2222"))});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.selectedId(), QStringLiteral("card-2"));
    QCOMPARE(model.selectedMethod().subtitle, QStringLiteral("Mastercard ... 2222"));
}

void PaymentMethodModelTest::setMethodsDropsSelectionWhenMethodRemoved()
{
    PaymentMethodModel model;
    model.setMethods(sampleMethods());
    model.select(QStringLiteral("stars"));

    QSignalSpy spy(&model, &PaymentMethodModel::changed);
    model.setMethods({makeMethod(QStringLiteral("card-1"), QStringLiteral("Personal card"))});

    // selectedId() must never name a row the list no longer offers.
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.selectedId(), QString());
    QVERIFY(model.selectedMethod().id.isEmpty());
}

void PaymentMethodModelTest::methodsReturnsCopy()
{
    PaymentMethodModel model;
    model.setMethods({makeMethod(QStringLiteral("card-1"), QStringLiteral("Personal card"))});

    QVector<PaymentMethodRow> copy = model.methods();
    copy[0].title = QStringLiteral("Mutated");

    // Handing out a copy keeps the model's own state private.
    QCOMPARE(model.methods().at(0).title, QStringLiteral("Personal card"));
}

void PaymentMethodModelTest::countMatchesMethods()
{
    PaymentMethodModel model;
    QCOMPARE(model.count(), 0);

    model.setMethods(sampleMethods());
    QCOMPARE(model.count(), int(model.methods().size()));
    QCOMPARE(model.count(), 3);
}

void PaymentMethodModelTest::savedMethodsFiltersSaved()
{
    PaymentMethodModel model;
    model.setMethods(sampleMethods());

    const QVector<PaymentMethodRow> saved = model.savedMethods();
    QCOMPARE(int(saved.size()), 2);
    QCOMPARE(saved.at(0).id, QStringLiteral("card-1"));
    QCOMPARE(saved.at(1).id, QStringLiteral("card-2"));
}

void PaymentMethodModelTest::savedMethodsKeepsOrder()
{
    PaymentMethodModel model;
    model.setMethods({makeMethod(QStringLiteral("card-9"), QStringLiteral("Later"),
                                 QStringLiteral("Visa ... 9"), QStringLiteral("card"), true),
                      makeMethod(QStringLiteral("stars"), QStringLiteral("Telegram Stars")),
                      makeMethod(QStringLiteral("card-3"), QStringLiteral("Earlier"),
                                 QStringLiteral("Visa ... 3"), QStringLiteral("card"), true)});

    // savedMethods is a filter, not a re-sort: the list order is the screen's.
    const QVector<PaymentMethodRow> saved = model.savedMethods();
    QCOMPARE(int(saved.size()), 2);
    QCOMPARE(saved.at(0).id, QStringLiteral("card-9"));
    QCOMPARE(saved.at(1).id, QStringLiteral("card-3"));
}

void PaymentMethodModelTest::savedMethodsEmptyWhenNoneSaved()
{
    PaymentMethodModel model;
    model.setMethods({makeMethod(QStringLiteral("stars"), QStringLiteral("Telegram Stars"))});

    QVERIFY(model.savedMethods().isEmpty());
    QCOMPARE(model.count(), 1);
}

void PaymentMethodModelTest::selectStoresKnownId()
{
    PaymentMethodModel model;
    model.setMethods(sampleMethods());
    QCOMPARE(model.selectedId(), QString());

    model.select(QStringLiteral("card-1"));

    QCOMPARE(model.selectedId(), QStringLiteral("card-1"));
    QCOMPARE(model.selectedMethod().title, QStringLiteral("Personal card"));
}

void PaymentMethodModelTest::selectEmitsChangedOnce()
{
    PaymentMethodModel model;
    model.setMethods(sampleMethods());
    QSignalSpy spy(&model, &PaymentMethodModel::changed);

    model.select(QStringLiteral("stars"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.selectedId(), QStringLiteral("stars"));
}

void PaymentMethodModelTest::selectUnknownIdIsIgnored()
{
    PaymentMethodModel model;
    model.setMethods(sampleMethods());
    model.select(QStringLiteral("card-1"));

    // A tap on a row the last reload removed must not select an invisible method.
    QSignalSpy spy(&model, &PaymentMethodModel::changed);
    model.select(QStringLiteral("gone"));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.selectedId(), QStringLiteral("card-1"));
}

void PaymentMethodModelTest::selectUnknownIdOnEmptyModelIsSilent()
{
    PaymentMethodModel model;
    QSignalSpy spy(&model, &PaymentMethodModel::changed);

    model.select(QStringLiteral("card-1"));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.selectedId(), QString());
}

void PaymentMethodModelTest::selectSameIdIsSilent()
{
    PaymentMethodModel model;
    model.setMethods(sampleMethods());
    model.select(QStringLiteral("card-1"));

    QSignalSpy spy(&model, &PaymentMethodModel::changed);
    model.select(QStringLiteral("card-1"));

    // Re-selecting the current row must not restart the screen's animation.
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.selectedId(), QStringLiteral("card-1"));
}

void PaymentMethodModelTest::selectEmptyClearsSelection()
{
    PaymentMethodModel model;
    model.setMethods(sampleMethods());
    model.select(QStringLiteral("card-2"));

    QSignalSpy spy(&model, &PaymentMethodModel::changed);
    model.select(QString());

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.selectedId(), QString());
    QVERIFY(model.selectedMethod().id.isEmpty());
}

void PaymentMethodModelTest::selectEmptyWithoutSelectionIsSilent()
{
    PaymentMethodModel model;
    model.setMethods(sampleMethods());
    QSignalSpy spy(&model, &PaymentMethodModel::changed);

    model.select(QString());

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.selectedId(), QString());
}

void PaymentMethodModelTest::selectAfterReload()
{
    PaymentMethodModel model;
    model.setMethods(sampleMethods());
    model.select(QStringLiteral("card-1"));
    model.setMethods(sampleMethods());

    // The reload kept the selection; picking another row still works.
    model.select(QStringLiteral("stars"));

    QCOMPARE(model.selectedId(), QStringLiteral("stars"));
    QCOMPARE(model.selectedMethod().kind, QStringLiteral("stars"));
}

void PaymentMethodModelTest::selectedMethodReturnsRow()
{
    PaymentMethodModel model;
    model.setMethods(sampleMethods());
    model.select(QStringLiteral("card-2"));

    const PaymentMethodRow row = model.selectedMethod();
    QCOMPARE(row.id, QStringLiteral("card-2"));
    QCOMPARE(row.title, QStringLiteral("Work card"));
    QCOMPARE(row.subtitle, QStringLiteral("Mastercard ... 1111"));
    QCOMPARE(row.kind, QStringLiteral("card"));
    QVERIFY(row.saved);
}

void PaymentMethodModelTest::selectedMethodDefaultWhenNoneSelected()
{
    PaymentMethodModel model;
    model.setMethods(sampleMethods());

    // A caller may read title/subtitle without a has-selection branch.
    const PaymentMethodRow row = model.selectedMethod();
    QVERIFY(row.id.isEmpty());
    QVERIFY(row.title.isEmpty());
    QVERIFY(row.subtitle.isEmpty());
    QVERIFY(row.kind.isEmpty());
    QVERIFY(!row.saved);
}

void PaymentMethodModelTest::selectedMethodDefaultAfterClear()
{
    PaymentMethodModel model;
    model.setMethods(sampleMethods());
    model.select(QStringLiteral("card-1"));
    model.clear();

    QVERIFY(model.selectedMethod().id.isEmpty());
    QCOMPARE(model.selectedId(), QString());
}

void PaymentMethodModelTest::clearForgetsMethodsAndSelection()
{
    PaymentMethodModel model;
    model.setMethods(sampleMethods());
    model.select(QStringLiteral("card-1"));

    QSignalSpy spy(&model, &PaymentMethodModel::changed);
    model.clear();

    QCOMPARE(spy.count(), 1);
    QVERIFY(model.methods().isEmpty());
    QVERIFY(model.savedMethods().isEmpty());
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.selectedId(), QString());
}

void PaymentMethodModelTest::clearOnEmptyModelIsSilent()
{
    PaymentMethodModel model;
    QSignalSpy spy(&model, &PaymentMethodModel::changed);

    model.clear();

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.methods().isEmpty());
}

void PaymentMethodModelTest::modelIsUsableAfterClear()
{
    PaymentMethodModel model;
    model.setMethods(sampleMethods());
    model.select(QStringLiteral("card-1"));
    model.clear();

    // A fresh answer after the checkout reopened behaves like a first load.
    model.setMethods({makeMethod(QStringLiteral("stars"), QStringLiteral("Telegram Stars"))});
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.selectedId(), QString());
    QVERIFY(model.savedMethods().isEmpty());

    model.select(QStringLiteral("stars"));
    QCOMPARE(model.selectedMethod().id, QStringLiteral("stars"));
}

void PaymentMethodModelTest::changedOnlyOnRealChanges()
{
    PaymentMethodModel model;
    const QVector<PaymentMethodRow> methods = sampleMethods();
    QSignalSpy spy(&model, &PaymentMethodModel::changed);

    model.setMethods(methods);                 // 1: first load
    model.setMethods(methods);                 // silent: same rows
    model.select(QStringLiteral("card-1"));    // 2: new selection
    model.select(QStringLiteral("card-1"));    // silent: same selection
    model.select(QStringLiteral("gone"));      // silent: unknown id
    model.select(QString());                   // 3: selection cleared
    model.select(QString());                   // silent: already cleared
    model.clear();                             // 4: rows dropped
    model.clear();                             // silent: already empty

    QCOMPARE(spy.count(), 4);
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.selectedId(), QString());
}

QTEST_GUILESS_MAIN(PaymentMethodModelTest)
#include "tst_payment_method_model.moc"
