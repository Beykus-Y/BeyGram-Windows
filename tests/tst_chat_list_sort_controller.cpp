#include "telegram/ChatListSortController.h"

#include <QSignalSpy>
#include <QString>
#include <QtTest>

#include <algorithm>

using namespace beygram::telegram;

namespace {

ChatData makeChat(qint64 id, const QString& title, qint64 order, qint64 lastMessageDate,
                  int unreadCount, bool pinned = false)
{
    ChatData chat;
    chat.id = id;
    chat.title = title;
    chat.order = order;
    chat.lastMessageDate = lastMessageDate;
    chat.unreadCount = unreadCount;
    chat.pinned = pinned;
    chat.inMainList = true;
    return chat;
}

QVector<qint64> idsOf(const QVector<ChatData>& chats)
{
    QVector<qint64> ids;
    ids.reserve(chats.size());
    for (const ChatData& chat : chats)
        ids.append(chat.id);
    return ids;
}

// Three chats whose order, date and unread keys all point in the same
// direction, so a wrong key is visible in the resulting ids.
QVector<ChatData> sampleChats()
{
    return {
        makeChat(1, QStringLiteral("alpha"), 30, 300, 3),
        makeChat(2, QStringLiteral("bravo"), 20, 200, 1),
        makeChat(3, QStringLiteral("charlie"), 10, 100, 7),
    };
}

} // namespace

class ChatListSortControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithPositionDescendingAndPinnedFirst();
    void setModeEmitsOnlyOnChange();
    void setAscendingEmitsOnlyOnChange();
    void setPinnedFirstEmitsOnlyOnChange();
    void sortedHandlesEmptyAndSingleChat();
    void positionSortsByOrderDescending();
    void positionSortsByOrderAscending();
    void dateSortsByLastMessageDate();
    void unreadSortsByUnreadCount();
    void nameSortsWithLocaleAwareCompare();
    void nameDescendingIsTheReverseOfAscending();
    void pinnedChatsComeFirstInEveryMode();
    void pinnedGroupIsSortedByMode();
    void pinnedFirstDisabledKeepsModeOrder();
    void tiesKeepInputOrder();
    void sortedLeavesTheInputUntouched();
    void sortedIsRepeatableAndUsesCurrentSettings();
    void emitsChangedOncePerRealTransition();
};

void ChatListSortControllerTest::startsWithPositionDescendingAndPinnedFirst()
{
    const ChatListSortController controller;

    QVERIFY(controller.mode() == ChatSortMode::Position);
    QVERIFY(!controller.ascending());
    QVERIFY(controller.pinnedFirst());
}

void ChatListSortControllerTest::setModeEmitsOnlyOnChange()
{
    ChatListSortController controller;
    QSignalSpy spy(&controller, &ChatListSortController::changed);

    // The initial mode is Position, so asking for it changes nothing.
    controller.setMode(ChatSortMode::Position);
    QCOMPARE(spy.count(), 0);

    controller.setMode(ChatSortMode::Date);
    QCOMPARE(spy.count(), 1);
    QVERIFY(controller.mode() == ChatSortMode::Date);

    controller.setMode(ChatSortMode::Date);
    QCOMPARE(spy.count(), 1);

    controller.setMode(ChatSortMode::Unread);
    controller.setMode(ChatSortMode::Name);
    controller.setMode(ChatSortMode::Position);
    QCOMPARE(spy.count(), 4);
    QVERIFY(controller.mode() == ChatSortMode::Position);
}

void ChatListSortControllerTest::setAscendingEmitsOnlyOnChange()
{
    ChatListSortController controller;
    QSignalSpy spy(&controller, &ChatListSortController::changed);

    // Descending is the default, so it is not a change.
    controller.setAscending(false);
    QCOMPARE(spy.count(), 0);

    controller.setAscending(true);
    QCOMPARE(spy.count(), 1);
    QVERIFY(controller.ascending());

    controller.setAscending(true);
    QCOMPARE(spy.count(), 1);

    controller.setAscending(false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!controller.ascending());
}

void ChatListSortControllerTest::setPinnedFirstEmitsOnlyOnChange()
{
    ChatListSortController controller;
    QSignalSpy spy(&controller, &ChatListSortController::changed);

    // Pinning first is the default, so it is not a change.
    controller.setPinnedFirst(true);
    QCOMPARE(spy.count(), 0);

    controller.setPinnedFirst(false);
    QCOMPARE(spy.count(), 1);
    QVERIFY(!controller.pinnedFirst());

    controller.setPinnedFirst(false);
    QCOMPARE(spy.count(), 1);

    controller.setPinnedFirst(true);
    QCOMPARE(spy.count(), 2);
    QVERIFY(controller.pinnedFirst());
}

