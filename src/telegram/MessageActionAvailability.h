#pragma once

#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One entry of a message's action menu.
enum class MessageAction {
    Reply,
    Forward,
    Copy,
    Edit,
    Delete,
    Pin,
    Save,
    React,
    Translate,
    Report,
};

/// What the action menu needs to know about one message.
///
/// The defaults describe an unrestricted private chat holding a received
/// message: ordinary actions are offered, editing is not (the message is not
/// the account's own), and no special message kind applies.
struct ActionContext {
    bool outgoing = false;
    bool canSend = true;
    bool isChannel = false;
    bool isSecret = false;
    bool protectedContent = false;
    bool paidMedia = false;
    bool pending = false;
    bool failed = false;
    bool hasText = false;
    bool isService = false;
    bool canPin = false;
    bool canDelete = true;
    bool canEdit = false;
};

/// Returns the actions offered for the message, in MessageAction declaration
/// order.
///
/// A service message, a pending send or a failed send offers only Delete, and
/// only while canDelete is set; every other action is withheld because the
/// message cannot be acted on as ordinary content.
///
/// Protected content withdraws Copy, Save, Forward and Translate. Paid media
/// withdraws Save and Forward. A secret chat withdraws Forward and Save.
/// Delete follows canDelete, Pin follows canPin, and Report is offered only for
/// a message that is not outgoing.
///
/// Reply follows canSend. In a channel a subscriber has canSend false and is
/// therefore refused Reply and Edit, while an administrator still has canSend
/// true (the same distinction InteractionPolicy makes). Edit additionally
/// requires an outgoing message, canEdit and hasText.
[[nodiscard]] QVector<MessageAction> availableActions(const ActionContext& context);

/// True when the single action is offered for the message. Equivalent to
/// asking whether availableActions() contains it.
[[nodiscard]] bool isActionAvailable(MessageAction action, const ActionContext& context);

/// English menu label for the action.
[[nodiscard]] QString actionLabel(MessageAction action);

} // namespace beygram::telegram
