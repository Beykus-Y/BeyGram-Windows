#include "telegram/NotificationRouter.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QtTest>

using namespace beygram::telegram;

namespace {

/// Calls routeNotification with the defaults a plain incoming message has:
/// nothing muted, nothing secret, no protected content, not outgoing, not the
/// current chat, no mention, app in the background, previews on and muted chats
/// silent. A test switches on only the flags it is about.
NotificationRoute routeFor(bool chatMuted = false, bool isSecret = false,
                           bool hasProtectedContent = false, bool isOutgoing = false,
                           bool isCurrentChat = false, bool isMention = false,
                           bool appForeground = false, bool previewEnabled = true,
                           bool notifyMutedChats = false)
{
    return routeNotification(chatMuted, isSecret, hasProtectedContent, isOutgoing,
                             isCurrentChat, isMention, appForeground, previewEnabled,
                             notifyMutedChats);
}

/// Human-readable name of one bit combination, used as the failure message of
/// the exhaustive matrix so a failing row can be reproduced directly.
QByteArray describe(int bits)
{
    QStringList enabled;
    const QStringList names = {
        QStringLiteral("muted"),       QStringLiteral("secret"),
        QStringLiteral("protected"),   QStringLiteral("outgoing"),
        QStringLiteral("current"),     QStringLiteral("mention"),
        QStringLiteral("foreground"),  QStringLiteral("preview"),
        QStringLiteral("mutedAllowed"),
    };
    for (int index = 0; index < names.size(); ++index) {
        if ((bits & (1 << index)) != 0)
            enabled.append(names.at(index));
    }
    return (enabled.isEmpty() ? QStringLiteral("plain") : enabled.join(QLatin1Char('+')))
        .toLatin1();
}

/// Asserts that a route announces nothing at all.
void verifySilent(const NotificationRoute& route)
{
    QVERIFY(!route.notify);
    QVERIFY(!route.playSound);
    QVERIFY(!route.showPreview);
    QVERIFY(!route.isMention);
}

} // namespace

class NotificationRouterTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultsAnnounceNothing();
    void plainChatNotifiesWithSoundAndPreview();
    void outgoingMessagesNeverNotify();
    void currentChatNeverNotifies();
    void secretChatsNeverNotify();
    void protectedContentNeverNotifies();
    void suppressionWinsOverMentionAndMutedNotifications();
    void foregroundStillNotifies();
    void mutedChatStaysQuiet();
    void mutedChatNotifiesSilentlyWhenAllowed();
    void mentionDoesNotBypassTheMutedGate();
    void mentionIsReportedWhenItNotifies();
    void previewFlagOnlyHidesTheContent();
    void everyCombinationMatchesTheRules();
};

void NotificationRouterTest::defaultsAnnounceNothing()
{
    const NotificationRoute route;
    QVERIFY(!route.notify);
    QVERIFY(route.playSound);
    QVERIFY(route.showPreview);
    QVERIFY(!route.isMention);
}

void NotificationRouterTest::plainChatNotifiesWithSoundAndPreview()
{
    const NotificationRoute route = routeFor();
    QVERIFY(route.notify);
    QVERIFY(route.playSound);
    QVERIFY(route.showPreview);
    QVERIFY(!route.isMention);
}

void NotificationRouterTest::outgoingMessagesNeverNotify()
{
    verifySilent(routeFor(false, false, false, true));
    // Even with previews on, muted notifications allowed and a mention.
    verifySilent(routeFor(false, false, false, true, false, true, true, true, true));
}

void NotificationRouterTest::currentChatNeverNotifies()
{
    verifySilent(routeFor(false, false, false, false, true));
    verifySilent(routeFor(false, false, false, false, true, true, true, true, true));
}

void NotificationRouterTest::secretChatsNeverNotify()
{
    verifySilent(routeFor(false, true));
    verifySilent(routeFor(false, true, false, false, false, true, true, true, true));
}

void NotificationRouterTest::protectedContentNeverNotifies()
{
    verifySilent(routeFor(false, false, true));
    verifySilent(routeFor(false, false, true, false, false, true, true, true, true));
}

void NotificationRouterTest::suppressionWinsOverMentionAndMutedNotifications()
{
    // A mention in a muted chat whose notifications are allowed is announced,
    // but the same message in the chat being read is not.
    QVERIFY(routeFor(true, false, false, false, false, true, false, true, true).notify);
    verifySilent(routeFor(true, false, false, false, true, true, false, true, true));
    // Secret and protected content also win over an allowed muted mention.
    verifySilent(routeFor(true, true, false, false, false, true, false, true, true));
    verifySilent(routeFor(true, false, true, false, false, true, false, true, true));
}

