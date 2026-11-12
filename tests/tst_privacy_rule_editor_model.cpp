#include "telegram/PrivacyRuleEditorModel.h"

#include <QSignalSpy>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QString showStatus()
{
    return QStringLiteral("userPrivacySettingShowStatus");
}

QString allowCalls()
{
    return QStringLiteral("userPrivacySettingAllowCalls");
}

} // namespace

class PrivacyRuleEditorModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsPristine();
    void setSettingStoresName();
    void setSettingEmitsChangedOnce();
    void setSettingSameNameIsSilent();
    void setSettingClearedMakesEditorClean();
    void emptySettingIsNeverDirty();
    void setValueStoresChoice();
    void setValueEmitsChangedOnce();
    void setValueSameChoiceIsSilent();
    void setValueWithSettingIsDirty();
    void setValueBackToOriginalIsClean();
    void setOriginalStoresChoice();
    void setOriginalEmitsChangedOnce();
    void setOriginalSameChoiceIsSilent();
    void setOriginalClearsDirty();
    void setOriginalKeepsUnsavedValue();
    void setOriginalWhileSettingEmptyStaysClean();
    void setAllowCloseFriendsStoresFlag();
    void setAllowCloseFriendsEmitsChangedOnce();
    void setAllowCloseFriendsSameFlagIsSilent();
    void setAllowCloseFriendsDoesNotMakeDirty();
    void markSavedCopiesValueToOriginal();
    void markSavedEmitsChangedOnce();
    void markSavedKeepsSetting();
    void markSavedWhenCleanIsSilent();
    void markSavedThenEditIsDirtyAgain();
    void resetRestoresOriginalValue();
    void resetClearsSetting();
    void resetEmitsChangedOnce();
    void resetIsSilentWhenPristine();
    void resetKeepsAllowCloseFriends();
    void resetAfterSaveKeepsSavedValue();
    void editCycleEmitsOnlyOnRealChanges();
    void modelIsUsableAfterReset();
};

void PrivacyRuleEditorModelTest::startsPristine()
{
    PrivacyRuleEditorModel model;

    QVERIFY(model.setting().isEmpty());
    QCOMPARE(model.value(), PrivacyRuleValue::Everyone);
    QCOMPARE(model.original(), PrivacyRuleValue::Everyone);
    QVERIFY(!model.allowCloseFriends());
    QVERIFY(!model.dirty());
}

void PrivacyRuleEditorModelTest::setSettingStoresName()
{
    PrivacyRuleEditorModel model;
    model.setSetting(showStatus());

    QCOMPARE(model.setting(), showStatus());

    model.setSetting(allowCalls());
    QCOMPARE(model.setting(), allowCalls());
}

void PrivacyRuleEditorModelTest::setSettingEmitsChangedOnce()
{
    PrivacyRuleEditorModel model;
    QSignalSpy spy(&model, &PrivacyRuleEditorModel::changed);

    model.setSetting(showStatus());

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.setting(), showStatus());
}

void PrivacyRuleEditorModelTest::setSettingSameNameIsSilent()
{
    PrivacyRuleEditorModel model;
    model.setSetting(showStatus());

    QSignalSpy spy(&model, &PrivacyRuleEditorModel::changed);
    model.setSetting(showStatus());

    QCOMPARE(spy.count(), 0);
}

void PrivacyRuleEditorModelTest::setSettingClearedMakesEditorClean()
{
    PrivacyRuleEditorModel model;
    model.setSetting(showStatus());
    model.setValue(PrivacyRuleValue::Nobody);
    QVERIFY(model.dirty());

    QSignalSpy spy(&model, &PrivacyRuleEditorModel::changed);
    model.setSetting(QString());

    // Dropping the name does not undo the picked value, but a save can no longer
    // target anything, so the editor is clean again.
    QCOMPARE(spy.count(), 1);
    QVERIFY(model.setting().isEmpty());
    QCOMPARE(model.value(), PrivacyRuleValue::Nobody);
    QVERIFY(!model.dirty());
}

void PrivacyRuleEditorModelTest::emptySettingIsNeverDirty()
{
    PrivacyRuleEditorModel model;

    // No setting name was ever provided: the value is a draft, not an edit.
    model.setValue(PrivacyRuleValue::Contacts);
    QVERIFY(!model.dirty());

    model.setValue(PrivacyRuleValue::Nobody);
    QVERIFY(!model.dirty());

    model.setOriginal(PrivacyRuleValue::CloseFriends);
    QVERIFY(!model.dirty());
}

