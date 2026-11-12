#include "telegram/NotificationBadgeController.h"

#include <QSignalSpy>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

constexpr int kIntMax = std::numeric_limits<int>::max();

} // namespace

class NotificationBadgeControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setChatUnreadTracksOneChat();
    void ignoresZeroChatId();
    void clampsNegativeUnread();
    void keepsZeroUnreadChat();
    void remembersMutedStateAcrossZero();
    void setChatUnreadIsNoOpForSameValue();
    void mutedFlagChangeEmitsChanged();
    void removeChatDropsEntry();
    void removeUnknownChatIsNoOp();
    void totalsSumTrackedChats();
    void unmutedUnreadExcludesMutedChats();
    void badgeTextIsEmptyAtZero();
    void badgeTextShowsExactNumber();
    void badgeTextCapsAboveMax();
    void badgeTextClampsNonPositiveMax();
    void badgeTextUsesDefaultMax();
    void hasBadgeTracksTotal();
    void clearForgetsEveryChat();
    void clearOnEmptyIsNoOp();
    void emitsChangedOncePerRealTransition();
    void totalSaturatesInsteadOfWrapping();
};

void NotificationBadgeControllerTest::startsEmpty()
{
    const NotificationBadgeController controller;

    QCOMPARE(controller.totalUnread(), 0);
    QCOMPARE(controller.unmutedUnread(), 0);
    QCOMPARE(controller.chatCount(), 0);
    QCOMPARE(controller.badgeText(), QString());
    QVERIFY(!controller.hasBadge());
}

void NotificationBadgeControllerTest::setChatUnreadTracksOneChat()
{
    NotificationBadgeController controller;

    controller.setChatUnread(7, 3, false);

    QCOMPARE(controller.totalUnread(), 3);
    QCOMPARE(controller.unmutedUnread(), 3);
    QCOMPARE(controller.chatCount(), 1);
    QCOMPARE(controller.badgeText(), QStringLiteral("3"));
    QVERIFY(controller.hasBadge());
}

void NotificationBadgeControllerTest::ignoresZeroChatId()
{
    NotificationBadgeController controller;
    QSignalSpy spy(&controller, &NotificationBadgeController::changed);

    controller.setChatUnread(0, 5, false);

    QCOMPARE(controller.chatCount(), 0);
    QCOMPARE(controller.totalUnread(), 0);
    QCOMPARE(spy.count(), 0);
}

void NotificationBadgeControllerTest::clampsNegativeUnread()
{
    NotificationBadgeController controller;

    controller.setChatUnread(1, -4, false);

    QCOMPARE(controller.totalUnread(), 0);
    QCOMPARE(controller.chatCount(), 1);
    QVERIFY(!controller.hasBadge());

    // The stored value is the clamped zero, so setting zero again is a no-op.
    QSignalSpy spy(&controller, &NotificationBadgeController::changed);
    controller.setChatUnread(1, 0, false);
    QCOMPARE(spy.count(), 0);
}

void NotificationBadgeControllerTest::keepsZeroUnreadChat()
{
    NotificationBadgeController controller;

    controller.setChatUnread(1, 4, false);
    controller.setChatUnread(1, 0, false);

    QCOMPARE(controller.chatCount(), 1);
    QCOMPARE(controller.totalUnread(), 0);
    QCOMPARE(controller.badgeText(), QString());
}

void NotificationBadgeControllerTest::remembersMutedStateAcrossZero()
{
    NotificationBadgeController controller;

    controller.setChatUnread(1, 4, true);
    controller.setChatUnread(1, 0, true);
    controller.setChatUnread(1, 2, true);

    QCOMPARE(controller.totalUnread(), 2);
    QCOMPARE(controller.unmutedUnread(), 0);
    QCOMPARE(controller.chatCount(), 1);
}

void NotificationBadgeControllerTest::setChatUnreadIsNoOpForSameValue()
{
    NotificationBadgeController controller;
    controller.setChatUnread(1, 2, false);

    QSignalSpy spy(&controller, &NotificationBadgeController::changed);

    controller.setChatUnread(1, 2, false);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.totalUnread(), 2);
}

void NotificationBadgeControllerTest::mutedFlagChangeEmitsChanged()
{
    NotificationBadgeController controller;
    controller.setChatUnread(1, 2, false);

    QSignalSpy spy(&controller, &NotificationBadgeController::changed);

    controller.setChatUnread(1, 2, true);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.totalUnread(), 2);
    QCOMPARE(controller.unmutedUnread(), 0);
}

void NotificationBadgeControllerTest::removeChatDropsEntry()
{
    NotificationBadgeController controller;
    controller.setChatUnread(1, 2, false);
    controller.setChatUnread(2, 3, false);

    controller.removeChat(1);

    QCOMPARE(controller.chatCount(), 1);
    QCOMPARE(controller.totalUnread(), 3);
}

void NotificationBadgeControllerTest::removeUnknownChatIsNoOp()
{
    NotificationBadgeController controller;
    controller.setChatUnread(1, 2, false);

    QSignalSpy spy(&controller, &NotificationBadgeController::changed);

    controller.removeChat(99);
    controller.removeChat(0);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.chatCount(), 1);
    QCOMPARE(controller.totalUnread(), 2);
}

