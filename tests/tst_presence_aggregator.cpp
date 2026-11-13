#include "telegram/PresenceAggregator.h"

#include <QSignalSpy>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

// The ellipsis is the single character the aggregator paints, not three dots.
const QString kTyping = QStringLiteral("typing…");
const QString kRecording = QStringLiteral("recording…");

ChatPresence presenceWithTyping(int count)
{
    ChatPresence presence;
    presence.typingMembers = count;
    return presence;
}

ChatPresence presenceWithOnline(int count)
{
    ChatPresence presence;
    presence.onlineMembers = count;
    return presence;
}

} // namespace

class PresenceAggregatorTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void unknownChatReturnsDefaultPresence();

    void setPresenceStoresEveryField();
    void setPresenceRejectsInvalidChatId();
    void setPresenceClampsNegativeCounts();
    void setPresenceTrimsLastSeenText();
    void setPresenceReplacesPreviousRecord();
    void setPresenceIgnoresIdenticalRecord();
    void setPresenceEmptyRecordRemovesChat();
    void setPresenceEmptyOnUnknownChatIsSilent();
    void presenceReturnsStoredCopy();

    void statusTextIsEmptyWithoutPresence();
    void statusTextPrioritisesRecording();
    void statusTextPrioritisesTypingOverOnline();
    void statusTextTypingCountsOtherMembers();
    void statusTextRecordingHasNoCount();
    void statusTextShowsOnlineCount();
    void statusTextShowsBotOnline();
    void statusTextBotLosesToMembers();
    void statusTextFallsBackToLastSeen();
    void statusTextUncoversNextKind();

    void clearChatForgetsChat();
    void clearChatMissingChatIsSilent();
    void clearChatRejectsInvalidChatId();
    void clearDropsEveryChat();
    void clearOnEmptyAggregatorIsSilent();

    void chatsAreIsolated();
    void keepsLargeChatIds();
    void changedTracksOnlyRealChanges();
};

void PresenceAggregatorTest::startsEmpty()
{
    PresenceAggregator aggregator;
    QSignalSpy spy(&aggregator, &PresenceAggregator::changed);

    QVERIFY(aggregator.statusText(1).isEmpty());
    QCOMPARE(spy.count(), 0);
}

void PresenceAggregatorTest::unknownChatReturnsDefaultPresence()
{
    PresenceAggregator aggregator;

    const ChatPresence presence = aggregator.presence(12345);

    // A chat without a record reads as an empty presence, so callers can bind
    // a row without checking for existence first.
    QCOMPARE(presence.onlineMembers, 0);
    QCOMPARE(presence.typingMembers, 0);
    QCOMPARE(presence.recordingMembers, 0);
    QVERIFY(!presence.botOnline);
    QVERIFY(presence.lastSeenText.isEmpty());
}

void PresenceAggregatorTest::setPresenceStoresEveryField()
{
    PresenceAggregator aggregator;
    QSignalSpy spy(&aggregator, &PresenceAggregator::changed);

    ChatPresence incoming;
    incoming.onlineMembers = 4;
    incoming.typingMembers = 2;
    incoming.recordingMembers = 1;
    incoming.botOnline = true;
    incoming.lastSeenText = QStringLiteral("last seen recently");
    aggregator.setPresence(42, incoming);

    QCOMPARE(spy.count(), 1);
    const ChatPresence stored = aggregator.presence(42);
    QCOMPARE(stored.onlineMembers, 4);
    QCOMPARE(stored.typingMembers, 2);
    QCOMPARE(stored.recordingMembers, 1);
    QVERIFY(stored.botOnline);
    QCOMPARE(stored.lastSeenText, QStringLiteral("last seen recently"));
}

void PresenceAggregatorTest::setPresenceRejectsInvalidChatId()
{
    PresenceAggregator aggregator;
    QSignalSpy spy(&aggregator, &PresenceAggregator::changed);

    ChatPresence incoming = presenceWithTyping(3);
    aggregator.setPresence(0, incoming);
    aggregator.setPresence(-9, incoming);

    QCOMPARE(spy.count(), 0);
    QVERIFY(aggregator.statusText(0).isEmpty());
    QVERIFY(aggregator.statusText(-9).isEmpty());
    QCOMPARE(aggregator.presence(0).typingMembers, 0);
    QCOMPARE(aggregator.presence(-9).typingMembers, 0);
}