void NotificationRouterTest::foregroundStillNotifies()
{
    const NotificationRoute route = routeFor(false, false, false, false, false, false, true);
    QVERIFY(route.notify);
    QVERIFY(route.playSound);
    QVERIFY(route.showPreview);
}

void NotificationRouterTest::mutedChatStaysQuiet()
{
    verifySilent(routeFor(true));
    // Muted notifications are off, so even a mention is silent.
    verifySilent(routeFor(true, false, false, false, false, true));
    // Foreground does not change the mute decision.
    verifySilent(routeFor(true, false, false, false, false, false, true));
}

void NotificationRouterTest::mutedChatNotifiesSilentlyWhenAllowed()
{
    const NotificationRoute route =
        routeFor(true, false, false, false, false, false, false, true, true);
    QVERIFY(route.notify);
    QVERIFY(!route.playSound);
    QVERIFY(route.showPreview);
    QVERIFY(!route.isMention);
}

void NotificationRouterTest::mentionDoesNotBypassTheMutedGate()
{
    const NotificationRoute blocked = routeFor(true, false, false, false, false, true);
    verifySilent(blocked);

    const NotificationRoute allowed =
        routeFor(true, false, false, false, false, true, false, true, true);
    QVERIFY(allowed.notify);
    QVERIFY(allowed.isMention);
    QVERIFY(!allowed.playSound);
}

void NotificationRouterTest::mentionIsReportedWhenItNotifies()
{
    const NotificationRoute route = routeFor(false, false, false, false, false, true);
    QVERIFY(route.notify);
    QVERIFY(route.isMention);
    QVERIFY(route.playSound);
    QVERIFY(route.showPreview);
}

void NotificationRouterTest::previewFlagOnlyHidesTheContent()
{
    const NotificationRoute hidden = routeFor(false, false, false, false, false, false, false,
                                              false);
    QVERIFY(hidden.notify);
    QVERIFY(hidden.playSound);
    QVERIFY(!hidden.showPreview);

    const NotificationRoute shownWithMention =
        routeFor(false, false, false, false, false, true, true, true);
    QVERIFY(shownWithMention.notify);
    QVERIFY(shownWithMention.showPreview);
    QVERIFY(shownWithMention.isMention);
}

void NotificationRouterTest::everyCombinationMatchesTheRules()
{
    // All 512 combinations of the nine inputs, checked against the rules stated
    // independently of the implementation.
    for (int bits = 0; bits < (1 << 9); ++bits) {
        const bool chatMuted = (bits & (1 << 0)) != 0;
        const bool isSecret = (bits & (1 << 1)) != 0;
        const bool hasProtectedContent = (bits & (1 << 2)) != 0;
        const bool isOutgoing = (bits & (1 << 3)) != 0;
        const bool isCurrentChat = (bits & (1 << 4)) != 0;
        const bool isMention = (bits & (1 << 5)) != 0;
        const bool appForeground = (bits & (1 << 6)) != 0;
        const bool previewEnabled = (bits & (1 << 7)) != 0;
        const bool notifyMutedChats = (bits & (1 << 8)) != 0;

        const NotificationRoute route =
            routeFor(chatMuted, isSecret, hasProtectedContent, isOutgoing, isCurrentChat,
                     isMention, appForeground, previewEnabled, notifyMutedChats);

        const bool suppressed =
            isOutgoing || isCurrentChat || isSecret || hasProtectedContent;
        const bool expectedNotify = !suppressed && (!chatMuted || notifyMutedChats);

        const QByteArray label = describe(bits);
        QVERIFY2(route.notify == expectedNotify, label.constData());
        // A notification from a muted chat never plays a sound, and a route that
        // does not notify has no effect at all.
        QVERIFY2(route.playSound == (expectedNotify && !chatMuted), label.constData());
        QVERIFY2(route.showPreview == (expectedNotify && previewEnabled), label.constData());
        QVERIFY2(route.isMention == (expectedNotify && isMention), label.constData());
        // The window being focused never changes the outcome on its own.
        const NotificationRoute unfocused =
            routeFor(chatMuted, isSecret, hasProtectedContent, isOutgoing, isCurrentChat,
                     isMention, false, previewEnabled, notifyMutedChats);
        QVERIFY2(unfocused.notify == route.notify, label.constData());
        QVERIFY2(unfocused.playSound == route.playSound, label.constData());
        QVERIFY2(unfocused.showPreview == route.showPreview, label.constData());
        QVERIFY2(unfocused.isMention == route.isMention, label.constData());
    }
}

QTEST_GUILESS_MAIN(NotificationRouterTest)
#include "tst_notification_router.moc"
