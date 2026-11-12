#include "telegram/PaidReactions.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVector>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

QJsonObject updateFromJson(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

} // namespace

class PaidReactionsTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsSendRequest();
    void forwardsAnonymousFlag();
    void rejectsInvalidIds();
    void acceptsBoundaryStarCounts();
    void rejectsStarCountOutsideRange();
    void preservesLargeIdsThroughSerialization();
    void buildsPrivacyRequest();
    void parsesPaidReactionRows();
    void readsFlattenedSenderIds();
    void clampsCounts();
    void skipsMalformedRows();
    void preservesOrderAndDuplicateSenders();
    void ignoresMissingOrMalformedList();
};

void PaidReactionsTest::buildsSendRequest()
{
    const QJsonObject request =
        serialized(sendPaidReactionRequest(42, 99, 50, false));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("sendPaidReaction"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(numberAt(request, QStringLiteral("message_id")), qint64(99));
    QCOMPARE(numberAt(request, QStringLiteral("star_count")), qint64(50));
    QVERIFY(request.contains(QStringLiteral("is_anonymous")));
    QVERIFY(!request.value(QStringLiteral("is_anonymous")).toBool(true));
    QCOMPARE(request.size(), 5);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void PaidReactionsTest::forwardsAnonymousFlag()
{
    const QJsonObject anonymous =
        serialized(sendPaidReactionRequest(42, 99, 1, true));
    QVERIFY(anonymous.value(QStringLiteral("is_anonymous")).toBool(false));

    const QJsonObject publicReaction =
        serialized(sendPaidReactionRequest(42, 99, 1, false));
    QVERIFY(!publicReaction.value(QStringLiteral("is_anonymous")).toBool(true));
}

void PaidReactionsTest::rejectsInvalidIds()
{
    QVERIFY(sendPaidReactionRequest(0, 99, 10, false).isEmpty());
    QVERIFY(sendPaidReactionRequest(-1, 99, 10, false).isEmpty());
    QVERIFY(sendPaidReactionRequest(42, 0, 10, false).isEmpty());
    QVERIFY(sendPaidReactionRequest(42, -1, 10, false).isEmpty());
}

void PaidReactionsTest::acceptsBoundaryStarCounts()
{
    const QJsonObject minimum = serialized(sendPaidReactionRequest(42, 99, 1, false));
    QCOMPARE(numberAt(minimum, QStringLiteral("star_count")), qint64(1));

    const QJsonObject maximum = serialized(sendPaidReactionRequest(42, 99, 10000, false));
    QCOMPARE(numberAt(maximum, QStringLiteral("star_count")), qint64(10000));
}

void PaidReactionsTest::rejectsStarCountOutsideRange()
{
    // A different amount than the user confirmed must never be substituted.
    QVERIFY(sendPaidReactionRequest(42, 99, 0, false).isEmpty());
    QVERIFY(sendPaidReactionRequest(42, 99, -1, false).isEmpty());
    QVERIFY(sendPaidReactionRequest(42, 99, 10001, false).isEmpty());
    QVERIFY(sendPaidReactionRequest(42, 99, std::numeric_limits<qint64>::max(), false)
                .isEmpty());
}

void PaidReactionsTest::preservesLargeIdsThroughSerialization()
{
    // Above 2^53, so a double round-trip would lose precision.
    const qint64 chatId = 9007199254740993LL;
    const qint64 messageId = 9007199254740994LL;

    const QJsonObject request =
        serialized(sendPaidReactionRequest(chatId, messageId, 500, true));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), chatId);
    QCOMPARE(numberAt(request, QStringLiteral("message_id")), messageId);
    QCOMPARE(numberAt(request, QStringLiteral("star_count")), qint64(500));
}

void PaidReactionsTest::buildsPrivacyRequest()
{
    const QJsonObject request = serialized(getPaidReactionPrivacyRequest());
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getPaidReactionPrivacy"));
    QCOMPARE(request.size(), 1);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void PaidReactionsTest::parsesPaidReactionRows()
{
    const QJsonObject update = updateFromJson(R"({
        "@type": "updatePaidReactions",
        "chat_id": 42,
        "message_id": 99,
        "paid_reactions": [
            {
                "@type": "paidReaction",
                "sender_id": { "@type": "messageSenderUser", "user_id": 7 },
                "star_count": 50,
                "is_anonymous": false
            },
            {
                "@type": "paidReaction",
                "sender_id": { "@type": "messageSenderUser", "user_id": "8" },
                "star_count": "1",
                "is_anonymous": true
            }
        ]
    })");

    const QVector<PaidReactionRow> rows = paidReactionsFromUpdate(update);
    QCOMPARE(rows.size(), 2);

    QCOMPARE(rows.at(0).senderUserId, qint64(7));
    QCOMPARE(rows.at(0).starCount, qint64(50));
    QVERIFY(!rows.at(0).anonymous);

    QCOMPARE(rows.at(1).senderUserId, qint64(8));
    QCOMPARE(rows.at(1).starCount, qint64(1));
    QVERIFY(rows.at(1).anonymous);
}

