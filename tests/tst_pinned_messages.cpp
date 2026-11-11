#include "telegram/PinnedMessages.h"

#include <QByteArray>
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

QJsonObject updateFromJson(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

} // namespace

class PinnedMessagesTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsGetPinnedMessageRequest();
    void buildsPinRequestShape();
    void honoursPinFlags();
    void unpinsAllWithZeroOrNegativeId();
    void unpinsSingleMessage();
    void readsPinnedIdFromNumber();
    void readsPinnedIdFromString();
    void ignoresMissingOrInvalidPinnedId();
    void readsPinnedIdFromMessageIsPinned();
    void preservesLargeIdsThroughJson();
};

void PinnedMessagesTest::buildsGetPinnedMessageRequest()
{
    const QJsonObject request = serialized(getChatPinnedMessageRequest(42));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getChatPinnedMessage"));
    QCOMPARE(idOf(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(int(request.size()), 2);
    QVERIFY(!request.contains(QStringLiteral("message_id")));
}

void PinnedMessagesTest::buildsPinRequestShape()
{
    const QJsonObject request = serialized(pinChatMessageRequest(42, 900));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(), QStringLiteral("pinChatMessage"));
    QCOMPARE(idOf(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(idOf(request, QStringLiteral("message_id")), qint64(900));
    QCOMPARE(request.value(QStringLiteral("disable_notification")).type(), QJsonValue::Bool);
    QCOMPARE(request.value(QStringLiteral("only_for_self")).type(), QJsonValue::Bool);
    QVERIFY(!request.value(QStringLiteral("disable_notification")).toBool());
    QVERIFY(!request.value(QStringLiteral("only_for_self")).toBool());
    QCOMPARE(int(request.size()), 5);
}

void PinnedMessagesTest::honoursPinFlags()
{
    const QJsonObject silent =
        serialized(pinChatMessageRequest(42, 900, true, false));
    QVERIFY(silent.value(QStringLiteral("disable_notification")).toBool());
    QVERIFY(!silent.value(QStringLiteral("only_for_self")).toBool());

    const QJsonObject personal =
        serialized(pinChatMessageRequest(42, 900, false, true));
    QVERIFY(!personal.value(QStringLiteral("disable_notification")).toBool());
    QVERIFY(personal.value(QStringLiteral("only_for_self")).toBool());

    const QJsonObject both = serialized(pinChatMessageRequest(42, 900, true, true));
    QVERIFY(both.value(QStringLiteral("disable_notification")).toBool());
    QVERIFY(both.value(QStringLiteral("only_for_self")).toBool());
}

void PinnedMessagesTest::unpinsAllWithZeroOrNegativeId()
{
    const QJsonObject byDefault = serialized(unpinChatMessageRequest(42));
    QCOMPARE(byDefault.value(QStringLiteral("@type")).toString(),
             QStringLiteral("unpinAllChatMessages"));
    QCOMPARE(idOf(byDefault, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(int(byDefault.size()), 2);
    QVERIFY(!byDefault.contains(QStringLiteral("message_id")));

    const QJsonObject explicitZero = serialized(unpinChatMessageRequest(42, 0));
    QCOMPARE(QJsonDocument(explicitZero).toJson(QJsonDocument::Compact),
             QJsonDocument(byDefault).toJson(QJsonDocument::Compact));

    const QJsonObject negative = serialized(unpinChatMessageRequest(42, -7));
    QCOMPARE(QJsonDocument(negative).toJson(QJsonDocument::Compact),
             QJsonDocument(byDefault).toJson(QJsonDocument::Compact));
}

void PinnedMessagesTest::unpinsSingleMessage()
{
    const QJsonObject request = serialized(unpinChatMessageRequest(42, 900));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("unpinChatMessage"));
    QCOMPARE(idOf(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(idOf(request, QStringLiteral("message_id")), qint64(900));
    QCOMPARE(int(request.size()), 3);
}

void PinnedMessagesTest::readsPinnedIdFromNumber()
{
    const QJsonObject update = updateFromJson(R"({
        "@type": "updateChatPinnedMessage",
        "chat_id": 42,
        "pinned_message_id": 900
    })");

    QCOMPARE(pinnedMessageIdFromUpdate(update), qint64(900));

    const QJsonObject unpinned = updateFromJson(R"({
        "@type": "updateChatPinnedMessage",
        "chat_id": 42,
        "pinned_message_id": 0
    })");
    QCOMPARE(pinnedMessageIdFromUpdate(unpinned), qint64(0));
}

void PinnedMessagesTest::readsPinnedIdFromString()
{
    const QJsonObject update = updateFromJson(R"({
        "@type": "updateChatPinnedMessage",
        "chat_id": "42",
        "pinned_message_id": "1234567890123"
    })");

    QCOMPARE(pinnedMessageIdFromUpdate(update), qint64(1234567890123LL));
}

void PinnedMessagesTest::ignoresMissingOrInvalidPinnedId()
{
    QCOMPARE(pinnedMessageIdFromUpdate(QJsonObject{}), qint64(0));

    const QJsonObject missingId = updateFromJson(R"({
        "@type": "updateChatPinnedMessage",
        "chat_id": 42
    })");
    QCOMPARE(pinnedMessageIdFromUpdate(missingId), qint64(0));

    const QJsonObject nullId = updateFromJson(R"({
        "@type": "updateChatPinnedMessage",
        "pinned_message_id": null
    })");
    QCOMPARE(pinnedMessageIdFromUpdate(nullId), qint64(0));

    const QJsonObject boolId = updateFromJson(R"({
        "@type": "updateChatPinnedMessage",
        "pinned_message_id": true
    })");
    QCOMPARE(pinnedMessageIdFromUpdate(boolId), qint64(0));

    const QJsonObject objectId = updateFromJson(R"({
        "@type": "updateChatPinnedMessage",
        "pinned_message_id": { "id": 900 }
    })");
    QCOMPARE(pinnedMessageIdFromUpdate(objectId), qint64(0));

    const QJsonObject negativeId = updateFromJson(R"({
        "@type": "updateChatPinnedMessage",
        "pinned_message_id": -5
    })");
    QCOMPARE(pinnedMessageIdFromUpdate(negativeId), qint64(0));

    const QJsonObject otherUpdate = updateFromJson(R"({
        "@type": "updateChatLastMessage",
        "chat_id": 42,
        "pinned_message_id": 900
    })");
    QCOMPARE(pinnedMessageIdFromUpdate(otherUpdate), qint64(0));
}

