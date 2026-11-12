#include "telegram/MessageGroupingModel.h"

#include <QDate>
#include <QDateTime>
#include <QSignalSpy>
#include <QTime>
#include <QtTest>

using namespace beygram::telegram;

namespace {

constexpr int kBaseYear = 2024;
constexpr int kBaseMonth = 5;
constexpr int kBaseDay = 10;
constexpr qint64 kHour = 60 * 60;

// Local midnight of the base day shifted by `offsetDays`, in Unix seconds.
qint64 dayStart(int offsetDays = 0)
{
    const QDate day(kBaseYear, kBaseMonth, kBaseDay);
    return QDateTime(day.addDays(offsetDays), QTime(0, 0)).toSecsSinceEpoch();
}

// Local midnight of the day a timestamp falls on.
qint64 localDayStart(qint64 secsSinceEpoch)
{
    const QDate day = QDateTime::fromSecsSinceEpoch(secsSinceEpoch).date();
    return QDateTime(day, QTime(0, 0)).toSecsSinceEpoch();
}

// A message at `hour` o'clock on the base day shifted by `offsetDays`.
qint64 at(int offsetDays, int hour)
{
    return dayStart(offsetDays) + hour * kHour;
}

MessageData message(qint64 id, qint64 date, qint64 senderId)
{
    MessageData data;
    data.id = id;
    data.chatId = 42;
    data.date = date;
    data.senderUserId = senderId;
    return data;
}

} // namespace

class MessageGroupingModelTest final : public QObject {
    Q_OBJECT

private slots:
    void emptyModelHasNoRows();
    void firstRowAlwaysShowsDateSeparator();
    void sameDayMessagesShareOneSeparator();
    void newDayStartsSeparatorWithDayStart();
    void preservesInputOrder();
    void epochDateUsesItsLocalDayStart();
    void unreadDividerMarksMatchingRow();
    void unreadDividerIgnoredForZero();
    void unreadDividerIgnoredWhenAbsent();
    void unreadDividerShownAtMostOnce();
    void separatorAndDividerShareRow();
    void sameSenderMarksOnlyConsecutiveMessages();
    void zeroSenderNeverGroups();
    void sameSenderIgnoresDateBoundary();
    void clearResetsRows();
    void emitsChangedOnlyOnRealChanges();
};

void MessageGroupingModelTest::emptyModelHasNoRows()
{
    const MessageGroupingModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.rows().isEmpty());
    QCOMPARE(model.unreadDividerIndex(), -1);
}

void MessageGroupingModelTest::firstRowAlwaysShowsDateSeparator()
{
    MessageGroupingModel model;
    model.setMessages({message(1, at(0, 10), 7)});

    const QVector<GroupingRow> rows = model.rows();
    QCOMPARE(rows.size(), 1);
    QVERIFY(rows.at(0).showDateSeparator);
    QCOMPARE(rows.at(0).separatorDate, dayStart(0));
    QCOMPARE(rows.at(0).messageId, qint64(1));
    // The first message has no predecessor to continue.
    QVERIFY(!rows.at(0).sameSenderAsPrevious);
}

void MessageGroupingModelTest::sameDayMessagesShareOneSeparator()
{
    MessageGroupingModel model;
    model.setMessages({
        message(1, at(0, 8), 7),
        message(2, at(0, 12), 7),
        message(3, at(0, 23), 7),
    });

    const QVector<GroupingRow> rows = model.rows();
    QCOMPARE(rows.size(), 3);
    QVERIFY(rows.at(0).showDateSeparator);
    QVERIFY(!rows.at(1).showDateSeparator);
    QVERIFY(!rows.at(2).showDateSeparator);
    QCOMPARE(rows.at(0).separatorDate, dayStart(0));
    QCOMPARE(rows.at(1).separatorDate, qint64(0));
    QCOMPARE(rows.at(2).separatorDate, qint64(0));
}

