#include "telegram/ChatBackgroundController.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class ChatBackgroundControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithDefaults();
    void setBackgroundStoresEveryKind();
    void setBackgroundClearsValueForNone();
    void setBackgroundRejectsBlankValueForOtherKinds();
    void setBackgroundEmitsOnlyOnRealChange();
    void setDimmerClampsToRange();
    void setBlurClampsToRange();
    void dimmerAndBlurEmitOnlyOnRealChange();
    void setForChatAcceptsGlobalAndNegativeIds();
    void setForChatKeepsTheLook();
    void setForChatEmitsOnlyOnRealChange();
    void isDefaultTracksTheLookOnly();
    void resetRestoresDefaults();
    void resetEmitsOnceAndOnlyWhenNeeded();
    void changedTracksOnlyRealChanges();
};

void ChatBackgroundControllerTest::startsWithDefaults()
{
    ChatBackgroundController controller;
    QSignalSpy spy(&controller, &ChatBackgroundController::changed);

    QCOMPARE(controller.kind(), BackgroundKind::None);
    QVERIFY(controller.value().isEmpty());
    QCOMPARE(controller.dimmer(), 0);
    QCOMPARE(controller.blur(), 0);
    QCOMPARE(controller.chatId(), qint64(0));
    QVERIFY(controller.isDefault());
    QCOMPARE(spy.count(), 0);
}

void ChatBackgroundControllerTest::setBackgroundStoresEveryKind()
{
    ChatBackgroundController controller;

    controller.setBackground(BackgroundKind::Solid, QStringLiteral("#1b2836"));
    QCOMPARE(controller.kind(), BackgroundKind::Solid);
    QCOMPARE(controller.value(), QStringLiteral("#1b2836"));

    controller.setBackground(BackgroundKind::Gradient, QStringLiteral("blue-900"));
    QCOMPARE(controller.kind(), BackgroundKind::Gradient);
    QCOMPARE(controller.value(), QStringLiteral("blue-900"));

    controller.setBackground(BackgroundKind::Pattern, QStringLiteral("dots"));
    QCOMPARE(controller.kind(), BackgroundKind::Pattern);
    QCOMPARE(controller.value(), QStringLiteral("dots"));

    controller.setBackground(BackgroundKind::Wallpaper, QStringLiteral("file:///bg.jpg"));
    QCOMPARE(controller.kind(), BackgroundKind::Wallpaper);
    QCOMPARE(controller.value(), QStringLiteral("file:///bg.jpg"));

    QVERIFY(!controller.isDefault());
}

void ChatBackgroundControllerTest::setBackgroundClearsValueForNone()
{
    ChatBackgroundController controller;
    controller.setBackground(BackgroundKind::Solid, QStringLiteral("#ffffff"));

    // Whatever the caller passes, None never keeps a value behind.
    controller.setBackground(BackgroundKind::None, QStringLiteral("#ffffff"));
    QCOMPARE(controller.kind(), BackgroundKind::None);
    QVERIFY(controller.value().isEmpty());
    QVERIFY(controller.isDefault());

    controller.setBackground(BackgroundKind::None, QString());
    QCOMPARE(controller.kind(), BackgroundKind::None);
    QVERIFY(controller.value().isEmpty());
}

void ChatBackgroundControllerTest::setBackgroundRejectsBlankValueForOtherKinds()
{
    ChatBackgroundController controller;
    controller.setBackground(BackgroundKind::Solid, QStringLiteral("#1b2836"));
    QSignalSpy spy(&controller, &ChatBackgroundController::changed);

    const BackgroundKind kinds[] = {
        BackgroundKind::Solid,
        BackgroundKind::Gradient,
        BackgroundKind::Pattern,
        BackgroundKind::Wallpaper,
    };

    for (const BackgroundKind kind : kinds) {
        controller.setBackground(kind, QString());
        // A kind without a value cannot be painted, so the old background stays.
        QCOMPARE(controller.kind(), BackgroundKind::Solid);
        QCOMPARE(controller.value(), QStringLiteral("#1b2836"));

        controller.setBackground(kind, QStringLiteral("   "));
        QCOMPARE(controller.kind(), BackgroundKind::Solid);
        QCOMPARE(controller.value(), QStringLiteral("#1b2836"));
    }

    QCOMPARE(spy.count(), 0);
}

