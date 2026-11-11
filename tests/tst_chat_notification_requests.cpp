#include "telegram/ChatNotificationRequests.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QtTest>

using namespace beygram::telegram;

namespace {

constexpr qint64 kMuteForever = 2147483647LL;

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

QJsonObject updateFromJson(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

QJsonObject settingsOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("notification_settings")).toObject();
}

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

QJsonObject settingsFor(const ChatNotificationState& state)
{
    return settingsOf(serialized(setChatNotificationSettingsRequest(42, state)));
}

} // namespace

class ChatNotificationRequestsTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesUpdateChatNotificationSettings();
    void parsesInheritedDefaults();
    void ignoresMuteForWhileMuteIsInherited();
    void readsSoundFromRingtoneId();
    void readsStringEncodedMuteFor();
    void clampsMuteForToInt32();
    void acceptsBareSettingsAndChatObjects();
    void keepsDefaultsForMissingOrWronglyTypedFields();
    void buildsSetRequestShape();
    void writesEverySchemaField();
    void mapsMuteState();
    void mapsSoundState();
    void writesPreviewAndMentionsAsOverrides();
    void keepsStoryAndPinnedSettingsInherited();
    void preservesLargeChatIds();
    void roundTripsStateThroughRequestJson();
};

void ChatNotificationRequestsTest::parsesUpdateChatNotificationSettings()
{
    const QJsonObject update = updateFromJson(R"({
        "@type": "updateChatNotificationSettings",
        "chat_id": 42,
        "notification_settings": {
            "use_default_mute_for": false,
            "mute_for": 3600,
            "use_default_sound": false,
            "sound_id": 12345,
            "use_default_show_preview": false,
            "show_preview": false,
            "use_default_mute_stories": true,
            "mute_stories": false,
            "use_default_story_sound": true,
            "story_sound_id": -1,
            "use_default_show_story_poster": true,
            "show_story_poster": true,
            "use_default_disable_pinned_message_notifications": true,
            "disable_pinned_message_notifications": false,
            "use_default_disable_mention_notifications": false,
            "disable_mention_notifications": true
        }
    })");

    const ChatNotificationState state = notificationStateFromUpdate(update);

    QVERIFY(state.muted);
    QVERIFY(!state.useDefaultMute);
    QCOMPARE(state.muteFor, qint64(3600));
    QVERIFY(state.sound);
    QVERIFY(!state.showPreview);
    QVERIFY(state.mentionsOnly);
}

void ChatNotificationRequestsTest::parsesInheritedDefaults()
{
    const QJsonObject update = updateFromJson(R"({
        "@type": "updateChatNotificationSettings",
        "chat_id": 42,
        "notification_settings": {
            "use_default_mute_for": true,
            "mute_for": 0,
            "use_default_sound": true,
            "sound_id": -1,
            "use_default_show_preview": true,
            "show_preview": true,
            "use_default_mute_stories": true,
            "mute_stories": false,
            "use_default_story_sound": true,
            "story_sound_id": -1,
            "use_default_show_story_poster": true,
            "show_story_poster": true,
            "use_default_disable_pinned_message_notifications": true,
            "disable_pinned_message_notifications": false,
            "use_default_disable_mention_notifications": true,
            "disable_mention_notifications": false
        }
    })");

    const ChatNotificationState state = notificationStateFromUpdate(update);

    QVERIFY(!state.muted);
    QVERIFY(state.useDefaultMute);
    QCOMPARE(state.muteFor, qint64(0));
    QVERIFY(state.sound);
    QVERIFY(state.showPreview);
    QVERIFY(!state.mentionsOnly);
}

void ChatNotificationRequestsTest::ignoresMuteForWhileMuteIsInherited()
{
    // TDLib stores no mute at all for a chat that follows its scope, so a stale
    // mute_for next to use_default_mute_for = true must not report a mute.
    const QJsonObject update = updateFromJson(R"({
        "@type": "updateChatNotificationSettings",
        "chat_id": 42,
        "notification_settings": {
            "use_default_mute_for": true,
            "mute_for": 900
        }
    })");

    const ChatNotificationState state = notificationStateFromUpdate(update);

    QVERIFY(state.useDefaultMute);
    QVERIFY(!state.muted);
    QCOMPARE(state.muteFor, qint64(0));
}

