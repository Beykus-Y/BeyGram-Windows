#include "telegram/UnreadCounter.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

UnreadCounts parse(const char* json)
{
    return unreadCountsFromUpdate(QJsonDocument::fromJson(json).object());
}

constexpr int kIntMax = std::numeric_limits<int>::max();

} // namespace

class UnreadCounterTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesChatCountUpdate();
    void parsesMessageCountUpdate();
    void fallsBackToUnmutedCount();
    void parsesMentions();
    void parsesMarkedUnread();
    void clampsNegativeCounts();
    void acceptsNumericStrings();
    void ignoresUnknownUpdateTypes();
    void ignoresNonMainChatLists();
    void deltaAppliesToMainList();
    void deltaIgnoresOtherLists();
    void deltaClampsAtZero();
    void deltaSaturatesInsteadOfWrapping();
    void deltaKeepsMarkedUnread();
};

void UnreadCounterTest::parsesChatCountUpdate()
{
    const UnreadCounts counts = parse(R"({
        "@type": "updateUnreadChatCount",
        "chat_list": { "@type": "chatListMain" },
        "total_count": 40,
        "unread_count": 6,
        "unread_unmuted_count": 4
    })");

    // A chat-count update reports chats only; the message counter stays zero so
    // the caller merges the fields it needs.
    QCOMPARE(counts.chats, 6);
    QCOMPARE(counts.messages, 0);
    QCOMPARE(counts.mentions, 0);
    QVERIFY(!counts.markedUnread);
}

void UnreadCounterTest::parsesMessageCountUpdate()
{
    const UnreadCounts counts = parse(R"({
        "@type": "updateUnreadMessageCount",
        "chat_list": { "@type": "chatListMain" },
        "unread_count": 23,
        "unread_unmuted_count": 11
    })");

    QCOMPARE(counts.messages, 23);
    QCOMPARE(counts.chats, 0);
    QCOMPARE(counts.mentions, 0);
}

void UnreadCounterTest::fallsBackToUnmutedCount()
{
    // Without unread_count the unmuted share is the only available count.
    const UnreadCounts counts = parse(R"({
        "@type": "updateUnreadChatCount",
        "chat_list": { "@type": "chatListMain" },
        "unread_unmuted_count": 3
    })");

    QCOMPARE(counts.chats, 3);

    // A present unread_count always wins over the unmuted share.
    const UnreadCounts both = parse(R"({
        "@type": "updateUnreadChatCount",
        "chat_list": { "@type": "chatListMain" },
        "unread_count": 9,
        "unread_unmuted_count": 2
    })");
    QCOMPARE(both.chats, 9);

    // Neither field: nothing unread.
    const UnreadCounts none = parse(R"({
        "@type": "updateUnreadMessageCount",
        "chat_list": { "@type": "chatListMain" }
    })");
    QCOMPARE(none.messages, 0);
}

void UnreadCounterTest::parsesMentions()
{
    const UnreadCounts direct = parse(R"({
        "@type": "updateUnreadChatCount",
        "chat_list": { "@type": "chatListMain" },
        "unread_count": 5,
        "mention_count": 2
    })");
    QCOMPARE(direct.mentions, 2);

    const UnreadCounts prefixed = parse(R"({
        "@type": "updateUnreadChatCount",
        "chat_list": { "@type": "chatListMain" },
        "unread_count": 5,
        "unread_mention_count": 4
    })");
    QCOMPARE(prefixed.mentions, 4);

    // The unprefixed key wins when both shapes are present.
    const UnreadCounts both = parse(R"({
        "@type": "updateUnreadChatCount",
        "chat_list": { "@type": "chatListMain" },
        "mention_count": 1,
        "unread_mention_count": 7
    })");
    QCOMPARE(both.mentions, 1);
}