void ChatListSortControllerTest::sortedHandlesEmptyAndSingleChat()
{
    const ChatListSortController controller;

    QVERIFY(controller.sorted({}).isEmpty());

    const QVector<ChatData> single{makeChat(42, QStringLiteral("solo"), 1, 10, 2)};
    QCOMPARE(idsOf(controller.sorted(single)), (QVector<qint64>{42}));
}

void ChatListSortControllerTest::positionSortsByOrderDescending()
{
    ChatListSortController controller;
    controller.setMode(ChatSortMode::Position);

    // Higher TDLib order means higher in the list.
    QCOMPARE(idsOf(controller.sorted(sampleChats())), (QVector<qint64>{1, 2, 3}));

    controller.setAscending(true);
    QCOMPARE(idsOf(controller.sorted(sampleChats())), (QVector<qint64>{3, 2, 1}));
}

void ChatListSortControllerTest::positionSortsByOrderAscending()
{
    ChatListSortController controller;
    controller.setMode(ChatSortMode::Position);
    controller.setAscending(true);

    const QVector<ChatData> chats{
        makeChat(7, QStringLiteral("seven"), 50, 0, 0),
        makeChat(8, QStringLiteral("eight"), -20, 0, 0),
        makeChat(9, QStringLiteral("nine"), 0, 0, 0),
    };

    QCOMPARE(idsOf(controller.sorted(chats)), (QVector<qint64>{8, 9, 7}));
}

void ChatListSortControllerTest::dateSortsByLastMessageDate()
{
    ChatListSortController controller;
    controller.setMode(ChatSortMode::Date);

    // Newest message first by default.
    QCOMPARE(idsOf(controller.sorted(sampleChats())), (QVector<qint64>{1, 2, 3}));

    controller.setAscending(true);
    QCOMPARE(idsOf(controller.sorted(sampleChats())), (QVector<qint64>{3, 2, 1}));

    // Date wins over order when the two disagree.
    const QVector<ChatData> chats{
        makeChat(1, QStringLiteral("one"), 900, 10, 0),
        makeChat(2, QStringLiteral("two"), 100, 500, 0),
    };
    controller.setAscending(false);
    QCOMPARE(idsOf(controller.sorted(chats)), (QVector<qint64>{2, 1}));
}

void ChatListSortControllerTest::unreadSortsByUnreadCount()
{
    ChatListSortController controller;
    controller.setMode(ChatSortMode::Unread);

    // Most unread first by default.
    QCOMPARE(idsOf(controller.sorted(sampleChats())), (QVector<qint64>{3, 1, 2}));

    controller.setAscending(true);
    QCOMPARE(idsOf(controller.sorted(sampleChats())), (QVector<qint64>{2, 1, 3}));

    // Chats without unread messages sort last in descending mode.
    const QVector<ChatData> chats{
        makeChat(1, QStringLiteral("read"), 0, 0, 0),
        makeChat(2, QStringLiteral("busy"), 0, 0, 12),
        makeChat(3, QStringLiteral("idle"), 0, 0, 4),
    };
    controller.setAscending(false);
    QCOMPARE(idsOf(controller.sorted(chats)), (QVector<qint64>{2, 3, 1}));
}

void ChatListSortControllerTest::nameSortsWithLocaleAwareCompare()
{
    ChatListSortController controller;
    controller.setMode(ChatSortMode::Name);
    controller.setAscending(true);

    // Plain lowercase titles have the same order in every locale.
    const QVector<ChatData> chats{
        makeChat(1, QStringLiteral("charlie"), 0, 0, 0),
        makeChat(2, QStringLiteral("alpha"), 0, 0, 0),
        makeChat(3, QStringLiteral("bravo"), 0, 0, 0),
    };
    QCOMPARE(idsOf(controller.sorted(chats)), (QVector<qint64>{2, 3, 1}));

    // Mixed case is decided by localeAwareCompare, not by code point order, so
    // assert the invariant that neighbours never compare as out of order.
    const QVector<ChatData> mixed{
        makeChat(10, QStringLiteral("delta"), 0, 0, 0),
        makeChat(11, QStringLiteral("Alpha"), 0, 0, 0),
        makeChat(12, QStringLiteral("charlie"), 0, 0, 0),
        makeChat(13, QStringLiteral("Bravo"), 0, 0, 0),
    };
    const QVector<ChatData> result = controller.sorted(mixed);
    QCOMPARE(result.size(), qsizetype(4));
    for (qsizetype i = 1; i < result.size(); ++i) {
        QVERIFY(QString::localeAwareCompare(result.at(i - 1).title, result.at(i).title) <= 0);
    }
    // Case-insensitive and case-sensitive collation agree on this order.
    QCOMPARE(idsOf(result), (QVector<qint64>{11, 13, 12, 10}));
}

