#include "telegram/StarActions.h"

#include "telegram/PaymentRequests.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

bool boolAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toBool();
}

QJsonObject ownerOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("owner_id")).toObject();
}

} // namespace

class StarActionsTest final : public QObject {
    Q_OBJECT

private slots:
    void balanceUsesStarTransactions();
    void balanceMatchesThePaymentBuilder();
    void balanceCarriesTheFirstPageOnly();
    void balanceSurvivesSerialization();
    void paymentOptionsHasOnlyItsType();
    void withdrawalTrimsPassword();
    void withdrawalRejectsBlankPassword();
    void withdrawalSurvivesSerialization();
    void revenueStatisticsRejectsNonPositiveOwner();
    void revenueStatisticsNamesTheOwner();
    void revenueStatisticsPreservesLargeOwner();
    void refundRejectsNonPositiveUser();
    void refundRejectsBlankChargeId();
    void refundTrimsChargeId();
    void refundPreservesLargeUser();
    void subscriptionsRequestShape();
    void subscriptionsSurviveSerialization();
    void editSubscriptionRejectsBlankId();
    void editSubscriptionTrimsId();
    void editSubscriptionForwardsCanceledFlag();
    void editSubscriptionSurvivesSerialization();
};

void StarActionsTest::balanceUsesStarTransactions()
{
    const QJsonObject request = serialized(getStarBalanceRequest());

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getStarTransactions"));
    QCOMPARE(request.size(), 6);
    QVERIFY(!request.contains(QStringLiteral("@extra")));

    const QJsonObject owner = ownerOf(request);
    QCOMPARE(owner.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSenderUser"));
    QCOMPARE(owner.size(), 2);
    QCOMPARE(numberAt(owner, QStringLiteral("user_id")), qint64(0));
}

void StarActionsTest::balanceMatchesThePaymentBuilder()
{
    // The balance is read from the existing getStarTransactions builder rather
    // than from a second copy of its shape.
    QCOMPARE(getStarBalanceRequest(), getStarTransactionsRequest(0, 1));
}

void StarActionsTest::balanceCarriesTheFirstPageOnly()
{
    const QJsonObject request = getStarBalanceRequest();

    // Empty subscription and offset and a null direction select every
    // transaction, and limit 1 is the smallest page that still carries
    // star_amount.
    QCOMPARE(request.value(QStringLiteral("subscription_id")).toString(),
             QString());
    QCOMPARE(request.value(QStringLiteral("offset")).toString(), QString());
    QVERIFY(request.contains(QStringLiteral("direction")));
    QVERIFY(request.value(QStringLiteral("direction")).isNull());
    QCOMPARE(numberAt(request, QStringLiteral("limit")), qint64(1));
}

void StarActionsTest::balanceSurvivesSerialization()
{
    const QJsonObject request = serialized(getStarBalanceRequest());

    QCOMPARE(numberAt(request, QStringLiteral("limit")), qint64(1));
    QVERIFY(request.value(QStringLiteral("direction")).isNull());
    QCOMPARE(numberAt(ownerOf(request), QStringLiteral("user_id")), qint64(0));
}

void StarActionsTest::paymentOptionsHasOnlyItsType()
{
    const QJsonObject request = serialized(getStarPaymentOptionsRequest());

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getStarPaymentOptions"));
    QCOMPARE(request.size(), 1);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void StarActionsTest::withdrawalTrimsPassword()
{
    const QJsonObject request = serialized(
        getStarWithdrawalUrlRequest(QStringLiteral("  secret pass  ")));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getStarWithdrawalUrl"));
    QCOMPARE(request.value(QStringLiteral("password")).toString(),
             QStringLiteral("secret pass"));
    QCOMPARE(request.size(), 2);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void StarActionsTest::withdrawalRejectsBlankPassword()
{
    // A blank password names no 2-step verification password, so the caller
    // gets an empty object instead of a request TDLib would reject.
    QVERIFY(getStarWithdrawalUrlRequest(QString()).isEmpty());
    QVERIFY(getStarWithdrawalUrlRequest(QStringLiteral("   ")).isEmpty());
    QVERIFY(getStarWithdrawalUrlRequest(QStringLiteral("\t\r\n")).isEmpty());
}

void StarActionsTest::withdrawalSurvivesSerialization()
{
    const QJsonObject request = serialized(
        getStarWithdrawalUrlRequest(QStringLiteral("  p@ss word  ")));

    QCOMPARE(request.value(QStringLiteral("password")).toString(),
             QStringLiteral("p@ss word"));
    QCOMPARE(request.size(), 2);
}

void StarActionsTest::revenueStatisticsRejectsNonPositiveOwner()
{
    QVERIFY(getStarRevenueStatisticsRequest(0).isEmpty());
    QVERIFY(getStarRevenueStatisticsRequest(-1).isEmpty());
    QVERIFY(getStarRevenueStatisticsRequest(-9007199254740993LL).isEmpty());
}

void StarActionsTest::revenueStatisticsNamesTheOwner()
{
    const QJsonObject request = serialized(getStarRevenueStatisticsRequest(777));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getStarRevenueStatistics"));
    QCOMPARE(request.size(), 3);
    QVERIFY(!request.contains(QStringLiteral("@extra")));

    const QJsonObject owner = ownerOf(request);
    QCOMPARE(owner.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSenderUser"));
    QCOMPARE(owner.size(), 2);
    QCOMPARE(numberAt(owner, QStringLiteral("user_id")), qint64(777));

    // The graph is requested for the light theme.
    QVERIFY(request.contains(QStringLiteral("is_dark")));
    QVERIFY(!boolAt(request, QStringLiteral("is_dark")));
}

void StarActionsTest::revenueStatisticsPreservesLargeOwner()
{
    // Above 2^53, so a double round-trip would lose precision.
    const qint64 ownerId = 9007199254740993LL;

    const QJsonObject request = serialized(getStarRevenueStatisticsRequest(ownerId));
    QCOMPARE(numberAt(ownerOf(request), QStringLiteral("user_id")), ownerId);
}

void StarActionsTest::refundRejectsNonPositiveUser()
{
    QVERIFY(refundStarPaymentRequest(0, QStringLiteral("charge-1")).isEmpty());
    QVERIFY(refundStarPaymentRequest(-1, QStringLiteral("charge-1")).isEmpty());
}

void StarActionsTest::refundRejectsBlankChargeId()
{
    QVERIFY(refundStarPaymentRequest(42, QString()).isEmpty());
    QVERIFY(refundStarPaymentRequest(42, QStringLiteral("   ")).isEmpty());
    QVERIFY(refundStarPaymentRequest(42, QStringLiteral("\t\r\n")).isEmpty());
}

void StarActionsTest::refundTrimsChargeId()
{
    const QJsonObject request = serialized(
        refundStarPaymentRequest(42, QStringLiteral("  charge-1  ")));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("refundStarPayment"));
    QCOMPARE(numberAt(request, QStringLiteral("user_id")), qint64(42));
    QCOMPARE(request.value(QStringLiteral("telegram_payment_charge_id")).toString(),
             QStringLiteral("charge-1"));
    QCOMPARE(request.size(), 3);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void StarActionsTest::refundPreservesLargeUser()
{
    const qint64 userId = 9007199254740993LL;

    const QJsonObject request =
        serialized(refundStarPaymentRequest(userId, QStringLiteral("charge-1")));
    QCOMPARE(numberAt(request, QStringLiteral("user_id")), userId);
}

void StarActionsTest::subscriptionsRequestShape()
{
    const QJsonObject request = serialized(getStarSubscriptionsRequest());

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getStarSubscriptions"));
    QCOMPARE(request.size(), 3);
    QVERIFY(!request.contains(QStringLiteral("@extra")));

    // Every subscription, starting at the first chunk.
    QVERIFY(request.contains(QStringLiteral("only_expiring")));
    QVERIFY(!boolAt(request, QStringLiteral("only_expiring")));
    QVERIFY(request.value(QStringLiteral("offset")).isString());
    QCOMPARE(request.value(QStringLiteral("offset")).toString(), QString());
}

void StarActionsTest::subscriptionsSurviveSerialization()
{
    const QJsonObject request = serialized(getStarSubscriptionsRequest());

    QVERIFY(!boolAt(request, QStringLiteral("only_expiring")));
    QCOMPARE(request.value(QStringLiteral("offset")).toString(), QString());
}

void StarActionsTest::editSubscriptionRejectsBlankId()
{
    QVERIFY(editStarSubscriptionRequest(QString(), true).isEmpty());
    QVERIFY(editStarSubscriptionRequest(QStringLiteral("   "), true).isEmpty());
    QVERIFY(editStarSubscriptionRequest(QStringLiteral("\t\r\n"), false).isEmpty());
}

void StarActionsTest::editSubscriptionTrimsId()
{
    const QJsonObject request = serialized(
        editStarSubscriptionRequest(QStringLiteral("  sub-1  "), false));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("editStarSubscription"));
    QCOMPARE(request.value(QStringLiteral("subscription_id")).toString(),
             QStringLiteral("sub-1"));
    QVERIFY(!boolAt(request, QStringLiteral("is_canceled")));
    QCOMPARE(request.size(), 3);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void StarActionsTest::editSubscriptionForwardsCanceledFlag()
{
    const QJsonObject canceled =
        editStarSubscriptionRequest(QStringLiteral("sub-1"), true);
    QVERIFY(boolAt(canceled, QStringLiteral("is_canceled")));

    const QJsonObject reenabled =
        editStarSubscriptionRequest(QStringLiteral("sub-1"), false);
    QVERIFY(!boolAt(reenabled, QStringLiteral("is_canceled")));
}

void StarActionsTest::editSubscriptionSurvivesSerialization()
{
    const QJsonObject request = serialized(
        editStarSubscriptionRequest(QStringLiteral("opaque-sub-id"), true));

    QCOMPARE(request.value(QStringLiteral("subscription_id")).toString(),
             QStringLiteral("opaque-sub-id"));
    QVERIFY(boolAt(request, QStringLiteral("is_canceled")));
}

QTEST_GUILESS_MAIN(StarActionsTest)
#include "tst_star_actions.moc"
