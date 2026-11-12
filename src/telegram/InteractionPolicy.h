#pragma once

#include <QJsonObject>
#include <QString>

namespace beygram::telegram {

/// What the current account may do in a chat, or with one message of it.
///
/// The struct is a plain value: no pointers, no session state and no secrets,
/// so it can be cached with a chat row and copied across threads. The defaults
/// describe an unrestricted private chat holding a received message: every
/// ordinary action is offered, editing is not (an incoming message is not
/// yours), and no special chat kind applies.
struct InteractionPolicy {
    bool canSendMessages = true;
    bool canSendMedia = true;
    bool canSendPolls = true;
    bool canSendStickers = true;

    bool canReply = true;
    bool canForward = true;
    bool canCopy = true;
    bool canSave = true;

    bool canReact = true;
    bool canEdit = false;
    bool canDelete = true;
    bool canPin = true;

    bool canCreateTopics = true;
    bool canManageTopics = false;

    bool isChannel = false;
    bool isSecret = false;
    bool isBot = false;
    bool isPremiumRequired = false;
};

/// Builds the policy of a chat from a TDLib chat object.
///
/// Shape follows the pinned td_api.tl (TDLib 1.8.69, see
/// docs/WINDOWS_TDLIB.md):
///
///   chat id:int53 type:ChatType title:string ... permissions:chatPermissions
///        ... = Chat;
///   chatPermissions can_send_basic_messages:Bool can_send_audios:Bool
///                   can_send_documents:Bool can_send_photos:Bool
///                   can_send_videos:Bool can_send_video_notes:Bool
///                   can_send_voice_notes:Bool can_send_polls:Bool
///                   can_send_other_messages:Bool can_add_link_previews:Bool
///                   can_react_to_messages:Bool can_pin_messages:Bool
///                   can_create_topics:Bool = ChatPermissions;
///
/// The type object decides the chat kind: chatTypeSupergroup with is_channel
/// sets isChannel, and chatTypeSecret sets isSecret. isBot is read from an
/// is_bot field on the chat payload (or on its type object), because TDLib's
/// chatTypePrivate carries only a user id; isPremiumRequired is read from
/// is_premium. A raw TDLib chat has neither field, so both stay false until an
/// enriched chat row supplies them.
///
/// permissions holds the non-administrator member rights TDLib reports in
/// chat.permissions. When the field is absent the defaults above are kept: a
/// private chat has no restriction object, and an unknown payload must not look
/// restricted. When it is present, every recognised flag overrides its field;
/// an absent flag keeps the default, while a flag that is present but not a
/// JSON boolean is read as false, so malformed data can never grant a right.
///
/// Recognised flags:
///   can_send_basic_messages (legacy can_send_messages) -> canSendMessages
///   can_send_media_messages, or the pinned split media flags -> canSendMedia
///   can_send_polls -> canSendPolls
///   can_send_other_messages -> canSendStickers
///   can_pin_messages -> canPin
///   can_react_to_messages -> canReact
///   can_create_topics (legacy can_manage_topics) -> canCreateTopics
///
/// The legacy can_send_media_messages is used only when no split media flag is
/// present; otherwise every split kind must be granted, mirroring
/// ChatPermissions. can_send_other_messages also covers animations, games and
/// inline bots, which the policy folds into canSendStickers because it has no
/// separate fields for them.
///
/// Link previews (can_add_link_previews, formerly can_add_web_page_previews)
/// have no counterpart here. A denied preview only affects the decoration of a
/// text message; it does not forbid text or attachments, so folding it into
/// canSendMedia or canSendMessages would hide actions Telegram still allows.
///
/// canEdit stays false and canManageTopics stays false: editing a message
/// depends on the message's author, and managing topics needs administrator
/// rights that chat.permissions does not report. The same applies to a channel
/// administrator, because chat.permissions describes regular members: a caller
/// that knows the account administers the channel raises canSendMessages (and
/// canEdit when appropriate) before asking interactionPolicyFromMessage.
[[nodiscard]] InteractionPolicy interactionPolicyFromChat(const QJsonObject& chat);

/// Narrows a chat policy for one message. It never grants a capability: every
/// change turns a true into false, and a message without markers returns
/// chatPolicy unchanged.
///
///   - has_protected_content, or the per-user protected flags of a
///     getMessageProperties answer, withdraws canCopy and canSave.
///   - a secret chat, or a message marked is_secret, withdraws canForward.
///   - paid media (messagePaidMedia content, or an is_paid/paid_media marker)
///     withdraws canForward and canSave.
///   - a channel post withdraws canReply and canEdit unless the chat policy
///     still allows posting (canSendMessages). A subscriber's policy has it
///     false; a caller that knows the account administers the channel raises
///     it before this call.
///
/// The rules are independent, so protected paid media loses copy, save and
/// forward. Only the markers above are read; forwarding and copying otherwise
/// keep the chat policy's value, so a caller with additional per-message
/// rights clears those fields itself.
[[nodiscard]] InteractionPolicy interactionPolicyFromMessage(
    const QJsonObject& message, const InteractionPolicy& chatPolicy);

/// Returns a stable, non-localized key for why this chat refuses outgoing
/// messages, or an empty string when it accepts them.
///
/// Priority: isPremiumRequired -> "premium"; then a secret chat -> "" (a
/// secret chat is private, not restricted, and never produces a denial key);
/// then a chat whose canSendMessages is false -> "restricted", or "channel"
/// when isChannel is set. A channel administrator still has canSendMessages
/// true and therefore no reason.
///
/// A chat that can send text but not media, polls or stickers still returns
/// "": this answers whether the composer may open at all, and the individual
/// send flags gate those attach actions. The caller localizes the key; no
/// logging or user-facing text belongs here.
[[nodiscard]] QString policyDenyReason(const InteractionPolicy& policy);

} // namespace beygram::telegram