void PaidReactionsTest::readsFlattenedSenderIds()
{
    // A trimmed update may carry the sender id as a plain field instead of a
    // messageSenderUser object.
    const QJsonObject update = updateFromJson(R"({
        "paid_reactions": [
            { "sender_user_id": 5, "star_count": 10 },
            { "user_id": 6, "star_count": 20 },
            { "sender_id": 7, "star_count": 30 },
            { "sender_user_id": 8, "star_count": 40, "anonymous": true }
        ]
    })");

    const QVector<PaidReactionRow> rows = paidReactionsFromUpdate(update);
    QCOMPARE(rows.size(), 4);
    QCOMPARE(rows.at(0).senderUserId, qint64(5));
    QCOMPARE(rows.at(0).starCount, qint64(10));
    QCOMPARE(rows.at(1).senderUserId, qint64(6));
    QCOMPARE(rows.at(1).starCount, qint64(20));
    QCOMPARE(rows.at(2).senderUserId, qint64(7));
    QCOMPARE(rows.at(2).starCount, qint64(30));
    QCOMPARE(rows.at(3).senderUserId, qint64(8));
    QCOMPARE(rows.at(3).starCount, qint64(40));
    QVERIFY(rows.at(3).anonymous);
}

void PaidReactionsTest::clampsCounts()
{
    const QJsonObject update = updateFromJson(R"({
        "paid_reactions": [
            { "sender_user_id": 1, "star_count": -10 },
            { "sender_user_id": 2, "star_count": 0 },
            { "sender_user_id": 3, "star_count": 10001 },
            { "sender_user_id": 4, "star_count": "250" },
            { "sender_user_id": 5 }
        ]
    })");

    const QVector<PaidReactionRow> rows = paidReactionsFromUpdate(update);
    QCOMPARE(rows.size(), 5);
    QCOMPARE(rows.at(0).starCount, qint64(0));
    QCOMPARE(rows.at(1).starCount, qint64(0));
    QCOMPARE(rows.at(2).starCount, qint64(10000));
    QCOMPARE(rows.at(3).starCount, qint64(250));
    QCOMPARE(rows.at(4).starCount, qint64(0));
}

void PaidReactionsTest::skipsMalformedRows()
{
    const QJsonObject update = updateFromJson(R"({
        "paid_reactions": [
            null,
            5,
            "row",
            {},
            { "star_count": 10 },
            { "sender_user_id": 0, "star_count": 10 },
            { "sender_user_id": -5, "star_count": 10 },
            { "sender_id": { "@type": "messageSenderChat", "chat_id": 9 }, "star_count": 10 },
            { "sender_id": { "@type": "messageSenderUser", "user_id": 0 }, "star_count": 10 },
            { "sender_id": { "@type": "messageSenderUser" }, "star_count": 10 },
            { "sender_id": {}, "star_count": 10 },
            {
                "@type": "paidReaction",
                "sender_id": { "@type": "messageSenderUser", "user_id": 11 },
                "star_count": 10,
                "is_anonymous": true
            }
        ]
    })");

    const QVector<PaidReactionRow> rows = paidReactionsFromUpdate(update);
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.at(0).senderUserId, qint64(11));
    QCOMPARE(rows.at(0).starCount, qint64(10));
    QVERIFY(rows.at(0).anonymous);
}

void PaidReactionsTest::preservesOrderAndDuplicateSenders()
{
    const QJsonObject update = updateFromJson(R"({
        "paid_reactions": [
            { "sender_user_id": 5, "star_count": 1 },
            { "sender_user_id": 5, "star_count": 2 },
            { "sender_user_id": 3, "star_count": 3 }
        ]
    })");

    const QVector<PaidReactionRow> rows = paidReactionsFromUpdate(update);
    QCOMPARE(rows.size(), 3);
    QCOMPARE(rows.at(0).senderUserId, qint64(5));
    QCOMPARE(rows.at(0).starCount, qint64(1));
    QCOMPARE(rows.at(1).senderUserId, qint64(5));
    QCOMPARE(rows.at(1).starCount, qint64(2));
    QCOMPARE(rows.at(2).senderUserId, qint64(3));
    QCOMPARE(rows.at(2).starCount, qint64(3));
}

void PaidReactionsTest::ignoresMissingOrMalformedList()
{
    QVERIFY(paidReactionsFromUpdate(QJsonObject()).isEmpty());
    QVERIFY(paidReactionsFromUpdate(
                updateFromJson(R"({ "paid_reactions": null })"))
                .isEmpty());
    QVERIFY(paidReactionsFromUpdate(
                updateFromJson(R"({ "paid_reactions": {} })"))
                .isEmpty());
    QVERIFY(paidReactionsFromUpdate(
                updateFromJson(R"({ "paid_reactions": "row" })"))
                .isEmpty());
    QVERIFY(paidReactionsFromUpdate(
                updateFromJson(R"({ "paid_reactions": [] })"))
                .isEmpty());
}

QTEST_GUILESS_MAIN(PaidReactionsTest)
#include "tst_paid_reactions.moc"
