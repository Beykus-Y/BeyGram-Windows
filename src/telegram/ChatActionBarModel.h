#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// The bar TDLib shows above the message list of one chat, reduced to the kinds
/// this client paints. TDLib names the bar with a chatActionBar object; the
/// mapping is documented below so the transport can translate without guessing.
///
///   chatActionBarReportSpam can_unblock:Bool = ChatActionBar;
///   chatActionBarReportUnrelatedLocation = ChatActionBar;
///   chatActionBarInviteMembers = ChatActionBar;
///   chatActionBarReportAddBlock can_unblock:Bool = ChatActionBar;
///   chatActionBarAddContact = ChatActionBar;
///   chatActionBarSharePhoneNumber = ChatActionBar;
///   chatActionBarJoinRequest title:string is_channel:Bool request_date:int32
///                           = ChatActionBar;
///
/// Mapping, kind by kind:
///
///   ReportSpam     chatActionBarReportSpam (can_unblock is not modelled here;
///                  the blocked case is the separate UnblockUser kind).
///   AddContact     chatActionBarAddContact.
///   SharePhone     chatActionBarSharePhoneNumber.
///   BlockUser      chatActionBarReportAddBlock with can_unblock false.
///   UnblockUser    chatActionBarReportAddBlock (or chatActionBarReportSpam)
///                  with can_unblock true. Kept apart from BlockUser because
///                  the label and the request differ: ChatBlocking's
///                  blockSenderRequest sends blocked=true for BlockUser and
///                  blocked=false for UnblockUser.
///   JoinGroup      chatActionBarInviteMembers. A chat awaiting approval is
///                  chatActionBarJoinRequest, which this enum does not model.
///   RestoreChat    no TDLib counterpart: it is this client's bar for putting
///                  a chat back into the chat list, so it is driven locally.
///   ReportLocation chatActionBarReportUnrelatedLocation.
///
/// ChatBlocking.h was checked for reuse and has no action enum or struct: it
/// holds only pure request builders. This enum therefore stands on its own, and
/// ReportSpam/BlockUser/UnblockUser are expected to be answered with those
/// builders (reportMessageRequest, blockSenderRequest) rather than duplicated
/// here.
enum class ActionBarKind {
    None,           ///< No bar is shown.
    ReportSpam,     ///< The sender is unknown and may be spam.
    AddContact,     ///< The peer can be added to contacts.
    SharePhone,     ///< The peer asks for this account's phone number.
    BlockUser,      ///< The peer can be blocked.
    UnblockUser,    ///< The peer is blocked and can be unblocked.
    JoinGroup,      ///< The chat can be joined or its members invited.
    RestoreChat,    ///< The chat can be restored to the chat list.
    ReportLocation, ///< The chat reports a location unrelated to its members.
};

/// The state of one chat's action bar: which kind is shown, the label the bar
/// carries, and whether a request for it is in flight.
///
/// The model only stores and validates that state. It never sends anything to
/// TDLib and never formats a label on its own, because Telegram localises bar
/// texts and the transport receives the final string. A view binds its button
/// to the getters, calls the setters from its handlers, and repaints on
/// changed().
///
/// The invariant is that text() and busy() only carry a value for a visible
/// bar: kind None always comes with an empty text and busy false. Text is
/// trimmed, and an empty text is allowed for a visible kind, because a bar may
/// be shown before its label arrives.
///
/// Every setter emits changed() only when the stored state really moved, so a
/// view can repaint unconditionally and repeated updates stay silent.
class ChatActionBarModel final : public QObject {
    Q_OBJECT

public:
    explicit ChatActionBarModel(QObject* parent = nullptr);

    /// Selects the bar `kind` with label `text`. The text is trimmed; a blank
    /// text is stored as empty and keeps the bar visible. None clears the text
    /// and the busy flag, and ignores `text`, so an invisible bar can never
    /// carry a stale label. Switching between two visible kinds keeps the busy
    /// flag, since the request already in flight still belongs to this bar.
    void setAction(ActionBarKind kind, const QString& text);

    [[nodiscard]] ActionBarKind kind() const;

    /// The label of the bar, trimmed. Empty while the bar is invisible.
    [[nodiscard]] QString text() const;

    /// True whenever a bar should be painted, i.e. kind() is not None.
    [[nodiscard]] bool visible() const;

    /// Marks a request for the current bar as in flight, which the view uses to
    /// disable the button. Ignored while the bar is invisible: there is no
    /// button to disable and None must stay with busy false.
    void setBusy(bool busy);
    [[nodiscard]] bool busy() const;

    /// Hides the bar and drops its text and busy flag. Emits changed() only
    /// when there was something to clear.
    void clear();

signals:
    /// Emitted whenever the kind, text or busy flag really changed. Ignored
    /// calls and no-op setters stay silent.
    void changed();

private:
    ActionBarKind m_kind = ActionBarKind::None;
    QString m_text;
    bool m_busy = false;
};

} // namespace beygram::telegram
