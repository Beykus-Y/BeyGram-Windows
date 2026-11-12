#include "telegram/PaymentRequests.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
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

QStringList addressFields()
{
    return {
        QStringLiteral("country_code"),
        QStringLiteral("state"),
        QStringLiteral("city"),
        QStringLiteral("street_line1"),
        QStringLiteral("street_line2"),
        QStringLiteral("postal_code"),
    };
}

QJsonObject invoiceOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("input_invoice")).toObject();
}

QJsonObject ownerOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("owner_id")).toObject();
}

} // namespace

class PaymentRequestsTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsPaymentFormRequest();
    void preservesLargeInvoiceIds();
    void buildsValidateOrderInfoRequest();
    void normalizesPartialOrderInfo();
    void treatsNonStringOrderFieldsAsMissing();
    void dropsUnknownOrderInfoKeys();
    void forwardsAllowSave();
    void buildsStarsPaymentFormRequest();
    void usesShippingOptionId();
    void keepsInvoiceIdentityOutOfSendPaymentForm();
    void refusesRegularPaymentProvider();
    void refusesNonPositivePaymentFormId();
    void buildsStarTransactionsRequest();
    void buildsOwnerStarTransactionsRequest();
    void clampsStarTransactionsLimit();
    void encodesStarTransactionsCursor();
    void passesCursorThrough();
};

void PaymentRequestsTest::buildsPaymentFormRequest()
{
    const QJsonObject request = getPaymentFormRequest(777, 42);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getPaymentForm"));
    QCOMPARE(request.size(), 3);

    // The pinned schema has no top-level chat_id/message_id: the invoice is an
    // InputInvoice object.
    QVERIFY(!request.contains(QStringLiteral("chat_id")));
    QVERIFY(!request.contains(QStringLiteral("message_id")));

    const QJsonObject invoice = invoiceOf(request);
    QCOMPARE(invoice.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputInvoiceMessage"));
    QCOMPARE(numberAt(invoice, QStringLiteral("chat_id")), qint64(777));
    QCOMPARE(numberAt(invoice, QStringLiteral("message_id")), qint64(42));
    QCOMPARE(invoice.size(), 3);

    // theme must be present and explicitly null ("use the default theme").
    QVERIFY(request.contains(QStringLiteral("theme")));
    QVERIFY(request.value(QStringLiteral("theme")).isNull());

    const QJsonObject encoded = serialized(request);
    QVERIFY(encoded.value(QStringLiteral("theme")).isNull());
    QCOMPARE(numberAt(invoiceOf(encoded), QStringLiteral("message_id")), qint64(42));
}

void PaymentRequestsTest::preservesLargeInvoiceIds()
{
    // Above 2^53, so a double round-trip would lose precision.
    const qint64 chatId = 9007199254740993LL;
    const qint64 messageId = 9007199254740995LL;

    const QJsonObject invoice =
        invoiceOf(serialized(getPaymentFormRequest(chatId, messageId)));
    QCOMPARE(numberAt(invoice, QStringLiteral("chat_id")), chatId);
    QCOMPARE(numberAt(invoice, QStringLiteral("message_id")), messageId);
}

void PaymentRequestsTest::buildsValidateOrderInfoRequest()
{
    const QJsonObject request = validateOrderInfoRequest(10, 20, QJsonObject{});
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("validateOrderInfo"));
    QCOMPARE(request.size(), 4);
    QVERIFY(!request.contains(QStringLiteral("chat_id")));

    const QJsonObject invoice = invoiceOf(request);
    QCOMPARE(invoice.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputInvoiceMessage"));
    QCOMPARE(numberAt(invoice, QStringLiteral("chat_id")), qint64(10));
    QCOMPARE(numberAt(invoice, QStringLiteral("message_id")), qint64(20));

    // Every orderInfo field is a string, and shipping_address is always a
    // complete address object rather than null.
    const QJsonObject order = request.value(QStringLiteral("order_info")).toObject();
    QCOMPARE(order.size(), 4);
    QCOMPARE(order.value(QStringLiteral("name")).toString(), QString());
    QCOMPARE(order.value(QStringLiteral("phone_number")).toString(), QString());
    QCOMPARE(order.value(QStringLiteral("email_address")).toString(), QString());

    const QJsonObject address =
        order.value(QStringLiteral("shipping_address")).toObject();
    QCOMPARE(address.size(), 6);
    for (const QString& field : addressFields()) {
        QVERIFY2(address.value(field).isString(), qPrintable(field));
        QCOMPARE(address.value(field).toString(), QString());
    }

    // allowSave defaults to true and is a real boolean.
    QVERIFY(request.value(QStringLiteral("allow_save")).isBool());
    QCOMPARE(request.value(QStringLiteral("allow_save")).toBool(false), true);
}

void PaymentRequestsTest::normalizesPartialOrderInfo()
{
    const QJsonObject request = validateOrderInfoRequest(
        1, 2,
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("Ada Lovelace")},
            {QStringLiteral("phone_number"), QStringLiteral("+44 20 7946 0000")},
            {QStringLiteral("email_address"), QStringLiteral("ada@example.com")},
            {QStringLiteral("shipping_address"),
             QJsonObject{
                 {QStringLiteral("city"), QStringLiteral("London")},
                 {QStringLiteral("postal_code"), QStringLiteral("N1")},
             }},
        });

    const QJsonObject order = request.value(QStringLiteral("order_info")).toObject();
    QCOMPARE(order.value(QStringLiteral("name")).toString(),
             QStringLiteral("Ada Lovelace"));
    QCOMPARE(order.value(QStringLiteral("phone_number")).toString(),
             QStringLiteral("+44 20 7946 0000"));
    QCOMPARE(order.value(QStringLiteral("email_address")).toString(),
             QStringLiteral("ada@example.com"));

    // The address keeps what the caller set and fills the rest with empty
    // strings, so no field is missing or null.
    const QJsonObject address =
        order.value(QStringLiteral("shipping_address")).toObject();
    QCOMPARE(address.size(), 6);
    QCOMPARE(address.value(QStringLiteral("city")).toString(),
             QStringLiteral("London"));
    QCOMPARE(address.value(QStringLiteral("postal_code")).toString(),
             QStringLiteral("N1"));
    QCOMPARE(address.value(QStringLiteral("country_code")).toString(), QString());
    QCOMPARE(address.value(QStringLiteral("state")).toString(), QString());
    QCOMPARE(address.value(QStringLiteral("street_line1")).toString(), QString());
    QCOMPARE(address.value(QStringLiteral("street_line2")).toString(), QString());
}

