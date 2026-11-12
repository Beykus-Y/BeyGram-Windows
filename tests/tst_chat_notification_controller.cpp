#include "telegram/ChatNotificationController.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

// TDLib's "muted forever" duration: the int32 maximum of mute_for.
constexpr qint64 kMuteForever = 2147483647LL;

QJsonObject settingsOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("notification_settings")).toObject();
}

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

} // namespace

class ChatNotificationControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithoutChat();
    void loadsChatAsCleanBaseline();
    void reloadDiscardsPendingEdit();
    void switchesChatCleanly();
    void editingFlagMarksDirty();
    void revertingEditClearsDirty();
    void markSavedAcceptsCurrentFlags();
    void markSavedIsSilentWhenClean();
    void markSavedThenEditIsDirtyAgain();
    void requestIsEmptyWithoutChat();
    void requestCarriesMuteSoundPreview();
    void requestWritesZeroMuteWhenUnmuted();
    void requestWritesDefaultRingtoneForSound();
    void requestShapeAndChatId();
    void preservesNegativeChatId();
    void settersIgnoredWithoutChat();
    void signalsOnlyOnRealChange();
    void requestSurvivesSerialization();
};

void ChatNotificationControllerTest::startsWithoutChat()
{
    const ChatNotificationController controller;

    QCOMPARE(controller.chatId(), qint64(0));
    QVERIFY(!controller.muted());
    QVERIFY(controller.sound());
    QVERIFY(controller.preview());
    QVERIFY(!controller.dirty());
    QVERIFY(controller.request().isEmpty());
}

void ChatNotificationControllerTest::loadsChatAsCleanBaseline()
{
    ChatNotificationController controller;

    // Non-default flags are still a clean state: they are what TDLib reported.
    controller.setChat(42, true, false, false);

    QCOMPARE(controller.chatId(), qint64(42));
    QVERIFY(controller.muted());
    QVERIFY(!controller.sound());
    QVERIFY(!controller.preview());
    QVERIFY(!controller.dirty());
    QVERIFY(controller.request().isEmpty());
}

void ChatNotificationControllerTest::reloadDiscardsPendingEdit()
{
    ChatNotificationController controller;
    controller.setChat(42, false, true, true);
    controller.setMuted(true);
    QVERIFY(controller.dirty());

    // Reloading the same chat takes the incoming flags as the saved ones.
    controller.setChat(42, false, true, true);

    QVERIFY(!controller.dirty());
    QVERIFY(!controller.muted());
    QVERIFY(controller.request().isEmpty());
}

void ChatNotificationControllerTest::switchesChatCleanly()
{
    ChatNotificationController controller;
    controller.setChat(42, false, true, true);
    controller.setPreview(false);
    QVERIFY(controller.dirty());

    controller.setChat(43, true, true, true);

    QCOMPARE(controller.chatId(), qint64(43));
    QVERIFY(controller.muted());
    QVERIFY(controller.sound());
    QVERIFY(controller.preview());
    QVERIFY(!controller.dirty());
    QVERIFY(controller.request().isEmpty());
}

void ChatNotificationControllerTest::editingFlagMarksDirty()
{
    ChatNotificationController controller;
    controller.setChat(42, false, true, true);

    controller.setMuted(true);
    QVERIFY(controller.dirty());
    QVERIFY(controller.muted());

    controller.setSound(false);
    QVERIFY(controller.dirty());
    QVERIFY(!controller.sound());

    controller.setPreview(false);
    QVERIFY(controller.dirty());
    QVERIFY(!controller.preview());

    QCOMPARE(controller.request().value(QStringLiteral("@type")).toString(),
             QStringLiteral("setChatNotificationSettings"));
    QCOMPARE(numberAt(controller.request(), QStringLiteral("chat_id")), qint64(42));
}

void ChatNotificationControllerTest::revertingEditClearsDirty()
{
    ChatNotificationController controller;
    controller.setChat(42, false, true, true);

    controller.setMuted(true);
    controller.setMuted(false);
    QVERIFY(!controller.dirty());

    controller.setSound(false);
    controller.setSound(true);
    QVERIFY(!controller.dirty());

    controller.setPreview(false);
    controller.setPreview(true);
    QVERIFY(!controller.dirty());
    QVERIFY(controller.request().isEmpty());
}

void ChatNotificationControllerTest::markSavedAcceptsCurrentFlags()
{
    ChatNotificationController controller;
    controller.setChat(42, false, true, true);
    controller.setMuted(true);
    controller.setSound(false);

    controller.markSaved();

    QVERIFY(!controller.dirty());
    QVERIFY(controller.request().isEmpty());

    // Saving changes what counts as saved, not what the screen shows.
    QCOMPARE(controller.chatId(), qint64(42));
    QVERIFY(controller.muted());
    QVERIFY(!controller.sound());
    QVERIFY(controller.preview());
}

void ChatNotificationControllerTest::markSavedIsSilentWhenClean()
{
    ChatNotificationController controller;
    controller.setChat(42, false, true, true);
    QSignalSpy spy(&controller, &ChatNotificationController::changed);

    controller.markSaved();

    QCOMPARE(spy.count(), 0);
    QVERIFY(!controller.dirty());
}

void ChatNotificationControllerTest::markSavedThenEditIsDirtyAgain()
{
    ChatNotificationController controller;
    controller.setChat(42, false, true, true);
    controller.setMuted(true);
    controller.markSaved();
    QVERIFY(!controller.dirty());

    // The saved mute is now true, so turning it off is a real change.
    controller.setMuted(false);
    QVERIFY(controller.dirty());

    controller.markSaved();
    QVERIFY(!controller.dirty());
}

