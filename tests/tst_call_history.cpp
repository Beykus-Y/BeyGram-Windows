#include "telegram/CallHistory.h"

#include <QJsonDocument>
#include <QtTest>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& request)
{
    return QJsonDocument::fromJson(QJsonDocument(request).toJson(QJsonDocument::Compact)).object();
}

} // namespace

class CallHistoryTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesIncomingCall();
    void parsesOutgoingCall();
    void parsesMissedCall();
    void parsesCancelledOutgoingCall();
    void parsesDeclinedCall();
    void parsesVideoCall();
    void parsesDisconnectedHungUpAndUpgradedCalls();
    void readsLegacyNestedCall();
    void reportsLegacyCallStateError();
    void ignoresNonCallMessage();
    void reportsUnknownDiscardReason();
    void clampsDurationAndReadsStringIds();
    void buildsSearchRequest();
};

void CallHistoryTest::parsesIncomingCall()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "@type": "message",
        "id": 501,
        "chat_id": 42,
        "date": 1700000100,
        "is_outgoing": false,
        "content": {
            "@type": "messageCall",
            "unique_id": 9876543210123,
            "is_video": false,
            "discard_reason": { "@type": "callDiscardReasonEmpty" },
            "duration": 42
        }
    })")
                                    .object();

    const CallRecord record = callFromMessage(message);
    QCOMPARE(record.messageId, qint64(501));
    QCOMPARE(record.chatId, qint64(42));
    QCOMPARE(record.date, qint64(1700000100));
    QVERIFY(!record.outgoing);
    QVERIFY(!record.video);
    QCOMPARE(record.status, QStringLiteral("ended"));
    QCOMPARE(record.duration, 42);
}

void CallHistoryTest::parsesOutgoingCall()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "@type": "message",
        "id": 502,
        "chat_id": 42,
        "date": 1700000200,
        "is_outgoing": true,
        "content": {
            "@type": "messageCall",
            "unique_id": 0,
            "is_video": false,
            "discard_reason": { "@type": "callDiscardReasonEmpty" },
            "duration": 125
        }
    })")
                                    .object();

    const CallRecord record = callFromMessage(message);
    QVERIFY(record.outgoing);
    QVERIFY(!record.video);
    QCOMPARE(record.status, QStringLiteral("ended"));
    QCOMPARE(record.duration, 125);
}

void CallHistoryTest::parsesMissedCall()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "@type": "message",
        "id": 503,
        "chat_id": 42,
        "date": 1700000300,
        "is_outgoing": false,
        "content": {
            "@type": "messageCall",
            "unique_id": 7001,
            "is_video": false,
            "discard_reason": { "@type": "callDiscardReasonMissed" },
            "duration": 0
        }
    })")
                                    .object();

    const CallRecord record = callFromMessage(message);
    QVERIFY(!record.outgoing);
    QCOMPARE(record.status, QStringLiteral("missed"));
    QCOMPARE(record.duration, 0);
}

void CallHistoryTest::parsesCancelledOutgoingCall()
{
    // TDLib reports a caller who gave up with the same reason as a missed
    // incoming call; only the direction separates the two.
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "@type": "message",
        "id": 504,
        "chat_id": 42,
        "date": 1700000400,
        "is_outgoing": true,
        "content": {
            "@type": "messageCall",
            "is_video": false,
            "discard_reason": { "@type": "callDiscardReasonMissed" },
            "duration": 0
        }
    })")
                                    .object();

    const CallRecord record = callFromMessage(message);
    QVERIFY(record.outgoing);
    QCOMPARE(record.status, QStringLiteral("cancelled"));
}

void CallHistoryTest::parsesDeclinedCall()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "@type": "message",
        "id": 505,
        "chat_id": 42,
        "date": 1700000500,
        "is_outgoing": true,
        "content": {
            "@type": "messageCall",
            "is_video": false,
            "discard_reason": { "@type": "callDiscardReasonDeclined" },
            "duration": 0
        }
    })")
                                    .object();

    const CallRecord record = callFromMessage(message);
    QVERIFY(record.outgoing);
    QCOMPARE(record.status, QStringLiteral("declined"));
    QCOMPARE(record.duration, 0);
}

