#include "ui/PrivacyFacade.h"

#include <QList>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QtTest>

using namespace beygram::ui;

namespace {

QString showStatus()
{
    return QStringLiteral("userPrivacySettingShowStatus");
}

QString allowCalls()
{
    return QStringLiteral("userPrivacySettingAllowCalls");
}

QString everyone()
{
    return QStringLiteral("everyone");
}

QString contacts()
{
    return QStringLiteral("contacts");
}

QString closeFriends()
{
    return QStringLiteral("closeFriends");
}

QString nobody()
{
    return QStringLiteral("nobody");
}

QString allowAllRule()
{
    return QStringLiteral("userPrivacySettingRuleAllowAll");
}

QString allowContactsRule()
{
    return QStringLiteral("userPrivacySettingRuleAllowContacts");
}

} // namespace

class PrivacyFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsIdle();
    void setSettingsLoadsRows();
    void setSettingsReplacesPreviousRows();
    void setSettingsDropsNamelessAndDuplicateRows();
    void setSettingsIsSilentForSameData();
    void setSettingsMapsEveryRuleName();
    void setSettingsReadsUnknownRuleAsNobody();
    void setSettingsKeepsNamesAndValuesAligned();
    void beginEditSelectsTheRow();
    void beginEditRejectsOutOfRangeIndex();
    void beginEditOnEmptyListIsIgnored();
    void beginEditSwitchesRows();
    void cancelEditReturnsToIdle();
    void cancelEditWhenIdleIsSilent();
    void setEditingValueMakesCanSaveTrue();
    void setEditingValueRejectsUnknownChoice();
    void setEditingValueAcceptsLooseSpellings();
    void setEditingValueWithoutEditIsIgnored();
    void setEditingValueBackToConfirmedValueClearsCanSave();
    void setEditingValueSameChoiceIsSilent();
    void markSavedAnnouncesTheEdit();
    void markSavedWhenCleanIsSilent();
    void markSavedWithoutEditIsSilent();
    void markSavedKeepsTheEditorOnTheSetting();
    void markSavedIsAnnouncedOncePerEdit();
    void serverAnswerAfterSaveKeepsTheEditorClean();
    void refreshWhileDraftIsDirtyKeepsTheDraft();
    void refreshWhileDraftIsCleanFollowsTheServerValue();
    void refreshDropsTheEditOfAVanishedSetting();
    void refreshMovesTheEditToItsNewIndex();
    void saveCycleForTwoSettings();
};

void PrivacyFacadeTest::startsIdle()
{
    PrivacyFacade facade;

    QVERIFY(facade.settingNames().isEmpty());
    QVERIFY(facade.settingValues().isEmpty());
    QCOMPARE(facade.editingIndex(), -1);
    QVERIFY(facade.editingValue().isEmpty());
    QVERIFY(!facade.canSave());
}

void PrivacyFacadeTest::setSettingsLoadsRows()
{
    PrivacyFacade facade;
    QSignalSpy spy(&facade, &PrivacyFacade::changed);

    facade.setSettings(QStringList() << showStatus() << allowCalls(),
                       QStringList() << allowAllRule() << allowContactsRule());

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.settingNames(), QStringList() << showStatus() << allowCalls());
    QCOMPARE(facade.settingValues(), QStringList() << everyone() << contacts());
    QCOMPARE(facade.editingIndex(), -1);
    QVERIFY(!facade.canSave());
}

void PrivacyFacadeTest::setSettingsReplacesPreviousRows()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowAllRule());

    facade.setSettings(QStringList() << allowCalls(),
                       QStringList() << QStringLiteral("userPrivacySettingRuleRestrictAll"));

    // The list mirrors the latest answer instead of merging with the old one.
    QCOMPARE(facade.settingNames(), QStringList() << allowCalls());
    QCOMPARE(facade.settingValues(), QStringList() << nobody());
}

void PrivacyFacadeTest::setSettingsDropsNamelessAndDuplicateRows()
{
    PrivacyFacade facade;

    // A merged answer: a nameless row and a second row for a setting already
    // listed. The model keeps the first row of a name, so the first value wins.
    facade.setSettings(QStringList() << QString() << showStatus() << showStatus() << allowCalls(),
                       QStringList() << QStringLiteral("userPrivacySettingRuleRestrictAll")
                                     << allowAllRule()
                                     << QStringLiteral("userPrivacySettingRuleRestrictAll")
                                     << allowContactsRule());

    QCOMPARE(facade.settingNames(), QStringList() << showStatus() << allowCalls());
    QCOMPARE(facade.settingValues(), QStringList() << everyone() << contacts());
}

