#include "telegram/ChatMuteSchedule.h"

#include <QSignalSpy>
#include <QString>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

constexpr qint64 kNow = 1'000'000;
constexpr qint64 kMinute = 60;
constexpr qint64 kHour = 60 * kMinute;
constexpr qint64 kDay = 24 * kHour;

QString labelAfter(ChatMuteSchedule& schedule, qint64 remaining)
{
    schedule.setMutedUntil(1, kNow + remaining);
    return schedule.label(1, kNow);
}

} // namespace

class ChatMuteScheduleTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setForeverMutes();
    void timedMuteExpiresAtItsSecond();
    void zeroUnmutes();
    void changedOnlyOnRealChanges();
    void ignoresInvalidChatIds();
    void ignoresUnsupportedExpiries();
    void expiredMutesReportUnmuted();
    void expiredEntriesArePrunedSilently();
    void labelUsesTheLargestWholeUnit();
    void labelShowsAtLeastOneMinute();
    void labelHandlesForeverAndUnknownChats();
    void labelHandlesExtremeTimes();
    void clearChatForgetsOnlyThatChat();
    void clearChatIsSilentWhenAbsent();
    void clearForgetsEverything();
};

void ChatMuteScheduleTest::startsEmpty()
{
    const ChatMuteSchedule schedule;

    QCOMPARE(schedule.mutedUntil(1), 0);
    QCOMPARE(schedule.mutedUntil(0), 0);
    QVERIFY(!schedule.isMuted(1, kNow));
    QCOMPARE(schedule.label(1, kNow), QStringLiteral("Unmuted"));
}

void ChatMuteScheduleTest::setForeverMutes()
{
    ChatMuteSchedule schedule;
    QSignalSpy spy(&schedule, &ChatMuteSchedule::changed);

    schedule.setMutedUntil(42, -1);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(schedule.mutedUntil(42), -1);
    QVERIFY(schedule.isMuted(42, 0));
    QVERIFY(schedule.isMuted(42, std::numeric_limits<qint64>::max()));
    QCOMPARE(schedule.label(42, 0), QStringLiteral("Muted forever"));
    QCOMPARE(schedule.label(42, std::numeric_limits<qint64>::max()),
             QStringLiteral("Muted forever"));
}

void ChatMuteScheduleTest::timedMuteExpiresAtItsSecond()
{
    ChatMuteSchedule schedule;
    schedule.setMutedUntil(7, 2000);

    QCOMPARE(schedule.mutedUntil(7), 2000);
    QVERIFY(schedule.isMuted(7, 1000));
    QVERIFY(schedule.isMuted(7, 1999));
    QVERIFY2(!schedule.isMuted(7, 2000), "the expiry second is already unmuted");

    // The query dropped the expired record, so the raw accessor is back to 0.
    QCOMPARE(schedule.mutedUntil(7), 0);

    schedule.setMutedUntil(8, 2000);
    QVERIFY(!schedule.isMuted(8, 2001));
}

void ChatMuteScheduleTest::zeroUnmutes()
{
    ChatMuteSchedule schedule;
    schedule.setMutedUntil(7, 2000);
    QSignalSpy spy(&schedule, &ChatMuteSchedule::changed);

    schedule.setMutedUntil(7, 0);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(schedule.mutedUntil(7), 0);
    QVERIFY(!schedule.isMuted(7, 1000));
    QCOMPARE(schedule.label(7, 1000), QStringLiteral("Unmuted"));
}

void ChatMuteScheduleTest::changedOnlyOnRealChanges()
{
    ChatMuteSchedule schedule;
    QSignalSpy spy(&schedule, &ChatMuteSchedule::changed);

    schedule.setMutedUntil(1, 1000);
    QCOMPARE(spy.count(), 1);
    schedule.setMutedUntil(1, 1000);
    QCOMPARE(spy.count(), 1);

    schedule.setMutedUntil(1, 2000);
    QCOMPARE(spy.count(), 2);
    schedule.setMutedUntil(1, 2000);
    QCOMPARE(spy.count(), 2);

    schedule.setMutedUntil(1, -1);
    QCOMPARE(spy.count(), 3);
    schedule.setMutedUntil(1, -1);
    QCOMPARE(spy.count(), 3);

    schedule.setMutedUntil(1, 0);
    QCOMPARE(spy.count(), 4);
    schedule.setMutedUntil(1, 0);
    QCOMPARE(spy.count(), 4);

    // A second chat is its own record, so writing it is a real change.
    schedule.setMutedUntil(2, 5000);
    QCOMPARE(spy.count(), 5);

    // The first chat is already unmuted, so repeating that write is silent.
    schedule.setMutedUntil(1, 0);
    QCOMPARE(spy.count(), 5);
}

