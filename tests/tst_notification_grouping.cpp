#include "telegram/NotificationGrouping.h"

#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

GroupedNotification makeNotification(qint64 chatId, const QString& body, qint64 newestDate,
                                     int count = 1, const QString& title = {},
                                     bool mention = false)
{
    GroupedNotification notification;
    notification.chatId = chatId;
    notification.title = title;
    notification.body = body;
    notification.count = count;
    notification.newestDate = newestDate;
    notification.mention = mention;
    return notification;
}

QVector<qint64> chatIdsOf(const QVector<GroupedNotification>& groups)
{
    QVector<qint64> ids;
    ids.reserve(groups.size());
    for (const GroupedNotification& group : groups)
        ids.append(group.chatId);
    return ids;
}

} // namespace

class NotificationGroupingTest final : public QObject {
    Q_OBJECT

private slots:
    void mergesNotificationsOfOneChat();
    void keepsDistinctChatsApart();
    void sortsByNewestFirst();
    void breaksDateTiesByChatId();
    void laterEntryWinsDateTie();
    void capsPerGroupCount();
    void capsNumberOfGroups();
    void nonPositiveLimits();
    void ignoresNonPositiveChatIds();
    void clampsNegativeCounts();
    void saturatesInsteadOfOverflowing();
    void handlesEmptyInput();
};

void NotificationGroupingTest::mergesNotificationsOfOneChat()
{
    // The newest entry comes first on purpose: merging must not depend on order.
    const QVector<GroupedNotification> incoming{
        makeNotification(7, QStringLiteral("Newest"), 300, 2, QStringLiteral("Team")),
        makeNotification(7, QStringLiteral("Older"), 100, 3, QStringLiteral("Team"), true),
    };

    const QVector<GroupedNotification> groups = groupNotifications(incoming);

    QCOMPARE(groups.size(), 1);
    QCOMPARE(groups.at(0).chatId, qint64(7));
    QCOMPARE(groups.at(0).count, 5);
    QCOMPARE(groups.at(0).newestDate, qint64(300));
    QCOMPARE(groups.at(0).title, QStringLiteral("Team"));
    QCOMPARE(groups.at(0).body, QStringLiteral("Newest"));
    QVERIFY(groups.at(0).mention);
}

void NotificationGroupingTest::keepsDistinctChatsApart()
{
    const QVector<GroupedNotification> incoming{
        makeNotification(7, QStringLiteral("From the team"), 200, 4, QStringLiteral("Team")),
        makeNotification(9, QStringLiteral("From Alice"), 100, 1, QStringLiteral("Alice")),
    };

    const QVector<GroupedNotification> groups = groupNotifications(incoming);

    QCOMPARE(groups.size(), 2);
    QCOMPARE(chatIdsOf(groups), (QVector<qint64>{7, 9}));
    QCOMPARE(groups.at(0).count, 4);
    QCOMPARE(groups.at(0).body, QStringLiteral("From the team"));
    QCOMPARE(groups.at(1).count, 1);
    QCOMPARE(groups.at(1).body, QStringLiteral("From Alice"));
    QVERIFY(!groups.at(0).mention);
    QVERIFY(!groups.at(1).mention);
}

void NotificationGroupingTest::sortsByNewestFirst()
{
    const QVector<GroupedNotification> incoming{
        makeNotification(1, QStringLiteral("a"), 100),
        makeNotification(2, QStringLiteral("b"), 500),
        makeNotification(3, QStringLiteral("c"), 300),
    };

    const QVector<GroupedNotification> groups = groupNotifications(incoming);

    QCOMPARE(chatIdsOf(groups), (QVector<qint64>{2, 3, 1}));
    QCOMPARE(groups.at(0).newestDate, qint64(500));
    QCOMPARE(groups.at(2).newestDate, qint64(100));
}

void NotificationGroupingTest::breaksDateTiesByChatId()
{
    // Equal timestamps must still produce a stable, input-order-independent
    // result, so the lower chat id comes first.
    const QVector<GroupedNotification> incoming{
        makeNotification(9, QStringLiteral("nine"), 100),
        makeNotification(4, QStringLiteral("four"), 100),
        makeNotification(6, QStringLiteral("six"), 100),
    };

    const QVector<GroupedNotification> groups = groupNotifications(incoming);

    QCOMPARE(chatIdsOf(groups), (QVector<qint64>{4, 6, 9}));
}

void NotificationGroupingTest::laterEntryWinsDateTie()
{
    const QVector<GroupedNotification> incoming{
        makeNotification(7, QStringLiteral("First"), 100),
        makeNotification(7, QStringLiteral("Second"), 100),
    };

    const QVector<GroupedNotification> groups = groupNotifications(incoming);

    QCOMPARE(groups.size(), 1);
    QCOMPARE(groups.at(0).body, QStringLiteral("Second"));
    QCOMPARE(groups.at(0).count, 2);
}