void ChatNotificationRequestsTest::readsSoundFromRingtoneId()
{
    // 0 is the silent ringtone, any other id means "plays a sound".
    const QJsonObject silent = updateFromJson(R"({
        "@type": "updateChatNotificationSettings",
        "chat_id": 42,
        "notification_settings": {
            "use_default_sound": false,
            "sound_id": 0
        }
    })");
    QVERIFY(!notificationStateFromUpdate(silent).sound);

    const QJsonObject custom = updateFromJson(R"({
        "@type": "updateChatNotificationSettings",
        "chat_id": 42,
        "notification_settings": {
            "use_default_sound": false,
            "sound_id": 987654321
        }
    })");
    QVERIFY(notificationStateFromUpdate(custom).sound);

    // An inherited sound ignores sound_id: the scope decides and is not part of
    // this state, so the chat is reported as sounding.
    const QJsonObject inherited = updateFromJson(R"({
        "@type": "updateChatNotificationSettings",
        "chat_id": 42,
        "notification_settings": {
            "use_default_sound": true,
            "sound_id": 0
        }
    })");
    QVERIFY(notificationStateFromUpdate(inherited).sound);
}

void ChatNotificationRequestsTest::readsStringEncodedMuteFor()
{
    const QJsonObject update = updateFromJson(R"({
        "@type": "updateChatNotificationSettings",
        "chat_id": "42",
        "notification_settings": {
            "use_default_mute_for": false,
            "mute_for": "1800"
        }
    })");

    const ChatNotificationState state = notificationStateFromUpdate(update);

    QVERIFY(state.muted);
    QCOMPARE(state.muteFor, qint64(1800));
}

void ChatNotificationRequestsTest::clampsMuteForToInt32()
{
    const QJsonObject negative = updateFromJson(R"({
        "@type": "updateChatNotificationSettings",
        "chat_id": 42,
        "notification_settings": {
            "use_default_mute_for": false,
            "mute_for": -60
        }
    })");
    const ChatNotificationState negativeState = notificationStateFromUpdate(negative);
    QVERIFY(!negativeState.muted);
    QCOMPARE(negativeState.muteFor, qint64(0));

    const QJsonObject huge = updateFromJson(R"({
        "@type": "updateChatNotificationSettings",
        "chat_id": 42,
        "notification_settings": {
            "use_default_mute_for": false,
            "mute_for": 99999999999
        }
    })");
    const ChatNotificationState hugeState = notificationStateFromUpdate(huge);
    QVERIFY(hugeState.muted);
    QCOMPARE(hugeState.muteFor, kMuteForever);
}

void ChatNotificationRequestsTest::acceptsBareSettingsAndChatObjects()
{
    const QJsonObject bare = updateFromJson(R"({
        "use_default_mute_for": false,
        "mute_for": 60,
        "show_preview": false
    })");
    const ChatNotificationState bareState = notificationStateFromUpdate(bare);
    QVERIFY(bareState.muted);
    QCOMPARE(bareState.muteFor, qint64(60));
    QVERIFY(!bareState.showPreview);

    // A chat object carries the same settings under notification_settings.
    const QJsonObject chat = updateFromJson(R"({
        "@type": "chat",
        "id": 42,
        "notification_settings": {
            "use_default_mute_for": false,
            "mute_for": 60,
            "show_preview": false
        }
    })");
    const ChatNotificationState chatState = notificationStateFromUpdate(chat);
    QVERIFY(chatState.muted);
    QCOMPARE(chatState.muteFor, qint64(60));
    QVERIFY(!chatState.showPreview);
}

void ChatNotificationRequestsTest::keepsDefaultsForMissingOrWronglyTypedFields()
{
    const ChatNotificationState empty = notificationStateFromUpdate(QJsonObject{});
    QVERIFY(!empty.muted);
    QVERIFY(empty.useDefaultMute);
    QCOMPARE(empty.muteFor, qint64(0));
    QVERIFY(empty.sound);
    QVERIFY(empty.showPreview);
    QVERIFY(!empty.mentionsOnly);

    const QJsonObject malformed = updateFromJson(R"({
        "@type": "updateChatNotificationSettings",
        "chat_id": 42,
        "notification_settings": {
            "use_default_mute_for": "yes",
            "mute_for": true,
            "use_default_sound": "true",
            "sound_id": null,
            "show_preview": 0,
            "disable_mention_notifications": "no"
        }
    })");
    const ChatNotificationState state = notificationStateFromUpdate(malformed);
    QVERIFY(!state.muted);
    QVERIFY(state.useDefaultMute);
    QCOMPARE(state.muteFor, qint64(0));
    QVERIFY(state.sound);
    QVERIFY(state.showPreview);
    QVERIFY(!state.mentionsOnly);
}

