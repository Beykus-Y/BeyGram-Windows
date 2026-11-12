#include "telegram/PrivacySettingsModel.h"

#include <QSignalSpy>
#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

PrivacySettingRow makeRow(const QString& setting, const QString& value,
                          bool editable = true)
{
    PrivacySettingRow row;
    row.setting = setting;
    row.value = value;
    row.editable = editable;
    return row;
}

QString showStatus()
{
    return QStringLiteral("userPrivacySettingShowStatus");
}

QString allowCalls()
{
    return QStringLiteral("userPrivacySettingAllowCalls");
}

QString showPhoneNumber()
{
    return QStringLiteral("userPrivacySettingShowPhoneNumber");
}

QString allowAll()
{
    return QStringLiteral("userPrivacySettingRuleAllowAll");
}

QString allowContacts()
{
    return QStringLiteral("userPrivacySettingRuleAllowContacts");
}

QString restrictAll()
{
    return QStringLiteral("userPrivacySettingRuleRestrictAll");
}

} // namespace

class PrivacySettingsModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setRowsKeepsOrder();
    void setRowsEmitsChangedOnce();
    void setRowsDropsDuplicateSettings();
    void setRowsDropsRowsWithoutSettingName();
    void setRowsWithSameRowsDoesNotEmit();
    void setRowsReplacesPreviousRows();
    void rowsReturnsCopy();
    void valueForFindsRow();
    void valueForUnknownSettingIsEmpty();
    void setValueUpdatesExistingRow();
    void setValueEmitsChangedOnce();
    void setValueSameValueIsNoOp();
    void setValueUnknownSettingIsRejected();
    void setValueNonEditableRowIsRejected();
    void setValueNeverCreatesRow();
    void clearForgetsRows();
    void clearOnEmptyModelIsSilent();
    void modelIsUsableAfterClear();
};

void PrivacySettingsModelTest::startsEmpty()
{
    PrivacySettingsModel model;

    QVERIFY(model.rows().isEmpty());
    QVERIFY(model.valueFor(showStatus()).isEmpty());
}

void PrivacySettingsModelTest::setRowsKeepsOrder()
{
    PrivacySettingsModel model;
    model.setRows({makeRow(showStatus(), allowAll()),
                   makeRow(allowCalls(), allowContacts()),
                   makeRow(showPhoneNumber(), restrictAll(), false)});

    const QVector<PrivacySettingRow> rows = model.rows();
    QCOMPARE(rows.size(), 3);
    QCOMPARE(rows.at(0).setting, showStatus());
    QCOMPARE(rows.at(0).value, allowAll());
    QVERIFY(rows.at(0).editable);
    QCOMPARE(rows.at(1).setting, allowCalls());
    QCOMPARE(rows.at(1).value, allowContacts());
    QVERIFY(rows.at(1).editable);
    QCOMPARE(rows.at(2).setting, showPhoneNumber());
    QCOMPARE(rows.at(2).value, restrictAll());
    QVERIFY(!rows.at(2).editable);
}

void PrivacySettingsModelTest::setRowsEmitsChangedOnce()
{
    PrivacySettingsModel model;
    QSignalSpy spy(&model, &PrivacySettingsModel::changed);

    model.setRows({makeRow(showStatus(), allowAll())});

    QCOMPARE(spy.count(), 1);
}

void PrivacySettingsModelTest::setRowsDropsDuplicateSettings()
{
    PrivacySettingsModel model;

    // The first answer for a setting wins, so a merged snapshot does not depend
    // on which duplicate happened to arrive last.
    model.setRows({makeRow(showStatus(), allowAll()),
                   makeRow(showStatus(), restrictAll()),
                   makeRow(allowCalls(), allowContacts())});

    const QVector<PrivacySettingRow> rows = model.rows();
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).setting, showStatus());
    QCOMPARE(rows.at(0).value, allowAll());
    QCOMPARE(rows.at(1).setting, allowCalls());
    QCOMPARE(model.valueFor(showStatus()), allowAll());
}

void PrivacySettingsModelTest::setRowsDropsRowsWithoutSettingName()
{
    PrivacySettingsModel model;
    model.setRows({makeRow(QString(), allowAll()),
                   makeRow(showStatus(), allowAll())});

    const QVector<PrivacySettingRow> rows = model.rows();
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.at(0).setting, showStatus());
    QVERIFY(model.valueFor(QString()).isEmpty());
}

void PrivacySettingsModelTest::setRowsWithSameRowsDoesNotEmit()
{
    PrivacySettingsModel model;
    const QVector<PrivacySettingRow> rows{makeRow(showStatus(), allowAll()),
                                         makeRow(allowCalls(), allowContacts())};
    model.setRows(rows);

    QSignalSpy spy(&model, &PrivacySettingsModel::changed);
    model.setRows(rows);

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.rows() == rows);
}

void PrivacySettingsModelTest::setRowsReplacesPreviousRows()
{
    PrivacySettingsModel model;
    model.setRows({makeRow(showStatus(), allowAll()),
                   makeRow(allowCalls(), allowContacts())});

    // The second snapshot is authoritative, including the editability flags:
    // a setting the new answer marks read-only must stop accepting writes.
    model.setRows({makeRow(allowCalls(), allowContacts()),
                   makeRow(showPhoneNumber(), restrictAll(), false)});

    const QVector<PrivacySettingRow> rows = model.rows();
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).setting, allowCalls());
    QCOMPARE(rows.at(1).setting, showPhoneNumber());
    QVERIFY(!rows.at(1).editable);
    QVERIFY(model.valueFor(showStatus()).isEmpty());
    QVERIFY(!model.setValue(showPhoneNumber(), allowAll()));
}