void ChatBackgroundControllerTest::setBackgroundEmitsOnlyOnRealChange()
{
    ChatBackgroundController controller;
    QSignalSpy spy(&controller, &ChatBackgroundController::changed);

    // The initial state is already None with no value.
    controller.setBackground(BackgroundKind::None, QString());
    QCOMPARE(spy.count(), 0);
    controller.setBackground(BackgroundKind::None, QStringLiteral("ignored"));
    QCOMPARE(spy.count(), 0);

    controller.setBackground(BackgroundKind::Solid, QStringLiteral("#111111"));
    QCOMPARE(spy.count(), 1);

    controller.setBackground(BackgroundKind::Solid, QStringLiteral("#111111"));
    QCOMPARE(spy.count(), 1);

    // Same kind, different value is still a real change.
    controller.setBackground(BackgroundKind::Solid, QStringLiteral("#222222"));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(controller.value(), QStringLiteral("#222222"));

    controller.setBackground(BackgroundKind::Wallpaper, QStringLiteral("#222222"));
    QCOMPARE(spy.count(), 3);
    QCOMPARE(controller.kind(), BackgroundKind::Wallpaper);
}

void ChatBackgroundControllerTest::setDimmerClampsToRange()
{
    ChatBackgroundController controller;

    controller.setDimmer(-1);
    QCOMPARE(controller.dimmer(), 0);

    controller.setDimmer(0);
    QCOMPARE(controller.dimmer(), 0);

    controller.setDimmer(45);
    QCOMPARE(controller.dimmer(), 45);

    controller.setDimmer(100);
    QCOMPARE(controller.dimmer(), 100);

    controller.setDimmer(101);
    QCOMPARE(controller.dimmer(), 100);

    controller.setDimmer(1000000);
    QCOMPARE(controller.dimmer(), 100);
}

void ChatBackgroundControllerTest::setBlurClampsToRange()
{
    ChatBackgroundController controller;

    controller.setBlur(-42);
    QCOMPARE(controller.blur(), 0);

    controller.setBlur(0);
    QCOMPARE(controller.blur(), 0);

    controller.setBlur(15);
    QCOMPARE(controller.blur(), 15);

    controller.setBlur(100);
    QCOMPARE(controller.blur(), 100);

    controller.setBlur(101);
    QCOMPARE(controller.blur(), 100);

    controller.setBlur(9999);
    QCOMPARE(controller.blur(), 100);
}

void ChatBackgroundControllerTest::dimmerAndBlurEmitOnlyOnRealChange()
{
    ChatBackgroundController controller;
    QSignalSpy spy(&controller, &ChatBackgroundController::changed);

    controller.setDimmer(0);
    controller.setBlur(0);
    QCOMPARE(spy.count(), 0);

    controller.setDimmer(20); // 1
    controller.setDimmer(20); // no-op
    QCOMPARE(spy.count(), 1);

    // Clamping happens before the change check: -10 becomes 0, which differs
    // from the stored 20 and is therefore a real change.
    controller.setDimmer(-10); // 2
    QCOMPARE(spy.count(), 2);
    QCOMPARE(controller.dimmer(), 0);

    controller.setDimmer(150); // 3
    controller.setDimmer(200); // no-op, both clamp to 100
    QCOMPARE(spy.count(), 3);
    QCOMPARE(controller.dimmer(), 100);

    controller.setBlur(30); // 4
    controller.setBlur(30); // no-op
    QCOMPARE(spy.count(), 4);
    QCOMPARE(controller.blur(), 30);
}

void ChatBackgroundControllerTest::setForChatAcceptsGlobalAndNegativeIds()
{
    ChatBackgroundController controller;

    controller.setForChat(0);
    QCOMPARE(controller.chatId(), qint64(0));

    controller.setForChat(-1001234567890LL);
    QCOMPARE(controller.chatId(), qint64(-1001234567890LL));

    controller.setForChat(777);
    QCOMPARE(controller.chatId(), qint64(777));

    controller.setForChat(-1);
    QCOMPARE(controller.chatId(), qint64(-1));

    // Back to the global scope.
    controller.setForChat(0);
    QCOMPARE(controller.chatId(), qint64(0));
}

void ChatBackgroundControllerTest::setForChatKeepsTheLook()
{
    ChatBackgroundController controller;
    controller.setBackground(BackgroundKind::Gradient, QStringLiteral("sunset"));
    controller.setDimmer(35);
    controller.setBlur(10);

    controller.setForChat(-100200300);

    // Switching scope must not silently discard the chosen background.
    QCOMPARE(controller.kind(), BackgroundKind::Gradient);
    QCOMPARE(controller.value(), QStringLiteral("sunset"));
    QCOMPARE(controller.dimmer(), 35);
    QCOMPARE(controller.blur(), 10);
}

