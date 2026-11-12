#pragma once

#include <QJsonObject>

namespace beygram::telegram {

/// Non-administrator member rights of a chat, parsed from TDLib's
/// chatPermissions. The struct holds no pointers, no session state and no
/// secrets, so it can be cached per account and copied across threads.
///
/// The defaults are the safe reading of a chat that reports no permissions at
/// all: nothing may be sent or administered, while link previews stay enabled
/// because Telegram grants them to regular members unless it takes them away.
struct ChatPermissions {
    bool canSendMessages = false;
    bool canSendMedia = false;
    bool canSendPolls = false;
    bool canSendOther = false;
    bool canAddWebPreviews = true;
    bool canChangeInfo = false;
    bool canInviteUsers = false;
    bool canPinMessages = false;
    bool canManageTopics = false;
};

/// Parses chat.permissions of a TDLib chat object.
///
/// Shape follows the pinned td_api.tl (TDLib 1.8.69, commit 4c6377aa, see
/// docs/WINDOWS_TDLIB.md):
///
///   chat id:int53 type:ChatType title:string ... permissions:chatPermissions ...
///        = Chat;
///   chatPermissions can_send_basic_messages:Bool can_send_audios:Bool
///                   can_send_documents:Bool can_send_photos:Bool
///                   can_send_videos:Bool can_send_video_notes:Bool
///                   can_send_voice_notes:Bool can_send_polls:Bool
///                   can_send_other_messages:Bool can_add_link_previews:Bool
///                   can_react_to_messages:Bool can_edit_tag:Bool
///                   can_change_info:Bool can_invite_users:Bool
///                   can_pin_messages:Bool can_create_topics:Bool
///                   = ChatPermissions;
///
/// That revision split four older flags, so both spellings are read: the
/// pinned name is used when it is present and the legacy one otherwise, which
/// keeps a chat cached by an older client parseable.
///
///   can_send_basic_messages     <- can_send_messages
///   can_send_audios/documents/photos/videos/video_notes/voice_notes
///                               <- can_send_media_messages
///   can_add_link_previews       <- can_add_web_page_previews
///   can_create_topics           <- can_manage_topics
///
/// canSendMedia needs every media kind: attaching media is offered only when
/// all of the split flags are granted, so one denied (or missing) kind keeps it
/// false. A flag that is present but not a JSON boolean is treated as denied,
/// so malformed data can never grant a right; only an absent flag keeps the
/// struct default.
[[nodiscard]] ChatPermissions permissionsFromChat(const QJsonObject& chat);

/// Parses the member permissions of a TDLib supergroup object.
///
/// The pinned schema keeps supergroup free of permissions: regular member
/// rights live on chat.permissions, which permissionsFromChat reads. This
/// therefore parses an embedded permissions object when the caller has one
/// (a cached payload from an older revision, or a supergroup merged with the
/// chat that owns it) and otherwise returns the struct defaults. An object
/// that already is a chatPermissions, or that carries the can_* flags directly,
/// is accepted as well.
[[nodiscard]] ChatPermissions permissionsFromSupergroup(const QJsonObject& supergroup);

} // namespace beygram::telegram