void PrivacySettingsModelTest::rowsReturnsCopy()
{
    PrivacySettingsModel model;
    model.setRows({makeRow(showStatus(), allowAll())});

    QVector<PrivacySettingRow> copy = model.rows();
    QCOMPARE(copy.size(), 1);
    copy[0].value = restrictAll();

    // Editing the returned vector must not reach into the model.
    QCOMPARE(model.valueFor(showStatus()), allowAll());
}

void PrivacySettingsModelTest::valueForFindsRow()
{
    PrivacySettingsModel model;
    model.setRows({makeRow(showStatus(), allowAll()),
                   makeRow(allowCalls(), allowContacts())});

    QCOMPARE(model.valueFor(showStatus()), allowAll());
    QCOMPARE(model.valueFor(allowCalls()), allowContacts());
}

void PrivacySettingsModelTest::valueForUnknownSettingIsEmpty()
{
    PrivacySettingsModel model;
    model.setRows({makeRow(showStatus(), allowAll())});

    QVERIFY(model.valueFor(allowCalls()).isEmpty());
}

void PrivacySettingsModelTest::setValueUpdatesExistingRow()
{
    PrivacySettingsModel model;
    model.setRows({makeRow(showStatus(), allowAll()),
                   makeRow(allowCalls(), allowContacts())});

    QVERIFY(model.setValue(showStatus(), restrictAll()));

    QCOMPARE(model.valueFor(showStatus()), restrictAll());
    // Only the addressed row changes.
    QCOMPARE(model.valueFor(allowCalls()), allowContacts());
    QCOMPARE(model.rows().size(), 2);
}

void PrivacySettingsModelTest::setValueEmitsChangedOnce()
{
    PrivacySettingsModel model;
    model.setRows({makeRow(showStatus(), allowAll())});

    QSignalSpy spy(&model, &PrivacySettingsModel::changed);
    QVERIFY(model.setValue(showStatus(), restrictAll()));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.rows().at(0).value, restrictAll());
}

void PrivacySettingsModelTest::setValueSameValueIsNoOp()
{
    PrivacySettingsModel model;
    model.setRows({makeRow(showStatus(), allowAll())});

    QSignalSpy spy(&model, &PrivacySettingsModel::changed);
    QVERIFY(!model.setValue(showStatus(), allowAll()));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.valueFor(showStatus()), allowAll());
}

void PrivacySettingsModelTest::setValueUnknownSettingIsRejected()
{
    PrivacySettingsModel model;
    model.setRows({makeRow(showStatus(), allowAll())});

    QSignalSpy spy(&model, &PrivacySettingsModel::changed);
    QVERIFY(!model.setValue(allowCalls(), allowContacts()));

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.valueFor(allowCalls()).isEmpty());
    QCOMPARE(model.rows().size(), 1);
}

void PrivacySettingsModelTest::setValueNonEditableRowIsRejected()
{
    PrivacySettingsModel model;
    model.setRows({makeRow(showPhoneNumber(), allowAll(), false)});

    QSignalSpy spy(&model, &PrivacySettingsModel::changed);
    QVERIFY(!model.setValue(showPhoneNumber(), restrictAll()));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.valueFor(showPhoneNumber()), allowAll());
}

void PrivacySettingsModelTest::setValueNeverCreatesRow()
{
    PrivacySettingsModel model;

    QVERIFY(!model.setValue(showStatus(), allowAll()));
    QVERIFY(model.rows().isEmpty());

    model.setRows({makeRow(showStatus(), allowAll())});
    QVERIFY(!model.setValue(showPhoneNumber(), restrictAll()));

    QCOMPARE(model.rows().size(), 1);
    QVERIFY(model.valueFor(showPhoneNumber()).isEmpty());
}

void PrivacySettingsModelTest::clearForgetsRows()
{
    PrivacySettingsModel model;
    model.setRows({makeRow(showStatus(), allowAll()),
                   makeRow(allowCalls(), allowContacts())});

    QSignalSpy spy(&model, &PrivacySettingsModel::changed);
    model.clear();

    QCOMPARE(spy.count(), 1);
    QVERIFY(model.rows().isEmpty());
    QVERIFY(model.valueFor(showStatus()).isEmpty());
}

void PrivacySettingsModelTest::clearOnEmptyModelIsSilent()
{
    PrivacySettingsModel model;
    QSignalSpy spy(&model, &PrivacySettingsModel::changed);

    model.clear();

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.rows().isEmpty());
}

void PrivacySettingsModelTest::modelIsUsableAfterClear()
{
    PrivacySettingsModel model;
    model.setRows({makeRow(showStatus(), allowAll())});
    model.clear();

    // A fresh snapshot after a sign-out must behave like a first load.
    model.setRows({makeRow(showStatus(), restrictAll())});
    QCOMPARE(model.rows().size(), 1);
    QVERIFY(model.setValue(showStatus(), allowContacts()));
    QCOMPARE(model.valueFor(showStatus()), allowContacts());
}

QTEST_GUILESS_MAIN(PrivacySettingsModelTest)
#include "tst_privacy_settings_model.moc"