void ChatMuteScheduleTest::ignoresInvalidChatIds()
{
    ChatMuteSchedule schedule;
    QSignalSpy spy(&schedule, &ChatMuteSchedule::changed);

    schedule.setMutedUntil(0, -1);
    schedule.setMutedUntil(-1, 1000);
    schedule.setMutedUntil(std::numeric_limits<qint64>::min(), 1000);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(schedule.mutedUntil(0), 0);
    QCOMPARE(schedule.mutedUntil(-1), 0);
    QVERIFY(!schedule.isMuted(0, 1000));
    QCOMPARE(schedule.label(0, 1000), QStringLiteral("Unmuted"));

    schedule.clearChat(0);
    schedule.clearChat(-1);
    QCOMPARE(spy.count(), 0);
}

void ChatMuteScheduleTest::ignoresUnsupportedExpiries()
{
    ChatMuteSchedule schedule;
    QSignalSpy spy(&schedule, &ChatMuteSchedule::changed);

    schedule.setMutedUntil(1, -2);
    schedule.setMutedUntil(1, -1000);
    schedule.setMutedUntil(1, std::numeric_limits<qint64>::min());

    QCOMPARE(spy.count(), 0);
    QCOMPARE(schedule.mutedUntil(1), 0);
    QVERIFY(!schedule.isMuted(1, kNow));

    schedule.setMutedUntil(1, 1000);
    QCOMPARE(spy.count(), 1);

    // A rejected value leaves the stored mute alone instead of turning it into
    // "forever" or clearing it.
    schedule.setMutedUntil(1, -2);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(schedule.mutedUntil(1), 1000);
    QVERIFY(schedule.isMuted(1, 999));
}

void ChatMuteScheduleTest::expiredMutesReportUnmuted()
{
    ChatMuteSchedule schedule;
    schedule.setMutedUntil(9, 100);

    QCOMPARE(schedule.mutedUntil(9), 100);
    QCOMPARE(schedule.label(9, 100), QStringLiteral("Unmuted"));
    QCOMPARE(schedule.mutedUntil(9), 0);

    schedule.setMutedUntil(10, 200);
    QVERIFY(!schedule.isMuted(10, 201));
    QCOMPARE(schedule.mutedUntil(10), 0);
    QCOMPARE(schedule.label(10, 201), QStringLiteral("Unmuted"));
}

void ChatMuteScheduleTest::expiredEntriesArePrunedSilently()
{
    ChatMuteSchedule schedule;
    schedule.setMutedUntil(5, 100);
    QSignalSpy spy(&schedule, &ChatMuteSchedule::changed);

    QVERIFY(!schedule.isMuted(5, 100));
    QCOMPARE(spy.count(), 0);
    QCOMPARE(schedule.mutedUntil(5), 0);

    // The record is gone, so storing the old expiry again is a new change. It
    // would be silent if the expired entry had been kept.
    schedule.setMutedUntil(5, 100);
    QCOMPARE(spy.count(), 1);

    // label() prunes on the same terms as isMuted().
    schedule.setMutedUntil(6, 300);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(schedule.label(6, 300), QStringLiteral("Unmuted"));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(schedule.mutedUntil(6), 0);
}

void ChatMuteScheduleTest::labelUsesTheLargestWholeUnit()
{
    ChatMuteSchedule schedule;

    QCOMPARE(labelAfter(schedule, kMinute), QStringLiteral("Muted for 1m"));
    QCOMPARE(labelAfter(schedule, kMinute + 1), QStringLiteral("Muted for 1m"));
    QCOMPARE(labelAfter(schedule, 2 * kMinute - 1), QStringLiteral("Muted for 1m"));
    QCOMPARE(labelAfter(schedule, 2 * kMinute), QStringLiteral("Muted for 2m"));
    QCOMPARE(labelAfter(schedule, kHour - 1), QStringLiteral("Muted for 59m"));
    QCOMPARE(labelAfter(schedule, kHour), QStringLiteral("Muted for 1h"));
    QCOMPARE(labelAfter(schedule, kHour + 59 * kMinute), QStringLiteral("Muted for 1h"));
    QCOMPARE(labelAfter(schedule, 2 * kHour), QStringLiteral("Muted for 2h"));
    QCOMPARE(labelAfter(schedule, kDay - 1), QStringLiteral("Muted for 23h"));
    QCOMPARE(labelAfter(schedule, kDay), QStringLiteral("Muted for 1d"));
    QCOMPARE(labelAfter(schedule, kDay + kHour), QStringLiteral("Muted for 1d"));
    QCOMPARE(labelAfter(schedule, 2 * kDay), QStringLiteral("Muted for 2d"));
    QCOMPARE(labelAfter(schedule, 30 * kDay + 12 * kHour), QStringLiteral("Muted for 30d"));
}

