#pragma once

#include "telegram/PrivacyRuleEditorModel.h"
#include "telegram/PrivacySettingsModel.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace beygram::ui {

/// The privacy settings screen, as one object the QML panel can bind to.
///
/// The facade owns a PrivacySettingsModel (which settings exist and what they
/// currently are) and a PrivacyRuleEditorModel (the one draft being edited), and
/// exposes them as plain properties, so PrivacyPanel.qml needs no C++ of its
/// own. It never talks to TDLib, never logs and never reads the clock: the
/// caller fetches getUserPrivacySettingRules, hands the flattened answer to
/// setSettings(), connects saveRequested() to setPrivacySettingRequest() and
/// calls markSaved() once TDLib answers Ok. Nothing here performs I/O, so the
/// tests stay deterministic.
///
/// Values travel as four tokens - "everyone", "contacts", "closeFriends",
/// "nobody" - which map onto PrivacyRuleValue as:
///
///   - everyone    -> PrivacyRuleValue::Everyone
///   - contacts    -> PrivacyRuleValue::Contacts
///   - closeFriends -> PrivacyRuleValue::CloseFriends
///   - nobody      -> PrivacyRuleValue::Nobody
///
/// setEditingValue() also accepts the loose spellings and the schema @type
/// names PrivacyRequests.h accepts ("all", "anybody", "noone", "close_friends",
/// "userPrivacySettingRuleAllowContacts", ...), so a caller that already holds a
/// rule name can hand it over unchanged.
///
/// "closeFriends" is offered and reported like the other three, but the pinned
/// schema has no scalar allow-close-friends base rule: setPrivacySettingRequest()
/// cannot express it and needs a full rules list (see PrivacyRuleEditorModel.h).
/// The facade reports the choice; whether it can be sent is the caller's call.
///
/// Invariants:
///
///  - settingNames() and settingValues() are parallel and in display order. The
///    name is the TDLib setting name, exactly the string saveRequested() carries;
///    the value is always one of the four tokens.
///  - A stored rule the editor cannot express - an id-carrying rule, a premium
///    rule, an empty value - reads as "nobody". That is the documented safe
///    fallback of PrivacyRuleValue, and the settings model keeps the real rule
///    untouched, so the facade never claims a permission it cannot verify.
///  - editingIndex() is -1 while nothing is being edited, otherwise the index of
///    the edited setting in settingNames(). editingValue() is "" while idle and
///    the picked token while editing.
///  - beginEdit() accepts only an in-range, named, editable row. Anything else
///    opens nothing, so a stale row cannot start an edit that has no target.
///    Calling it for another row abandons the previous draft, because the
///    facade only ever edits one setting.
///  - setEditingValue() ignores anything that is not one of the four audiences
///    (after folding case, underscores and spaces), and is ignored entirely
///    while no edit is open: a typo or a late click never becomes a permission.
///  - canSave() is true only while an editable row is edited, the pick differs
///    from the confirmed value, and the pick differs from what the row shows.
///  - markSaved() is the only place a request leaves the facade. It does
///    nothing unless canSave() holds, emits saveRequested() exactly once with
///    the setting name and the picked token, then marks the editor clean so the
///    panel disables Save. It deliberately leaves the settings model alone: the
///    list keeps showing the confirmed value until setSettings() brings TDLib's
///    answer, so an optimistic save cannot show up as a server state.
///  - setSettings() keeps an open edit attached to its setting across a reload:
///    the draft survives a refresh that reports a different server value, and a
///    refresh of a draft the user has not touched follows the new value instead
///    of making the editor look dirty on its own. An edit whose setting is gone
///    from the answer is dropped.
///  - changed() is emitted whenever either collaborator changed, so one call may
///    emit it more than once (a begin writes up to three editor fields). Bindings
///    must treat it as "re-read the properties", not as "one edit happened".
class PrivacyFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList settingNames READ settingNames NOTIFY changed)
    Q_PROPERTY(QStringList settingValues READ settingValues NOTIFY changed)
    Q_PROPERTY(int editingIndex READ editingIndex NOTIFY changed)
    Q_PROPERTY(QString editingValue READ editingValue NOTIFY changed)
    Q_PROPERTY(bool canSave READ canSave NOTIFY changed)

public:
    explicit PrivacyFacade(QObject* parent = nullptr);

    Q_DISABLE_COPY_MOVE(PrivacyFacade)

    /// The TDLib setting names, in display order, as saveRequested() reports
    /// them.
    [[nodiscard]] QStringList settingNames() const;

    /// The current audience of every setting, one of the four tokens, at the
    /// same indexes as settingNames().
    [[nodiscard]] QStringList settingValues() const;

    /// Replaces the whole list from two parallel lists: setting names and the
    /// stored values (rule names or tokens). The settings model keeps the first
    /// row of a name and drops nameless rows, so the two lists stay aligned.
    /// A short `values` list leaves the tail empty, which reads as "nobody".
    Q_INVOKABLE void setSettings(const QStringList& names, const QStringList& values);

    /// Opens the inline editor for the row at `index`, with the row's current
    /// value as the draft. Out-of-range, nameless and read-only rows are ignored.
    Q_INVOKABLE void beginEdit(int index);

    /// Abandons the open edit. A no-op when nothing is being edited.
    Q_INVOKABLE void cancelEdit();

    [[nodiscard]] int editingIndex() const;

    [[nodiscard]] QString editingValue() const;

    /// Picks the audience the next save would send. Ignored while no edit is
    /// open and for any value that is not one of the four tokens.
    Q_INVOKABLE void setEditingValue(const QString& value);

    [[nodiscard]] bool canSave() const;

    /// Announces the open edit and marks it clean. See the class comment: the
    /// settings model is not touched, so the panel keeps the confirmed value
    /// until the caller loads TDLib's answer.
    Q_INVOKABLE void markSaved();

signals:
    /// A setting name, a current value, the open edit, the draft or canSave()
    /// changed.
    void changed();

    /// A change to send, emitted only by markSaved() and only while canSave()
    /// held. `setting` is a name from settingNames(), `value` is one of the four
    /// tokens.
    void saveRequested(const QString& setting, const QString& value);

private:
    telegram::PrivacySettingsModel m_settings;
    telegram::PrivacyRuleEditorModel m_editor;

    /// Guards against a saveRequested() handler that calls markSaved() again:
    /// the same edit must not be announced twice.
    bool m_saving = false;
};

} // namespace beygram::ui