void ChatBackgroundControllerTest::setForChatEmitsOnlyOnRealChange()
{
    ChatBackgroundController controller;
    QSignalSpy spy(&controller, &ChatBackgroundController::changed);

    controller.setForChat(0);
    QCOMPARE(spy.count(), 0);

    controller.setForChat(42); // 1
    controller.setForChat(42); // no-op
    QCOMPARE(spy.count(), 1);

    controller.setForChat(-42); // 2, negative ids are distinct chats
    QCOMPARE(spy.count(), 2);

    controller.setForChat(0); // 3
    QCOMPARE(spy.count(), 3);
    QCOMPARE(controller.chatId(), qint64(0));
}

void ChatBackgroundControllerTest::isDefaultTracksTheLookOnly()
{
    ChatBackgroundController controller;
    QVERIFY(controller.isDefault());

    // The chat scope is not part of the look.
    controller.setForChat(-100200300);
    QVERIFY(controller.isDefault());

    controller.setDimmer(1);
    QVERIFY(!controller.isDefault());
    controller.setDimmer(0);
    QVERIFY(controller.isDefault());

    controller.setBlur(1);
    QVERIFY(!controller.isDefault());
    controller.setBlur(0);
    QVERIFY(controller.isDefault());

    controller.setBackground(BackgroundKind::Pattern, QStringLiteral("grid"));
    QVERIFY(!controller.isDefault());

    controller.setBackground(BackgroundKind::None, QString());
    QVERIFY(controller.isDefault());
}

void ChatBackgroundControllerTest::resetRestoresDefaults()
{
    ChatBackgroundController controller;
    controller.setBackground(BackgroundKind::Wallpaper, QStringLiteral("file:///bg.jpg"));
    controller.setDimmer(80);
    controller.setBlur(60);
    controller.setForChat(-100200300);

    controller.reset();

    QCOMPARE(controller.kind(), BackgroundKind::None);
    QVERIFY(controller.value().isEmpty());
    QCOMPARE(controller.dimmer(), 0);
    QCOMPARE(controller.blur(), 0);
    QCOMPARE(controller.chatId(), qint64(0));
    QVERIFY(controller.isDefault());
}

void ChatBackgroundControllerTest::resetEmitsOnceAndOnlyWhenNeeded()
{
    ChatBackgroundController controller;
    QSignalSpy spy(&controller, &ChatBackgroundController::changed);

    controller.reset();
    QCOMPARE(spy.count(), 0);

    // Only the scope moved, but reset() still has to report it.
    controller.setForChat(7);
    spy.clear();
    controller.reset();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.chatId(), qint64(0));

    controller.setBackground(BackgroundKind::Solid, QStringLiteral("#000000"));
    controller.setDimmer(50);
    controller.setBlur(50);
    controller.setForChat(9);
    spy.clear();

    // Several fields move, but it is one action and one signal.
    controller.reset();
    QCOMPARE(spy.count(), 1);

    controller.reset();
    QCOMPARE(spy.count(), 1);
}

void ChatBackgroundControllerTest::changedTracksOnlyRealChanges()
{
    ChatBackgroundController controller;
    QSignalSpy spy(&controller, &ChatBackgroundController::changed);

    controller.setBackground(BackgroundKind::Solid, QStringLiteral("#102030")); // 1
    controller.setBackground(BackgroundKind::Solid, QStringLiteral("#102030")); // no-op
    controller.setBackground(BackgroundKind::Solid, QString());                 // rejected
    controller.setBackground(BackgroundKind::Gradient, QStringLiteral("dusk")); // 2
    controller.setDimmer(50);                                                   // 3
    controller.setDimmer(50);                                                   // no-op
    controller.setDimmer(500);                                                  // 4, clamps to 100
    controller.setBlur(25);                                                     // 5
    controller.setBlur(-5);                                                     // 6, clamps to 0
    controller.setForChat(-100200300);                                          // 7
    controller.setForChat(-100200300);                                          // no-op
    controller.reset();                                                         // 8
    controller.reset();                                                         // no-op

    QCOMPARE(spy.count(), 8);
    QCOMPARE(controller.kind(), BackgroundKind::None);
    QVERIFY(controller.value().isEmpty());
    QCOMPARE(controller.dimmer(), 0);
    QCOMPARE(controller.blur(), 0);
    QCOMPARE(controller.chatId(), qint64(0));
    QVERIFY(controller.isDefault());
}

QTEST_GUILESS_MAIN(ChatBackgroundControllerTest)
#include "tst_chat_background_controller.moc"
