#include "telegram/UnreadSync.h"

#include <QStringList>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

UnreadEntry makeEntry(qint64 chatId, int unread, bool muted = false)
{
    UnreadEntry entry;
    entry.chatId = chatId;
    entry.unread = unread;
    entry.muted = muted;
    return entry;
}

/// Flattens entries to "chatId:unread:muted" so a failing QCOMPARE shows the
/// whole list instead of one field at a time.
QString describe(const QVector<UnreadEntry>& entries)
{
    QStringList parts;
    parts.reserve(entries.size());
    for (const UnreadEntry& entry : entries) {
        parts.append(QStringLiteral("%1:%2:%3")
                         .arg(entry.chatId)
                         .arg(entry.unread)
                         .arg(entry.muted ? 1 : 0));
    }
    return parts.join(QLatin1Char(' '));
}

QVector<qint64> chatIdsOf(const QVector<UnreadEntry>& entries)
{
    QVector<qint64> ids;
    ids.reserve(entries.size());
    for (const UnreadEntry& entry : entries)
        ids.append(entry.chatId);
    return ids;
}

} // namespace

class UnreadSyncTest final : public QObject {
    Q_OBJECT

private slots:
    void normalizesEmptyInput();
    void dropsNonPositiveChatIds();
    void clampsNegativeUnread();
    void keepsMutedChatsWithZeroUnread();
    void dedupesKeepingLastValue();
    void dedupeKeepsFirstPosition();
    void preservesInputOrder();
    void normalizeIsIdempotent();

    void diffOfIdenticalSnapshotsIsEmpty();
    void reportsUnreadChange();
    void reportsMuteChange();
    void reportsNewChat();
    void reportsNewMutedChatWithNoUnread();
    void reportsRemovedChatAsCleared();
    void reportsRemovedMutedChatWithNoUnread();
    void omitsUnchangedChats();
    void omitsQuietRemoval();
    void keepsNextOrderThenRemovals();
    void computesTotals();
    void excludesMutedChatsFromUnmutedTotal();
    void saturatesTotals();
    void normalizesBothSides();
    void handlesEmptySnapshots();
};

void UnreadSyncTest::normalizesEmptyInput()
{
    QVERIFY(normalizeUnread({}).isEmpty());
}

void UnreadSyncTest::dropsNonPositiveChatIds()
{
    const QVector<UnreadEntry> normalized =
        normalizeUnread({makeEntry(0, 4), makeEntry(-5, 9), makeEntry(7, 2)});

    QCOMPARE(describe(normalized), QStringLiteral("7:2:0"));
}

void UnreadSyncTest::clampsNegativeUnread()
{
    const QVector<UnreadEntry> normalized =
        normalizeUnread({makeEntry(1, -3), makeEntry(2, -1, true)});

    QCOMPARE(describe(normalized), QStringLiteral("1:0:0 2:0:1"));
}

void UnreadSyncTest::keepsMutedChatsWithZeroUnread()
{
    // Only the chat id filters entries out; a muted chat is still a chat.
    const QVector<UnreadEntry> normalized = normalizeUnread({makeEntry(8, 0, true)});

    QCOMPARE(describe(normalized), QStringLiteral("8:0:1"));
}

void UnreadSyncTest::dedupesKeepingLastValue()
{
    const QVector<UnreadEntry> normalized = normalizeUnread(
        {makeEntry(1, 5), makeEntry(2, 1), makeEntry(1, 7, true)});

    QCOMPARE(describe(normalized), QStringLiteral("1:7:1 2:1:0"));
}

void UnreadSyncTest::dedupeKeepsFirstPosition()
{
    // The surviving entry keeps the slot of its first appearance, so the order
    // of the other chats does not shift when a duplicate is folded in.
    const QVector<UnreadEntry> normalized =
        normalizeUnread({makeEntry(1, 2), makeEntry(2, 3), makeEntry(1, 4)});

    QCOMPARE(chatIdsOf(normalized), (QVector<qint64>{1, 2}));
    QCOMPARE(normalized.at(0).unread, 4);
}

void UnreadSyncTest::preservesInputOrder()
{
    const QVector<UnreadEntry> normalized =
        normalizeUnread({makeEntry(9, 1), makeEntry(3, 2), makeEntry(5, 3)});

    QCOMPARE(chatIdsOf(normalized), (QVector<qint64>{9, 3, 5}));
}

void UnreadSyncTest::normalizeIsIdempotent()
{
    const QVector<UnreadEntry> once =
        normalizeUnread({makeEntry(3, -1), makeEntry(1, 2), makeEntry(3, 4, true)});
    const QVector<UnreadEntry> twice = normalizeUnread(once);

    QCOMPARE(describe(twice), describe(once));
    QCOMPARE(describe(once), QStringLiteral("3:4:1 1:2:0"));
}

void UnreadSyncTest::diffOfIdenticalSnapshotsIsEmpty()
{
    const QVector<UnreadEntry> snapshot{makeEntry(1, 2), makeEntry(2, 0, true)};

    const UnreadDiff diff = diffUnread(snapshot, snapshot);

    QVERIFY(diff.changed.isEmpty());
    QCOMPARE(diff.totalUnread, 2);
    QCOMPARE(diff.unmutedUnread, 2);
}

void UnreadSyncTest::reportsUnreadChange()
{
    const UnreadDiff diff = diffUnread({makeEntry(1, 2)}, {makeEntry(1, 5)});

    QCOMPARE(describe(diff.changed), QStringLiteral("1:5:0"));
    QCOMPARE(diff.totalUnread, 5);
}