void ChatListSortControllerTest::nameDescendingIsTheReverseOfAscending()
{
    ChatListSortController controller;
    controller.setMode(ChatSortMode::Name);

    const QVector<ChatData> chats{
        makeChat(1, QStringLiteral("alpha"), 0, 0, 0),
        makeChat(2, QStringLiteral("bravo"), 0, 0, 0),
        makeChat(3, QStringLiteral("charlie"), 0, 0, 0),
    };

    controller.setAscending(true);
    const QVector<qint64> ascendingIds = idsOf(controller.sorted(chats));

    controller.setAscending(false);
    QVector<qint64> descendingIds = idsOf(controller.sorted(chats));
    std::reverse(descendingIds.begin(), descendingIds.end());

    QCOMPARE(descendingIds, ascendingIds);
}

void ChatListSortControllerTest::pinnedChatsComeFirstInEveryMode()
{
    const QVector<ChatData> chats{
        makeChat(1, QStringLiteral("alpha"), 300, 3000, 30, false),
        makeChat(2, QStringLiteral("bravo"), 100, 1000, 1, true),
        makeChat(3, QStringLiteral("charlie"), 200, 2000, 10, false),
        makeChat(4, QStringLiteral("delta"), 50, 500, 0, true),
    };

    ChatListSortController controller;
    QVERIFY(controller.pinnedFirst());

    // Pinned chats are hoisted even though every other key ranks them lower.
    controller.setMode(ChatSortMode::Position);
    QCOMPARE(idsOf(controller.sorted(chats)), (QVector<qint64>{2, 4, 1, 3}));

    controller.setMode(ChatSortMode::Date);
    QCOMPARE(idsOf(controller.sorted(chats)), (QVector<qint64>{2, 4, 1, 3}));

    controller.setMode(ChatSortMode::Unread);
    QCOMPARE(idsOf(controller.sorted(chats)), (QVector<qint64>{2, 4, 1, 3}));

    controller.setMode(ChatSortMode::Name);
    controller.setAscending(true);
    QCOMPARE(idsOf(controller.sorted(chats)), (QVector<qint64>{2, 4, 1, 3}));
}

void ChatListSortControllerTest::pinnedGroupIsSortedByMode()
{
    ChatListSortController controller;
    controller.setMode(ChatSortMode::Date);

    const QVector<ChatData> chats{
        makeChat(1, QStringLiteral("a"), 0, 100, 0, true),
        makeChat(2, QStringLiteral("b"), 0, 400, 0, true),
        makeChat(3, QStringLiteral("c"), 0, 200, 0, false),
        makeChat(4, QStringLiteral("d"), 0, 300, 0, false),
    };

    // Both groups follow the selected key, the pin split only comes first.
    QCOMPARE(idsOf(controller.sorted(chats)), (QVector<qint64>{2, 1, 4, 3}));

    controller.setAscending(true);
    QCOMPARE(idsOf(controller.sorted(chats)), (QVector<qint64>{1, 2, 3, 4}));
}

void ChatListSortControllerTest::pinnedFirstDisabledKeepsModeOrder()
{
    ChatListSortController controller;
    controller.setMode(ChatSortMode::Date);
    controller.setPinnedFirst(false);

    const QVector<ChatData> chats{
        makeChat(1, QStringLiteral("alpha"), 0, 100, 0, true),
        makeChat(2, QStringLiteral("bravo"), 0, 400, 0, false),
        makeChat(3, QStringLiteral("charlie"), 0, 200, 0, true),
    };

    // Without the pin rule the pinned flags are ignored entirely.
    QCOMPARE(idsOf(controller.sorted(chats)), (QVector<qint64>{2, 3, 1}));
}

