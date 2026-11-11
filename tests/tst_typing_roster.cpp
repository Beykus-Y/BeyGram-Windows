#include "telegram/TypingRoster.h"

#include <QSignalSpy>
#include <QtTest>

using beygram::telegram::TypingRoster;

namespace {

// Time is always explicit, so every deadline below is easy to reason about.
constexpr qint64 kTypingTimeoutMs = 5000;

const QString kTyping = QStringLiteral("chatActionTyping");
const QString kCancel = QStringLiteral("chatActionCancel");

} // namespace

class TypingRosterTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void labelsSingleTypist();
    void extendsDeadlineOnRepeatedTyping();
    void expiresFiveSecondsAfterLastAction();
    void cancelRemovesImmediately();
    void otherActionsRemoveTypist();
    void labelsTwoNames();
    void labelsThreeNames();
    void labelsCrowdAsCount();
    void ordersNamesAlphabetically();
    void updatesNameForSameUser();
    void separateUsersExpireIndependently();
    void clearRemovesEveryone();
    void expireWithoutTypistsDoesNotSignal();
    void keepsLargeUserIds();
    void labelsNamelessTypist();
};

void TypingRosterTest::startsEmpty()
{
    TypingRoster roster;
    QCOMPARE(roster.count(), 0);
    QVERIFY(roster.label().isEmpty());
}

void TypingRosterTest::labelsSingleTypist()
{
    TypingRoster roster;
    QSignalSpy spy(&roster, &TypingRoster::changed);

    roster.applyAction(1, QStringLiteral("Alice"), kTyping, 1000);

    QCOMPARE(roster.count(), 1);
    QCOMPARE(roster.label(), QStringLiteral("Alice"));
    QCOMPARE(spy.count(), 1);
}

void TypingRosterTest::extendsDeadlineOnRepeatedTyping()
{
    TypingRoster roster;
    QSignalSpy spy(&roster, &TypingRoster::changed);

    roster.applyAction(1, QStringLiteral("Alice"), kTyping, 1000);
    // A heartbeat moves the deadline but does not change the label.
    roster.applyAction(1, QStringLiteral("Alice"), kTyping, 4000);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(roster.count(), 1);

    roster.expire(8999);
    QCOMPARE(roster.count(), 1);
    QCOMPARE(roster.label(), QStringLiteral("Alice"));

    roster.expire(9000);
    QCOMPARE(roster.count(), 0);
    QVERIFY(roster.label().isEmpty());
    QCOMPARE(spy.count(), 2);
}

void TypingRosterTest::expiresFiveSecondsAfterLastAction()
{
    TypingRoster roster;
    QSignalSpy spy(&roster, &TypingRoster::changed);

    roster.applyAction(1, QStringLiteral("Alice"), kTyping, 0);
    QCOMPARE(roster.count(), 1);

    roster.expire(kTypingTimeoutMs - 1);
    QCOMPARE(roster.count(), 1);

    roster.expire(kTypingTimeoutMs);
    QCOMPARE(roster.count(), 0);
    QVERIFY(roster.label().isEmpty());
    QCOMPARE(spy.count(), 2);
}

void TypingRosterTest::cancelRemovesImmediately()
{
    TypingRoster roster;
    QSignalSpy spy(&roster, &TypingRoster::changed);

    roster.applyAction(1, QStringLiteral("Alice"), kTyping, 1000);
    roster.applyAction(1, QStringLiteral("Alice"), kCancel, 1001);

    QCOMPARE(roster.count(), 0);
    QVERIFY(roster.label().isEmpty());
    QCOMPARE(spy.count(), 2);

    // Cancelling a user who is not typing changes nothing.
    roster.applyAction(2, QStringLiteral("Bob"), kCancel, 1002);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(roster.count(), 0);
}

void TypingRosterTest::otherActionsRemoveTypist()
{
    TypingRoster roster;
    QSignalSpy spy(&roster, &TypingRoster::changed);

    roster.applyAction(1, QStringLiteral("Alice"), kTyping, 1000);
    roster.applyAction(1, QStringLiteral("Alice"),
                       QStringLiteral("chatActionUploadingPhoto"), 1001);

    QCOMPARE(roster.count(), 0);
    QVERIFY(roster.label().isEmpty());
    QCOMPARE(spy.count(), 2);

    // An action this roster does not display never adds anybody.
    roster.applyAction(3, QStringLiteral("Carol"), QStringLiteral("chatActionRecordingVideo"),
                       1002);
    QCOMPARE(roster.count(), 0);
    QCOMPARE(spy.count(), 2);
}

void TypingRosterTest::labelsTwoNames()
{
    TypingRoster roster;

    // Applied out of order on purpose: the label is not arrival order.
    roster.applyAction(2, QStringLiteral("Bob"), kTyping, 0);
    roster.applyAction(1, QStringLiteral("Alice"), kTyping, 0);

    QCOMPARE(roster.count(), 2);
    QCOMPARE(roster.label(), QStringLiteral("Alice and Bob"));
}