void MessageGroupingModelTest::newDayStartsSeparatorWithDayStart()
{
    MessageGroupingModel model;
    model.setMessages({
        message(1, at(0, 23), 7),
        message(2, at(1, 0), 7),
        message(3, at(2, 9), 7),
    });

    const QVector<GroupingRow> rows = model.rows();
    QCOMPARE(rows.size(), 3);
    QVERIFY(rows.at(0).showDateSeparator);
    QVERIFY(rows.at(1).showDateSeparator);
    QVERIFY(rows.at(2).showDateSeparator);
    QCOMPARE(rows.at(0).separatorDate, dayStart(0));
    QCOMPARE(rows.at(1).separatorDate, dayStart(1));
    QCOMPARE(rows.at(2).separatorDate, dayStart(2));
}

void MessageGroupingModelTest::preservesInputOrder()
{
    MessageGroupingModel model;
    // Newest first, then a jump back to an earlier day: the model must not
    // sort, and each change of day starts a new separator.
    model.setMessages({
        message(30, at(2, 10), 7),
        message(20, at(1, 10), 7),
        message(10, at(2, 10), 7),
        message(5, at(2, 18), 7),
    });

    const QVector<GroupingRow> rows = model.rows();
    QCOMPARE(rows.size(), 4);
    QCOMPARE(rows.at(0).messageId, qint64(30));
    QCOMPARE(rows.at(1).messageId, qint64(20));
    QCOMPARE(rows.at(2).messageId, qint64(10));
    QCOMPARE(rows.at(3).messageId, qint64(5));

    QVERIFY(rows.at(0).showDateSeparator);
    QVERIFY(rows.at(1).showDateSeparator);
    QVERIFY(rows.at(2).showDateSeparator);
    QVERIFY(!rows.at(3).showDateSeparator);
    QCOMPARE(rows.at(2).separatorDate, dayStart(2));
}

void MessageGroupingModelTest::epochDateUsesItsLocalDayStart()
{
    MessageGroupingModel model;
    model.setMessages({message(1, 0, 7)});

    const QVector<GroupingRow> rows = model.rows();
    QCOMPARE(rows.size(), 1);
    QVERIFY(rows.at(0).showDateSeparator);
    QCOMPARE(rows.at(0).separatorDate, localDayStart(0));
}

void MessageGroupingModelTest::unreadDividerMarksMatchingRow()
{
    MessageGroupingModel model;
    model.setMessages({
        message(1, at(0, 8), 7),
        message(2, at(0, 9), 7),
        message(3, at(0, 10), 7),
    }, 2);

    const QVector<GroupingRow> rows = model.rows();
    QCOMPARE(rows.size(), 3);
    QVERIFY(!rows.at(0).showUnreadDivider);
    QVERIFY(rows.at(1).showUnreadDivider);
    QVERIFY(!rows.at(2).showUnreadDivider);
    QCOMPARE(model.unreadDividerIndex(), 1);
}

void MessageGroupingModelTest::unreadDividerIgnoredForZero()
{
    MessageGroupingModel model;
    model.setMessages({
        message(1, at(0, 8), 7),
        message(2, at(0, 9), 7),
    }, 0);

    for (const GroupingRow& row : model.rows())
        QVERIFY(!row.showUnreadDivider);
    QCOMPARE(model.unreadDividerIndex(), -1);
}

void MessageGroupingModelTest::unreadDividerIgnoredWhenAbsent()
{
    MessageGroupingModel model;
    model.setMessages({
        message(1, at(0, 8), 7),
        message(2, at(0, 9), 7),
    }, 99);

    for (const GroupingRow& row : model.rows())
        QVERIFY(!row.showUnreadDivider);
    QCOMPARE(model.unreadDividerIndex(), -1);
}

void MessageGroupingModelTest::unreadDividerShownAtMostOnce()
{
    MessageGroupingModel model;
    // Duplicate ids are not expected from TDLib, but the divider must still be
    // placed only once.
    model.setMessages({
        message(4, at(0, 8), 7),
        message(4, at(0, 9), 7),
        message(4, at(0, 10), 7),
    }, 4);

    const QVector<GroupingRow> rows = model.rows();
    QCOMPARE(rows.size(), 3);
    QVERIFY(rows.at(0).showUnreadDivider);
    QVERIFY(!rows.at(1).showUnreadDivider);
    QVERIFY(!rows.at(2).showUnreadDivider);
    QCOMPARE(model.unreadDividerIndex(), 0);
}