void PaymentRequestsTest::treatsNonStringOrderFieldsAsMissing()
{
    const QJsonObject order =
        validateOrderInfoRequest(
            1, 2,
            QJsonObject{
                {QStringLiteral("name"), 42},
                {QStringLiteral("phone_number"), QJsonValue::Null},
                {QStringLiteral("email_address"), QJsonArray{QStringLiteral("a")}},
                {QStringLiteral("shipping_address"), QStringLiteral("Berlin")},
            })
            .value(QStringLiteral("order_info"))
            .toObject();

    QCOMPARE(order.value(QStringLiteral("name")).toString(), QString());
    QCOMPARE(order.value(QStringLiteral("phone_number")).toString(), QString());
    QCOMPARE(order.value(QStringLiteral("email_address")).toString(), QString());

    // A non-object address becomes the empty address, never a string.
    const QJsonObject address =
        order.value(QStringLiteral("shipping_address")).toObject();
    QCOMPARE(address.size(), 6);
    for (const QString& field : addressFields())
        QCOMPARE(address.value(field).toString(), QString());
}

void PaymentRequestsTest::dropsUnknownOrderInfoKeys()
{
    const QJsonObject order =
        validateOrderInfoRequest(
            1, 2,
            QJsonObject{
                {QStringLiteral("name"), QStringLiteral("Grace")},
                {QStringLiteral("save"), true},
                {QStringLiteral("note"), QStringLiteral("not a schema field")},
                {QStringLiteral("shipping_address"),
                 QJsonObject{
                     {QStringLiteral("country_code"), QStringLiteral("DE")},
                     {QStringLiteral("planet"), QStringLiteral("Mars")},
                 }},
            })
            .value(QStringLiteral("order_info"))
            .toObject();

    QCOMPARE(order.size(), 4);
    QVERIFY(!order.contains(QStringLiteral("save")));
    QVERIFY(!order.contains(QStringLiteral("note")));
    QCOMPARE(order.value(QStringLiteral("name")).toString(),
             QStringLiteral("Grace"));

    const QJsonObject address =
        order.value(QStringLiteral("shipping_address")).toObject();
    QCOMPARE(address.size(), 6);
    QVERIFY(!address.contains(QStringLiteral("planet")));
    QCOMPARE(address.value(QStringLiteral("country_code")).toString(),
             QStringLiteral("DE"));
}

void PaymentRequestsTest::forwardsAllowSave()
{
    const QJsonObject declined = validateOrderInfoRequest(1, 2, QJsonObject{}, false);
    QVERIFY(declined.value(QStringLiteral("allow_save")).isBool());
    QCOMPARE(declined.value(QStringLiteral("allow_save")).toBool(true), false);

    const QJsonObject accepted = validateOrderInfoRequest(1, 2, QJsonObject{});
    QCOMPARE(accepted.value(QStringLiteral("allow_save")).toBool(false), true);
}