void PrivacyRuleEditorModelTest::setValueStoresChoice()
{
    PrivacyRuleEditorModel model;
    model.setValue(PrivacyRuleValue::Contacts);
    QCOMPARE(model.value(), PrivacyRuleValue::Contacts);

    model.setValue(PrivacyRuleValue::CloseFriends);
    QCOMPARE(model.value(), PrivacyRuleValue::CloseFriends);

    model.setValue(PrivacyRuleValue::Nobody);
    QCOMPARE(model.value(), PrivacyRuleValue::Nobody);

    // Picking a value never moves the confirmed one.
    QCOMPARE(model.original(), PrivacyRuleValue::Everyone);
}

void PrivacyRuleEditorModelTest::setValueEmitsChangedOnce()
{
    PrivacyRuleEditorModel model;
    QSignalSpy spy(&model, &PrivacyRuleEditorModel::changed);

    model.setValue(PrivacyRuleValue::Nobody);

    QCOMPARE(spy.count(), 1);
}

void PrivacyRuleEditorModelTest::setValueSameChoiceIsSilent()
{
    PrivacyRuleEditorModel model;
    model.setValue(PrivacyRuleValue::Contacts);

    QSignalSpy spy(&model, &PrivacyRuleEditorModel::changed);
    model.setValue(PrivacyRuleValue::Contacts);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.value(), PrivacyRuleValue::Contacts);
}

void PrivacyRuleEditorModelTest::setValueWithSettingIsDirty()
{
    PrivacyRuleEditorModel model;
    model.setSetting(showStatus());

    model.setValue(PrivacyRuleValue::Contacts);

    QVERIFY(model.dirty());
    QCOMPARE(model.original(), PrivacyRuleValue::Everyone);
}

void PrivacyRuleEditorModelTest::setValueBackToOriginalIsClean()
{
    PrivacyRuleEditorModel model;
    model.setSetting(showStatus());
    model.setOriginal(PrivacyRuleValue::Contacts);

    model.setValue(PrivacyRuleValue::Nobody);
    QVERIFY(model.dirty());

    // Returning to the confirmed value is a real change (the getter moves) but
    // leaves nothing to save.
    QSignalSpy spy(&model, &PrivacyRuleEditorModel::changed);
    model.setValue(PrivacyRuleValue::Contacts);

    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.dirty());
}

void PrivacyRuleEditorModelTest::setOriginalStoresChoice()
{
    PrivacyRuleEditorModel model;
    model.setOriginal(PrivacyRuleValue::CloseFriends);

    QCOMPARE(model.original(), PrivacyRuleValue::CloseFriends);
    QCOMPARE(model.value(), PrivacyRuleValue::Everyone);
}

void PrivacyRuleEditorModelTest::setOriginalEmitsChangedOnce()
{
    PrivacyRuleEditorModel model;
    QSignalSpy spy(&model, &PrivacyRuleEditorModel::changed);

    model.setOriginal(PrivacyRuleValue::Nobody);

    QCOMPARE(spy.count(), 1);
}

void PrivacyRuleEditorModelTest::setOriginalSameChoiceIsSilent()
{
    PrivacyRuleEditorModel model;
    model.setOriginal(PrivacyRuleValue::Nobody);

    QSignalSpy spy(&model, &PrivacyRuleEditorModel::changed);
    model.setOriginal(PrivacyRuleValue::Nobody);

    QCOMPARE(spy.count(), 0);
}

void PrivacyRuleEditorModelTest::setOriginalClearsDirty()
{
    PrivacyRuleEditorModel model;
    model.setSetting(showStatus());
    model.setValue(PrivacyRuleValue::Nobody);
    QVERIFY(model.dirty());

    // The server turned out to already have the picked value.
    model.setOriginal(PrivacyRuleValue::Nobody);

    QVERIFY(!model.dirty());
    QCOMPARE(model.value(), PrivacyRuleValue::Nobody);
}