void UnreadCounterTest::parsesMarkedUnread()
{
    const UnreadCounts flag = parse(R"({
        "@type": "updateUnreadChatCount",
        "chat_list": { "@type": "chatListMain" },
        "unread_count": 1,
        "is_marked_as_unread": true
    })");
    QVERIFY(flag.markedUnread);

    const UnreadCounts count = parse(R"({
        "@type": "updateUnreadChatCount",
        "chat_list": { "@type": "chatListMain" },
        "unread_count": 1,
        "marked_as_unread_count": 2
    })");
    QVERIFY(count.markedUnread);

    const UnreadCounts none = parse(R"({
        "@type": "updateUnreadChatCount",
        "chat_list": { "@type": "chatListMain" },
        "unread_count": 1,
        "marked_as_unread_count": 0,
        "is_marked_as_unread": false
    })");
    QVERIFY(!none.markedUnread);
}

void UnreadCounterTest::clampsNegativeCounts()
{
    const UnreadCounts counts = parse(R"({
        "@type": "updateUnreadChatCount",
        "chat_list": { "@type": "chatListMain" },
        "unread_count": -4,
        "mention_count": -9,
        "marked_as_unread_count": -1
    })");

    QCOMPARE(counts.chats, 0);
    QCOMPARE(counts.mentions, 0);
    QVERIFY(!counts.markedUnread);

    // A negative unread_count is still a present value, so the unmuted fallback
    // must not be used to resurrect a count the payload reported as negative.
    const UnreadCounts negative = parse(R"({
        "@type": "updateUnreadMessageCount",
        "chat_list": { "@type": "chatListMain" },
        "unread_count": -1,
        "unread_unmuted_count": 8
    })");
    QCOMPARE(negative.messages, 0);
}

void UnreadCounterTest::acceptsNumericStrings()
{
    const UnreadCounts counts = parse(R"({
        "@type": "updateUnreadChatCount",
        "chat_list": { "@type": "chatListMain" },
        "unread_count": "12",
        "mention_count": "3"
    })");

    QCOMPARE(counts.chats, 12);
    QCOMPARE(counts.mentions, 3);
}

void UnreadCounterTest::ignoresUnknownUpdateTypes()
{
    const UnreadCounts other = parse(R"({
        "@type": "updateChatReadInbox",
        "chat_id": 42,
        "unread_count": 5
    })");
    QCOMPARE(other.chats, 0);
    QCOMPARE(other.messages, 0);

    const UnreadCounts empty = unreadCountsFromUpdate(QJsonObject{});
    QCOMPARE(empty.chats, 0);
    QCOMPARE(empty.messages, 0);
    QVERIFY(!empty.markedUnread);
}

void UnreadCounterTest::ignoresNonMainChatLists()
{
    const UnreadCounts archive = parse(R"({
        "@type": "updateUnreadChatCount",
        "chat_list": { "@type": "chatListArchive" },
        "unread_count": 4
    })");
    QCOMPARE(archive.chats, 0);

    const UnreadCounts folder = parse(R"({
        "@type": "updateUnreadMessageCount",
        "chat_list": { "@type": "chatListFolder", "chat_folder_id": 2 },
        "unread_count": 7
    })");
    QCOMPARE(folder.messages, 0);

    // A bare string list type is accepted too, and a missing list is the main one.
    const UnreadCounts archiveString = parse(R"({
        "@type": "updateUnreadChatCount",
        "chat_list": "chatListArchive",
        "unread_count": 4
    })");
    QCOMPARE(archiveString.chats, 0);

    const UnreadCounts mainString = parse(R"({
        "@type": "updateUnreadChatCount",
        "chat_list": "chatListMain",
        "unread_count": 4
    })");
    QCOMPARE(mainString.chats, 4);

    const UnreadCounts noList = parse(R"({
        "@type": "updateUnreadChatCount",
        "unread_count": 4
    })");
    QCOMPARE(noList.chats, 4);
}