void PaymentRequestsTest::buildsStarsPaymentFormRequest()
{
    const QJsonObject request = sendPaymentFormRequest(
        11, 22, 33, QString(), QString(), QString(), QJsonObject{});
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("sendPaymentForm"));
    QCOMPARE(request.size(), 7);
    QVERIFY(!request.contains(QStringLiteral("chat_id")));

    const QJsonObject invoice = invoiceOf(request);
    QCOMPARE(invoice.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputInvoiceMessage"));
    QCOMPARE(numberAt(invoice, QStringLiteral("chat_id")), qint64(11));
    QCOMPARE(numberAt(invoice, QStringLiteral("message_id")), qint64(22));

    QCOMPARE(numberAt(request, QStringLiteral("payment_form_id")), qint64(33));

    // The schema's empty strings and empty credentials for a Stars payment.
    QVERIFY(request.value(QStringLiteral("order_info_id")).isString());
    QCOMPARE(request.value(QStringLiteral("order_info_id")).toString(), QString());
    QVERIFY(request.value(QStringLiteral("shipping_option_id")).isString());
    QCOMPARE(request.value(QStringLiteral("shipping_option_id")).toString(),
             QString());
    QVERIFY(request.contains(QStringLiteral("credentials")));
    QVERIFY(request.value(QStringLiteral("credentials")).isNull());

    QCOMPARE(numberAt(request, QStringLiteral("tip_amount")), qint64(0));

    const QJsonObject encoded = serialized(request);
    QCOMPARE(numberAt(encoded, QStringLiteral("payment_form_id")), qint64(33));
    QVERIFY(encoded.value(QStringLiteral("credentials")).isNull());
}

void PaymentRequestsTest::usesShippingOptionId()
{
    const QJsonObject shipping{
        {QStringLiteral("@type"), QStringLiteral("shippingOption")},
        {QStringLiteral("id"), QStringLiteral("post-1")},
        {QStringLiteral("title"), QStringLiteral("Post")},
        {QStringLiteral("price_parts"), QJsonArray()},
    };

    const QJsonObject request = sendPaymentFormRequest(
        1, 2, 3, QString(), QString(), QString(), shipping);
    QCOMPARE(request.value(QStringLiteral("shipping_option_id")).toString(),
             QStringLiteral("post-1"));
    // A chosen option does not change the Stars payment itself.
    QCOMPARE(request.value(QStringLiteral("order_info_id")).toString(), QString());
    QVERIFY(request.value(QStringLiteral("credentials")).isNull());

    // No id is the empty choice, and a non-string id is not an option id.
    const QJsonObject untitled = sendPaymentFormRequest(
        1, 2, 3, QString(), QString(), QString(),
        QJsonObject{{QStringLiteral("title"), QStringLiteral("Post")}});
    QCOMPARE(untitled.value(QStringLiteral("shipping_option_id")).toString(),
             QString());

    const QJsonObject numericId = sendPaymentFormRequest(
        1, 2, 3, QString(), QString(), QString(),
        QJsonObject{{QStringLiteral("id"), 7}});
    QCOMPARE(numericId.value(QStringLiteral("shipping_option_id")).toString(),
             QString());
}

void PaymentRequestsTest::keepsInvoiceIdentityOutOfSendPaymentForm()
{
    // A Stars invoice has a title and a payload, but sendPaymentForm names the
    // invoice by input_invoice and has no field for either.
    const QJsonObject request = sendPaymentFormRequest(
        1, 2, 3, QStringLiteral("Stars invoice"), QStringLiteral("payload-1"),
        QString(), QJsonObject{});
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("sendPaymentForm"));
    QCOMPARE(request.size(), 7);
    QVERIFY(!request.contains(QStringLiteral("title")));
    QVERIFY(!request.contains(QStringLiteral("payload")));
    QVERIFY(!request.contains(QStringLiteral("provider_token")));
}

void PaymentRequestsTest::refusesRegularPaymentProvider()
{
    // A provider token means a regular (real-money) invoice, whose credentials
    // this builder cannot produce. An empty object is "do not send".
    QVERIFY(sendPaymentFormRequest(1, 2, 3, QStringLiteral("Invoice"),
                                   QStringLiteral("payload"),
                                   QStringLiteral("1234:TEST"), QJsonObject{})
                .isEmpty());
}

void PaymentRequestsTest::refusesNonPositivePaymentFormId()
{
    QVERIFY(sendPaymentFormRequest(1, 2, 0, QString(), QString(), QString(),
                                   QJsonObject{})
                .isEmpty());
    QVERIFY(sendPaymentFormRequest(1, 2, -5, QString(), QString(), QString(),
                                   QJsonObject{})
                .isEmpty());
}