void PrivacyRuleEditorModelTest::setOriginalKeepsUnsavedValue()
{
    PrivacyRuleEditorModel model;
    model.setSetting(showStatus());
    model.setValue(PrivacyRuleValue::Contacts);

    // A refresh that reports a different server value must not throw away the
    // edit the user is making.
    model.setOriginal(PrivacyRuleValue::Nobody);

    QCOMPARE(model.value(), PrivacyRuleValue::Contacts);
    QCOMPARE(model.original(), PrivacyRuleValue::Nobody);
    QVERIFY(model.dirty());
}

void PrivacyRuleEditorModelTest::setOriginalWhileSettingEmptyStaysClean()
{
    PrivacyRuleEditorModel model;
    model.setOriginal(PrivacyRuleValue::Nobody);

    QVERIFY(!model.dirty());
}

void PrivacyRuleEditorModelTest::setAllowCloseFriendsStoresFlag()
{
    PrivacyRuleEditorModel model;
    QVERIFY(!model.allowCloseFriends());

    model.setAllowCloseFriends(true);
    QVERIFY(model.allowCloseFriends());

    model.setAllowCloseFriends(false);
    QVERIFY(!model.allowCloseFriends());
}

void PrivacyRuleEditorModelTest::setAllowCloseFriendsEmitsChangedOnce()
{
    PrivacyRuleEditorModel model;
    QSignalSpy spy(&model, &PrivacyRuleEditorModel::changed);

    model.setAllowCloseFriends(true);

    QCOMPARE(spy.count(), 1);
    QVERIFY(model.allowCloseFriends());
}

void PrivacyRuleEditorModelTest::setAllowCloseFriendsSameFlagIsSilent()
{
    PrivacyRuleEditorModel model;
    model.setAllowCloseFriends(true);

    QSignalSpy spy(&model, &PrivacyRuleEditorModel::changed);
    model.setAllowCloseFriends(true);

    QCOMPARE(spy.count(), 0);
}

void PrivacyRuleEditorModelTest::setAllowCloseFriendsDoesNotMakeDirty()
{
    PrivacyRuleEditorModel model;
    model.setSetting(showStatus());

    // The flag belongs to userPrivacySettingAllowCloseFriends, a different
    // setting, so it must not mark this audience as unsaved.
    model.setAllowCloseFriends(true);

    QVERIFY(!model.dirty());
}

void PrivacyRuleEditorModelTest::markSavedCopiesValueToOriginal()
{
    PrivacyRuleEditorModel model;
    model.setSetting(showStatus());
    model.setValue(PrivacyRuleValue::Contacts);

    model.markSaved();

    QCOMPARE(model.original(), PrivacyRuleValue::Contacts);
    QCOMPARE(model.value(), PrivacyRuleValue::Contacts);
    QVERIFY(!model.dirty());
}

void PrivacyRuleEditorModelTest::markSavedEmitsChangedOnce()
{
    PrivacyRuleEditorModel model;
    model.setSetting(showStatus());
    model.setValue(PrivacyRuleValue::Nobody);

    QSignalSpy spy(&model, &PrivacyRuleEditorModel::changed);
    model.markSaved();

    QCOMPARE(spy.count(), 1);
}

void PrivacyRuleEditorModelTest::markSavedKeepsSetting()
{
    PrivacyRuleEditorModel model;
    model.setSetting(showStatus());
    model.setValue(PrivacyRuleValue::Nobody);

    model.markSaved();

    // The editor stays pointed at the same setting, ready for the next edit.
    QCOMPARE(model.setting(), showStatus());
}

void PrivacyRuleEditorModelTest::markSavedWhenCleanIsSilent()
{
    PrivacyRuleEditorModel model;
    model.setSetting(showStatus());
    model.setValue(PrivacyRuleValue::Contacts);
    model.markSaved();

    QSignalSpy spy(&model, &PrivacyRuleEditorModel::changed);
    model.markSaved();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.original(), PrivacyRuleValue::Contacts);
}

void PrivacyRuleEditorModelTest::markSavedThenEditIsDirtyAgain()
{
    PrivacyRuleEditorModel model;
    model.setSetting(showStatus());
    model.setValue(PrivacyRuleValue::Contacts);
    model.markSaved();

    model.setValue(PrivacyRuleValue::Nobody);

    QVERIFY(model.dirty());
    QCOMPARE(model.original(), PrivacyRuleValue::Contacts);
    QCOMPARE(model.value(), PrivacyRuleValue::Nobody);
}

