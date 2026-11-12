#include "PaymentRequests.h"

#include <QJsonValue>

#include <algorithm>

namespace beygram::telegram {
namespace {

// TDLib rejects a negative limit and Telegram returns at most 100 transactions
// per chunk; 0 would only ask for an empty page.
constexpr int kMinStarTransactionsLimit = 1;
constexpr int kMaxStarTransactionsLimit = 100;

QJsonObject inputInvoiceMessage(qint64 chatId, qint64 messageId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("inputInvoiceMessage")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
    };
}

// Only a string is a schema value for these fields. Anything else (a number, a
// null, an object) is treated as "not provided" rather than stringified into a
// value the caller never typed.
QString stringField(const QJsonObject& object, const QString& key)
{
    const QJsonValue value = object.value(key);
    return value.isString() ? value.toString() : QString();
}

// orderInfo.shipping_address is an address, not a nullable value, so the field
// is always a complete object. A missing or non-object value becomes the empty
// address.
QJsonObject normalizedAddress(const QJsonValue& value)
{
    const QJsonObject address = value.toObject();
    return QJsonObject{
        {QStringLiteral("country_code"),
         stringField(address, QStringLiteral("country_code"))},
        {QStringLiteral("state"), stringField(address, QStringLiteral("state"))},
        {QStringLiteral("city"), stringField(address, QStringLiteral("city"))},
        {QStringLiteral("street_line1"),
         stringField(address, QStringLiteral("street_line1"))},
        {QStringLiteral("street_line2"),
         stringField(address, QStringLiteral("street_line2"))},
        {QStringLiteral("postal_code"),
         stringField(address, QStringLiteral("postal_code"))},
    };
}

// TDLib types every orderInfo member as a string and shipping_address as an
// address object. Unknown keys are dropped, so the payload is exactly the shape
// the schema declares; the values are left for TDLib to validate.
QJsonObject normalizedOrderInfo(const QJsonObject& orderInfo)
{
    return QJsonObject{
        {QStringLiteral("name"), stringField(orderInfo, QStringLiteral("name"))},
        {QStringLiteral("phone_number"),
         stringField(orderInfo, QStringLiteral("phone_number"))},
        {QStringLiteral("email_address"),
         stringField(orderInfo, QStringLiteral("email_address"))},
        {QStringLiteral("shipping_address"),
         normalizedAddress(orderInfo.value(QStringLiteral("shipping_address")))},
    };
}

int clampedLimit(int limit)
{
    return std::clamp(limit, kMinStarTransactionsLimit, kMaxStarTransactionsLimit);
}

QJsonObject starTransactionsRequest(qint64 ownerUserId, const QString& offset, int limit)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getStarTransactions")},
        {QStringLiteral("owner_id"),
         QJsonObject{
             {QStringLiteral("@type"), QStringLiteral("messageSenderUser")},
             {QStringLiteral("user_id"), ownerUserId},
         }},
        // Empty subscription_id selects every transaction, not just a Star
        // Subscription's.
        {QStringLiteral("subscription_id"), QString()},
        // A null direction is the schema's "both incoming and outgoing".
        {QStringLiteral("direction"), QJsonValue::Null},
        {QStringLiteral("offset"), offset},
        {QStringLiteral("limit"), clampedLimit(limit)},
    };
}

// The cursor is opaque, so only the empty string has a schema meaning in the
// integer signature: a positive offset is its decimal form, which TDLib will
// not match against a real cursor.
QString cursorFromOffset(int offset)
{
    return offset <= 0 ? QString() : QString::number(offset);
}

} // namespace

QJsonObject getPaymentFormRequest(qint64 chatId, qint64 messageId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getPaymentForm")},
        {QStringLiteral("input_invoice"), inputInvoiceMessage(chatId, messageId)},
        // Null is the schema's "use the default theme".
        {QStringLiteral("theme"), QJsonValue::Null},
    };
}

QJsonObject validateOrderInfoRequest(qint64 chatId, qint64 messageId,
                                     const QJsonObject& orderInfo, bool allowSave)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("validateOrderInfo")},
        {QStringLiteral("input_invoice"), inputInvoiceMessage(chatId, messageId)},
        {QStringLiteral("order_info"), normalizedOrderInfo(orderInfo)},
        {QStringLiteral("allow_save"), allowSave},
    };
}

QJsonObject sendPaymentFormRequest(qint64 chatId, qint64 messageId,
                                   qint64 paymentFormId, const QString& title,
                                   const QString& payload,
                                   const QString& providerToken,
                                   const QJsonObject& shipping)
{
    // The pinned sendPaymentForm has no invoice title or payload field: the
    // invoice is named by input_invoice, so both stay out of the payload.
    Q_UNUSED(title);
    Q_UNUSED(payload);

    // A provider token means a regular (real-money) invoice. Its credentials
    // come from the provider's SDK, which this signature cannot carry, so the
    // form cannot be completed here. A non-positive form id is not something
    // getPaymentForm could have returned either.
    if (!providerToken.isEmpty() || paymentFormId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("sendPaymentForm")},
        {QStringLiteral("input_invoice"), inputInvoiceMessage(chatId, messageId)},
        {QStringLiteral("payment_form_id"), paymentFormId},
        // No validateOrderInfo result is carried by this signature, so the id
        // is the empty string the schema asks for.
        {QStringLiteral("order_info_id"), QString()},
        // A Stars form has no shipping, so an absent choice is the empty id.
        {QStringLiteral("shipping_option_id"),
         stringField(shipping, QStringLiteral("id"))},
        // Null is the schema's "payment in Telegram Stars".
        {QStringLiteral("credentials"), QJsonValue::Null},
        {QStringLiteral("tip_amount"), 0},
    };
}

QJsonObject getStarTransactionsRequest(int offset, int limit)
{
    // The schema has no "current user" owner token. User 0 names no account, so
    // a pinned server rejects this request; it exists only so the payload can be
    // built before getMe() resolves.
    return starTransactionsRequest(0, cursorFromOffset(offset), limit);
}

QJsonObject getStarTransactionsRequest(qint64 ownerUserId, int offset, int limit)
{
    return starTransactionsRequest(ownerUserId, cursorFromOffset(offset), limit);
}

QJsonObject getStarTransactionsRequest(qint64 ownerUserId, const QString& offset,
                                       int limit)
{
    return starTransactionsRequest(ownerUserId, offset, limit);
}

} // namespace beygram::telegram