void ChatListSortControllerTest::tiesKeepInputOrder()
{
    ChatListSortController controller;

    const QVector<ChatData> sameOrder{
        makeChat(1, QStringLiteral("one"), 5, 10, 1),
        makeChat(2, QStringLiteral("two"), 5, 20, 2),
        makeChat(3, QStringLiteral("three"), 5, 30, 3),
    };
    controller.setMode(ChatSortMode::Position);
    QCOMPARE(idsOf(controller.sorted(sameOrder)), (QVector<qint64>{1, 2, 3}));
    controller.setAscending(true);
    QCOMPARE(idsOf(controller.sorted(sameOrder)), (QVector<qint64>{1, 2, 3}));

    const QVector<ChatData> sameDate{
        makeChat(4, QStringLiteral("four"), 0, 1000, 0),
        makeChat(5, QStringLiteral("five"), 0, 1000, 0),
        makeChat(6, QStringLiteral("six"), 0, 1000, 0),
    };
    controller.setMode(ChatSortMode::Date);
    controller.setAscending(false);
    QCOMPARE(idsOf(controller.sorted(sameDate)), (QVector<qint64>{4, 5, 6}));

    const QVector<ChatData> sameUnread{
        makeChat(7, QStringLiteral("seven"), 0, 0, 4),
        makeChat(8, QStringLiteral("eight"), 0, 0, 4),
        makeChat(9, QStringLiteral("nine"), 0, 0, 4),
    };
    controller.setMode(ChatSortMode::Unread);
    QCOMPARE(idsOf(controller.sorted(sameUnread)), (QVector<qint64>{7, 8, 9}));

    const QVector<ChatData> sameTitle{
        makeChat(10, QStringLiteral("twin"), 0, 0, 0),
        makeChat(11, QStringLiteral("twin"), 0, 0, 0),
        makeChat(12, QStringLiteral("twin"), 0, 0, 0),
    };
    controller.setMode(ChatSortMode::Name);
    controller.setAscending(true);
    QCOMPARE(idsOf(controller.sorted(sameTitle)), (QVector<qint64>{10, 11, 12}));
}

void ChatListSortControllerTest::sortedLeavesTheInputUntouched()
{
    const ChatListSortController controller;
    const QVector<ChatData> input = sampleChats();

    const QVector<ChatData> result = controller.sorted(input);

    QCOMPARE(idsOf(input), (QVector<qint64>{1, 2, 3}));
    QCOMPARE(idsOf(result), (QVector<qint64>{1, 2, 3}));

    // A reversed input stays reversed in the caller's copy even after sorting.
    QVector<ChatData> reversed{makeChat(3, QStringLiteral("c"), 10, 0, 0),
                               makeChat(2, QStringLiteral("b"), 20, 0, 0),
                               makeChat(1, QStringLiteral("a"), 30, 0, 0)};
    const QVector<ChatData> sortedCopy = controller.sorted(reversed);
    QCOMPARE(idsOf(reversed), (QVector<qint64>{3, 2, 1}));
    QCOMPARE(idsOf(sortedCopy), (QVector<qint64>{1, 2, 3}));
}

void ChatListSortControllerTest::sortedIsRepeatableAndUsesCurrentSettings()
{
    ChatListSortController controller;
    controller.setMode(ChatSortMode::Date);
    controller.setAscending(true);

    const QVector<ChatData> chats = sampleChats();

    // Repeated calls on the same state return the same order.
    QCOMPARE(idsOf(controller.sorted(chats)), (QVector<qint64>{3, 2, 1}));
    QCOMPARE(idsOf(controller.sorted(chats)), (QVector<qint64>{3, 2, 1}));

    // Changing the mode is picked up by the next call.
    controller.setMode(ChatSortMode::Unread);
    QCOMPARE(idsOf(controller.sorted(chats)), (QVector<qint64>{2, 1, 3}));

    // So is changing the direction.
    controller.setAscending(false);
    QCOMPARE(idsOf(controller.sorted(chats)), (QVector<qint64>{3, 1, 2}));
}

void ChatListSortControllerTest::emitsChangedOncePerRealTransition()
{
    ChatListSortController controller;
    QSignalSpy spy(&controller, &ChatListSortController::changed);

    controller.setMode(ChatSortMode::Date);      // 1
    controller.setMode(ChatSortMode::Date);      // no-op
    controller.setAscending(true);               // 2
    controller.setAscending(true);               // no-op
    controller.setPinnedFirst(false);            // 3
    controller.setPinnedFirst(false);            // no-op
    controller.setMode(ChatSortMode::Name);      // 4
    controller.setAscending(false);              // 5
    controller.setPinnedFirst(true);             // 6

    QCOMPARE(spy.count(), 6);

    // Sorting itself never emits: it is a pure query.
    controller.sorted(sampleChats());
    controller.sorted({});
    QCOMPARE(spy.count(), 6);
}

QTEST_GUILESS_MAIN(ChatListSortControllerTest)
#include "tst_chat_list_sort_controller.moc"
