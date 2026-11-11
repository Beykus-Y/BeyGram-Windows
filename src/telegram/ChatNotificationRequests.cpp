#include "ChatNotificationRequests.h"

#include "TelegramTypes.h"

#include <QJsonValue>
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
constexpr qint64 kSilentSoundId = 0;

// TDLib sends real booleans. Anything else (missing, null, a number, a string)
// keeps the fallback instead of silently reading as false.
bool boolField(const QJsonObject& object, const QString& key, bool fallback)
{
    const QJsonValue value = object.value(key);
    return value.isBool() ? value.toBool() : fallback;
}

qint64 clampedMuteFor(const QJsonValue& value)
{
    const qint64 muteFor = jsonInt64(value);
    if (muteFor <= 0)
        return 0;
    return std::min(muteFor, kMuteForever);
}

bool soundEnabled(const QJsonObject& settings)
{
    // While use_default_sound is true TDLib ignores sound_id and takes the sound
    // from the chat's notification scope, which this state does not carry.
    const QJsonValue useDefaultSound = settings.value(QStringLiteral("use_default_sound"));
    if (useDefaultSound.isBool() && useDefaultSound.toBool())
        return true;

    const QJsonValue soundId = settings.value(QStringLiteral("sound_id"));
    if (soundId.isDouble() || soundId.isString())
        return jsonInt64(soundId) != 0;

    // Older payloads carry a plain boolean instead of a ringtone id.
    return boolField(settings, QStringLiteral("sound"), true);
}

bool mentionsOnly(const QJsonObject& settings)
{
    const QJsonValue disable =
        settings.value(QStringLiteral("disable_mention_notifications"));
    if (disable.isBool())
        return disable.toBool();

    // Alias accepted for callers that hand over the flag under this name.
    return boolField(settings, QStringLiteral("mentions_only"), false);
}

QJsonObject notificationSettingsOf(const QJsonObject& update)
{
    const QJsonValue settings = update.value(QStringLiteral("notification_settings"));
    return settings.isObject() ? settings.toObject() : update;
}

} // namespace

ChatNotificationState notificationStateFromUpdate(const QJsonObject& update)
{
    const QJsonObject settings = notificationSettingsOf(update);

    ChatNotificationState state;
    state.useDefaultMute =
        boolField(settings, QStringLiteral("use_default_mute_for"), state.useDefaultMute);

    // TDLib derives the mute from mute_for only when the chat owns the setting;
    // get_dialog_notification_settings stores no mute at all for an inherited one.
    state.muteFor =
        state.useDefaultMute ? 0 : clampedMuteFor(settings.value(QStringLiteral("mute_for")));
    state.muted = state.muteFor > 0;

    state.sound = soundEnabled(settings);
    state.showPreview =
        boolField(settings, QStringLiteral("show_preview"), state.showPreview);
    state.mentionsOnly = mentionsOnly(settings);

    return state;
}

QJsonObject setChatNotificationSettingsRequest(qint64 chatId,
                                              const ChatNotificationState& state)
{
    const qint64 muteFor = state.muted
        ? (state.muteFor > 0 ? std::min(state.muteFor, kMuteForever) : kMuteForever)
        : 0;

    // Every field is present: TDLib rejects a partial chatNotificationSettings.
    // Story and pinned-message notifications stay inherited.
    const QJsonObject notificationSettings{
        {QStringLiteral("use_default_mute_for"), state.useDefaultMute},
        {QStringLiteral("mute_for"), static_cast<int>(muteFor)},
        {QStringLiteral("use_default_sound"), state.sound},
        {QStringLiteral("sound_id"), state.sound ? kDefaultSoundId : kSilentSoundId},
        {QStringLiteral("use_default_show_preview"), false},
        {QStringLiteral("show_preview"), state.showPreview},
        {QStringLiteral("use_default_mute_stories"), true},
        {QStringLiteral("mute_stories"), false},
        {QStringLiteral("use_default_story_sound"), true},
        {QStringLiteral("story_sound_id"), kDefaultSoundId},
        {QStringLiteral("use_default_show_story_poster"), true},
        {QStringLiteral("show_story_poster"), true},
        {QStringLiteral("use_default_disable_pinned_message_notifications"), true},
        {QStringLiteral("disable_pinned_message_notifications"), false},
        {QStringLiteral("use_default_disable_mention_notifications"), false},
        {QStringLiteral("disable_mention_notifications"), state.mentionsOnly},
    };

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setChatNotificationSettings")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("notification_settings"), notificationSettings},
    };
}

} // namespace beygram::telegram
