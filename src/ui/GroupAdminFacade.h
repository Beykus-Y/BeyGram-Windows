#pragma once

#include "telegram/GroupAdministration.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace beygram::ui {

/// The group-administration panel as one QML-facing object.
///
/// The facade holds the administrator rows the panel shows and the permission
/// draft for the selected member. It never talks to TDLib, never logs and never
/// reads the clock: the caller turns getChatAdministrators into the three
/// parallel lists setMembers() takes, and forwards rightsSaveRequested() to a
/// setChatMemberStatus request carrying the named rights.
///
/// Rows
/// ----
/// memberIds(), memberNames() and memberStatuses() are parallel and keep the
/// order the caller supplied. An id is trimmed, must be a positive Telegram user
/// id and is reported in canonical decimal form; rows with an empty, non-numeric
/// or non-positive id are dropped, and only the first row of a repeated id is
/// kept, so two lines can never target the same user. Names are trimmed, and a
/// name that is only whitespace reads as empty. A short names or statuses list
/// leaves the tail empty: an empty name is reported as empty (the panel falls
/// back to the id) and an empty status reads as "administrator".
///
/// memberStatuses() is "creator" for the owner and "administrator" for everyone
/// else. "creator", "owner" and the schema spellings chatMemberStatusCreator /
/// chatMemberStatusOwner are recognized in any case and with separators;
/// every other value, including an empty one, is an administrator because this
/// list is the administrator list.
///
/// Draft
/// -----
/// One member is edited at a time. setMembers() and a selection change reset the
/// draft to all-denied: the payload carries no rights, so carrying a previous
/// member's grants over would invent permissions. Selecting the row that is
/// already selected is a no-op and keeps the draft.
///
/// The five rights are the ones this editor models, in canonical order:
/// canChangeInfo, canDeleteMessages, canBanUsers, canInviteUsers,
/// canPinMessages. setRight() also accepts the TDLib spellings
/// (can_change_info, can_delete_messages, can_restrict_members / can_ban_users,
/// can_invite_users, can_pin_messages), ignoring case, underscores, dashes and
/// spaces. Unknown names and the AdminRights fields this panel does not model
/// (topics, promotion, video chats, anonymity) are ignored, so a typo never
/// grants anything.
///
/// The owner is listed but not editable: selectedEditable() is false, the five
/// rights read as denied, and setRight()/save() do nothing. Telegram changes an
/// owner's rights through ownership, not through an administrator status.
///
/// Saving
/// ------
/// save() announces the selected member's full enabled set through
/// rightsSaveRequested(), in canonical order, including an empty list when all
/// five rights are off. It keeps no saved baseline because setMembers() never
/// receives one: the caller decides whether a request is needed, and a second
/// save re-sends the same state. The draft is left untouched, so the panel keeps
/// showing what was sent.
///
/// changed() is emitted once per call that actually changed an exposed value;
/// repeated identical calls stay silent.
class GroupAdminFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList memberNames READ memberNames NOTIFY changed)
    Q_PROPERTY(QStringList memberIds READ memberIds NOTIFY changed)
    Q_PROPERTY(QStringList memberStatuses READ memberStatuses NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY changed)
    Q_PROPERTY(bool selectedEditable READ selectedEditable NOTIFY changed)
    Q_PROPERTY(bool canChangeInfo READ canChangeInfo NOTIFY changed)
    Q_PROPERTY(bool canDeleteMessages READ canDeleteMessages NOTIFY changed)
    Q_PROPERTY(bool canBanUsers READ canBanUsers NOTIFY changed)
    Q_PROPERTY(bool canInviteUsers READ canInviteUsers NOTIFY changed)
    Q_PROPERTY(bool canPinMessages READ canPinMessages NOTIFY changed)

public:
    explicit GroupAdminFacade(QObject* parent = nullptr);

    Q_DISABLE_COPY_MOVE(GroupAdminFacade)

    /// Names of the loaded administrators, parallel to memberIds().
    [[nodiscard]] QStringList memberNames() const;

    /// Telegram user ids of the loaded administrators, as canonical decimal
    /// strings, in the order the caller supplied them.
    [[nodiscard]] QStringList memberIds() const;

    /// "creator" or "administrator" for each entry of memberIds().
    [[nodiscard]] QStringList memberStatuses() const;

    /// Index of the member being edited, or -1 when no member is selected.
    [[nodiscard]] int selectedIndex() const;

    /// True when a member is selected and is not the owner. The panel uses this
    /// to disable the switches and Save; setRight() and save() enforce it too.
    [[nodiscard]] bool selectedEditable() const;

    /// The draft rights of the selected member, all false without a selection.
    [[nodiscard]] bool canChangeInfo() const;
    [[nodiscard]] bool canDeleteMessages() const;
    [[nodiscard]] bool canBanUsers() const;
    [[nodiscard]] bool canInviteUsers() const;
    [[nodiscard]] bool canPinMessages() const;

    /// Replaces the whole member list from three parallel lists: user ids, names
    /// and statuses. The lists are read positionally and may be shorter than
    /// ids; a missing name or status reads as documented above.
    ///
    /// Invalid and repeated ids are dropped as documented above. The previous
    /// selection and draft are cleared, because a new list starts a new editing
    /// session and the payload carries no rights to restore.
    Q_INVOKABLE void setMembers(const QStringList& ids, const QStringList& names,
                                const QStringList& statuses);

    /// Selects the row at `index`. -1 clears the selection. An index below -1
    /// or past the end is ignored, so a stale delegate cannot clear a valid
    /// selection. Selecting another row resets the draft; selecting the current
    /// row is a no-op and keeps it.
    Q_INVOKABLE void selectMember(int index);

    /// Stores value in the right named name for the selected member.
    ///
    /// The canonical camelCase names and the TDLib spellings are accepted as
    /// described above. An unknown name, no selection, the owner, or a value the
    /// right already holds changes nothing and emits nothing.
    Q_INVOKABLE void setRight(const QString& name, bool value);

    /// Announces the selected member's enabled rights through
    /// rightsSaveRequested(). A no-op without a selection or for the owner. The
    /// draft is not marked saved, so the panel keeps showing it and a later save
    /// re-sends the same state.
    Q_INVOKABLE void save();

signals:
    /// A member row, the selection, its editability or a draft right changed.
    void changed();

    /// A save to perform. userId is one of memberIds(); enabledRights holds the
    /// canonical names of the rights that are on, in canonical order, and is
    /// empty when all five are off.
    void rightsSaveRequested(const QString& userId, const QStringList& enabledRights);

private:
    struct MemberRow {
        QString id;
        QString name;
        bool creator = false;

        bool operator==(const MemberRow& other) const = default;
    };

    /// The draft of the selected member, or all-denied without a selection.
    [[nodiscard]] telegram::AdminRights selectedRights() const;

    QVector<MemberRow> m_members;
    int m_selectedIndex = -1;
    telegram::AdminRights m_rights;
};

} // namespace beygram::ui