void PrivacyRuleEditorModelTest::resetRestoresOriginalValue()
{
    PrivacyRuleEditorModel model;
    model.setSetting(showStatus());
    model.setOriginal(PrivacyRuleValue::Contacts);
    model.setValue(PrivacyRuleValue::Nobody);

    model.reset();

    QCOMPARE(model.value(), PrivacyRuleValue::Contacts);
    QCOMPARE(model.original(), PrivacyRuleValue::Contacts);
    QVERIFY(!model.dirty());
}

void PrivacyRuleEditorModelTest::resetClearsSetting()
{
    PrivacyRuleEditorModel model;
    model.setSetting(showStatus());
    model.setValue(PrivacyRuleValue::Nobody);

    model.reset();

    QVERIFY(model.setting().isEmpty());
}

void PrivacyRuleEditorModelTest::resetEmitsChangedOnce()
{
    PrivacyRuleEditorModel model;
    model.setSetting(showStatus());
    model.setValue(PrivacyRuleValue::Nobody);

    QSignalSpy spy(&model, &PrivacyRuleEditorModel::changed);
    model.reset();

    QCOMPARE(spy.count(), 1);
}

void PrivacyRuleEditorModelTest::resetIsSilentWhenPristine()
{
    PrivacyRuleEditorModel model;
    QSignalSpy spy(&model, &PrivacyRuleEditorModel::changed);

    model.reset();

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.setting().isEmpty());
    QCOMPARE(model.value(), PrivacyRuleValue::Everyone);
}

void PrivacyRuleEditorModelTest::resetKeepsAllowCloseFriends()
{
    PrivacyRuleEditorModel model;
    model.setSetting(showStatus());
    model.setValue(PrivacyRuleValue::Nobody);
    model.setAllowCloseFriends(true);

    model.reset();

    // The flag tracks a separate setting; reset() must not silently claim the
    // server state flipped it back.
    QVERIFY(model.allowCloseFriends());
}

void PrivacyRuleEditorModelTest::resetAfterSaveKeepsSavedValue()
{
    PrivacyRuleEditorModel model;
    model.setSetting(showStatus());
    model.setValue(PrivacyRuleValue::Contacts);
    model.markSaved();

    model.reset();

    // After a save the confirmed value is the saved one, so abandoning a later
    // (empty) edit keeps it instead of falling back to the first value seen.
    QCOMPARE(model.value(), PrivacyRuleValue::Contacts);
    QCOMPARE(model.original(), PrivacyRuleValue::Contacts);
}

void PrivacyRuleEditorModelTest::editCycleEmitsOnlyOnRealChanges()
{
    PrivacyRuleEditorModel model;
    QSignalSpy spy(&model, &PrivacyRuleEditorModel::changed);

    model.setSetting(showStatus());                 // 1
    model.setSetting(showStatus());                 // no-op
    model.setValue(PrivacyRuleValue::Contacts);     // 2
    model.setValue(PrivacyRuleValue::Contacts);     // no-op
    model.setOriginal(PrivacyRuleValue::Everyone);  // no-op (already Everyone)
    model.markSaved();                              // 3
    model.markSaved();                              // no-op
    model.setAllowCloseFriends(true);               // 4
    model.setAllowCloseFriends(true);               // no-op
    model.setValue(PrivacyRuleValue::Contacts);     // no-op
    model.reset();                                  // 5 (setting cleared, value unchanged)
    model.reset();                                  // no-op

    QCOMPARE(spy.count(), 5);
    QVERIFY(model.setting().isEmpty());
    QCOMPARE(model.value(), PrivacyRuleValue::Contacts);
    QVERIFY(!model.dirty());
}

void PrivacyRuleEditorModelTest::modelIsUsableAfterReset()
{
    PrivacyRuleEditorModel model;
    model.setSetting(showStatus());
    model.setValue(PrivacyRuleValue::Nobody);
    model.reset();

    // A reset editor behaves like a first load for the next setting.
    model.setSetting(allowCalls());
    model.setOriginal(PrivacyRuleValue::Contacts);
    model.setValue(PrivacyRuleValue::Everyone);

    QCOMPARE(model.setting(), allowCalls());
    QVERIFY(model.dirty());

    model.markSaved();
    QVERIFY(!model.dirty());
    QCOMPARE(model.original(), PrivacyRuleValue::Everyone);
}

QTEST_GUILESS_MAIN(PrivacyRuleEditorModelTest)
#include "tst_privacy_rule_editor_model.moc"