void ChatNotificationRequestsTest::buildsSetRequestShape()
{
    const QJsonObject request =
        serialized(setChatNotificationSettingsRequest(42, ChatNotificationState{}));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setChatNotificationSettings"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(42));
    QVERIFY(request.value(QStringLiteral("notification_settings")).isObject());
    QCOMPARE(int(request.size()), 3);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void ChatNotificationRequestsTest::writesEverySchemaField()
{
    const QJsonObject settings = settingsFor(ChatNotificationState{});

    const QStringList booleans{
        QStringLiteral("use_default_mute_for"),
        QStringLiteral("use_default_sound"),
        QStringLiteral("use_default_show_preview"),
        QStringLiteral("show_preview"),
        QStringLiteral("use_default_mute_stories"),
        QStringLiteral("mute_stories"),
        QStringLiteral("use_default_story_sound"),
        QStringLiteral("use_default_show_story_poster"),
        QStringLiteral("show_story_poster"),
        QStringLiteral("use_default_disable_pinned_message_notifications"),
        QStringLiteral("disable_pinned_message_notifications"),
        QStringLiteral("use_default_disable_mention_notifications"),
        QStringLiteral("disable_mention_notifications"),
    };
    for (const QString& key : booleans) {
        QVERIFY2(settings.value(key).isBool(), qPrintable(key));
    }

    const QStringList numbers{
        QStringLiteral("mute_for"),
        QStringLiteral("sound_id"),
        QStringLiteral("story_sound_id"),
    };
    for (const QString& key : numbers) {
        QVERIFY2(settings.value(key).isDouble(), qPrintable(key));
    }

    QCOMPARE(int(settings.size()), int(booleans.size() + numbers.size()));
}

void ChatNotificationRequestsTest::mapsMuteState()
{
    // Not muted: mute_for 0, even when the state still carries a leftover value.
    ChatNotificationState unmuted;
    unmuted.useDefaultMute = false;
    unmuted.muteFor = 3600;
    QCOMPARE(numberAt(settingsFor(unmuted), QStringLiteral("mute_for")), qint64(0));

    // Muted for an hour.
    ChatNotificationState hour;
    hour.muted = true;
    hour.useDefaultMute = false;
    hour.muteFor = 3600;
    QCOMPARE(numberAt(settingsFor(hour), QStringLiteral("mute_for")), qint64(3600));
    QVERIFY(!settingsFor(hour).value(QStringLiteral("use_default_mute_for")).toBool());

    // Muted with no duration means "muted forever".
    ChatNotificationState foreverState;
    foreverState.muted = true;
    foreverState.useDefaultMute = false;
    QCOMPARE(numberAt(settingsFor(foreverState), QStringLiteral("mute_for")), kMuteForever);

    // A negative duration is treated the same way rather than unmuting.
    ChatNotificationState negative;
    negative.muted = true;
    negative.useDefaultMute = false;
    negative.muteFor = -5;
    QCOMPARE(numberAt(settingsFor(negative), QStringLiteral("mute_for")), kMuteForever);

    // Durations above int32 are clamped to the value TDLib reads as forever.
    ChatNotificationState huge;
    huge.muted = true;
    huge.useDefaultMute = false;
    huge.muteFor = 99999999999LL;
    QCOMPARE(numberAt(settingsFor(huge), QStringLiteral("mute_for")), kMuteForever);

    // An inherited mute is written through use_default_mute_for.
    ChatNotificationState inherited;
    QVERIFY(settingsFor(inherited).value(QStringLiteral("use_default_mute_for")).toBool());
    QCOMPARE(numberAt(settingsFor(inherited), QStringLiteral("mute_for")), qint64(0));
}

void ChatNotificationRequestsTest::mapsSoundState()
{
    ChatNotificationState audible;
    audible.sound = true;
    const QJsonObject audibleSettings = settingsFor(audible);
    QVERIFY(audibleSettings.value(QStringLiteral("use_default_sound")).toBool());
    QCOMPARE(numberAt(audibleSettings, QStringLiteral("sound_id")), qint64(-1));

    ChatNotificationState silent;
    silent.sound = false;
    const QJsonObject silentSettings = settingsFor(silent);
    QVERIFY(!silentSettings.value(QStringLiteral("use_default_sound")).toBool());
    QCOMPARE(numberAt(silentSettings, QStringLiteral("sound_id")), qint64(0));
}

void ChatNotificationRequestsTest::writesPreviewAndMentionsAsOverrides()
{
    ChatNotificationState state;
    state.showPreview = false;
    state.mentionsOnly = true;

    const QJsonObject settings = settingsFor(state);

    QVERIFY(!settings.value(QStringLiteral("use_default_show_preview")).toBool());
    QVERIFY(!settings.value(QStringLiteral("show_preview")).toBool());
    QVERIFY(!settings.value(QStringLiteral("use_default_disable_mention_notifications")).toBool());
    QVERIFY(settings.value(QStringLiteral("disable_mention_notifications")).toBool());

    // The defaults are written explicitly too, so saving one field never leaves
    // the other inherited by accident.
    const QJsonObject defaults = settingsFor(ChatNotificationState{});
    QVERIFY(!defaults.value(QStringLiteral("use_default_show_preview")).toBool());
    QVERIFY(defaults.value(QStringLiteral("show_preview")).toBool());
    QVERIFY(!defaults.value(QStringLiteral("use_default_disable_mention_notifications")).toBool());
    QVERIFY(!defaults.value(QStringLiteral("disable_mention_notifications")).toBool());
}

void ChatNotificationRequestsTest::keepsStoryAndPinnedSettingsInherited()
{
    const QJsonObject settings = settingsFor(ChatNotificationState{});

    QVERIFY(settings.value(QStringLiteral("use_default_mute_stories")).toBool());
    QVERIFY(!settings.value(QStringLiteral("mute_stories")).toBool());
    QVERIFY(settings.value(QStringLiteral("use_default_story_sound")).toBool());
    QCOMPARE(numberAt(settings, QStringLiteral("story_sound_id")), qint64(-1));
    QVERIFY(settings.value(QStringLiteral("use_default_show_story_poster")).toBool());
    QVERIFY(settings.value(QStringLiteral("show_story_poster")).toBool());
    QVERIFY(settings.value(QStringLiteral("use_default_disable_pinned_message_notifications")).toBool());
    QVERIFY(!settings.value(QStringLiteral("disable_pinned_message_notifications")).toBool());
}

void ChatNotificationRequestsTest::preservesLargeChatIds()
{
    // Telegram chat ids exceed 32 bits; they must not be truncated.
    const qint64 chatId = -1001234567890LL;
    const QJsonObject request =
        serialized(setChatNotificationSettingsRequest(chatId, ChatNotificationState{}));

    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), chatId);
}