void NotificationBadgeControllerTest::totalsSumTrackedChats()
{
    NotificationBadgeController controller;

    controller.setChatUnread(1, 2, false);
    controller.setChatUnread(2, 3, true);
    controller.setChatUnread(3, 5, false);

    QCOMPARE(controller.totalUnread(), 10);
    QCOMPARE(controller.chatCount(), 3);
}

void NotificationBadgeControllerTest::unmutedUnreadExcludesMutedChats()
{
    NotificationBadgeController controller;

    controller.setChatUnread(1, 2, false);
    controller.setChatUnread(2, 3, true);
    controller.setChatUnread(3, 5, false);

    QCOMPARE(controller.unmutedUnread(), 7);
}

void NotificationBadgeControllerTest::badgeTextIsEmptyAtZero()
{
    const NotificationBadgeController controller;

    QCOMPARE(controller.badgeText(), QString());
    QCOMPARE(controller.badgeText(0), QString());
    QCOMPARE(controller.badgeText(5), QString());
}

void NotificationBadgeControllerTest::badgeTextShowsExactNumber()
{
    NotificationBadgeController controller;
    controller.setChatUnread(1, 4, false);

    QCOMPARE(controller.badgeText(99), QStringLiteral("4"));
    QCOMPARE(controller.badgeText(4), QStringLiteral("4"));
}

void NotificationBadgeControllerTest::badgeTextCapsAboveMax()
{
    NotificationBadgeController controller;
    controller.setChatUnread(1, 150, false);

    QCOMPARE(controller.badgeText(99), QStringLiteral("99+"));
    QCOMPARE(controller.badgeText(9), QStringLiteral("9+"));

    controller.setChatUnread(1, 1, false);
    QCOMPARE(controller.badgeText(99), QStringLiteral("1"));
}

void NotificationBadgeControllerTest::badgeTextClampsNonPositiveMax()
{
    NotificationBadgeController controller;
    controller.setChatUnread(1, 3, false);

    QCOMPARE(controller.badgeText(0), QStringLiteral("1+"));
    QCOMPARE(controller.badgeText(-5), QStringLiteral("1+"));

    controller.setChatUnread(1, 1, false);
    QCOMPARE(controller.badgeText(0), QStringLiteral("1"));
}

void NotificationBadgeControllerTest::badgeTextUsesDefaultMax()
{
    NotificationBadgeController controller;
    controller.setChatUnread(1, 99, false);
    QCOMPARE(controller.badgeText(), QStringLiteral("99"));

    controller.setChatUnread(1, 100, false);
    QCOMPARE(controller.badgeText(), QStringLiteral("99+"));
}

void NotificationBadgeControllerTest::hasBadgeTracksTotal()
{
    NotificationBadgeController controller;

    controller.setChatUnread(1, 2, true);
    QVERIFY(controller.hasBadge());

    controller.setChatUnread(1, 0, true);
    QVERIFY(!controller.hasBadge());
}

void NotificationBadgeControllerTest::clearForgetsEveryChat()
{
    NotificationBadgeController controller;
    controller.setChatUnread(1, 2, false);
    controller.setChatUnread(2, 0, true);

    controller.clear();

    QCOMPARE(controller.chatCount(), 0);
    QCOMPARE(controller.totalUnread(), 0);
    QCOMPARE(controller.unmutedUnread(), 0);
    QCOMPARE(controller.badgeText(), QString());
    QVERIFY(!controller.hasBadge());
}

void NotificationBadgeControllerTest::clearOnEmptyIsNoOp()
{
    NotificationBadgeController controller;
    QSignalSpy spy(&controller, &NotificationBadgeController::changed);

    controller.clear();

    QCOMPARE(spy.count(), 0);
}

void NotificationBadgeControllerTest::emitsChangedOncePerRealTransition()
{
    NotificationBadgeController controller;
    QSignalSpy spy(&controller, &NotificationBadgeController::changed);

    controller.setChatUnread(1, 1, false);
    controller.setChatUnread(1, 1, false);
    controller.setChatUnread(1, 2, false);
    controller.setChatUnread(0, 2, false);
    controller.removeChat(2);
    controller.setChatUnread(2, 3, false);
    controller.removeChat(2);
    controller.removeChat(2);
    controller.clear();
    controller.clear();

    // setChatUnread(1,1), setChatUnread(1,2), setChatUnread(2,3), removeChat(2)
    // and clear() are the five real transitions.
    QCOMPARE(spy.count(), 5);
}

void NotificationBadgeControllerTest::totalSaturatesInsteadOfWrapping()
{
    NotificationBadgeController controller;

    controller.setChatUnread(1, kIntMax, false);
    controller.setChatUnread(2, kIntMax, false);

    QCOMPARE(controller.totalUnread(), kIntMax);
    QCOMPARE(controller.unmutedUnread(), kIntMax);
    QCOMPARE(controller.badgeText(kIntMax), QString::number(kIntMax));
    QVERIFY(controller.hasBadge());
}

QTEST_GUILESS_MAIN(NotificationBadgeControllerTest)

#include "tst_notification_badge_controller.moc"