void PresenceAggregatorTest::setPresenceClampsNegativeCounts()
{
    PresenceAggregator aggregator;
    QSignalSpy spy(&aggregator, &PresenceAggregator::changed);

    ChatPresence incoming;
    incoming.onlineMembers = -1;
    incoming.typingMembers = -7;
    incoming.recordingMembers = -3;
    incoming.botOnline = true;
    aggregator.setPresence(5, incoming);

    // Every count was negative, so only the bot flag is left to show.
    QCOMPARE(spy.count(), 1);
    const ChatPresence stored = aggregator.presence(5);
    QCOMPARE(stored.onlineMembers, 0);
    QCOMPARE(stored.typingMembers, 0);
    QCOMPARE(stored.recordingMembers, 0);
    QCOMPARE(aggregator.statusText(5), QStringLiteral("online"));
}

void PresenceAggregatorTest::setPresenceTrimsLastSeenText()
{
    PresenceAggregator aggregator;

    ChatPresence incoming;
    incoming.lastSeenText = QStringLiteral("  last seen 5 minutes ago \n");
    aggregator.setPresence(5, incoming);

    QCOMPARE(aggregator.presence(5).lastSeenText,
             QStringLiteral("last seen 5 minutes ago"));
    QCOMPARE(aggregator.statusText(5), QStringLiteral("last seen 5 minutes ago"));
}

void PresenceAggregatorTest::setPresenceReplacesPreviousRecord()
{
    PresenceAggregator aggregator;
    QSignalSpy spy(&aggregator, &PresenceAggregator::changed);

    ChatPresence oldRecord;
    oldRecord.onlineMembers = 3;
    oldRecord.lastSeenText = QStringLiteral("last seen yesterday");
    aggregator.setPresence(5, oldRecord);
    QCOMPARE(spy.count(), 1);

    aggregator.setPresence(5, presenceWithTyping(1));

    // The new record replaces the old one instead of merging into it.
    QCOMPARE(spy.count(), 2);
    const ChatPresence stored = aggregator.presence(5);
    QCOMPARE(stored.onlineMembers, 0);
    QVERIFY(stored.lastSeenText.isEmpty());
    QCOMPARE(stored.typingMembers, 1);
    QCOMPARE(aggregator.statusText(5), kTyping);
}

void PresenceAggregatorTest::setPresenceIgnoresIdenticalRecord()
{
    PresenceAggregator aggregator;
    QSignalSpy spy(&aggregator, &PresenceAggregator::changed);

    ChatPresence incoming;
    incoming.typingMembers = 2;
    incoming.lastSeenText = QStringLiteral(" last seen recently ");
    aggregator.setPresence(5, incoming);
    QCOMPARE(spy.count(), 1);

    // Same values with the same padding normalise to the stored form, so
    // nothing visible changed.
    aggregator.setPresence(5, incoming);
    incoming.lastSeenText = QStringLiteral("last seen recently");
    aggregator.setPresence(5, incoming);
    QCOMPARE(spy.count(), 1);
}

void PresenceAggregatorTest::setPresenceEmptyRecordRemovesChat()
{
    PresenceAggregator aggregator;
    QSignalSpy spy(&aggregator, &PresenceAggregator::changed);

    ChatPresence incoming;
    incoming.lastSeenText = QStringLiteral("last seen recently");
    aggregator.setPresence(5, incoming);
    QCOMPARE(spy.count(), 1);

    aggregator.setPresence(5, ChatPresence{});

    // An empty record has nothing to paint, so it is dropped instead of kept.
    QCOMPARE(spy.count(), 2);
    QVERIFY(aggregator.statusText(5).isEmpty());
    QVERIFY(aggregator.presence(5).lastSeenText.isEmpty());
}