void UnreadCounterTest::deltaAppliesToMainList()
{
    UnreadCounts counts;
    counts.chats = 10;
    counts.messages = 20;
    counts.mentions = 2;

    const UnreadCounts grown = applyUnreadDelta(counts, QStringLiteral("chatListMain"), 3, 5, 1);
    QCOMPARE(grown.chats, 13);
    QCOMPARE(grown.messages, 25);
    QCOMPARE(grown.mentions, 3);

    // TDLib omits the list for the main one, so an empty type means the main list.
    const UnreadCounts implicit = applyUnreadDelta(counts, QString(), -2, -4, -1);
    QCOMPARE(implicit.chats, 8);
    QCOMPARE(implicit.messages, 16);
    QCOMPARE(implicit.mentions, 1);

    const UnreadCounts padded = applyUnreadDelta(counts, QStringLiteral(" chatListMain "), 1, 1, 1);
    QCOMPARE(padded.chats, 11);
    QCOMPARE(padded.messages, 21);
    QCOMPARE(padded.mentions, 3);

    // Zero deltas leave the totals alone.
    const UnreadCounts unchanged = applyUnreadDelta(counts, QStringLiteral("chatListMain"), 0, 0, 0);
    QCOMPARE(unchanged.chats, counts.chats);
    QCOMPARE(unchanged.messages, counts.messages);
    QCOMPARE(unchanged.mentions, counts.mentions);
}

void UnreadCounterTest::deltaIgnoresOtherLists()
{
    UnreadCounts counts;
    counts.chats = 5;
    counts.messages = 9;
    counts.mentions = 1;

    const UnreadCounts archive
        = applyUnreadDelta(counts, QStringLiteral("chatListArchive"), 4, 4, 4);
    QCOMPARE(archive.chats, 5);
    QCOMPARE(archive.messages, 9);
    QCOMPARE(archive.mentions, 1);

    const UnreadCounts folder
        = applyUnreadDelta(counts, QStringLiteral("chatListFolder"), -5, -9, -1);
    QCOMPARE(folder.chats, 5);
    QCOMPARE(folder.messages, 9);
    QCOMPARE(folder.mentions, 1);
}

void UnreadCounterTest::deltaClampsAtZero()
{
    UnreadCounts counts;
    counts.chats = 2;
    counts.messages = 1;
    counts.mentions = 0;

    const UnreadCounts clamped
        = applyUnreadDelta(counts, QStringLiteral("chatListMain"), -10, -1, -3);
    QCOMPARE(clamped.chats, 0);
    QCOMPARE(clamped.messages, 0);
    QCOMPARE(clamped.mentions, 0);
}

void UnreadCounterTest::deltaSaturatesInsteadOfWrapping()
{
    UnreadCounts counts;
    counts.chats = kIntMax;
    counts.messages = kIntMax - 1;
    counts.mentions = kIntMax;

    const UnreadCounts grown
        = applyUnreadDelta(counts, QStringLiteral("chatListMain"), 1, 10, kIntMax);
    QCOMPARE(grown.chats, kIntMax);
    QCOMPARE(grown.messages, kIntMax);
    QCOMPARE(grown.mentions, kIntMax);
}

void UnreadCounterTest::deltaKeepsMarkedUnread()
{
    UnreadCounts counts;
    counts.chats = 3;
    counts.markedUnread = true;

    const UnreadCounts delta
        = applyUnreadDelta(counts, QStringLiteral("chatListMain"), -3, 0, 0);
    QCOMPARE(delta.chats, 0);
    QVERIFY(delta.markedUnread);

    const UnreadCounts ignored
        = applyUnreadDelta(counts, QStringLiteral("chatListArchive"), -3, 0, 0);
    QVERIFY(ignored.markedUnread);
    QCOMPARE(ignored.chats, 3);
}

QTEST_GUILESS_MAIN(UnreadCounterTest)
#include "tst_unread_counter.moc"
