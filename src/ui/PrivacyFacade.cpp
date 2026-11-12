#include "ui/PrivacyFacade.h"

#include <QChar>
#include <QLatin1Char>
#include <QString>
#include <QStringList>
#include <QVector>

#include <utility>

namespace beygram::ui {
namespace {

using telegram::PrivacyRuleValue;
using telegram::PrivacySettingRow;

// The four choices, spelled the way the panel and saveRequested() report them.
QString tokenFor(PrivacyRuleValue value)
{
    switch (value) {
    case PrivacyRuleValue::Everyone:
        return QStringLiteral("everyone");
    case PrivacyRuleValue::Contacts:
        return QStringLiteral("contacts");
    case PrivacyRuleValue::CloseFriends:
        return QStringLiteral("closeFriends");
    case PrivacyRuleValue::Nobody:
        break;
    }
    return QStringLiteral("nobody");
}

// Folds case, underscores and spaces so "close_friends", "Close Friends" and
// "closeFriends" are one spelling, the same tolerance PrivacyRequests.h applies
// to a rule name.
QString folded(const QString& text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar character : text) {
        if (character == QLatin1Char('_') || character.isSpace())
            continue;
        out.append(character.toLower());
    }
    return out;
}

// Reads one audience choice. Returns false for anything the editor cannot
// express, so a caller can tell "nobody" from a typo.
bool parseRuleValue(const QString& text, PrivacyRuleValue* value)
{
    const QString key = folded(text);
    if (key.isEmpty())
        return false;

    if (key == QLatin1String("everyone") || key == QLatin1String("all")
        || key == QLatin1String("anybody")
        || key == QLatin1String("userprivacysettingruleallowall")) {
        *value = PrivacyRuleValue::Everyone;
        return true;
    }
    if (key == QLatin1String("contacts")
        || key == QLatin1String("userprivacysettingruleallowcontacts")) {
        *value = PrivacyRuleValue::Contacts;
        return true;
    }
    if (key == QLatin1String("closefriends")
        || key == QLatin1String("userprivacysettingruleallowclosefriends")) {
        *value = PrivacyRuleValue::CloseFriends;
        return true;
    }
    if (key == QLatin1String("nobody") || key == QLatin1String("noone")
        || key == QLatin1String("userprivacysettingrulerestrictall")) {
        *value = PrivacyRuleValue::Nobody;
        return true;
    }
    return false;
}

// Reads a stored setting value back into the editor's enum. An id-carrying
// rule, a premium rule or an empty value has no audience this client can show,
// so it reads as Nobody: the restrictive reading is the only safe fallback, and
// the settings model keeps the real rule untouched.
PrivacyRuleValue storedValue(const QString& stored)
{
    PrivacyRuleValue value = PrivacyRuleValue::Nobody;
    if (!parseRuleValue(stored, &value))
        return PrivacyRuleValue::Nobody;
    return value;
}

int indexOfSetting(const QVector<PrivacySettingRow>& rows, const QString& setting)
{
    if (setting.isEmpty())
        return -1;

    for (int index = 0; index < rows.size(); ++index) {
        if (rows.at(index).setting == setting)
            return index;
    }
    return -1;
}

} // namespace

PrivacyFacade::PrivacyFacade(QObject* parent)
    : QObject(parent)
    , m_settings(this)
    , m_editor(this)
{
    // The facade holds no state of its own: every property is derived from one
    // of the two collaborators, so their changes are the facade's changes.
    connect(&m_settings, &telegram::PrivacySettingsModel::changed,
            this, &PrivacyFacade::changed);
    connect(&m_editor, &telegram::PrivacyRuleEditorModel::changed,
            this, &PrivacyFacade::changed);
}

QStringList PrivacyFacade::settingNames() const
{
    const QVector<PrivacySettingRow> rows = m_settings.rows();

    QStringList names;
    names.reserve(rows.size());
    for (const PrivacySettingRow& row : rows)
        names.append(row.setting);
    return names;
}

QStringList PrivacyFacade::settingValues() const
{
    const QVector<PrivacySettingRow> rows = m_settings.rows();

    QStringList values;
    values.reserve(rows.size());
    for (const PrivacySettingRow& row : rows)
        values.append(tokenFor(storedValue(row.value)));
    return values;
}

