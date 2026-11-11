#include "telegram/ScheduledMessages.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QtTest>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& request)
{
    return QJsonDocument::fromJson(QJsonDocument(request).toJson(QJsonDocument::Compact)).object();
}

qint64 idOf(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

QVector<qint64> idsOf(const QJsonObject& request)
{
    QVector<qint64> ids;
    const QJsonArray array = request.value(QStringLiteral("message_ids")).toArray();
    ids.reserve(array.size());
    for (const QJsonValue& value : array)
        ids.append(value.toVariant().toLongLong());
    return ids;
}

} // namespace

class ScheduledMessagesTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsScheduledSendState();
    void buildsOnlineSendState();
    void embedsSchedulingStateInSendOptions();
    void buildsGetScheduledMessagesRequest();
    void buildsDeleteRequestForScheduledMessages();
    void filtersDuplicateAndNonPositiveIds();
    void handlesEmptyIds();
    void preservesLargeIdsThroughJson();
};

void ScheduledMessagesTest::buildsScheduledSendState()
{
    const QJsonObject state = serialized(scheduledSendState(1'900'000'000LL));

    QCOMPARE(state.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSchedulingStateSendAtDate"));
    QCOMPARE(idOf(state, QStringLiteral("send_date")), qint64(1'900'000'000LL));
    QCOMPARE(state.value(QStringLiteral("repeat_period")).toInt(), 0);
    QCOMPARE(int(state.size()), 3);
}

void ScheduledMessagesTest::buildsOnlineSendState()
{
    const QJsonObject state = serialized(onlineSendState());

    QCOMPARE(state.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSchedulingStateSendWhenOnline"));
    QCOMPARE(int(state.size()), 1);
}

void ScheduledMessagesTest::embedsSchedulingStateInSendOptions()
{
    // The schema puts the state at messageSendOptions.scheduling_state, so the
    // built object must survive being nested in a request.
    const QJsonObject options{
        {QStringLiteral("@type"), QStringLiteral("messageSendOptions")},
        {QStringLiteral("scheduling_state"), scheduledSendState(1'900'000'000LL)},
    };
    const QJsonObject roundTripped = serialized(options);
    const QJsonObject state =
        roundTripped.value(QStringLiteral("scheduling_state")).toObject();

    QCOMPARE(state.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSchedulingStateSendAtDate"));
    QCOMPARE(idOf(state, QStringLiteral("send_date")), qint64(1'900'000'000LL));

    const QJsonObject onlineOptions{
        {QStringLiteral("@type"), QStringLiteral("messageSendOptions")},
        {QStringLiteral("scheduling_state"), onlineSendState()},
    };
    QCOMPARE(serialized(onlineOptions)
                 .value(QStringLiteral("scheduling_state"))
                 .toObject()
                 .value(QStringLiteral("@type"))
                 .toString(),
             QStringLiteral("messageSchedulingStateSendWhenOnline"));
}

void ScheduledMessagesTest::buildsGetScheduledMessagesRequest()
{
    const QJsonObject request = serialized(getScheduledMessagesRequest(42));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getChatScheduledMessages"));
    QCOMPARE(idOf(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(int(request.size()), 2);
    QVERIFY(!request.contains(QStringLiteral("message_ids")));
}

void ScheduledMessagesTest::buildsDeleteRequestForScheduledMessages()
{
    const QVector<qint64> input{100, 101, 102};
    const QJsonObject request = serialized(deleteScheduledMessagesRequest(42, input));

    // TDLib 1.8.69 has no deleteScheduledMessages; scheduled messages are
    // cancelled with deleteMessages(revoke = false).
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("deleteMessages"));
    QCOMPARE(idOf(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(request.value(QStringLiteral("message_ids")).type(), QJsonValue::Array);
    QCOMPARE(idsOf(request), input);
    QVERIFY(request.value(QStringLiteral("revoke")).isBool());
    QVERIFY(!request.value(QStringLiteral("revoke")).toBool());
    QCOMPARE(int(request.size()), 4);
}

void ScheduledMessagesTest::filtersDuplicateAndNonPositiveIds()
{
    const QVector<qint64> input{0, -3, 8, 8, 0, 9, -1};
    const QJsonObject request = serialized(deleteScheduledMessagesRequest(42, input));

    const QVector<qint64> expected{8, 9};
    QCOMPARE(idsOf(request), expected);

    // A list of only unusable ids leaves a well-formed but empty array.
    const QJsonObject allInvalid =
        serialized(deleteScheduledMessagesRequest(42, QVector<qint64>{0, -1, -1}));
    QCOMPARE(allInvalid.value(QStringLiteral("message_ids")).toArray().size(), 0);
    QCOMPARE(idsOf(allInvalid), QVector<qint64>{});
}

void ScheduledMessagesTest::handlesEmptyIds()
{
    const QVector<qint64> input;
    const QJsonObject request = serialized(deleteScheduledMessagesRequest(42, input));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("deleteMessages"));
    QCOMPARE(idOf(request, QStringLiteral("chat_id")), qint64(42));
    QVERIFY(request.value(QStringLiteral("message_ids")).isArray());
    QVERIFY(!request.value(QStringLiteral("message_ids")).isNull());
    QCOMPARE(request.value(QStringLiteral("message_ids")).toArray().size(), 0);
    QCOMPARE(idsOf(request), QVector<qint64>{});
    QVERIFY(!request.value(QStringLiteral("revoke")).toBool());
}

void ScheduledMessagesTest::preservesLargeIdsThroughJson()
{
    const qint64 chatId = -1001234567890LL;
    const qint64 messageId = 1234567890123LL;

    const QJsonObject get = serialized(getScheduledMessagesRequest(chatId));
    QCOMPARE(idOf(get, QStringLiteral("chat_id")), chatId);

    const QJsonObject remove =
        serialized(deleteScheduledMessagesRequest(chatId, QVector<qint64>{messageId}));
    QCOMPARE(idOf(remove, QStringLiteral("chat_id")), chatId);
    QCOMPARE(idsOf(remove), QVector<qint64>{messageId});

    const QJsonObject state = serialized(scheduledSendState(1'900'000'000LL));
    QCOMPARE(idOf(state, QStringLiteral("send_date")), qint64(1'900'000'000LL));
}

QTEST_GUILESS_MAIN(ScheduledMessagesTest)
#include "tst_scheduled_messages.moc"
