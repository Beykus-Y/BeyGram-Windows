#pragma once

#include "ChatPermissions.h"

#include <QObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// The rights of one member of one chat, as the member-permissions editor shows
/// them: the plain member rights TDLib reports for a chat plus the admin flag
/// that comes from the member's status.
///
/// This is deliberately a flat set of bools rather than a ChatPermissions: a
/// ChatPermissions models the chat-wide defaults parsed from chat.permissions
/// (see ChatPermissions.h) and carries rights that a per-member editor cannot
/// edit (canSendOther, canAddWebPreviews, canChangeInfo) while lacking the one
/// that matters most here, isAdmin. Only the overlapping rights line up one to
/// one, and memberRightsFromChatPermissions() copies exactly those, so the two
/// types stay separate without duplicating the parsing rules.
///
/// The defaults deny everything: a member whose rights were never loaded may
/// send and administer nothing, and is not an admin.
struct MemberRights {
    bool canSendMessages = false;
    bool canSendMedia = false;
    bool canSendPolls = false;
    bool canAddMembers = false;
    bool canPinMessages = false;
    bool canManageTopics = false;
    bool isAdmin = false;
};

/// Seeds the member rights that a chat's default permissions also express, for a
/// member whose rights have not been read from TDLib yet.
///
///   canSendMessages, canSendMedia, canSendPolls, canPinMessages,
///   canManageTopics   <- the same field of ChatPermissions
///   canAddMembers     <- canInviteUsers
///
/// isAdmin stays false: chatPermissions describes what regular members may do,
/// so admin status can only come from the member's own status. The remaining
/// ChatPermissions fields have no counterpart in a per-member editor and are
/// dropped.
[[nodiscard]] MemberRights memberRightsFromChatPermissions(const ChatPermissions& permissions);

/// The draft behind the "edit member rights" dialog: which member of which chat
/// is being edited, the rights the dialog currently shows, and the rights the
/// dialog started from.
///
/// A session starts with begin(), which takes the rights TDLib reported as both
/// the draft and the baseline, so a freshly opened dialog is never dirty. The
/// dialog then flips one right at a time with setRight(), markSaved() accepts the
/// draft once TDLib confirmed the change, and cancel() abandons it when the user
/// backs out. canSave() is the Save button: there is a session and it differs
/// from what was loaded.
///
/// Nothing here talks to TDLib and nothing is logged; the caller turns a canSave()
/// state into a setChatMemberStatus request.
class ChatMemberPermissionsModel final : public QObject {
    Q_OBJECT

public:
    explicit ChatMemberPermissionsModel(QObject* parent = nullptr);

    /// Starts editing userId inside chatId with rights as both the draft and the
    /// baseline. A session already in flight is replaced, and its pending edit is
    /// dropped rather than carried over.
    ///
    /// Telegram ids are never 0 (chat ids may be negative), so a zero chat id
    /// or a non-positive user id cannot name a member: such a call is ignored
    /// and leaves the model as it was, which keeps a half-loaded view from
    /// editing the wrong member.
    void begin(qint64 chatId, qint64 userId, const MemberRights& rights);

    /// Abandons the session and clears all state, so chatId() and userId() go
    /// back to 0 and rights() back to the defaults. No-op when no session is
    /// active.
    void cancel();

    /// True while a member is being edited.
    [[nodiscard]] bool active() const;

    /// Chat owning the edited member, or 0 when no session is active.
    [[nodiscard]] qint64 chatId() const;

    /// Member being edited, or 0 when no session is active.
    [[nodiscard]] qint64 userId() const;

    /// Rights the dialog currently shows, or the defaults when no session is
    /// active.
    [[nodiscard]] MemberRights rights() const;

    /// Stores value in the right named name, where name is the field name of
    /// MemberRights ("canSendMessages", "canSendMedia", "canSendPolls",
    /// "canAddMembers", "canPinMessages", "canManageTopics").
    ///
    /// Returns false when the name is unknown, when no session is active, or for
    /// "isAdmin": admin status is granted through the member's status, not
    /// through the rights editor, so it is read-only here. A rejected call
    /// changes nothing. An accepted call returns true even when the right already
    /// held that value; changed() is emitted only for an actual change.
    bool setRight(const QString& name, bool value);

    /// True when the draft differs from the rights the session started from.
    /// Always false without an active session, because there is no draft to
    /// compare.
    [[nodiscard]] bool dirty() const;

    /// Accepts the draft as the new baseline after TDLib confirmed it, so the
    /// dialog is clean again while still showing the saved values. Emits
    /// changed() only when that clears a dirty state.
    void markSaved();

    /// True when there is something to submit: an active session whose draft
    /// differs from the baseline.
    [[nodiscard]] bool canSave() const;

signals:
    /// Emitted whenever the observed state actually changed: a session was
    /// started or abandoned, a right was flipped, or markSaved() cleared the
    /// edit. Repeated identical calls stay silent.
    void changed();

private:
    qint64 m_chatId = 0, m_userId = 0;
    MemberRights m_rights, m_original;
    bool m_active = false;
};

} // namespace beygram::telegram
