#pragma once

#include <QJsonObject>
#include <QtGlobal>

namespace beygram::telegram {

/// Pure builders for the per-chat actions of the chat list context menu: pin,
/// mark as unread, translation and mute. They contain no logging and no
/// secrets; the transport adds correlation.
///
/// Shapes follow the TDLib schema pinned by this client (1.8.69, commit
/// 4c6377aa, see docs/WINDOWS_TDLIB.md):
///
///   toggleChatIsPinned chat_list:ChatList chat_id:int53 is_pinned:Bool = Ok;
///   toggleChatIsMarkedAsUnread chat_id:int53 is_marked_as_unread:Bool = Ok;
///   toggleChatIsTranslatable chat_id:int53 is_translatable:Bool = Ok;
///   setChatNotificationSettings chat_id:int53
///       notification_settings:chatNotificationSettings = Ok;
///
/// Unlike the marked-as-unread and translatable toggles, toggleChatIsPinned
/// takes a chat_list in this revision: the pinned state is stored per list, so
/// the list is not optional and is never sent as a null chat_list (null means
/// the main list only for the list-returning methods such as getChats).

/// Builds toggleChatIsPinned for the main chat list. Pinning inside a folder is
/// a different list (chatListFolder) and cannot be expressed through this
/// builder, whose signature carries no chat list: a chat that lives in a folder
/// keeps its folder position, while this request changes the main-list one.
[[nodiscard]] QJsonObject toggleChatIsPinnedRequest(qint64 chatId, bool pinned);

/// Builds toggleChatIsMarkedAsUnread. The flag is sent as
/// is_marked_as_unread, the name TDLib uses in the schema; the update that
/// follows is updateChatIsMarkedAsUnread.
[[nodiscard]] QJsonObject toggleChatIsMarkedAsUnreadRequest(qint64 chatId, bool unread);

/// Builds toggleChatIsTranslatable. enabled=true allows translating the chat's
/// messages; enabled=false turns the chat's translation off.
[[nodiscard]] QJsonObject toggleChatIsTranslatableRequest(qint64 chatId, bool enabled);

/// Builds a mute-only setChatNotificationSettings.
///
/// This overload carries just the mute, so every other member of
/// chatNotificationSettings is written as "inherit from the chat's notification
/// scope" and a mute toggle never rewrites the chat's sound, preview or
/// mention settings. That is the difference from the state-based overload in
/// ChatNotificationRequests.h, which owns those fields and writes them as
/// explicit per-chat overrides.
///
/// mute_for is an int32 count of seconds; 2147483647 is how TDLib spells
/// "muted forever". muted=true with a non-positive muteFor writes that value,
/// and larger durations are clamped to it. muted=false always writes 0 and
/// clears the chat's own mute, so the chat is explicitly unmuted even when its
/// scope is muted.
[[nodiscard]] QJsonObject setChatNotificationSettingsRequest(qint64 chatId, bool muted,
                                                            qint64 muteFor);

} // namespace beygram::telegram
