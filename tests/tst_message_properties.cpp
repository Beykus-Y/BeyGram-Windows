#include "telegram/MessageProperties.h"

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

QJsonObject responseFromJson(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

} // namespace

class MessagePropertiesTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsRequestShape();
    void preservesLargeIdsThroughJson();
    void parsesFullResponse();
    void keepsDefaultsForPartialResponse();
    void ignoresMalformedFields();
    void keepsReactionsFalseWhenSchemaOmitsThem();
    void withdrawsSaveForProtectedContent();
    void returnsDefaultsForErrorResponse();
    void returnsDefaultsForEmptyResponse();
};

void MessagePropertiesTest::buildsRequestShape()
{
    const QJsonObject request = serialized(getMessagePropertiesRequest(42, 900));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getMessageProperties"));
    QCOMPARE(idOf(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(idOf(request, QStringLiteral("message_id")), qint64(900));
    QCOMPARE(int(request.size()), 3);
}

void MessagePropertiesTest::preservesLargeIdsThroughJson()
{
    const qint64 chatId = -1001234567890LL;
    const qint64 messageId = 1234567890123LL;

    const QJsonObject request = serialized(getMessagePropertiesRequest(chatId, messageId));
    QCOMPARE(idOf(request, QStringLiteral("chat_id")), chatId);
    QCOMPARE(idOf(request, QStringLiteral("message_id")), messageId);
}

void MessagePropertiesTest::parsesFullResponse()
{
    const QJsonObject response = responseFromJson(R"({
        "@type": "messageProperties",
        "can_be_forwarded": true,
        "can_be_saved": true,
        "can_be_copied": false,
        "can_be_deleted_only_for_self": true,
        "can_be_deleted_for_all_users": true,
        "can_get_added_reactions": true
    })");

    const MessageProperties properties = propertiesFromResponse(response);
    QVERIFY(properties.canBeForwarded);
    QVERIFY(properties.canBeSaved);
    QVERIFY(!properties.canBeCopied);
    QVERIFY(properties.canBeDeletedOnlyForSelf);
    QVERIFY(properties.canBeDeletedForAllUsers);
    QVERIFY(properties.canGetAddedReactions);
}

void MessagePropertiesTest::keepsDefaultsForPartialResponse()
{
    // Only one field arrived: everything else must stay at its default, and
    // the default for local copying is true.
    const QJsonObject response = responseFromJson(R"({
        "@type": "messageProperties",
        "can_be_forwarded": true
    })");

    const MessageProperties properties = propertiesFromResponse(response);
    QVERIFY(properties.canBeForwarded);
    QVERIFY(!properties.canBeSaved);
    QVERIFY(properties.canBeCopied);
    QVERIFY(!properties.canBeDeletedOnlyForSelf);
    QVERIFY(!properties.canBeDeletedForAllUsers);
    QVERIFY(!properties.canGetAddedReactions);

    const QJsonObject nothing = responseFromJson(R"({
        "@type": "messageProperties"
    })");
    const MessageProperties bare = propertiesFromResponse(nothing);
    QVERIFY(!bare.canBeForwarded);
    QVERIFY(!bare.canBeSaved);
    QVERIFY(bare.canBeCopied);
    QVERIFY(!bare.canBeDeletedOnlyForSelf);
    QVERIFY(!bare.canBeDeletedForAllUsers);
    QVERIFY(!bare.canGetAddedReactions);
}

void MessagePropertiesTest::ignoresMalformedFields()
{
    // A field of the wrong type is treated as absent, not as false: the
    // documented default survives.
    const QJsonObject response = responseFromJson(R"({
        "@type": "messageProperties",
        "can_be_copied": "false",
        "can_be_saved": null,
        "can_be_forwarded": 1,
        "can_be_deleted_for_all_users": {}
    })");

    const MessageProperties properties = propertiesFromResponse(response);
    QVERIFY(!properties.canBeForwarded);
    QVERIFY(!properties.canBeSaved);
    QVERIFY(properties.canBeCopied);
    QVERIFY(!properties.canBeDeletedForAllUsers);
}

void MessagePropertiesTest::keepsReactionsFalseWhenSchemaOmitsThem()
{
    // Newer TDLib revisions dropped can_get_added_reactions.
    const QJsonObject response = responseFromJson(R"({
        "@type": "messageProperties",
        "can_be_forwarded": true,
        "can_be_saved": false,
        "can_be_copied": true,
        "can_be_deleted_only_for_self": false,
        "can_be_deleted_for_all_users": false
    })");

    QVERIFY(!propertiesFromResponse(response).canGetAddedReactions);
}

void MessagePropertiesTest::withdrawsSaveForProtectedContent()
{
    const QJsonObject byOther = responseFromJson(R"({
        "@type": "messageProperties",
        "can_be_saved": true,
        "has_protected_content_by_other_user": true
    })");
    QVERIFY(!propertiesFromResponse(byOther).canBeSaved);

    const QJsonObject byCurrent = responseFromJson(R"({
        "@type": "messageProperties",
        "can_be_saved": true,
        "has_protected_content_by_current_user": true
    })");
    QVERIFY(!propertiesFromResponse(byCurrent).canBeSaved);

    const QJsonObject unprotected = responseFromJson(R"({
        "@type": "messageProperties",
        "can_be_saved": true,
        "has_protected_content_by_other_user": false
    })");
    QVERIFY(propertiesFromResponse(unprotected).canBeSaved);
}

void MessagePropertiesTest::returnsDefaultsForErrorResponse()
{
    const QJsonObject response = responseFromJson(R"({
        "@type": "error",
        "code": 400,
        "message": "Message not found"
    })");

    const MessageProperties properties = propertiesFromResponse(response);
    QVERIFY(!properties.canBeForwarded);
    QVERIFY(!properties.canBeSaved);
    QVERIFY(properties.canBeCopied);
    QVERIFY(!properties.canBeDeletedOnlyForSelf);
    QVERIFY(!properties.canBeDeletedForAllUsers);
    QVERIFY(!properties.canGetAddedReactions);
}

void MessagePropertiesTest::returnsDefaultsForEmptyResponse()
{
    const MessageProperties properties = propertiesFromResponse(QJsonObject{});
    QVERIFY(!properties.canBeForwarded);
    QVERIFY(!properties.canBeSaved);
    QVERIFY(properties.canBeCopied);
    QVERIFY(!properties.canBeDeletedOnlyForSelf);
    QVERIFY(!properties.canBeDeletedForAllUsers);
    QVERIFY(!properties.canGetAddedReactions);
}

QTEST_GUILESS_MAIN(MessagePropertiesTest)
#include "tst_message_properties.moc"