void PinnedMessagesTest::readsPinnedIdFromMessageIsPinned()
{
    // TDLib 1.7.0+ replaced updateChatPinnedMessage with this update.
    const QJsonObject pinned = updateFromJson(R"({
        "@type": "updateMessageIsPinned",
        "chat_id": 42,
        "message_id": 900,
        "is_pinned": true
    })");
    QCOMPARE(pinnedMessageIdFromUpdate(pinned), qint64(900));

    const QJsonObject unpinned = updateFromJson(R"({
        "@type": "updateMessageIsPinned",
        "chat_id": 42,
        "message_id": 900,
        "is_pinned": false
    })");
    QCOMPARE(pinnedMessageIdFromUpdate(unpinned), qint64(0));

    const QJsonObject stringId = updateFromJson(R"({
        "@type": "updateMessageIsPinned",
        "message_id": "901",
        "is_pinned": true
    })");
    QCOMPARE(pinnedMessageIdFromUpdate(stringId), qint64(901));

    const QJsonObject missingFlag = updateFromJson(R"({
        "@type": "updateMessageIsPinned",
        "message_id": 900
    })");
    QCOMPARE(pinnedMessageIdFromUpdate(missingFlag), qint64(0));
}

void PinnedMessagesTest::preservesLargeIdsThroughJson()
{
    const qint64 chatId = -1001234567890LL;
    const qint64 messageId = 1234567890123LL;

    const QJsonObject pin = serialized(pinChatMessageRequest(chatId, messageId, true, true));
    QCOMPARE(idOf(pin, QStringLiteral("chat_id")), chatId);
    QCOMPARE(idOf(pin, QStringLiteral("message_id")), messageId);

    const QJsonObject unpin = serialized(unpinChatMessageRequest(chatId, messageId));
    QCOMPARE(idOf(unpin, QStringLiteral("chat_id")), chatId);
    QCOMPARE(idOf(unpin, QStringLiteral("message_id")), messageId);

    const QJsonObject get = serialized(getChatPinnedMessageRequest(chatId));
    QCOMPARE(idOf(get, QStringLiteral("chat_id")), chatId);
}

QTEST_GUILESS_MAIN(PinnedMessagesTest)
#include "tst_pinned_messages.moc"