void PresenceAggregatorTest::setPresenceEmptyOnUnknownChatIsSilent()
{
    PresenceAggregator aggregator;
    QSignalSpy spy(&aggregator, &PresenceAggregator::changed);

    aggregator.setPresence(5, ChatPresence{});
    aggregator.setPresence(6, ChatPresence{});

    QCOMPARE(spy.count(), 0);
    QVERIFY(aggregator.statusText(5).isEmpty());
    QVERIFY(aggregator.statusText(6).isEmpty());
}

void PresenceAggregatorTest::presenceReturnsStoredCopy()
{
    PresenceAggregator aggregator;

    aggregator.setPresence(5, presenceWithOnline(2));

    ChatPresence copy = aggregator.presence(5);
    copy.onlineMembers = 99;

    // The getter hands out a snapshot; editing it cannot reach the store.
    QCOMPARE(aggregator.presence(5).onlineMembers, 2);
    QCOMPARE(aggregator.statusText(5), QStringLiteral("2 online"));
}

void PresenceAggregatorTest::statusTextIsEmptyWithoutPresence()
{
    PresenceAggregator aggregator;

    QVERIFY(aggregator.statusText(7).isEmpty());

    aggregator.setPresence(7, presenceWithTyping(1));
    QCOMPARE(aggregator.statusText(7), kTyping);

    aggregator.setPresence(7, ChatPresence{});
    QVERIFY(aggregator.statusText(7).isEmpty());
}

void PresenceAggregatorTest::statusTextPrioritisesRecording()
{
    PresenceAggregator aggregator;

    ChatPresence incoming;
    incoming.typingMembers = 3;
    incoming.onlineMembers = 9;
    incoming.lastSeenText = QStringLiteral("last seen recently");
    aggregator.setPresence(7, incoming);
    QCOMPARE(aggregator.statusText(7), QStringLiteral("typing… +2"));

    incoming.recordingMembers = 1;
    aggregator.setPresence(7, incoming);
    QCOMPARE(aggregator.statusText(7), kRecording);
}

void PresenceAggregatorTest::statusTextPrioritisesTypingOverOnline()
{
    PresenceAggregator aggregator;

    ChatPresence incoming;
    incoming.onlineMembers = 8;
    incoming.lastSeenText = QStringLiteral("last seen yesterday");
    aggregator.setPresence(7, incoming);
    QCOMPARE(aggregator.statusText(7), QStringLiteral("8 online"));

    incoming.typingMembers = 1;
    aggregator.setPresence(7, incoming);
    QCOMPARE(aggregator.statusText(7), kTyping);
}

void PresenceAggregatorTest::statusTextTypingCountsOtherMembers()
{
    PresenceAggregator aggregator;

    aggregator.setPresence(7, presenceWithTyping(1));
    QCOMPARE(aggregator.statusText(7), kTyping);

    aggregator.setPresence(7, presenceWithTyping(2));
    QCOMPARE(aggregator.statusText(7), QStringLiteral("typing… +1"));

    aggregator.setPresence(7, presenceWithTyping(5));
    QCOMPARE(aggregator.statusText(7), QStringLiteral("typing… +4"));
}

void PresenceAggregatorTest::statusTextRecordingHasNoCount()
{
    PresenceAggregator aggregator;

    ChatPresence incoming;
    incoming.recordingMembers = 3;
    incoming.typingMembers = 2;
    aggregator.setPresence(7, incoming);

    // Recording names the action only; the count suffix belongs to typing.
    QCOMPARE(aggregator.statusText(7), kRecording);
}

void PresenceAggregatorTest::statusTextShowsOnlineCount()
{
    PresenceAggregator aggregator;

    aggregator.setPresence(7, presenceWithOnline(1));
    QCOMPARE(aggregator.statusText(7), QStringLiteral("1 online"));

    aggregator.setPresence(7, presenceWithOnline(7));
    QCOMPARE(aggregator.statusText(7), QStringLiteral("7 online"));
}