void CallHistoryTest::parsesVideoCall()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "@type": "message",
        "id": 506,
        "chat_id": -1001234567890,
        "date": 1700000600,
        "is_outgoing": false,
        "content": {
            "@type": "messageCall",
            "unique_id": 7002,
            "is_video": true,
            "discard_reason": { "@type": "callDiscardReasonEmpty" },
            "duration": 300
        }
    })")
                                    .object();

    const CallRecord record = callFromMessage(message);
    QVERIFY(!record.outgoing);
    QVERIFY(record.video);
    QCOMPARE(record.status, QStringLiteral("ended"));
    QCOMPARE(record.duration, 300);
    QCOMPARE(record.chatId, qint64(-1001234567890LL));
}

void CallHistoryTest::parsesDisconnectedHungUpAndUpgradedCalls()
{
    const QJsonObject disconnected = QJsonDocument::fromJson(R"({
        "id": 507,
        "chat_id": 42,
        "content": {
            "@type": "messageCall",
            "is_video": false,
            "discard_reason": { "@type": "callDiscardReasonDisconnected" },
            "duration": 61
        }
    })")
                                         .object();
    QCOMPARE(callFromMessage(disconnected).status, QStringLiteral("disconnected"));
    QCOMPARE(callFromMessage(disconnected).duration, 61);

    const QJsonObject hungUp = QJsonDocument::fromJson(R"({
        "id": 508,
        "chat_id": 42,
        "content": {
            "@type": "messageCall",
            "is_video": false,
            "discard_reason": { "@type": "callDiscardReasonHungUp" },
            "duration": 12
        }
    })")
                                   .object();
    QCOMPARE(callFromMessage(hungUp).status, QStringLiteral("ended"));

    const QJsonObject upgraded = QJsonDocument::fromJson(R"({
        "id": 509,
        "chat_id": 42,
        "content": {
            "@type": "messageCall",
            "is_video": true,
            "discard_reason": {
                "@type": "callDiscardReasonUpgradeToGroupCall",
                "invite_link": "https://t.me/+abcdef"
            },
            "duration": 8
        }
    })")
                                      .object();
    const CallRecord upgradedRecord = callFromMessage(upgraded);
    QCOMPARE(upgradedRecord.status, QStringLiteral("upgraded"));
    QVERIFY(upgradedRecord.video);
}

void CallHistoryTest::readsLegacyNestedCall()
{
    // Older revisions nested the live call object, which carried the direction
    // and the discard reason in its state.
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "@type": "message",
        "id": 510,
        "chat_id": 42,
        "date": 1700000700,
        "content": {
            "@type": "messageCall",
            "call": {
                "@type": "call",
                "id": 1,
                "user_id": 4242,
                "is_outgoing": true,
                "is_video": true,
                "state": {
                    "@type": "callStateDiscarded",
                    "reason": { "@type": "callDiscardReasonDeclined" },
                    "need_rating": false,
                    "need_debug_information": false,
                    "need_log": false
                }
            },
            "discard_reason": { "@type": "callDiscardReasonDeclined" }
        }
    })")
                                    .object();

    const CallRecord record = callFromMessage(message);
    QCOMPARE(record.messageId, qint64(510));
    QVERIFY(record.outgoing);
    QVERIFY(record.video);
    QCOMPARE(record.status, QStringLiteral("declined"));

    // The message flag wins when both shapes carry a direction.
    const QJsonObject messageFlag = QJsonDocument::fromJson(R"({
        "id": 511,
        "chat_id": 42,
        "is_outgoing": false,
        "content": {
            "@type": "messageCall",
            "call": { "@type": "call", "is_outgoing": true, "is_video": false },
            "discard_reason": { "@type": "callDiscardReasonMissed" }
        }
    })")
                                        .object();
    const CallRecord flagged = callFromMessage(messageFlag);
    QVERIFY(!flagged.outgoing);
    QCOMPARE(flagged.status, QStringLiteral("missed"));
}

void CallHistoryTest::reportsLegacyCallStateError()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "id": 512,
        "chat_id": 42,
        "content": {
            "@type": "messageCall",
            "call": {
                "@type": "call",
                "is_outgoing": true,
                "is_video": false,
                "state": {
                    "@type": "callStateError",
                    "error": { "code": 400, "message": "Call error" }
                }
            }
        }
    })")
                                    .object();

    const CallRecord record = callFromMessage(message);
    QCOMPARE(record.status, QStringLiteral("failed"));
    QCOMPARE(record.duration, 0);
}