void ChatMuteScheduleTest::labelShowsAtLeastOneMinute()
{
    ChatMuteSchedule schedule;

    QCOMPARE(labelAfter(schedule, 1), QStringLiteral("Muted for 1m"));
    QCOMPARE(labelAfter(schedule, 59), QStringLiteral("Muted for 1m"));
    QCOMPARE(labelAfter(schedule, kMinute), QStringLiteral("Muted for 1m"));
}

void ChatMuteScheduleTest::labelHandlesForeverAndUnknownChats()
{
    ChatMuteSchedule schedule;

    QCOMPARE(schedule.label(1, kNow), QStringLiteral("Unmuted"));
    QCOMPARE(schedule.label(0, kNow), QStringLiteral("Unmuted"));
    QCOMPARE(schedule.label(-1, kNow), QStringLiteral("Unmuted"));

    schedule.setMutedUntil(1, -1);
    QCOMPARE(schedule.label(1, std::numeric_limits<qint64>::min()),
             QStringLiteral("Muted forever"));
    QCOMPARE(schedule.label(1, std::numeric_limits<qint64>::max()),
             QStringLiteral("Muted forever"));
}

void ChatMuteScheduleTest::labelHandlesExtremeTimes()
{
    ChatMuteSchedule schedule;

    // nowUnix is far in the past, so a signed difference would overflow.
    schedule.setMutedUntil(1, 86400);
    QCOMPARE(schedule.label(1, -86400), QStringLiteral("Muted for 2d"));

    schedule.setMutedUntil(1, std::numeric_limits<qint64>::max());
    const quint64 span = static_cast<quint64>(std::numeric_limits<qint64>::max())
        - static_cast<quint64>(std::numeric_limits<qint64>::min());
    QCOMPARE(schedule.label(1, std::numeric_limits<qint64>::min()),
             QStringLiteral("Muted for %1d").arg(span / static_cast<quint64>(kDay)));
}

void ChatMuteScheduleTest::clearChatForgetsOnlyThatChat()
{
    ChatMuteSchedule schedule;
    schedule.setMutedUntil(1, -1);
    schedule.setMutedUntil(2, 5000);
    QSignalSpy spy(&schedule, &ChatMuteSchedule::changed);

    schedule.clearChat(1);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(schedule.mutedUntil(1), 0);
    QVERIFY(!schedule.isMuted(1, 0));
    QCOMPARE(schedule.label(1, 0), QStringLiteral("Unmuted"));

    QCOMPARE(schedule.mutedUntil(2), 5000);
    QVERIFY(schedule.isMuted(2, 4000));
    QCOMPARE(schedule.label(2, 4000), QStringLiteral("Muted for 16m"));
}

void ChatMuteScheduleTest::clearChatIsSilentWhenAbsent()
{
    ChatMuteSchedule schedule;
    QSignalSpy spy(&schedule, &ChatMuteSchedule::changed);

    schedule.clearChat(1);
    schedule.clearChat(0);
    schedule.clearChat(-1);
    schedule.clear();

    QCOMPARE(spy.count(), 0);
}

void ChatMuteScheduleTest::clearForgetsEverything()
{
    ChatMuteSchedule schedule;
    schedule.setMutedUntil(1, -1);
    schedule.setMutedUntil(2, 5000);
    schedule.setMutedUntil(3, 100); // expired, but still stored until a query
    QSignalSpy spy(&schedule, &ChatMuteSchedule::changed);

    schedule.clear();

    QCOMPARE(spy.count(), 1);
    const qint64 ids[] = {1, 2, 3};
    for (const qint64 chatId : ids) {
        QCOMPARE(schedule.mutedUntil(chatId), 0);
        QVERIFY(!schedule.isMuted(chatId, kNow));
        QCOMPARE(schedule.label(chatId, kNow), QStringLiteral("Unmuted"));
    }

    schedule.clear();
    QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(ChatMuteScheduleTest)
#include "tst_chat_mute_schedule.moc"