void PresenceAggregatorTest::statusTextShowsBotOnline()
{
    PresenceAggregator aggregator;

    ChatPresence incoming;
    incoming.botOnline = true;
    incoming.lastSeenText = QStringLiteral("last seen recently");
    aggregator.setPresence(7, incoming);

    // A bot has no member count, so it can only say "online", and that wins
    // over the stored last-seen text.
    QCOMPARE(aggregator.statusText(7), QStringLiteral("online"));
}

void PresenceAggregatorTest::statusTextBotLosesToMembers()
{
    PresenceAggregator aggregator;

    ChatPresence incoming;
    incoming.botOnline = true;

    incoming.onlineMembers = 2;
    aggregator.setPresence(7, incoming);
    QCOMPARE(aggregator.statusText(7), QStringLiteral("2 online"));

    incoming.typingMembers = 1;
    aggregator.setPresence(7, incoming);
    QCOMPARE(aggregator.statusText(7), kTyping);

    incoming.recordingMembers = 1;
    aggregator.setPresence(7, incoming);
    QCOMPARE(aggregator.statusText(7), kRecording);
}

void PresenceAggregatorTest::statusTextFallsBackToLastSeen()
{
    PresenceAggregator aggregator;

    ChatPresence incoming;
    incoming.lastSeenText = QStringLiteral("last seen within a week");
    aggregator.setPresence(7, incoming);

    QCOMPARE(aggregator.statusText(7), QStringLiteral("last seen within a week"));
}

void PresenceAggregatorTest::statusTextUncoversNextKind()
{
    PresenceAggregator aggregator;

    ChatPresence incoming;
    incoming.recordingMembers = 2;
    incoming.typingMembers = 3;
    incoming.onlineMembers = 4;
    incoming.botOnline = true;
    incoming.lastSeenText = QStringLiteral("last seen 5 minutes ago");
    aggregator.setPresence(7, incoming);
    QCOMPARE(aggregator.statusText(7), kRecording);

    // Dropping the strongest kind uncovers the next one instead of clearing.
    incoming.recordingMembers = 0;
    aggregator.setPresence(7, incoming);
    QCOMPARE(aggregator.statusText(7), QStringLiteral("typing… +2"));

    incoming.typingMembers = 0;
    aggregator.setPresence(7, incoming);
    QCOMPARE(aggregator.statusText(7), QStringLiteral("4 online"));

    incoming.onlineMembers = 0;
    aggregator.setPresence(7, incoming);
    QCOMPARE(aggregator.statusText(7), QStringLiteral("online"));

    incoming.botOnline = false;
    aggregator.setPresence(7, incoming);
    QCOMPARE(aggregator.statusText(7), QStringLiteral("last seen 5 minutes ago"));

    aggregator.setPresence(7, ChatPresence{});
    QVERIFY(aggregator.statusText(7).isEmpty());
}

void PresenceAggregatorTest::clearChatForgetsChat()
{
    PresenceAggregator aggregator;
    QSignalSpy spy(&aggregator, &PresenceAggregator::changed);

    aggregator.setPresence(7, presenceWithOnline(2));
    QCOMPARE(spy.count(), 1);

    aggregator.clearChat(7);

    QCOMPARE(spy.count(), 2);
    QVERIFY(aggregator.statusText(7).isEmpty());
    QCOMPARE(aggregator.presence(7).onlineMembers, 0);
}

void PresenceAggregatorTest::clearChatMissingChatIsSilent()
{
    PresenceAggregator aggregator;
    QSignalSpy spy(&aggregator, &PresenceAggregator::changed);

    aggregator.clearChat(7);

    QCOMPARE(spy.count(), 0);
}

void PresenceAggregatorTest::clearChatRejectsInvalidChatId()
{
    PresenceAggregator aggregator;
    QSignalSpy spy(&aggregator, &PresenceAggregator::changed);

    aggregator.setPresence(7, presenceWithOnline(2));
    QCOMPARE(spy.count(), 1);

    aggregator.clearChat(0);
    aggregator.clearChat(-3);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(aggregator.presence(7).onlineMembers, 2);
}