void TypingRosterTest::labelsThreeNames()
{
    TypingRoster roster;

    roster.applyAction(3, QStringLiteral("Carol"), kTyping, 0);
    roster.applyAction(1, QStringLiteral("Alice"), kTyping, 0);
    roster.applyAction(2, QStringLiteral("Bob"), kTyping, 0);

    QCOMPARE(roster.count(), 3);
    QCOMPARE(roster.label(), QStringLiteral("Alice, Bob and Carol"));
}

void TypingRosterTest::labelsCrowdAsCount()
{
    TypingRoster roster;

    roster.applyAction(1, QStringLiteral("Alice"), kTyping, 0);
    roster.applyAction(2, QStringLiteral("Bob"), kTyping, 0);
    roster.applyAction(3, QStringLiteral("Carol"), kTyping, 0);
    roster.applyAction(4, QStringLiteral("Dave"), kTyping, 0);
    QCOMPARE(roster.label(), QStringLiteral("4 people"));

    roster.applyAction(5, QStringLiteral("Eve"), kTyping, 0);
    QCOMPARE(roster.count(), 5);
    QCOMPARE(roster.label(), QStringLiteral("5 people"));

    // Everybody stops at once: the label clears.
    roster.expire(kTypingTimeoutMs);
    QVERIFY(roster.label().isEmpty());
    QCOMPARE(roster.count(), 0);
}

void TypingRosterTest::ordersNamesAlphabetically()
{
    TypingRoster roster;

    // Case is ignored for ordering, so a lower-case name does not sort last.
    roster.applyAction(1, QStringLiteral("bob"), kTyping, 0);
    roster.applyAction(2, QStringLiteral("Alice"), kTyping, 0);

    QCOMPARE(roster.label(), QStringLiteral("Alice and bob"));
}

void TypingRosterTest::updatesNameForSameUser()
{
    TypingRoster roster;
    QSignalSpy spy(&roster, &TypingRoster::changed);

    roster.applyAction(1, QStringLiteral("Alice"), kTyping, 1000);
    // The same account renamed itself: one entry, new label.
    roster.applyAction(1, QStringLiteral("Alicia"), kTyping, 2000);

    QCOMPARE(roster.count(), 1);
    QCOMPARE(roster.label(), QStringLiteral("Alicia"));
    QCOMPARE(spy.count(), 2);
}

void TypingRosterTest::separateUsersExpireIndependently()
{
    TypingRoster roster;
    QSignalSpy spy(&roster, &TypingRoster::changed);

    roster.applyAction(1, QStringLiteral("Alice"), kTyping, 0);
    roster.applyAction(2, QStringLiteral("Bob"), kTyping, 2000);

    roster.expire(5000);
    QCOMPARE(roster.count(), 1);
    QCOMPARE(roster.label(), QStringLiteral("Bob"));
    QCOMPARE(spy.count(), 3);

    roster.expire(7000);
    QCOMPARE(roster.count(), 0);
    QVERIFY(roster.label().isEmpty());
    QCOMPARE(spy.count(), 4);
}

void TypingRosterTest::clearRemovesEveryone()
{
    TypingRoster roster;
    roster.applyAction(1, QStringLiteral("Alice"), kTyping, 0);
    roster.applyAction(2, QStringLiteral("Bob"), kTyping, 0);

    QSignalSpy spy(&roster, &TypingRoster::changed);
    roster.clear();

    QCOMPARE(roster.count(), 0);
    QVERIFY(roster.label().isEmpty());
    QCOMPARE(spy.count(), 1);

    // Clearing an already empty roster is not a change.
    roster.clear();
    QCOMPARE(spy.count(), 1);
}

void TypingRosterTest::expireWithoutTypistsDoesNotSignal()
{
    TypingRoster roster;
    QSignalSpy spy(&roster, &TypingRoster::changed);

    roster.expire(123456);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(roster.count(), 0);
}

void TypingRosterTest::keepsLargeUserIds()
{
    // Telegram user ids exceed 32 bits; they must not be truncated.
    const qint64 userId = 4242424242LL;

    TypingRoster roster;
    roster.applyAction(userId, QStringLiteral("Alice"), kTyping, 0);
    QCOMPARE(roster.count(), 1);

    roster.applyAction(userId, QStringLiteral("Alice"), kCancel, 1);
    QCOMPARE(roster.count(), 0);
}

void TypingRosterTest::labelsNamelessTypist()
{
    TypingRoster roster;

    roster.applyAction(1, QStringLiteral("   "), kTyping, 0);

    // A nameless typist still counts, so the label must not read as "nobody".
    QCOMPARE(roster.count(), 1);
    QVERIFY(!roster.label().isEmpty());
    QCOMPARE(roster.label(), QStringLiteral("Someone"));
}

QTEST_GUILESS_MAIN(TypingRosterTest)
#include "tst_typing_roster.moc"
