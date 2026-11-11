#pragma once

#include <QJsonObject>
#include <QtGlobal>

namespace beygram::telegram {

/// Per-chat notification overrides, mirrored from TDLib's chatNotificationSettings.
///
/// Shape follows the pinned td_api.tl (TDLib 1.8.69):
///   chatNotificationSettings use_default_mute_for:Bool mute_for:int32
///       use_default_sound:Bool sound_id:int64 use_default_show_preview:Bool
///       show_preview:Bool use_default_mute_stories:Bool mute_stories:Bool
///       use_default_story_sound:Bool story_sound_id:int64
///       use_default_show_story_poster:Bool show_story_poster:Bool
///       use_default_disable_pinned_message_notifications:Bool
///       disable_pinned_message_notifications:Bool
///       use_default_disable_mention_notifications:Bool
///       disable_mention_notifications:Bool = ChatNotificationSettings;
///
/// Only the fields the per-chat notification UI owns are modelled. Story and
/// pinned-message settings are not modelled: requests write them as "inherit",
/// so saving this chat never changes them.
struct ChatNotificationState {
    /// True while the chat carries a mute of its own that has not expired.
    /// A chat that follows its notification scope reports false together with
    /// useDefaultMute true: the scope mute lives outside this per-chat state.
    bool muted = false;
    /// True while the mute is inherited from the chat's notification scope.
    bool useDefaultMute = true;
    /// True when a notification for this chat plays a sound.
    bool sound = true;
    /// True when message content may be shown in a notification.
    bool showPreview = true;
    /// Mirrors TDLib's disable_mention_notifications flag: true when a message
    /// that mentions the user is announced like any other unread message instead
    /// of with a mention-specific notification. Ordinary messages still notify,
    /// and this object has no "mute everything except mentions" bit.
    bool mentionsOnly = false;
    /// Seconds left before the mute expires; 0 when the chat is not muted.
    qint64 muteFor = 0;
};

/// Reads the chat-level notification state of one chat.
///
/// Accepts the updateChatNotificationSettings update itself, a chat object, or a
/// bare chatNotificationSettings object: the settings are taken from
/// "notification_settings" when that member is an object and from the object
/// itself otherwise.
///
/// Missing or wrongly typed fields keep the defaults above. Two fields are
/// normalised the way TDLib itself normalises them, because their raw value is
/// ignored while the matching use-default flag is set: an inherited mute reports
/// muteFor 0, and an inherited sound reports sound true (the scope decides, and
/// the scope is not part of this state).
///
/// The update's chat_id is not returned: the caller routes the update to a chat
/// itself, and a bare settings object has no chat id at all.
[[nodiscard]] ChatNotificationState notificationStateFromUpdate(const QJsonObject& update);

/// Builds setChatNotificationSettings chat_id:int53
/// notification_settings:chatNotificationSettings = Ok.
///
/// Every chatNotificationSettings member is written because TDLib rejects a
/// partial object. Mapping notes:
///   - muted is authoritative: false writes mute_for 0, true with a
///     non-positive muteFor writes 2147483647 ("muted forever"). Values above
///     the int32 range are clamped.
///   - useDefaultMute is written as use_default_mute_for. TDLib ignores
///     mute_for while that flag is true.
///   - sound true writes use_default_sound true with sound_id -1 (the
///     app-dependent default sound); sound false writes an explicit silent chat
///     (use_default_sound false, sound_id 0). A custom per-chat ringtone id
///     cannot be expressed and is replaced.
///   - showPreview and mentionsOnly are always written as explicit overrides,
///     so saving turns an inherited preview or mention value into a chat value.
[[nodiscard]] QJsonObject setChatNotificationSettingsRequest(qint64 chatId,
                                                            const ChatNotificationState& state);

} // namespace beygram::telegram
