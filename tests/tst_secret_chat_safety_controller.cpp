#include "telegram/SecretChatSafetyController.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class SecretChatSafetyControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithoutChat();
    void loadsSecretChat();
    void loadsRegularChat();
    void keepsNegativeChatId();
    void zeroChatIdClears();
    void clearRestoresDefaults();
    void clearIsSilentWhenAlreadyDefault();
    void switchingChatOnlyChangesIdentity();
    void reloadingSameChatIsSilent();
    void ttlClampsToSupportedRange();
    void ttlAcceptsSupportedBounds();
    void screenshotFlagRoundTrips();
    void regularChatAlwaysAllowsNotificationsAndCopy();
    void secretChatBlocksNotificationsAndCopy();
    void warnsOnlyForSecretChatWithScreenshotsAllowed();
    void changedOnlyOnRealChanges();
};

void SecretChatSafetyControllerTest::startsWithoutChat()
{
    const SecretChatSafetyController controller;

    QCOMPARE(controller.chatId(), qint64(0));
    QVERIFY(!controller.secret());
    QCOMPARE(controller.ttlSeconds(), 0);
    QVERIFY(controller.screenshotAllowed());
    QVERIFY(!controller.shouldWarnBeforeScreenshot());
    QVERIFY(controller.notificationsAllowed());
    QVERIFY(controller.copyAllowed());
}

void SecretChatSafetyControllerTest::loadsSecretChat()
{
    SecretChatSafetyController controller;

    controller.setChat(42, true);

    QCOMPARE(controller.chatId(), qint64(42));
    QVERIFY(controller.secret());
    QCOMPARE(controller.ttlSeconds(), 0);
    QVERIFY(controller.screenshotAllowed());
    // Screenshots are still allowed, so the other side would be told about one.
    QVERIFY(controller.shouldWarnBeforeScreenshot());
    QVERIFY(!controller.notificationsAllowed());
    QVERIFY(!controller.copyAllowed());
}

void SecretChatSafetyControllerTest::loadsRegularChat()
{
    SecretChatSafetyController controller;

    controller.setChat(42, false);

    QCOMPARE(controller.chatId(), qint64(42));
    QVERIFY(!controller.secret());
    QVERIFY(!controller.shouldWarnBeforeScreenshot());
    QVERIFY(controller.notificationsAllowed());
    QVERIFY(controller.copyAllowed());
}

void SecretChatSafetyControllerTest::keepsNegativeChatId()
{
    // Supergroup and channel ids are negative and must not be truncated.
    const qint64 chatId = -1001234567890LL;

    SecretChatSafetyController controller;
    controller.setChat(chatId, false);

    QCOMPARE(controller.chatId(), chatId);
}

void SecretChatSafetyControllerTest::zeroChatIdClears()
{
    SecretChatSafetyController controller;
    controller.setChat(42, true);
    controller.setTtlSeconds(3600);
    controller.setScreenshotAllowed(false);

    QSignalSpy spy(&controller, &SecretChatSafetyController::changed);
    controller.setChat(0, true);

    // Unloading is one action, so it announces itself once.
    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.chatId(), qint64(0));
    QVERIFY(!controller.secret());
    QCOMPARE(controller.ttlSeconds(), 0);
    QVERIFY(controller.screenshotAllowed());
    QVERIFY(!controller.shouldWarnBeforeScreenshot());
    QVERIFY(controller.notificationsAllowed());
    QVERIFY(controller.copyAllowed());
}

void SecretChatSafetyControllerTest::clearRestoresDefaults()
{
    SecretChatSafetyController controller;
    controller.setChat(42, true);
    controller.setTtlSeconds(3600);
    controller.setScreenshotAllowed(false);

    QSignalSpy spy(&controller, &SecretChatSafetyController::changed);
    controller.clear();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.chatId(), qint64(0));
    QVERIFY(!controller.secret());
    QCOMPARE(controller.ttlSeconds(), 0);
    QVERIFY(controller.screenshotAllowed());
    QVERIFY(!controller.shouldWarnBeforeScreenshot());
    QVERIFY(controller.notificationsAllowed());
    QVERIFY(controller.copyAllowed());
}

void SecretChatSafetyControllerTest::clearIsSilentWhenAlreadyDefault()
{
    SecretChatSafetyController controller;
    QSignalSpy spy(&controller, &SecretChatSafetyController::changed);

    controller.clear();
    controller.setChat(0, true);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.chatId(), qint64(0));
    QVERIFY(controller.screenshotAllowed());
}

void SecretChatSafetyControllerTest::switchingChatOnlyChangesIdentity()
{
    SecretChatSafetyController controller;
    controller.setChat(42, true);
    controller.setTtlSeconds(3600);
    controller.setScreenshotAllowed(false);

    controller.setChat(43, false);

    // The caller reads the new chat's TTL and screenshot flag from TDLib, so
    // setChat() itself must not invent them.
    QCOMPARE(controller.chatId(), qint64(43));
    QVERIFY(!controller.secret());
    QCOMPARE(controller.ttlSeconds(), 3600);
    QVERIFY(!controller.screenshotAllowed());
    // The secret-chat rules follow the new chat, not the stored flags.
    QVERIFY(controller.notificationsAllowed());
    QVERIFY(controller.copyAllowed());
    QVERIFY(!controller.shouldWarnBeforeScreenshot());
}

