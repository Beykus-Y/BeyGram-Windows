#include "StarActions.h"

#include "PaymentRequests.h"

namespace beygram::telegram {
namespace {

// The pinned schema has no "current user" MessageSender token. User 0 is the
// documented placeholder used by PaymentRequests.h when no account id is known.
constexpr qint64 kUnknownOwnerUserId = 0;

// Balance is read from the star_amount member of a starTransactions answer, so
// the smallest possible page is enough.
constexpr int kBalancePageLimit = 1;

QJsonObject messageSenderUser(qint64 userId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageSenderUser")},
        {QStringLiteral("user_id"), userId},
    };
}

} // namespace

QJsonObject getStarBalanceRequest()
{
    return getStarTransactionsRequest(kUnknownOwnerUserId, 0, kBalancePageLimit);
}

QJsonObject getStarPaymentOptionsRequest()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getStarPaymentOptions")},
    };
}

QJsonObject getStarWithdrawalUrlRequest(const QString& password)
{
    const QString trimmedPassword = password.trimmed();
    if (trimmedPassword.isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getStarWithdrawalUrl")},
        {QStringLiteral("password"), trimmedPassword},
    };
}

QJsonObject getStarRevenueStatisticsRequest(qint64 ownerId)
{
    if (ownerId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getStarRevenueStatistics")},
        {QStringLiteral("owner_id"), messageSenderUser(ownerId)},
        // The schema's theme flag; this client asks for the light graph.
        {QStringLiteral("is_dark"), false},
    };
}

QJsonObject refundStarPaymentRequest(qint64 userId,
                                     const QString& telegramPaymentChargeId)
{
    if (userId <= 0)
        return {};

    const QString chargeId = telegramPaymentChargeId.trimmed();
    if (chargeId.isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("refundStarPayment")},
        {QStringLiteral("user_id"), userId},
        {QStringLiteral("telegram_payment_charge_id"), chargeId},
    };
}

QJsonObject getStarSubscriptionsRequest()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getStarSubscriptions")},
        // False selects every subscription, not only the expiring ones.
        {QStringLiteral("only_expiring"), false},
        // Empty offset asks for the first chunk.
        {QStringLiteral("offset"), QString()},
    };
}

QJsonObject editStarSubscriptionRequest(const QString& subscriptionId,
                                        bool isCanceled)
{
    const QString trimmedId = subscriptionId.trimmed();
    if (trimmedId.isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("editStarSubscription")},
        {QStringLiteral("subscription_id"), trimmedId},
        {QStringLiteral("is_canceled"), isCanceled},
    };
}

} // namespace beygram::telegram