void ChatNotificationControllerTest::requestIsEmptyWithoutChat()
{
    ChatNotificationController controller;

    // Chat id 0 is "no chat": the flags may be stored, but nothing is saved.
    controller.setChat(0, true, false, false);

    QCOMPARE(controller.chatId(), qint64(0));
    QVERIFY(controller.muted());
    QVERIFY(!controller.sound());
    QVERIFY(!controller.preview());
    QVERIFY(!controller.dirty());
    QVERIFY(controller.request().isEmpty());
}

void ChatNotificationControllerTest::requestCarriesMuteSoundPreview()
{
    ChatNotificationController controller;
    controller.setChat(42, false, true, true);
    controller.setMuted(true);
    controller.setSound(false);
    controller.setPreview(false);

    const QJsonObject settings = settingsOf(serialized(controller.request()));

    QVERIFY(!settings.value(QStringLiteral("use_default_mute_for")).toBool());
    QCOMPARE(numberAt(settings, QStringLiteral("mute_for")), kMuteForever);
    QVERIFY(!settings.value(QStringLiteral("use_default_sound")).toBool());
    QCOMPARE(numberAt(settings, QStringLiteral("sound_id")), qint64(0));
    QVERIFY(!settings.value(QStringLiteral("use_default_show_preview")).toBool());
    QVERIFY(!settings.value(QStringLiteral("show_preview")).toBool());
}

void ChatNotificationControllerTest::requestWritesZeroMuteWhenUnmuted()
{
    ChatNotificationController controller;
    controller.setChat(42, false, true, true);

    // Dirty through the preview alone: the mute must still be written as
    // unmuted rather than left out.
    controller.setPreview(false);

    const QJsonObject settings = settingsOf(serialized(controller.request()));

    QVERIFY(!settings.value(QStringLiteral("use_default_mute_for")).toBool());
    QCOMPARE(numberAt(settings, QStringLiteral("mute_for")), qint64(0));
}

void ChatNotificationControllerTest::requestWritesDefaultRingtoneForSound()
{
    ChatNotificationController controller;
    controller.setChat(42, false, false, true);

    controller.setSound(true);

    const QJsonObject settings = settingsOf(serialized(controller.request()));

    QVERIFY(settings.value(QStringLiteral("use_default_sound")).toBool());
    QCOMPARE(numberAt(settings, QStringLiteral("sound_id")), qint64(-1));
}

void ChatNotificationControllerTest::requestShapeAndChatId()
{
    ChatNotificationController controller;
    controller.setChat(42, false, true, true);
    controller.setMuted(true);

    const QJsonObject request = serialized(controller.request());

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setChatNotificationSettings"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(42));
    QVERIFY(request.value(QStringLiteral("notification_settings")).isObject());
    QCOMPARE(int(request.size()), 3);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void ChatNotificationControllerTest::preservesNegativeChatId()
{
    // Supergroup and channel ids are negative and must not be truncated.
    const qint64 chatId = -1001234567890LL;

    ChatNotificationController controller;
    controller.setChat(chatId, false, true, true);
    controller.setMuted(true);

    const QJsonObject request = serialized(controller.request());

    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), chatId);
}

void ChatNotificationControllerTest::settersIgnoredWithoutChat()
{
    ChatNotificationController controller;
    QSignalSpy spy(&controller, &ChatNotificationController::changed);

    controller.setMuted(true);
    controller.setSound(false);
    controller.setPreview(false);

    QVERIFY(!controller.muted());
    QVERIFY(controller.sound());
    QVERIFY(controller.preview());
    QCOMPARE(spy.count(), 0);
    QVERIFY(!controller.dirty());
    QVERIFY(controller.request().isEmpty());
}

void ChatNotificationControllerTest::signalsOnlyOnRealChange()
{
    ChatNotificationController controller;
    QSignalSpy spy(&controller, &ChatNotificationController::changed);

    controller.setChat(42, false, true, true);
    QCOMPARE(spy.count(), 1);

    controller.setChat(42, false, true, true);
    QCOMPARE(spy.count(), 1);

    controller.setMuted(true);
    QCOMPARE(spy.count(), 2);

    controller.setMuted(true);
    QCOMPARE(spy.count(), 2);

    controller.setSound(false);
    QCOMPARE(spy.count(), 3);

    controller.setSound(false);
    QCOMPARE(spy.count(), 3);

    controller.setPreview(false);
    QCOMPARE(spy.count(), 4);

    controller.setPreview(false);
    QCOMPARE(spy.count(), 4);

    // The value changed even though the chat stays dirty.
    controller.setMuted(false);
    QCOMPARE(spy.count(), 5);

    // markSaved clears dirty, which the save button has to see.
    controller.markSaved();
    QCOMPARE(spy.count(), 6);

    controller.markSaved();
    QCOMPARE(spy.count(), 6);

    controller.setChat(43, true, false, false);
    QCOMPARE(spy.count(), 7);

    controller.setChat(43, true, false, false);
    QCOMPARE(spy.count(), 7);
}

void ChatNotificationControllerTest::requestSurvivesSerialization()
{
    const qint64 chatId = -1001234567890LL;

    ChatNotificationController controller;
    controller.setChat(chatId, false, true, true);
    controller.setMuted(true);
    controller.setPreview(false);

    const QJsonObject request = serialized(controller.request());

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setChatNotificationSettings"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), chatId);

    const QJsonObject settings = settingsOf(request);
    QCOMPARE(numberAt(settings, QStringLiteral("mute_for")), kMuteForever);
    QVERIFY(!settings.value(QStringLiteral("show_preview")).toBool());
    QVERIFY(settings.value(QStringLiteral("use_default_sound")).toBool());
}

QTEST_GUILESS_MAIN(ChatNotificationControllerTest)
#include "tst_chat_notification_controller.moc"