void ChatNotificationRequestsTest::roundTripsStateThroughRequestJson()
{
    ChatNotificationState state;
    state.muted = true;
    state.useDefaultMute = false;
    state.muteFor = 3600;
    state.sound = false;
    state.showPreview = false;
    state.mentionsOnly = true;

    const ChatNotificationState parsed = notificationStateFromUpdate(settingsFor(state));

    QCOMPARE(parsed.muted, state.muted);
    QCOMPARE(parsed.useDefaultMute, state.useDefaultMute);
    QCOMPARE(parsed.muteFor, state.muteFor);
    QCOMPARE(parsed.sound, state.sound);
    QCOMPARE(parsed.showPreview, state.showPreview);
    QCOMPARE(parsed.mentionsOnly, state.mentionsOnly);

    // An inherited mute round-trips as inherited, not as an explicit zero.
    ChatNotificationState inherited;
    inherited.muted = false;
    inherited.useDefaultMute = true;
    const ChatNotificationState inheritedParsed =
        notificationStateFromUpdate(settingsFor(inherited));
    QVERIFY(!inheritedParsed.muted);
    QVERIFY(inheritedParsed.useDefaultMute);
    QCOMPARE(inheritedParsed.muteFor, qint64(0));
}

QTEST_GUILESS_MAIN(ChatNotificationRequestsTest)
#include "tst_chat_notification_requests.moc"