void PaymentRequestsTest::buildsStarTransactionsRequest()
{
    const QJsonObject request = getStarTransactionsRequest(0, 30);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getStarTransactions"));
    QCOMPARE(request.size(), 6);
    QVERIFY(!request.contains(QStringLiteral("chat_id")));

    // User 0 is the documented placeholder: the two-argument form has no
    // account id, so the caller must use the owner overload for a real request.
    const QJsonObject owner = ownerOf(request);
    QCOMPARE(owner.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSenderUser"));
    QCOMPARE(owner.size(), 2);
    QCOMPARE(numberAt(owner, QStringLiteral("user_id")), qint64(0));

    // Empty subscription and offset, null direction: every transaction of the
    // first chunk.
    QVERIFY(request.value(QStringLiteral("subscription_id")).isString());
    QCOMPARE(request.value(QStringLiteral("subscription_id")).toString(), QString());
    QVERIFY(request.contains(QStringLiteral("direction")));
    QVERIFY(request.value(QStringLiteral("direction")).isNull());
    QVERIFY(request.value(QStringLiteral("offset")).isString());
    QCOMPARE(request.value(QStringLiteral("offset")).toString(), QString());
    QCOMPARE(numberAt(request, QStringLiteral("limit")), qint64(30));
}

void PaymentRequestsTest::buildsOwnerStarTransactionsRequest()
{
    const QJsonObject request = getStarTransactionsRequest(999, 0, 30);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getStarTransactions"));
    QCOMPARE(numberAt(ownerOf(request), QStringLiteral("user_id")), qint64(999));
    QCOMPARE(numberAt(request, QStringLiteral("limit")), qint64(30));
    QCOMPARE(request.value(QStringLiteral("offset")).toString(), QString());
    QCOMPARE(request.value(QStringLiteral("subscription_id")).toString(), QString());
    QVERIFY(request.value(QStringLiteral("direction")).isNull());

    // Only the owner differs from the two-argument form.
    QCOMPARE(request.size(), getStarTransactionsRequest(0, 30).size());

    // The owner id survives the wire form even above 2^53.
    const qint64 largeOwner = 9007199254740993LL;
    const QJsonObject encoded =
        serialized(getStarTransactionsRequest(largeOwner, 0, 30));
    QCOMPARE(numberAt(ownerOf(encoded), QStringLiteral("user_id")), largeOwner);
}

void PaymentRequestsTest::clampsStarTransactionsLimit()
{
    const QVector<QPair<int, int>> cases{
        {0, 1},
        {-7, 1},
        {1, 1},
        {30, 30},
        {100, 100},
        {101, 100},
        {5000, 100},
    };

    for (const auto& testCase : cases) {
        const qint64 expected = static_cast<qint64>(testCase.second);
        QCOMPARE(numberAt(getStarTransactionsRequest(0, testCase.first),
                          QStringLiteral("limit")),
                 expected);
        QCOMPARE(numberAt(getStarTransactionsRequest(42, 0, testCase.first),
                          QStringLiteral("limit")),
                 expected);
    }
}

void PaymentRequestsTest::encodesStarTransactionsCursor()
{
    // A non-positive integer is the empty cursor that asks for the first chunk.
    QCOMPARE(getStarTransactionsRequest(0, 30)
                 .value(QStringLiteral("offset"))
                 .toString(),
             QString());
    QCOMPARE(getStarTransactionsRequest(-1, 30)
                 .value(QStringLiteral("offset"))
                 .toString(),
             QString());

    // A positive value is its decimal string; TDLib's cursor is opaque, so this
    // will not match a real one.
    QCOMPARE(getStarTransactionsRequest(7, 30)
                 .value(QStringLiteral("offset"))
                 .toString(),
             QStringLiteral("7"));
    QCOMPARE(getStarTransactionsRequest(42, 7, 30)
                 .value(QStringLiteral("offset"))
                 .toString(),
             QStringLiteral("7"));
}

void PaymentRequestsTest::passesCursorThrough()
{
    const QJsonObject first = getStarTransactionsRequest(42, QString(), 30);
    QCOMPARE(first.value(QStringLiteral("offset")).toString(), QString());
    QCOMPARE(numberAt(ownerOf(first), QStringLiteral("user_id")), qint64(42));

    const QJsonObject next =
        getStarTransactionsRequest(42, QStringLiteral("opaque-next-cursor"), 30);
    QCOMPARE(next.value(QStringLiteral("offset")).toString(),
             QStringLiteral("opaque-next-cursor"));
    QCOMPARE(numberAt(next, QStringLiteral("limit")), qint64(30));

    // The cursor overload clamps the limit as well.
    const QJsonObject clamped =
        getStarTransactionsRequest(42, QStringLiteral("cursor"), 500);
    QCOMPARE(numberAt(clamped, QStringLiteral("limit")), qint64(100));
}

QTEST_GUILESS_MAIN(PaymentRequestsTest)
#include "tst_payment_requests.moc"
