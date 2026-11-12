#include "telegram/ChatListActions.h"

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

QJsonObject settingsOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("notification_settings")).toObject();
}

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

bool boolAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toBool();
}

} // namespace

class ChatListActionsTest final : public QObject {
    Q_OBJECT

private slots:
    void pinsChatInMainList();
    void unpinsChatInMainList();
    void pinAlwaysCarriesAChatList();
    void markedAsUnreadRequestShape();
    void markedAsUnreadHasNoChatList();
    void translatableRequestShape();
    void translatableHasNoChatList();
    void togglesPreserveLargeChatIds();
    void requestsSurviveSerialization();
    void muteRequestShape();
    void muteWritesEverySchemaField();
    void muteMapsMuteState();
    void muteClampsMuteForToInt32();
    void muteLeavesOtherSettingsInherited();
    void mutePreservesLargeChatIds();
    void muteRoundTripsThroughJson();
};

void ChatListActionsTest::pinsChatInMainList()
{
    const QJsonObject request = serialized(toggleChatIsPinnedRequest(42, true));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("toggleChatIsPinned"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(42));
    QVERIFY(boolAt(request, QStringLiteral("is_pinned")));
    QCOMPARE(int(request.size()), 4);
    QVERIFY(!request.contains(QStringLiteral("@extra")));

    const QJsonObject chatList = request.value(QStringLiteral("chat_list")).toObject();
    QCOMPARE(chatList.value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatListMain"));
    QCOMPARE(int(chatList.size()), 1);
}

void ChatListActionsTest::unpinsChatInMainList()
{
    const QJsonObject request = serialized(toggleChatIsPinnedRequest(42, false));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("toggleChatIsPinned"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(42));
    QVERIFY(request.contains(QStringLiteral("is_pinned")));
    QVERIFY(!boolAt(request, QStringLiteral("is_pinned")));
}

void ChatListActionsTest::pinAlwaysCarriesAChatList()
{
    // toggleChatIsPinned takes a chat_list in this schema; a missing or null one
    // would either be rejected or silently mean "main", so both states must
    // carry the explicit chatListMain object.
    for (const bool pinned : {true, false}) {
        const QJsonValue chatList = toggleChatIsPinnedRequest(7, pinned)
                                        .value(QStringLiteral("chat_list"));
        QVERIFY(chatList.isObject());
        QVERIFY(!chatList.isNull());
        QCOMPARE(chatList.toObject().value(QStringLiteral("@type")).toString(),
                 QStringLiteral("chatListMain"));
    }
}

void ChatListActionsTest::markedAsUnreadRequestShape()
{
    const QJsonObject marked = serialized(toggleChatIsMarkedAsUnreadRequest(42, true));
    QCOMPARE(marked.value(QStringLiteral("@type")).toString(),
             QStringLiteral("toggleChatIsMarkedAsUnread"));
    QCOMPARE(numberAt(marked, QStringLiteral("chat_id")), qint64(42));
    QVERIFY(boolAt(marked, QStringLiteral("is_marked_as_unread")));
    QCOMPARE(int(marked.size()), 3);

    const QJsonObject read = serialized(toggleChatIsMarkedAsUnreadRequest(42, false));
    QCOMPARE(read.value(QStringLiteral("@type")).toString(),
             QStringLiteral("toggleChatIsMarkedAsUnread"));
    QVERIFY(read.contains(QStringLiteral("is_marked_as_unread")));
    QVERIFY(!boolAt(read, QStringLiteral("is_marked_as_unread")));
}

void ChatListActionsTest::markedAsUnreadHasNoChatList()
{
    // Only the pin toggle is list-scoped; a chat_list here would be an unknown
    // field for TDLib.
    const QJsonObject request = toggleChatIsMarkedAsUnreadRequest(42, true);
    QVERIFY(!request.contains(QStringLiteral("chat_list")));
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void ChatListActionsTest::translatableRequestShape()
{
    const QJsonObject enabled = serialized(toggleChatIsTranslatableRequest(42, true));
    QCOMPARE(enabled.value(QStringLiteral("@type")).toString(),
             QStringLiteral("toggleChatIsTranslatable"));
    QCOMPARE(numberAt(enabled, QStringLiteral("chat_id")), qint64(42));
    QVERIFY(boolAt(enabled, QStringLiteral("is_translatable")));
    QCOMPARE(int(enabled.size()), 3);

    const QJsonObject disabled = serialized(toggleChatIsTranslatableRequest(42, false));
    QCOMPARE(disabled.value(QStringLiteral("@type")).toString(),
             QStringLiteral("toggleChatIsTranslatable"));
    QVERIFY(disabled.contains(QStringLiteral("is_translatable")));
    QVERIFY(!boolAt(disabled, QStringLiteral("is_translatable")));
}

void ChatListActionsTest::translatableHasNoChatList()
{
    const QJsonObject request = toggleChatIsTranslatableRequest(42, false);
    QVERIFY(!request.contains(QStringLiteral("chat_list")));
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void ChatListActionsTest::togglesPreserveLargeChatIds()
{
    // Telegram chat ids exceed 32 bits; they must not be truncated.
    const qint64 chatId = -1001234567890LL;

    QCOMPARE(numberAt(serialized(toggleChatIsPinnedRequest(chatId, true)),
                      QStringLiteral("chat_id")),
             chatId);
    QCOMPARE(numberAt(serialized(toggleChatIsMarkedAsUnreadRequest(chatId, true)),
                      QStringLiteral("chat_id")),
             chatId);
    QCOMPARE(numberAt(serialized(toggleChatIsTranslatableRequest(chatId, true)),
                      QStringLiteral("chat_id")),
             chatId);
}

void ChatListActionsTest::requestsSurviveSerialization()
{
    // The builders must produce JSON that survives the compact encoding used by
    // the transport: booleans stay booleans and the chat list stays an object.
    const QJsonObject pinned = serialized(toggleChatIsPinnedRequest(5, true));
    QVERIFY(pinned.value(QStringLiteral("is_pinned")).isBool());
    QVERIFY(pinned.value(QStringLiteral("chat_list")).isObject());

    const QJsonObject unread = serialized(toggleChatIsMarkedAsUnreadRequest(5, true));
    QVERIFY(unread.value(QStringLiteral("is_marked_as_unread")).isBool());

    const QJsonObject translatable = serialized(toggleChatIsTranslatableRequest(5, false));
    QVERIFY(translatable.value(QStringLiteral("is_translatable")).isBool());
}

void ChatListActionsTest::muteRequestShape()
{
    const QJsonObject request =
        serialized(setChatNotificationSettingsRequest(42, true, 3600));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setChatNotificationSettings"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(42));
    QVERIFY(request.value(QStringLiteral("notification_settings")).isObject());
    QCOMPARE(int(request.size()), 3);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void ChatListActionsTest::muteWritesEverySchemaField()
{
    // TDLib rejects a partial chatNotificationSettings, so every member of the
    // schema object has to be present.
    const QJsonObject settings = settingsOf(serialized(
        setChatNotificationSettingsRequest(42, true, 60)));

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

void ChatListActionsTest::muteMapsMuteState()
{
    // Unmuted: mute_for 0, and the chat owns that value rather than inheriting
    // it, so unmuting overrides a muted scope.
    const QJsonObject unmuted =
        settingsOf(serialized(setChatNotificationSettingsRequest(42, false, 3600)));
    QVERIFY(!boolAt(unmuted, QStringLiteral("use_default_mute_for")));
    QCOMPARE(numberAt(unmuted, QStringLiteral("mute_for")), qint64(0));

    // Muted for an hour.
    const QJsonObject hour =
        settingsOf(serialized(setChatNotificationSettingsRequest(42, true, 3600)));
    QVERIFY(!boolAt(hour, QStringLiteral("use_default_mute_for")));
    QCOMPARE(numberAt(hour, QStringLiteral("mute_for")), qint64(3600));

    // Muted with no duration means "muted forever".
    const QJsonObject foreverState =
        settingsOf(serialized(setChatNotificationSettingsRequest(42, true, 0)));
    QCOMPARE(numberAt(foreverState, QStringLiteral("mute_for")), kMuteForever);

    // A negative duration is treated the same way rather than unmuting.
    const QJsonObject negative =
        settingsOf(serialized(setChatNotificationSettingsRequest(42, true, -5)));
    QCOMPARE(numberAt(negative, QStringLiteral("mute_for")), kMuteForever);
}

void ChatListActionsTest::muteClampsMuteForToInt32()
{
    // Durations above int32 are clamped to the value TDLib reads as forever.
    const QJsonObject huge =
        settingsOf(serialized(setChatNotificationSettingsRequest(42, true, 99999999999LL)));
    QCOMPARE(numberAt(huge, QStringLiteral("mute_for")), kMuteForever);

    // The int32 boundary itself is preserved.
    const QJsonObject boundary =
        settingsOf(serialized(setChatNotificationSettingsRequest(42, true, kMuteForever)));
    QCOMPARE(numberAt(boundary, QStringLiteral("mute_for")), kMuteForever);
}

void ChatListActionsTest::muteLeavesOtherSettingsInherited()
{
    // A mute toggle must not turn sound, previews, stories, pinned-message or
    // mention notifications into per-chat overrides: they stay "inherit" so the
    // chat's notification scope still decides them.
    for (const bool muted : {true, false}) {
        const QJsonObject settings = settingsOf(serialized(
            setChatNotificationSettingsRequest(42, muted, 3600)));

        QVERIFY(boolAt(settings, QStringLiteral("use_default_sound")));
        QCOMPARE(numberAt(settings, QStringLiteral("sound_id")), qint64(-1));
        QVERIFY(boolAt(settings, QStringLiteral("use_default_show_preview")));
        QVERIFY(boolAt(settings, QStringLiteral("show_preview")));
        QVERIFY(boolAt(settings, QStringLiteral("use_default_mute_stories")));
        QVERIFY(!boolAt(settings, QStringLiteral("mute_stories")));
        QVERIFY(boolAt(settings, QStringLiteral("use_default_story_sound")));
        QCOMPARE(numberAt(settings, QStringLiteral("story_sound_id")), qint64(-1));
        QVERIFY(boolAt(settings, QStringLiteral("use_default_show_story_poster")));
        QVERIFY(boolAt(settings, QStringLiteral("show_story_poster")));
        QVERIFY(boolAt(settings,
                       QStringLiteral("use_default_disable_pinned_message_notifications")));
        QVERIFY(!boolAt(settings, QStringLiteral("disable_pinned_message_notifications")));
        QVERIFY(boolAt(settings,
                       QStringLiteral("use_default_disable_mention_notifications")));
        QVERIFY(!boolAt(settings, QStringLiteral("disable_mention_notifications")));
    }
}

void ChatListActionsTest::mutePreservesLargeChatIds()
{
    const qint64 chatId = -1001234567890LL;
    const QJsonObject request =
        serialized(setChatNotificationSettingsRequest(chatId, true, 60));

    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), chatId);
}

void ChatListActionsTest::muteRoundTripsThroughJson()
{
    // What is read back after the transport encoding must still describe the
    // same mute and still be a complete settings object.
    const QJsonObject request =
        serialized(setChatNotificationSettingsRequest(-1009876543210LL, true, 86400));
    const QJsonObject settings = settingsOf(request);

    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(-1009876543210LL));
    QCOMPARE(numberAt(settings, QStringLiteral("mute_for")), qint64(86400));
    QVERIFY(!boolAt(settings, QStringLiteral("use_default_mute_for")));
    QCOMPARE(int(settings.size()), 16);
}

QTEST_GUILESS_MAIN(ChatListActionsTest)
#include "tst_chat_list_actions.moc"
