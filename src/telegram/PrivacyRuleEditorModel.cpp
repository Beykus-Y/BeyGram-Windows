#include "PrivacyRuleEditorModel.h"

namespace beygram::telegram {

PrivacyRuleEditorModel::PrivacyRuleEditorModel(QObject* parent)
    : QObject(parent)
{
}

void PrivacyRuleEditorModel::setSetting(const QString& setting)
{
    // Re-naming the setting to the name it already has is not an edit; emitting
    // here would make a screen rebuild look like a user change.
    if (m_setting == setting)
        return;

    m_setting = setting;
    emit changed();
}

QString PrivacyRuleEditorModel::setting() const
{
    return m_setting;
}

void PrivacyRuleEditorModel::setValue(PrivacyRuleValue value)
{
    if (m_value == value)
        return;

    m_value = value;
    emit changed();
}

PrivacyRuleValue PrivacyRuleEditorModel::value() const
{
    return m_value;
}

void PrivacyRuleEditorModel::setOriginal(PrivacyRuleValue value)
{
    // Only the confirmed side moves. A refresh of the server state must not
    // overwrite what the user is in the middle of picking.
    if (m_original == value)
        return;

    m_original = value;
    emit changed();
}

PrivacyRuleValue PrivacyRuleEditorModel::original() const
{
    return m_original;
}

bool PrivacyRuleEditorModel::dirty() const
{
    // No setting name means there is nothing a save could target, so an editor
    // that was never told what it edits is never dirty.
    if (m_setting.isEmpty())
        return false;

    return m_value != m_original;
}

void PrivacyRuleEditorModel::setAllowCloseFriends(bool allow)
{
    if (m_allowCloseFriends == allow)
        return;

    m_allowCloseFriends = allow;
    emit changed();
}

bool PrivacyRuleEditorModel::allowCloseFriends() const
{
    return m_allowCloseFriends;
}

void PrivacyRuleEditorModel::markSaved()
{
    // The value TDLib just accepted is the new confirmed value. When it already
    // was, the save confirmed what was on screen and nothing visible changed.
    if (m_value == m_original)
        return;

    m_original = m_value;
    emit changed();
}

void PrivacyRuleEditorModel::reset()
{
    // Both halves are restored in one step, so no observer sees the old setting
    // paired with the confirmed value.
    if (m_setting.isEmpty() && m_value == m_original)
        return;

    m_setting.clear();
    m_value = m_original;
    emit changed();
}

} // namespace beygram::telegram
