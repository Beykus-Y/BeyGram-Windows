#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Pure builders for the Telegram Stars account actions: balance, top-up
/// options, withdrawal, revenue statistics, refunds and subscriptions. They
/// shape the JSON payload only: no logging, no secrets and no @extra (the
/// transport adds the correlation id).
///
/// Shape follows the pinned td_api.tl (TDLib 1.8.69, commit 4c6377aa, see
/// docs/WINDOWS_TDLIB.md):
///
///   getStarPaymentOptions = StarPaymentOptions;
///   getStarWithdrawalUrl owner_id:MessageSender star_count:int53
///                        password:string = HttpUrl;
///   getStarRevenueStatistics owner_id:MessageSender is_dark:Bool
///                            = StarRevenueStatistics;
///   refundStarPayment user_id:int53 telegram_payment_charge_id:string = Ok;
///   getStarSubscriptions only_expiring:Bool offset:string = StarSubscriptions;
///   editStarSubscription subscription_id:string is_canceled:Bool = Ok;
///
/// There is no getStarBalance in the pinned schema: the owned amount is the
/// star_amount member of a starTransactions answer, so getStarBalanceRequest()
/// builds the smallest getStarTransactions page instead of inventing a method.

/// Builds the smallest getStarTransactions page (limit 1); the answer's
/// star_amount is the account's balance.
///
/// Like the two-argument getStarTransactionsRequest in PaymentRequests.h, this
/// signature carries no account id, so owner_id is messageSenderUser 0. User 0
/// names no account and a pinned server rejects the request; the form exists so
/// the payload can be assembled before getMe() resolves. A caller that has the
/// signed-in user id must use getStarTransactionsRequest(userId, 0, 1).
[[nodiscard]] QJsonObject getStarBalanceRequest();

/// Builds getStarPaymentOptions, which takes no arguments.
[[nodiscard]] QJsonObject getStarPaymentOptionsRequest();

/// Builds getStarWithdrawalUrl for the 2-step verification password.
///
/// password is trimmed; a blank password returns an empty object, which the
/// caller must not send. The signature carries neither the Stars owner nor the
/// number of Stars to withdraw, so the payload names only the method and the
/// password; the caller completes the pinned schema's owner_id and star_count
/// before sending.
[[nodiscard]] QJsonObject getStarWithdrawalUrlRequest(const QString& password);

/// Builds getStarRevenueStatistics for the owner named by ownerId. A
/// non-positive ownerId names no owner, so the builder returns an empty object.
/// is_dark is false: the statistics are requested for the light theme.
[[nodiscard]] QJsonObject getStarRevenueStatisticsRequest(qint64 ownerId);

/// Builds refundStarPayment for the user and the Telegram payment charge.
/// userId must be positive and telegramPaymentChargeId is trimmed; a
/// non-positive user or a blank charge id returns an empty object, which the
/// caller must not send.
[[nodiscard]] QJsonObject refundStarPaymentRequest(
    qint64 userId, const QString& telegramPaymentChargeId);

/// Builds the first getStarSubscriptions page: only_expiring is false (every
/// subscription) and offset is the empty cursor. Later pages use the opaque
/// starSubscriptions.next_offset cursor, which this argument-less form cannot
/// carry.
[[nodiscard]] QJsonObject getStarSubscriptionsRequest();

/// Builds editStarSubscription for one subscription. subscriptionId is trimmed
/// and a blank id returns an empty object. isCanceled=true cancels the
/// subscription; false allows the user to re-enable it.
[[nodiscard]] QJsonObject editStarSubscriptionRequest(const QString& subscriptionId,
                                                      bool isCanceled);

} // namespace beygram::telegram