void PresenceAggregatorTest::clearDropsEveryChat()
{
    PresenceAggregator aggregator;
    QSignalSpy spy(&aggregator, &PresenceAggregator::changed);

    aggregator.setPresence(1, presenceWithTyping(1));
    aggregator.setPresence(2, presenceWithOnline(3));
    ChatPresence lastSeen;
    lastSeen.lastSeenText = QStringLiteral("last seen recently");
    aggregator.setPresence(3, lastSeen);
    QCOMPARE(spy.count(), 3);

    aggregator.clear();

    QCOMPARE(spy.count(), 4);
    QVERIFY(aggregator.statusText(1).isEmpty());
    QVERIFY(aggregator.statusText(2).isEmpty());
    QVERIFY(aggregator.statusText(3).isEmpty());
}

void PresenceAggregatorTest::clearOnEmptyAggregatorIsSilent()
{
    PresenceAggregator aggregator;
    QSignalSpy spy(&aggregator, &PresenceAggregator::changed);

    aggregator.clear();
    aggregator.clear();

    QCOMPARE(spy.count(), 0);
}

void PresenceAggregatorTest::chatsAreIsolated()
{
    PresenceAggregator aggregator;

    aggregator.setPresence(1, presenceWithTyping(2));
    aggregator.setPresence(2, presenceWithOnline(1));

    QCOMPARE(aggregator.presence(1).typingMembers, 2);
    QCOMPARE(aggregator.presence(1).onlineMembers, 0);
    QCOMPARE(aggregator.presence(2).typingMembers, 0);
    QCOMPARE(aggregator.presence(2).onlineMembers, 1);
    QCOMPARE(aggregator.statusText(1), QStringLiteral("typing… +1"));
    QCOMPARE(aggregator.statusText(2), QStringLiteral("1 online"));

    aggregator.clearChat(1);

    QVERIFY(aggregator.statusText(1).isEmpty());
    QCOMPARE(aggregator.statusText(2), QStringLiteral("1 online"));
}

void PresenceAggregatorTest::keepsLargeChatIds()
{
    PresenceAggregator aggregator;

    // Telegram ids exceed 32 bits; the aggregator must not truncate them.
    const qint64 bigId = Q_INT64_C(7348291037465);
    const qint64 otherId = Q_INT64_C(7348291037466);
    aggregator.setPresence(bigId, presenceWithTyping(1));
    aggregator.setPresence(otherId, presenceWithOnline(2));

    QCOMPARE(aggregator.presence(bigId).typingMembers, 1);
    QCOMPARE(aggregator.presence(bigId).onlineMembers, 0);
    QCOMPARE(aggregator.statusText(bigId), kTyping);
    QCOMPARE(aggregator.statusText(otherId), QStringLiteral("2 online"));
}

void PresenceAggregatorTest::changedTracksOnlyRealChanges()
{
    PresenceAggregator aggregator;
    QSignalSpy spy(&aggregator, &PresenceAggregator::changed);

    aggregator.setPresence(7, presenceWithTyping(2)); // 1: added
    aggregator.setPresence(7, presenceWithTyping(2)); // silent: identical

    ChatPresence incoming = presenceWithTyping(2);
    incoming.lastSeenText = QStringLiteral("last seen recently");
    aggregator.setPresence(7, incoming); // 2: last seen added
    incoming.lastSeenText = QStringLiteral("  last seen recently  ");
    aggregator.setPresence(7, incoming); // silent: trimmed identical

    incoming.typingMembers = 0;
    aggregator.setPresence(7, incoming); // 3: typing cleared
    aggregator.setPresence(7, ChatPresence{}); // 4: record dropped
    aggregator.setPresence(7, ChatPresence{}); // silent: nothing stored
    aggregator.clearChat(7);                   // silent: already gone
    aggregator.clear();                        // silent: aggregator empty

    QCOMPARE(spy.count(), 4);
    QVERIFY(aggregator.statusText(7).isEmpty());
}

QTEST_GUILESS_MAIN(PresenceAggregatorTest)
#include "tst_presence_aggregator.moc"
