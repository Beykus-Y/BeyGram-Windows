#include "ChatListActions.h"

#include <QString>

#include <algorithm>
#include <limits>

namespace beygram::telegram {
namespace {

// mute_for is an int32 count of seconds; its maximum is how TDLib spells
// "muted forever".
constexpr qint64 kMuteForever = std::numeric_limits<int>::max();

// chatNotificationSettings::sound_id: -1 selects the app-dependent default
// sound, 0 disables the sound, a positive value names a saved ringtone.
constexpr qint64 kDefaultSoundId = -1;

// toggleChatIsPinned stores the pinned state per chat list, so the list is a
// required field of the request. The archive is reached through
// addChatToList, not through this toggle, and a folder needs chatListFolder,
// which this builder does not carry.
QJsonObject mainChatList()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("chatListMain")},
    };
}

} // namespace

QJsonObject toggleChatIsPinnedRequest(qint64 chatId, bool pinned)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("toggleChatIsPinned")},
        {QStringLiteral("chat_list"), mainChatList()},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("is_pinned"), pinned},
    };
}

QJsonObject toggleChatIsMarkedAsUnreadRequest(qint64 chatId, bool unread)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("toggleChatIsMarkedAsUnread")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("is_marked_as_unread"), unread},
    };
}

QJsonObject toggleChatIsTranslatableRequest(qint64 chatId, bool enabled)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("toggleChatIsTranslatable")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("is_translatable"), enabled},
    };
}

QJsonObject setChatNotificationSettingsRequest(qint64 chatId, bool muted, qint64 muteFor)
{
    const qint64 clampedMuteFor = muted
        ? (muteFor > 0 ? std::min(muteFor, kMuteForever) : kMuteForever)
        : 0;

    // Every member is present because TDLib rejects a partial
    // chatNotificationSettings. Only the mute is a per-chat value; the rest is
    // written as "inherit", so the scope still decides sound, previews,
    // stories, pinned-message and mention notifications.
    const QJsonObject notificationSettings{
        {QStringLiteral("use_default_mute_for"), false},
        {QStringLiteral("mute_for"), static_cast<int>(clampedMuteFor)},
        {QStringLiteral("use_default_sound"), true},
        {QStringLiteral("sound_id"), kDefaultSoundId},
        {QStringLiteral("use_default_show_preview"), true},
        {QStringLiteral("show_preview"), true},
        {QStringLiteral("use_default_mute_stories"), true},
        {QStringLiteral("mute_stories"), false},
        {QStringLiteral("use_default_story_sound"), true},
        {QStringLiteral("story_sound_id"), kDefaultSoundId},
        {QStringLiteral("use_default_show_story_poster"), true},
        {QStringLiteral("show_story_poster"), true},
        {QStringLiteral("use_default_disable_pinned_message_notifications"), true},
        {QStringLiteral("disable_pinned_message_notifications"), false},
        {QStringLiteral("use_default_disable_mention_notifications"), true},
        {QStringLiteral("disable_mention_notifications"), false},
    };

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setChatNotificationSettings")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("notification_settings"), notificationSettings},
    };
}

} // namespace beygram::telegram