void PrivacyFacadeTest::setSettingsIsSilentForSameData()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowAllRule());

    QSignalSpy spy(&facade, &PrivacyFacade::changed);
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowAllRule());

    QCOMPARE(spy.count(), 0);
}

void PrivacyFacadeTest::setSettingsMapsEveryRuleName()
{
    PrivacyFacade facade;

    facade.setSettings(QStringList() << QStringLiteral("a") << QStringLiteral("b")
                                     << QStringLiteral("c") << QStringLiteral("d"),
                       QStringList() << allowAllRule()
                                     << allowContactsRule()
                                     << QStringLiteral("userPrivacySettingRuleAllowCloseFriends")
                                     << QStringLiteral("userPrivacySettingRuleRestrictAll"));

    QCOMPARE(facade.settingValues(),
             QStringList() << everyone() << contacts() << closeFriends() << nobody());
}

void PrivacyFacadeTest::setSettingsReadsUnknownRuleAsNobody()
{
    PrivacyFacade facade;

    // Premium and id-carrying rules have no audience the editor can express, so
    // they read as the restrictive fallback instead of pretending a permission.
    facade.setSettings(QStringList() << QStringLiteral("a") << QStringLiteral("b")
                                     << QStringLiteral("c"),
                       QStringList() << QStringLiteral("userPrivacySettingRuleAllowPremiumUsers")
                                     << QStringLiteral("userPrivacySettingRuleAllowUsers"));

    QCOMPARE(facade.settingValues(), QStringList() << nobody() << nobody() << nobody());

    // A token is accepted as-is, so a caller can pass either spelling.
    facade.setSettings(QStringList() << QStringLiteral("a"), QStringList() << closeFriends());
    QCOMPARE(facade.settingValues(), QStringList() << closeFriends());
}

void PrivacyFacadeTest::setSettingsKeepsNamesAndValuesAligned()
{
    PrivacyFacade facade;

    facade.setSettings(QStringList() << showStatus() << showStatus(),
                       QStringList() << allowContactsRule() << QStringLiteral("nobody"));

    QCOMPARE(facade.settingNames().size(), facade.settingValues().size());
    QCOMPARE(facade.settingNames(), QStringList() << showStatus());
    // The first row for a setting wins, so its value is the one kept.
    QCOMPARE(facade.settingValues(), QStringList() << contacts());
}

void PrivacyFacadeTest::beginEditSelectsTheRow()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus() << allowCalls(),
                       QStringList() << allowAllRule() << allowContactsRule());

    QSignalSpy spy(&facade, &PrivacyFacade::changed);
    facade.beginEdit(1);

    // A begin writes up to three editor fields, so changed() may fire more than
    // once; what matters is that the panel is told to re-read.
    QVERIFY(spy.count() >= 1);
    QCOMPARE(facade.editingIndex(), 1);
    QCOMPARE(facade.editingValue(), contacts());
    // The draft starts on the confirmed value, so there is nothing to save.
    QVERIFY(!facade.canSave());
}

void PrivacyFacadeTest::beginEditRejectsOutOfRangeIndex()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowAllRule());

    QSignalSpy spy(&facade, &PrivacyFacade::changed);
    facade.beginEdit(-1);
    facade.beginEdit(1);
    facade.beginEdit(99);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.editingIndex(), -1);
    QVERIFY(facade.editingValue().isEmpty());
    QVERIFY(!facade.canSave());
}

void PrivacyFacadeTest::beginEditOnEmptyListIsIgnored()
{
    PrivacyFacade facade;
    QSignalSpy spy(&facade, &PrivacyFacade::changed);

    facade.beginEdit(0);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.editingIndex(), -1);
    QVERIFY(!facade.canSave());
}

void PrivacyFacadeTest::beginEditSwitchesRows()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus() << allowCalls(),
                       QStringList() << allowAllRule() << allowContactsRule());

    facade.beginEdit(0);
    facade.setEditingValue(nobody());
    QVERIFY(facade.canSave());

    // Only one setting is edited at a time: opening another row abandons the
    // draft of the first one.
    facade.beginEdit(1);

    QCOMPARE(facade.editingIndex(), 1);
    QCOMPARE(facade.editingValue(), contacts());
    QVERIFY(!facade.canSave());
}

void PrivacyFacadeTest::cancelEditReturnsToIdle()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowAllRule());
    facade.beginEdit(0);
    facade.setEditingValue(nobody());

    QSignalSpy spy(&facade, &PrivacyFacade::changed);
    facade.cancelEdit();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.editingIndex(), -1);
    QVERIFY(facade.editingValue().isEmpty());
    QVERIFY(!facade.canSave());
}

void PrivacyFacadeTest::cancelEditWhenIdleIsSilent()
{
    PrivacyFacade facade;
    QSignalSpy spy(&facade, &PrivacyFacade::changed);

    facade.cancelEdit();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.editingIndex(), -1);
}