void SecretChatSafetyControllerTest::reloadingSameChatIsSilent()
{
    SecretChatSafetyController controller;
    controller.setChat(42, true);

    QSignalSpy spy(&controller, &SecretChatSafetyController::changed);
    controller.setChat(42, true);
    QCOMPARE(spy.count(), 0);

    // The secret flag is part of the identity, so flipping it is a real change.
    controller.setChat(42, false);
    QCOMPARE(spy.count(), 1);
    QVERIFY(!controller.secret());
}

void SecretChatSafetyControllerTest::ttlClampsToSupportedRange()
{
    SecretChatSafetyController controller;
    controller.setChat(42, true);

    controller.setTtlSeconds(-5);
    QCOMPARE(controller.ttlSeconds(), 0);

    controller.setTtlSeconds(604801);
    QCOMPARE(controller.ttlSeconds(), 604800);

    controller.setTtlSeconds(999999999);
    QCOMPARE(controller.ttlSeconds(), 604800);
}

void SecretChatSafetyControllerTest::ttlAcceptsSupportedBounds()
{
    SecretChatSafetyController controller;
    controller.setChat(42, true);

    controller.setTtlSeconds(604800);
    QCOMPARE(controller.ttlSeconds(), 604800);

    controller.setTtlSeconds(86400);
    QCOMPARE(controller.ttlSeconds(), 86400);

    controller.setTtlSeconds(0);
    QCOMPARE(controller.ttlSeconds(), 0);
}

void SecretChatSafetyControllerTest::screenshotFlagRoundTrips()
{
    SecretChatSafetyController controller;
    controller.setChat(42, true);

    controller.setScreenshotAllowed(false);
    QVERIFY(!controller.screenshotAllowed());
    // Nothing can be taken, so there is nothing to warn about.
    QVERIFY(!controller.shouldWarnBeforeScreenshot());

    controller.setScreenshotAllowed(true);
    QVERIFY(controller.screenshotAllowed());
    QVERIFY(controller.shouldWarnBeforeScreenshot());
}

void SecretChatSafetyControllerTest::regularChatAlwaysAllowsNotificationsAndCopy()
{
    SecretChatSafetyController controller;
    controller.setChat(7, false);

    // The stored flags describe secret-chat protection; a regular chat ignores
    // them and keeps notifying and copying.
    controller.setScreenshotAllowed(false);
    controller.setTtlSeconds(604800);

    QVERIFY(controller.notificationsAllowed());
    QVERIFY(controller.copyAllowed());
    QVERIFY(!controller.shouldWarnBeforeScreenshot());
}

void SecretChatSafetyControllerTest::secretChatBlocksNotificationsAndCopy()
{
    SecretChatSafetyController controller;
    controller.setChat(7, true);
    controller.setScreenshotAllowed(true);

    QVERIFY(!controller.notificationsAllowed());
    QVERIFY(!controller.copyAllowed());
}

void SecretChatSafetyControllerTest::warnsOnlyForSecretChatWithScreenshotsAllowed()
{
    struct Case {
        bool secret;
        bool screenshotsAllowed;
        bool warn;
    };

    const Case cases[] = {
        {true, true, true},
        {true, false, false},
        {false, true, false},
        {false, false, false},
    };

    SecretChatSafetyController controller;
    for (const Case& c : cases) {
        controller.setChat(42, c.secret);
        controller.setScreenshotAllowed(c.screenshotsAllowed);

        QCOMPARE(controller.shouldWarnBeforeScreenshot(), c.warn);
    }
}

void SecretChatSafetyControllerTest::changedOnlyOnRealChanges()
{
    SecretChatSafetyController controller;
    QSignalSpy spy(&controller, &SecretChatSafetyController::changed);

    controller.setChat(42, true);
    QCOMPARE(spy.count(), 1);

    controller.setChat(42, true);
    QCOMPARE(spy.count(), 1);

    controller.setChat(43, true);
    QCOMPARE(spy.count(), 2);

    controller.setChat(43, false);
    QCOMPARE(spy.count(), 3);

    controller.setScreenshotAllowed(false);
    QCOMPARE(spy.count(), 4);

    controller.setScreenshotAllowed(false);
    QCOMPARE(spy.count(), 4);

    controller.setTtlSeconds(3600);
    QCOMPARE(spy.count(), 5);

    controller.setTtlSeconds(3600);
    QCOMPARE(spy.count(), 5);

    controller.setTtlSeconds(7200);
    QCOMPARE(spy.count(), 6);

    // Clamping to the maximum is still a real change.
    controller.setTtlSeconds(604801);
    QCOMPARE(spy.count(), 7);

    controller.setTtlSeconds(604800);
    QCOMPARE(spy.count(), 7);

    controller.clear();
    QCOMPARE(spy.count(), 8);

    controller.clear();
    QCOMPARE(spy.count(), 8);
}

QTEST_GUILESS_MAIN(SecretChatSafetyControllerTest)
#include "tst_secret_chat_safety_controller.moc"
