#include "telegram/MessageWindow.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject jsonObject(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

} // namespace

class MessageWindowTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsHistoryRequest();
    void buildsMessageRequest();
    void usesDocumentedDefaults();
    void clampsLimit();
    void clampsRadius();
    void keepsRadiusWithinLimit();
    void parsesNumericMessageId();
    void parsesStringMessageId();
    void returnsZeroForMissingMessageId();
};

void MessageWindowTest::buildsHistoryRequest()
{
    const MessageWindowRequest request = windowAroundMessageRequest(42, 1000, 15, 40);

    const QJsonObject& history = request.history;
    QCOMPARE(history.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getChatHistory"));
    QCOMPARE(history.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(42));
    QCOMPARE(history.value(QStringLiteral("from_message_id")).toVariant().toLongLong(),
             qint64(1000));
    QCOMPARE(history.value(QStringLiteral("offset")).toInt(), -15);
    QCOMPARE(history.value(QStringLiteral("limit")).toInt(), 40);
    QVERIFY(history.value(QStringLiteral("only_local")).isBool());
    QCOMPARE(history.value(QStringLiteral("only_local")).toBool(), false);

    // The transport adds correlation; the builders must not.
    QVERIFY(!history.contains(QStringLiteral("@extra")));

    // Negative chat ids (channels, basic groups) pass through untouched.
    const QJsonObject channel = windowAroundMessageRequest(-100123, 7, 1, 5).history;
    QCOMPARE(channel.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(-100123));
}

void MessageWindowTest::buildsMessageRequest()
{
    const MessageWindowRequest request = windowAroundMessageRequest(-100123, 777, 3, 9);

    const QJsonObject& message = request.message;
    QCOMPARE(message.value(QStringLiteral("@type")).toString(), QStringLiteral("getMessage"));
    QCOMPARE(message.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(-100123));
    QCOMPARE(message.value(QStringLiteral("message_id")).toVariant().toLongLong(), qint64(777));
    QVERIFY(!message.contains(QStringLiteral("@extra")));

    // The two requests always target the same message.
    QCOMPARE(request.history.value(QStringLiteral("from_message_id")).toVariant().toLongLong(),
             message.value(QStringLiteral("message_id")).toVariant().toLongLong());
    QCOMPARE(request.history.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             message.value(QStringLiteral("chat_id")).toVariant().toLongLong());
}

void MessageWindowTest::usesDocumentedDefaults()
{
    const MessageWindowRequest request = windowAroundMessageRequest(1, 2);

    QCOMPARE(request.history.value(QStringLiteral("offset")).toInt(), -15);
    QCOMPARE(request.history.value(QStringLiteral("limit")).toInt(), 40);
}

void MessageWindowTest::clampsLimit()
{
    QCOMPARE(windowAroundMessageRequest(1, 2, 0, 0).history.value(QStringLiteral("limit")).toInt(),
             1);
    QCOMPARE(windowAroundMessageRequest(1, 2, 0, -10).history.value(QStringLiteral("limit")).toInt(),
             1);
    QCOMPARE(windowAroundMessageRequest(1, 2, 0, 1).history.value(QStringLiteral("limit")).toInt(),
             1);
    QCOMPARE(windowAroundMessageRequest(1, 2, 0, 100).history.value(QStringLiteral("limit")).toInt(),
             100);
    QCOMPARE(windowAroundMessageRequest(1, 2, 0, 101).history.value(QStringLiteral("limit")).toInt(),
             100);
    QCOMPARE(windowAroundMessageRequest(1, 2, 0, 5000).history.value(QStringLiteral("limit")).toInt(),
             100);
}

void MessageWindowTest::clampsRadius()
{
    // A zero radius asks for exactly the message (offset 0).
    const QJsonObject exact = windowAroundMessageRequest(1, 2, 0, 40).history;
    QCOMPARE(exact.value(QStringLiteral("offset")).toInt(), 0);
    QVERIFY(exact.value(QStringLiteral("offset")).isDouble());

    // Negative radii are treated as zero.
    QCOMPARE(windowAroundMessageRequest(1, 2, -5, 40).history.value(QStringLiteral("offset")).toInt(),
             0);

    // TDLib accepts offsets down to -99.
    QCOMPARE(windowAroundMessageRequest(1, 2, 99, 100).history.value(QStringLiteral("offset")).toInt(),
             -99);
    QCOMPARE(windowAroundMessageRequest(1, 2, 100, 100).history.value(QStringLiteral("offset")).toInt(),
             -99);
    QCOMPARE(windowAroundMessageRequest(1, 2, 500, 100).history.value(QStringLiteral("offset")).toInt(),
             -99);
}

void MessageWindowTest::keepsRadiusWithinLimit()
{
    // TDLib requires -offset <= limit, so a radius larger than the limit is
    // reduced to the limit rather than raising the limit.
    const QJsonObject narrow = windowAroundMessageRequest(1, 2, 80, 10).history;
    QCOMPARE(narrow.value(QStringLiteral("offset")).toInt(), -10);
    QCOMPARE(narrow.value(QStringLiteral("limit")).toInt(), 10);

    const QJsonObject defaultLimit = windowAroundMessageRequest(1, 2, 100, 40).history;
    QCOMPARE(defaultLimit.value(QStringLiteral("offset")).toInt(), -40);
    QCOMPARE(defaultLimit.value(QStringLiteral("limit")).toInt(), 40);

    // A non-positive limit is clamped up to 1, and the radius follows it.
    const QJsonObject zeroLimit = windowAroundMessageRequest(1, 2, 15, 0).history;
    QCOMPARE(zeroLimit.value(QStringLiteral("limit")).toInt(), 1);
    QCOMPARE(zeroLimit.value(QStringLiteral("offset")).toInt(), -1);
}

void MessageWindowTest::parsesNumericMessageId()
{
    QCOMPARE(messageIdFromResponse(jsonObject(R"json({"@type": "message", "id": 123})json")),
             qint64(123));
    QCOMPARE(messageIdFromResponse(jsonObject(R"json({"@type": "message", "id": 0})json")),
             qint64(0));
}

void MessageWindowTest::parsesStringMessageId()
{
    // TDLib emits int64 as a JSON string when it exceeds the safe double range.
    QCOMPARE(messageIdFromResponse(
                 jsonObject(R"json({"@type": "message", "id": "9007199254740993"})json")),
             qint64(9007199254740993LL));
}

void MessageWindowTest::returnsZeroForMissingMessageId()
{
    QCOMPARE(messageIdFromResponse(QJsonObject{}), qint64(0));
    QCOMPARE(messageIdFromResponse(jsonObject(R"json({"@type": "message"})json")), qint64(0));
    QCOMPARE(messageIdFromResponse(jsonObject(R"json({"@type": "error", "code": 400,
                                                    "message": "Message not found"})json")),
             qint64(0));
    QCOMPARE(messageIdFromResponse(jsonObject(R"json({"id": null})json")), qint64(0));
    QCOMPARE(messageIdFromResponse(jsonObject(R"json({"id": true})json")), qint64(0));
}

QTEST_GUILESS_MAIN(MessageWindowTest)
#include "tst_message_window.moc"