void PrivacyFacadeTest::setEditingValueMakesCanSaveTrue()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowAllRule());
    facade.beginEdit(0);

    QSignalSpy spy(&facade, &PrivacyFacade::changed);
    facade.setEditingValue(nobody());

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.editingValue(), nobody());
    QVERIFY(facade.canSave());
    // The list still shows the confirmed value; the draft is not a server state.
    QCOMPARE(facade.settingValues(), QStringList() << everyone());
}

void PrivacyFacadeTest::setEditingValueRejectsUnknownChoice()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowAllRule());
    facade.beginEdit(0);

    QSignalSpy spy(&facade, &PrivacyFacade::changed);
    facade.setEditingValue(QStringLiteral("bogus"));
    facade.setEditingValue(QString());
    facade.setEditingValue(QStringLiteral("premium"));
    facade.setEditingValue(QStringLiteral("close friend"));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.editingValue(), everyone());
    QVERIFY(!facade.canSave());
}

void PrivacyFacadeTest::setEditingValueAcceptsLooseSpellings()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowContactsRule());
    facade.beginEdit(0);

    facade.setEditingValue(QStringLiteral("Close Friends"));
    QCOMPARE(facade.editingValue(), closeFriends());

    facade.setEditingValue(QStringLiteral("noone"));
    QCOMPARE(facade.editingValue(), nobody());

    facade.setEditingValue(QStringLiteral("ALL"));
    QCOMPARE(facade.editingValue(), everyone());

    facade.setEditingValue(QStringLiteral(" nobody "));
    QCOMPARE(facade.editingValue(), nobody());

    facade.setEditingValue(allowContactsRule());
    QCOMPARE(facade.editingValue(), contacts());
}

void PrivacyFacadeTest::setEditingValueWithoutEditIsIgnored()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowAllRule());

    QSignalSpy spy(&facade, &PrivacyFacade::changed);
    facade.setEditingValue(nobody());

    QCOMPARE(spy.count(), 0);
    QVERIFY(facade.editingValue().isEmpty());
    QVERIFY(!facade.canSave());
}

void PrivacyFacadeTest::setEditingValueBackToConfirmedValueClearsCanSave()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowContactsRule());
    facade.beginEdit(0);
    facade.setEditingValue(nobody());
    QVERIFY(facade.canSave());

    QSignalSpy spy(&facade, &PrivacyFacade::changed);
    facade.setEditingValue(contacts());

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.editingValue(), contacts());
    QVERIFY(!facade.canSave());
}

void PrivacyFacadeTest::setEditingValueSameChoiceIsSilent()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowContactsRule());
    facade.beginEdit(0);

    QSignalSpy spy(&facade, &PrivacyFacade::changed);
    facade.setEditingValue(contacts());

    QCOMPARE(spy.count(), 0);
    QVERIFY(!facade.canSave());
}

void PrivacyFacadeTest::markSavedAnnouncesTheEdit()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowAllRule());
    facade.beginEdit(0);
    facade.setEditingValue(nobody());

    QSignalSpy saveSpy(&facade, &PrivacyFacade::saveRequested);
    QSignalSpy spy(&facade, &PrivacyFacade::changed);
    facade.markSaved();

    QCOMPARE(saveSpy.count(), 1);
    const QList<QVariant> arguments = saveSpy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), showStatus());
    QCOMPARE(arguments.at(1).toString(), nobody());

    QCOMPARE(spy.count(), 1);
    QVERIFY(!facade.canSave());
}

void PrivacyFacadeTest::markSavedWhenCleanIsSilent()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowAllRule());
    facade.beginEdit(0);

    QSignalSpy saveSpy(&facade, &PrivacyFacade::saveRequested);
    QSignalSpy spy(&facade, &PrivacyFacade::changed);
    facade.markSaved();

    QCOMPARE(saveSpy.count(), 0);
    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.editingIndex(), 0);
    QCOMPARE(facade.editingValue(), everyone());
}

void PrivacyFacadeTest::markSavedWithoutEditIsSilent()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowAllRule());

    QSignalSpy saveSpy(&facade, &PrivacyFacade::saveRequested);
    facade.markSaved();

    QCOMPARE(saveSpy.count(), 0);
    QCOMPARE(facade.editingIndex(), -1);
}

void PrivacyFacadeTest::markSavedKeepsTheEditorOnTheSetting()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowAllRule());
    facade.beginEdit(0);
    facade.setEditingValue(nobody());

    facade.markSaved();

    // The editor stays pointed at the same row, clean, ready for the next edit.
    QCOMPARE(facade.editingIndex(), 0);
    QCOMPARE(facade.editingValue(), nobody());
    QVERIFY(!facade.canSave());
}