void MessageGroupingModelTest::separatorAndDividerShareRow()
{
    MessageGroupingModel model;
    model.setMessages({
        message(1, at(0, 8), 7),
        message(2, at(1, 9), 7),
    }, 2);

    const QVector<GroupingRow> rows = model.rows();
    QCOMPARE(rows.size(), 2);
    QVERIFY(rows.at(1).showDateSeparator);
    QVERIFY(rows.at(1).showUnreadDivider);
    QCOMPARE(rows.at(1).separatorDate, dayStart(1));
    QCOMPARE(model.unreadDividerIndex(), 1);
}

void MessageGroupingModelTest::sameSenderMarksOnlyConsecutiveMessages()
{
    MessageGroupingModel model;
    model.setMessages({
        message(1, at(0, 8), 7),
        message(2, at(0, 9), 7),
        message(3, at(0, 10), 8),
        message(4, at(0, 11), 7),
    });

    const QVector<GroupingRow> rows = model.rows();
    QCOMPARE(rows.size(), 4);
    QVERIFY(!rows.at(0).sameSenderAsPrevious);
    QVERIFY(rows.at(1).sameSenderAsPrevious);
    QVERIFY(!rows.at(2).sameSenderAsPrevious);
    QVERIFY(!rows.at(3).sameSenderAsPrevious);
}

void MessageGroupingModelTest::zeroSenderNeverGroups()
{
    MessageGroupingModel model;
    model.setMessages({
        message(1, at(0, 8), 0),
        message(2, at(0, 9), 0),
        message(3, at(0, 10), 5),
        message(4, at(0, 11), 0),
    });

    for (const GroupingRow& row : model.rows())
        QVERIFY(!row.sameSenderAsPrevious);
}

void MessageGroupingModelTest::sameSenderIgnoresDateBoundary()
{
    MessageGroupingModel model;
    model.setMessages({
        message(1, at(0, 23), 7),
        message(2, at(1, 1), 7),
    });

    const QVector<GroupingRow> rows = model.rows();
    QCOMPARE(rows.size(), 2);
    QVERIFY(rows.at(1).showDateSeparator);
    // Grouping follows the sender only; the separator does not reset it.
    QVERIFY(rows.at(1).sameSenderAsPrevious);
}

void MessageGroupingModelTest::clearResetsRows()
{
    MessageGroupingModel model;
    model.setMessages({message(1, at(0, 8), 7)}, 1);
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.unreadDividerIndex(), 0);

    model.clear();

    QCOMPARE(model.count(), 0);
    QVERIFY(model.rows().isEmpty());
    QCOMPARE(model.unreadDividerIndex(), -1);
}

void MessageGroupingModelTest::emitsChangedOnlyOnRealChanges()
{
    MessageGroupingModel model;
    QSignalSpy spy(&model, &MessageGroupingModel::changed);

    const QVector<MessageData> messages = {
        message(1, at(0, 8), 7),
        message(2, at(0, 9), 7),
    };

    model.setMessages(messages);
    QCOMPARE(spy.count(), 1);

    // Identical input produces identical rows and stays silent.
    model.setMessages(messages);
    QCOMPARE(spy.count(), 1);

    // Only the unread id changed, which does change the rows.
    model.setMessages(messages, 2);
    QCOMPARE(spy.count(), 2);
    model.setMessages(messages, 2);
    QCOMPARE(spy.count(), 2);

    // A message whose date crosses into the next day changes a row.
    model.setMessages({message(1, at(0, 8), 7), message(2, at(1, 9), 7)});
    QCOMPARE(spy.count(), 3);

    model.clear();
    QCOMPARE(spy.count(), 4);

    // Already empty: clearing again is a no-op.
    model.clear();
    QCOMPARE(spy.count(), 4);
}

QTEST_GUILESS_MAIN(MessageGroupingModelTest)
#include "tst_message_grouping_model.moc"