void CallHistoryTest::ignoresNonCallMessage()
{
    const CallRecord empty = callFromMessage(QJsonObject{});
    QCOMPARE(empty.messageId, qint64(0));
    QCOMPARE(empty.chatId, qint64(0));
    QCOMPARE(empty.date, qint64(0));
    QVERIFY(!empty.outgoing);
    QVERIFY(!empty.video);
    QVERIFY(empty.status.isEmpty());
    QCOMPARE(empty.duration, 0);

    const QJsonObject text = QJsonDocument::fromJson(R"({
        "@type": "message",
        "id": 513,
        "chat_id": 42,
        "content": { "@type": "messageText", "text": { "text": "hi" } }
    })")
                                 .object();
    const CallRecord textRecord = callFromMessage(text);
    QCOMPARE(textRecord.messageId, qint64(0));
    QVERIFY(textRecord.status.isEmpty());

    const QJsonObject noContent = QJsonDocument::fromJson(R"({
        "@type": "message",
        "id": 514,
        "chat_id": 42,
        "content": null
    })")
                                      .object();
    QVERIFY(callFromMessage(noContent).status.isEmpty());

    const QJsonObject wrongContent = QJsonDocument::fromJson(R"({
        "@type": "message",
        "id": 515,
        "content": "messageCall"
    })")
                                         .object();
    QVERIFY(callFromMessage(wrongContent).status.isEmpty());
}

void CallHistoryTest::reportsUnknownDiscardReason()
{
    const QJsonObject missing = QJsonDocument::fromJson(R"({
        "id": 516,
        "chat_id": 42,
        "content": {
            "@type": "messageCall",
            "is_video": false,
            "duration": 5
        }
    })")
                                    .object();
    const CallRecord missingRecord = callFromMessage(missing);
    QCOMPARE(missingRecord.status, QStringLiteral("unknown"));
    QCOMPARE(missingRecord.duration, 5);

    const QJsonObject future = QJsonDocument::fromJson(R"({
        "id": 517,
        "chat_id": 42,
        "content": {
            "@type": "messageCall",
            "is_video": false,
            "discard_reason": { "@type": "callDiscardReasonFuture" },
            "duration": 0
        }
    })")
                                   .object();
    QCOMPARE(callFromMessage(future).status, QStringLiteral("unknown"));
}

void CallHistoryTest::clampsDurationAndReadsStringIds()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "@type": "message",
        "id": "5010",
        "chat_id": "-1001234567890",
        "date": "1700000800",
        "content": {
            "@type": "messageCall",
            "is_video": false,
            "discard_reason": { "@type": "callDiscardReasonEmpty" },
            "duration": -7
        }
    })")
                                    .object();

    const CallRecord record = callFromMessage(message);
    QCOMPARE(record.messageId, qint64(5010));
    QCOMPARE(record.chatId, qint64(-1001234567890LL));
    QCOMPARE(record.date, qint64(1700000800));
    QCOMPARE(record.duration, 0);
}

void CallHistoryTest::buildsSearchRequest()
{
    const QJsonObject request = serialized(searchCallMessagesRequest(qint64(0), 50));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("searchCallMessages"));
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 50);
    QVERIFY(request.value(QStringLiteral("only_missed")).isBool());
    QVERIFY(!request.value(QStringLiteral("only_missed")).toBool(true));
    QVERIFY(request.value(QStringLiteral("offset")).isString());
    QVERIFY(request.value(QStringLiteral("offset")).toString().isEmpty());
    // The pinned schema pages with an opaque offset, not a message id.
    QVERIFY(!request.contains(QStringLiteral("from_message_id")));

    // TDLib only accepts limits in 1..100.
    QCOMPARE(serialized(searchCallMessagesRequest(qint64(0), 0))
                 .value(QStringLiteral("limit"))
                 .toInt(),
             1);
    QCOMPARE(serialized(searchCallMessagesRequest(qint64(0), -5))
                 .value(QStringLiteral("limit"))
                 .toInt(),
             1);
    QCOMPARE(serialized(searchCallMessagesRequest(qint64(0), 500))
                 .value(QStringLiteral("limit"))
                 .toInt(),
             100);

    // A fromMessageId is accepted but never reaches the wire as a number.
    const QJsonObject paged = serialized(searchCallMessagesRequest(qint64(1234567890123LL), 100));
    QCOMPARE(paged.value(QStringLiteral("offset")).toString(), QString());
    QCOMPARE(paged.value(QStringLiteral("limit")).toInt(), 100);
}

QTEST_GUILESS_MAIN(CallHistoryTest)
#include "tst_call_history.moc"
