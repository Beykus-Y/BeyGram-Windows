#pragma once

#include <QObject>
#include <QString>

namespace beygram::telegram {

/// The audience choices the privacy rule editor offers for one setting.
///
/// There is no rule enum to reuse: the pinned TDLib schema (1.8.69, commit
/// 4c6377aa, see docs/WINDOWS_TDLIB.md) names rules as strings, and this client
/// keeps them as strings too — PrivacyRuleSet::baseRule in PrivacyRules.h holds
/// the schema @type, and PrivacyRequests.h accepts the same names through
/// setPrivacySettingRequest(). This enum is therefore the editor's own value
/// type; the translation to a rule name stays with the caller, next to the
/// request it builds. The scalar rules that request builder accepts map as:
///
///   - Everyone     -> userPrivacySettingRuleAllowAll
///   - Contacts     -> userPrivacySettingRuleAllowContacts
///   - Nobody       -> userPrivacySettingRuleRestrictAll
///   - CloseFriends has no scalar base rule in this schema revision: the 11
///     UserPrivacySettingRule variants listed in PrivacyRequests.h contain no
///     allow-close-friends base rule, so setPrivacySettingRequest() cannot
///     express it and it needs a full rules list. The model stores the choice
///     without pretending it is sendable.
///
/// The enum deliberately has no premium or bot variant, matching
/// isAllowedByRules(): a rule this client cannot verify must never be shown as
/// a permission. Reading an unknown or premium base rule back into this enum is
/// the caller's problem, and "nobody" is the only safe fallback.
enum class PrivacyRuleValue {
    Everyone,
    Contacts,
    CloseFriends,
    Nobody,
};

/// Draft state of one privacy setting in the editor: the chosen audience, the
/// value the server last confirmed, and the extra "allow close friends" flag.
///
/// The model is a plain value holder. It never talks to TDLib, never reads the
/// clock and never logs; the caller fetches getUserPrivacySettingRules, feeds the
/// confirmed value in through setOriginal(), sends the edit itself and calls
/// markSaved() when TDLib answers Ok. Because nothing here performs I/O, the
/// tests stay deterministic.
///
/// Invariants:
///
///  - A setting name is required for an edit to count. dirty() is false while
///    setting() is empty, so an editor that has not been told which setting it
///    is editing can never offer a save, even if a value was picked.
///  - dirty() means exactly "there is a setting and the chosen value differs
///    from the confirmed one". The close-friends flag does not feed into it; it
///    belongs to its own setting (userPrivacySettingAllowCloseFriends) and is
///    saved on its own.
///  - markSaved() promotes the chosen value to the confirmed one and keeps the
///    setting name, so the same editor can be used again for the next edit. When
///    there was nothing to promote it is silent.
///  - reset() drops the setting name and puts the confirmed value back, which
///    also makes dirty() false. The close-friends flag is left alone: it tracks a
///    separate setting, and silently flipping it would misreport the server
///    state. If both parts were already pristine, reset() is silent.
///  - changed() is emitted only when a getter would really return something
///    different. Writing the value already stored, re-setting the same setting
///    name, re-confirming the same original and re-setting the same flag are all
///    silent, so a rebind or a refresh that brings the same data does not
///    repaint the screen or re-trigger a binding loop.
class PrivacyRuleEditorModel final : public QObject {
    Q_OBJECT

public:
    explicit PrivacyRuleEditorModel(QObject* parent = nullptr);

    /// Names the setting being edited, e.g. "userPrivacySettingShowStatus". An
    /// empty name is allowed and simply means "nothing is being edited": the
    /// model then reports dirty() == false whatever the value.
    void setSetting(const QString& setting);
    [[nodiscard]] QString setting() const;

    /// The audience the user has picked, which is what a save would send.
    void setValue(PrivacyRuleValue value);
    [[nodiscard]] PrivacyRuleValue value() const;

    /// The audience the server last confirmed. Setting it does not touch the
    /// picked value, so an unsaved edit survives a refresh of the confirmed
    /// state, and a caller that learns of a server-side change can still make
    /// the editor dirty.
    void setOriginal(PrivacyRuleValue value);
    [[nodiscard]] PrivacyRuleValue original() const;

    /// True only when setting() is non-empty and value() differs from
    /// original().
    [[nodiscard]] bool dirty() const;

    /// The userPrivacySettingAllowCloseFriends toggle, a separate setting from
    /// the audience above.
    void setAllowCloseFriends(bool allow);
    [[nodiscard]] bool allowCloseFriends() const;

    /// Called after TDLib accepted the edit: value() becomes original(), so the
    /// editor turns clean and stays usable. Silent when value() already equals
    /// original(), because then nothing changed.
    void markSaved();

    /// Abandons the edit: value() goes back to original() and the setting name is
    /// cleared. Silent when there was nothing to abandon.
    void reset();

signals:
    void changed();

private:
    QString m_setting;
    PrivacyRuleValue m_value = PrivacyRuleValue::Everyone;
    PrivacyRuleValue m_original = PrivacyRuleValue::Everyone;
    bool m_allowCloseFriends = false;
};

} // namespace beygram::telegram