void PrivacyFacade::setSettings(const QStringList& names, const QStringList& values)
{
    QVector<PrivacySettingRow> rows;
    rows.reserve(names.size());

    for (int index = 0; index < names.size(); ++index) {
        PrivacySettingRow row;
        row.setting = names.at(index).trimmed();
        // A short values list leaves the tail empty; the editor reads that as
        // "nobody" rather than inventing a permission.
        row.value = values.value(index);
        row.editable = true;
        rows.append(row);
    }

    // The model drops nameless rows and keeps the first row of a duplicate, so
    // a merged answer cannot leave two lines fighting over one value.
    m_settings.setRows(std::move(rows));

    // Keep an open edit attached to its setting across a reload. A draft the
    // user has not touched follows the new value, so a refresh cannot make the
    // editor look dirty on its own; a draft the user did pick survives, so a
    // refresh cannot throw the edit away.
    if (!m_editor.setting().isEmpty()) {
        const QVector<PrivacySettingRow> loaded = m_settings.rows();
        const int index = indexOfSetting(loaded, m_editor.setting());
        if (index < 0) {
            m_editor.reset();
        } else {
            const bool wasClean = !m_editor.dirty();
            m_editor.setOriginal(storedValue(loaded.at(index).value));
            if (wasClean)
                m_editor.setValue(m_editor.original());
        }
    }
}

void PrivacyFacade::beginEdit(int index)
{
    const QVector<PrivacySettingRow> rows = m_settings.rows();
    if (index < 0 || index >= rows.size())
        return;

    const PrivacySettingRow& row = rows.at(index);
    // A nameless or read-only row has nothing a save could target. setSettings()
    // loads every row as editable; the guard keeps a future read-only source
    // from opening an editor whose Save the server would refuse.
    if (row.setting.isEmpty() || !row.editable)
        return;

    const PrivacyRuleValue value = storedValue(row.value);

    // The name goes first: while no setting is named the editor is never dirty,
    // so the panel cannot flash a Save button between the writes.
    m_editor.setSetting(row.setting);
    m_editor.setOriginal(value);
    m_editor.setValue(value);
}

void PrivacyFacade::cancelEdit()
{
    // Silent when nothing is being edited, exactly like the editor model.
    m_editor.reset();
}

int PrivacyFacade::editingIndex() const
{
    if (m_editor.setting().isEmpty())
        return -1;

    return indexOfSetting(m_settings.rows(), m_editor.setting());
}

QString PrivacyFacade::editingValue() const
{
    // A setting that is not on screen any more has no draft to show, even if the
    // editor still remembers a value.
    if (editingIndex() < 0)
        return {};

    return tokenFor(m_editor.value());
}

void PrivacyFacade::setEditingValue(const QString& value)
{
    if (editingIndex() < 0)
        return;

    PrivacyRuleValue parsed = PrivacyRuleValue::Nobody;
    if (!parseRuleValue(value, &parsed))
        return; // Unknown choice: keep the draft, never guess a permission.

    m_editor.setValue(parsed);
}

bool PrivacyFacade::canSave() const
{
    const int index = editingIndex();
    if (index < 0)
        return false;

    const QVector<PrivacySettingRow> rows = m_settings.rows();
    if (index >= rows.size() || !rows.at(index).editable)
        return false;

    // dirty() already means "a setting is named and the pick differs from the
    // confirmed value". Comparing against the row's token as well keeps the
    // button off if a refresh moved the row under the draft.
    return m_editor.dirty()
           && editingValue() != tokenFor(storedValue(rows.at(index).value));
}

void PrivacyFacade::markSaved()
{
    if (!canSave() || m_saving)
        return;

    const QString setting = m_editor.setting();
    const QString value = tokenFor(m_editor.value());

    // The payload is captured before the signal, and the guard stops a handler
    // that calls back into markSaved() from announcing the same save twice.
    m_saving = true;
    emit saveRequested(setting, value);
    m_saving = false;

    // The editor turns clean, so canSave() goes false and the panel disables
    // Save. The settings model is untouched on purpose: the list keeps the
    // confirmed value until setSettings() brings TDLib's answer.
    m_editor.markSaved();
}

} // namespace beygram::ui