void NotificationGroupingTest::capsPerGroupCount()
{
    const QVector<GroupedNotification> incoming{
        makeNotification(7, QStringLiteral("a"), 100, 2),
        makeNotification(7, QStringLiteral("b"), 200, 2),
        makeNotification(7, QStringLiteral("c"), 300, 2),
    };

    const QVector<GroupedNotification> defaultCap = groupNotifications(incoming);
    QCOMPARE(defaultCap.size(), 1);
    QCOMPARE(defaultCap.at(0).count, 5);

    const QVector<GroupedNotification> tightCap = groupNotifications(incoming, 10, 2);
    QCOMPARE(tightCap.size(), 1);
    QCOMPARE(tightCap.at(0).count, 2);
}

void NotificationGroupingTest::capsNumberOfGroups()
{
    QVector<GroupedNotification> incoming;
    for (qint64 chatId = 1; chatId <= 12; ++chatId)
        incoming.append(makeNotification(chatId, QStringLiteral("body"), chatId));

    const QVector<GroupedNotification> defaultCap = groupNotifications(incoming);
    QCOMPARE(defaultCap.size(), 10);
    QCOMPARE(chatIdsOf(defaultCap),
             (QVector<qint64>{12, 11, 10, 9, 8, 7, 6, 5, 4, 3}));

    const QVector<GroupedNotification> tightCap = groupNotifications(incoming, 2);
    QCOMPARE(chatIdsOf(tightCap), (QVector<qint64>{12, 11}));
}

void NotificationGroupingTest::nonPositiveLimits()
{
    const QVector<GroupedNotification> incoming{
        makeNotification(7, QStringLiteral("a"), 100, 2),
        makeNotification(7, QStringLiteral("b"), 200, 2),
        makeNotification(8, QStringLiteral("c"), 300, 1),
    };

    QVERIFY(groupNotifications(incoming, 0).isEmpty());
    QVERIFY(groupNotifications(incoming, -1).isEmpty());

    // A non-positive per-group cap means "no cap", not "hide the count".
    const QVector<GroupedNotification> uncapped = groupNotifications(incoming, 10, 0);
    QCOMPARE(uncapped.size(), 2);
    QCOMPARE(uncapped.at(0).chatId, qint64(8));
    QCOMPARE(uncapped.at(1).chatId, qint64(7));
    QCOMPARE(uncapped.at(1).count, 4);
}

void NotificationGroupingTest::ignoresNonPositiveChatIds()
{
    const QVector<GroupedNotification> incoming{
        makeNotification(0, QStringLiteral("no chat"), 900, 3),
        makeNotification(-4, QStringLiteral("negative chat"), 800, 3),
        makeNotification(5, QStringLiteral("real"), 100, 1),
    };

    const QVector<GroupedNotification> groups = groupNotifications(incoming);

    QCOMPARE(groups.size(), 1);
    QCOMPARE(groups.at(0).chatId, qint64(5));
    QCOMPARE(groups.at(0).count, 1);
    QCOMPARE(groups.at(0).body, QStringLiteral("real"));
}

void NotificationGroupingTest::clampsNegativeCounts()
{
    const QVector<GroupedNotification> alone{
        makeNotification(7, QStringLiteral("a"), 100, -4),
    };
    const QVector<GroupedNotification> aloneGroups = groupNotifications(alone);
    QCOMPARE(aloneGroups.size(), 1);
    QCOMPARE(aloneGroups.at(0).count, 0);

    const QVector<GroupedNotification> mixed{
        makeNotification(7, QStringLiteral("a"), 100, -4),
        makeNotification(7, QStringLiteral("b"), 200, 3),
    };
    const QVector<GroupedNotification> mixedGroups = groupNotifications(mixed);
    QCOMPARE(mixedGroups.size(), 1);
    QCOMPARE(mixedGroups.at(0).count, 3);
}

void NotificationGroupingTest::saturatesInsteadOfOverflowing()
{
    const int huge = std::numeric_limits<int>::max();
    const QVector<GroupedNotification> incoming{
        makeNotification(7, QStringLiteral("a"), 100, huge),
        makeNotification(7, QStringLiteral("b"), 200, huge),
    };

    const QVector<GroupedNotification> groups = groupNotifications(incoming, 10, 0);

    QCOMPARE(groups.size(), 1);
    QCOMPARE(groups.at(0).count, huge);
}

void NotificationGroupingTest::handlesEmptyInput()
{
    const QVector<GroupedNotification> empty;
    QVERIFY(groupNotifications(empty).isEmpty());

    const QVector<GroupedNotification> onlyInvalid{
        makeNotification(0, QStringLiteral("no chat"), 100),
    };
    QVERIFY(groupNotifications(onlyInvalid).isEmpty());
}

QTEST_GUILESS_MAIN(NotificationGroupingTest)
#include "tst_notification_grouping.moc"