void UnreadSyncTest::reportsMuteChange()
{
    // Muting does not change the message count, so the entry has to be
    // reported on the strength of the muted flag alone.
    const UnreadDiff diff = diffUnread({makeEntry(1, 4)}, {makeEntry(1, 4, true)});

    QCOMPARE(describe(diff.changed), QStringLiteral("1:4:1"));
    QCOMPARE(diff.totalUnread, 4);
    QCOMPARE(diff.unmutedUnread, 0);
}

void UnreadSyncTest::reportsNewChat()
{
    const UnreadDiff diff = diffUnread({}, {makeEntry(6, 3)});

    QCOMPARE(describe(diff.changed), QStringLiteral("6:3:0"));
    QCOMPARE(diff.totalUnread, 3);
    QCOMPARE(diff.unmutedUnread, 3);
}

void UnreadSyncTest::reportsNewMutedChatWithNoUnread()
{
    // A brand-new chat is not unread 0/unmuted, so its mute flag is a change
    // even though it has nothing to read.
    const UnreadDiff diff = diffUnread({}, {makeEntry(6, 0, true)});

    QCOMPARE(describe(diff.changed), QStringLiteral("6:0:1"));
    QCOMPARE(diff.totalUnread, 0);
    QCOMPARE(diff.unmutedUnread, 0);
}

void UnreadSyncTest::reportsRemovedChatAsCleared()
{
    const UnreadDiff diff = diffUnread({makeEntry(4, 3, true)}, {});

    QCOMPARE(describe(diff.changed), QStringLiteral("4:0:0"));
    QCOMPARE(diff.totalUnread, 0);
    QCOMPARE(diff.unmutedUnread, 0);
}

void UnreadSyncTest::reportsRemovedMutedChatWithNoUnread()
{
    // Unmuting is a change even when the chat had nothing unread.
    const UnreadDiff diff = diffUnread({makeEntry(4, 0, true)}, {});

    QCOMPARE(describe(diff.changed), QStringLiteral("4:0:0"));
}

void UnreadSyncTest::omitsUnchangedChats()
{
    const UnreadDiff diff = diffUnread({makeEntry(1, 2), makeEntry(2, 0, true)},
                                       {makeEntry(1, 2), makeEntry(2, 0, true)});

    QVERIFY(diff.changed.isEmpty());
}

void UnreadSyncTest::omitsQuietRemoval()
{
    // Nothing to report: the chat was already read and unmuted, so its absence
    // changes neither of the fields a diff looks at.
    const UnreadDiff diff = diffUnread({makeEntry(4, 0)}, {});

    QVERIFY(diff.changed.isEmpty());
    QCOMPARE(diff.totalUnread, 0);
}

void UnreadSyncTest::keepsNextOrderThenRemovals()
{
    const UnreadDiff diff = diffUnread(
        {makeEntry(1, 1), makeEntry(2, 2), makeEntry(3, 3)},
        {makeEntry(3, 0), makeEntry(9, 4), makeEntry(1, 5)});

    QCOMPARE(describe(diff.changed),
             QStringLiteral("3:0:0 9:4:0 1:5:0 2:0:0"));
}

void UnreadSyncTest::computesTotals()
{
    const UnreadDiff diff = diffUnread({},
                                       {makeEntry(1, 2), makeEntry(2, 3, true),
                                        makeEntry(3, 4), makeEntry(-1, 100)});

    QCOMPARE(diff.totalUnread, 9);
    QCOMPARE(diff.unmutedUnread, 6);
}

void UnreadSyncTest::excludesMutedChatsFromUnmutedTotal()
{
    const UnreadDiff diff = diffUnread({}, {makeEntry(1, 7, true), makeEntry(2, 5)});

    QCOMPARE(diff.totalUnread, 12);
    QCOMPARE(diff.unmutedUnread, 5);
}

void UnreadSyncTest::saturatesTotals()
{
    const int maxInt = std::numeric_limits<int>::max();

    const UnreadDiff both =
        diffUnread({}, {makeEntry(1, maxInt), makeEntry(2, 5), makeEntry(3, 1, true)});
    QCOMPARE(both.totalUnread, maxInt);
    QCOMPARE(both.unmutedUnread, maxInt);

    const UnreadDiff mutedOnly =
        diffUnread({}, {makeEntry(1, maxInt, true), makeEntry(2, 2)});
    QCOMPARE(mutedOnly.totalUnread, maxInt);
    QCOMPARE(mutedOnly.unmutedUnread, 2);
}

void UnreadSyncTest::normalizesBothSides()
{
    // The negative first occurrence is overwritten by the later one, and the
    // non-positive ids on both sides are not changes.
    const QVector<UnreadEntry> previous{makeEntry(1, -4), makeEntry(1, 5), makeEntry(0, 9)};
    const QVector<UnreadEntry> next{makeEntry(1, 5), makeEntry(-2, 7)};

    const UnreadDiff diff = diffUnread(previous, next);

    QVERIFY(diff.changed.isEmpty());
    QCOMPARE(diff.totalUnread, 5);
    QCOMPARE(diff.unmutedUnread, 5);
}

void UnreadSyncTest::handlesEmptySnapshots()
{
    const UnreadDiff diff = diffUnread({}, {});

    QVERIFY(diff.changed.isEmpty());
    QCOMPARE(diff.totalUnread, 0);
    QCOMPARE(diff.unmutedUnread, 0);
}

QTEST_GUILESS_MAIN(UnreadSyncTest)
#include "tst_unread_sync.moc"