void PrivacyFacadeTest::markSavedIsAnnouncedOncePerEdit()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowAllRule());
    facade.beginEdit(0);
    facade.setEditingValue(nobody());

    QSignalSpy saveSpy(&facade, &PrivacyFacade::saveRequested);
    int handled = 0;
    connect(&facade, &PrivacyFacade::saveRequested, &facade, [&facade, &handled]() {
        ++handled;
        // A handler that reports the save back synchronously must not make the
        // facade announce the same edit a second time.
        facade.markSaved();
    });

    facade.markSaved();

    QCOMPARE(handled, 1);
    QCOMPARE(saveSpy.count(), 1);
    QVERIFY(!facade.canSave());
}

void PrivacyFacadeTest::serverAnswerAfterSaveKeepsTheEditorClean()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowAllRule());
    facade.beginEdit(0);
    facade.setEditingValue(nobody());
    facade.markSaved();

    QSignalSpy spy(&facade, &PrivacyFacade::changed);
    facade.setSettings(QStringList() << showStatus(),
                       QStringList() << QStringLiteral("userPrivacySettingRuleRestrictAll"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.settingValues(), QStringList() << nobody());
    QCOMPARE(facade.editingIndex(), 0);
    QCOMPARE(facade.editingValue(), nobody());
    QVERIFY(!facade.canSave());
}

void PrivacyFacadeTest::refreshWhileDraftIsDirtyKeepsTheDraft()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowAllRule());
    facade.beginEdit(0);
    facade.setEditingValue(contacts());

    // The server reports a different value while the user is picking: the draft
    // must survive, and it still differs from the new confirmed value.
    facade.setSettings(QStringList() << showStatus(),
                       QStringList() << QStringLiteral("userPrivacySettingRuleRestrictAll"));

    QCOMPARE(facade.settingValues(), QStringList() << nobody());
    QCOMPARE(facade.editingValue(), contacts());
    QVERIFY(facade.canSave());
}

void PrivacyFacadeTest::refreshWhileDraftIsCleanFollowsTheServerValue()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowAllRule());
    facade.beginEdit(0);

    // Nothing was picked yet, so the untouched draft follows the new value
    // instead of looking like an edit the user made.
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowContactsRule());

    QCOMPARE(facade.editingIndex(), 0);
    QCOMPARE(facade.editingValue(), contacts());
    QVERIFY(!facade.canSave());
}

void PrivacyFacadeTest::refreshDropsTheEditOfAVanishedSetting()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus(), QStringList() << allowAllRule());
    facade.beginEdit(0);
    facade.setEditingValue(nobody());

    // The new answer no longer lists the edited setting: saving it would target
    // a row the user cannot see any more.
    facade.setSettings(QStringList() << allowCalls(), QStringList() << allowContactsRule());

    QCOMPARE(facade.editingIndex(), -1);
    QVERIFY(facade.editingValue().isEmpty());
    QVERIFY(!facade.canSave());
}

void PrivacyFacadeTest::refreshMovesTheEditToItsNewIndex()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus() << allowCalls(),
                       QStringList() << allowAllRule() << allowContactsRule());
    facade.beginEdit(1);

    facade.setSettings(QStringList() << allowCalls() << showStatus(),
                       QStringList() << allowContactsRule() << allowAllRule());

    QCOMPARE(facade.editingIndex(), 0);
    QCOMPARE(facade.editingValue(), contacts());
    QVERIFY(!facade.canSave());
}

void PrivacyFacadeTest::saveCycleForTwoSettings()
{
    PrivacyFacade facade;
    facade.setSettings(QStringList() << showStatus() << allowCalls(),
                       QStringList() << allowAllRule() << allowAllRule());

    QSignalSpy saveSpy(&facade, &PrivacyFacade::saveRequested);

    facade.beginEdit(0);
    facade.setEditingValue(nobody());
    facade.markSaved();

    facade.beginEdit(1);
    facade.setEditingValue(contacts());
    QVERIFY(facade.canSave());
    facade.markSaved();

    QCOMPARE(saveSpy.count(), 2);

    const QList<QVariant> first = saveSpy.at(0);
    QCOMPARE(first.at(0).toString(), showStatus());
    QCOMPARE(first.at(1).toString(), nobody());

    const QList<QVariant> second = saveSpy.at(1);
    QCOMPARE(second.at(0).toString(), allowCalls());
    QCOMPARE(second.at(1).toString(), contacts());

    // Both edits were announced, and the facade is idle-clean afterwards.
    QCOMPARE(facade.editingIndex(), 1);
    QVERIFY(!facade.canSave());
}

QTEST_GUILESS_MAIN(PrivacyFacadeTest)
#include "tst_privacy_facade.moc"
