#include "telegram/PresenceRequests.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject parseObject(const QByteArray& json)
{
    return QJsonDocument::fromJson(json).object();
}

// Requests are compared after a JSON round trip so the assertions see exactly
// what would be handed to TDLib.
QJsonObject roundTrip(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

QJsonObject userWithStatus(const char* statusJson)
{
    return parseObject(QByteArrayLiteral("{\"@type\":\"user\",\"id\":42,\"status\":")
                       + QByteArray(statusJson) + QByteArrayLiteral("}"));
}

void verifyNoPresence(const QJsonObject& user)
{
    const PresenceInfo info = presenceFromUser(user);
    QCOMPARE(info.status, QString());
    QCOMPARE(info.lastSeen, qint64(0));
    QCOMPARE(info.online, false);
}

} // namespace

class PresenceRequestsTest final : public QObject {
    Q_OBJECT

private slots:
    void setOnlineRequestShape();
    void setOnlineRequestSurvivesSerialization();
    void profileLookupRequestsMatchSchema();
    void parsesOnlineStatus();
    void parsesOfflineStatus();
    void parsesDiscreteStatuses();
    void parsesEmptyStatus();
    void preservesUnknownStatus();
    void defaultsWhenStatusIsMissingOrMalformed();
    void ignoresTimesThatAreNotLastSeen();
    void clampsOfflineLastSeen();
    void presenceSurvivesSerialization();
};

void PresenceRequestsTest::setOnlineRequestShape()
{
    const QJsonObject online = setOnlineRequest(true);
    QCOMPARE(online.size(), 3);
    QCOMPARE(online.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setOption"));
    QCOMPARE(online.value(QStringLiteral("name")).toString(),
             QStringLiteral("online"));

    const QJsonValue value = online.value(QStringLiteral("value"));
    QVERIFY(value.isObject());
    const QJsonObject option = value.toObject();
    QCOMPARE(option.size(), 2);
    QCOMPARE(option.value(QStringLiteral("@type")).toString(),
             QStringLiteral("optionValueBoolean"));
    QVERIFY(option.value(QStringLiteral("value")).isBool());
    QCOMPARE(option.value(QStringLiteral("value")).toBool(), true);

    const QJsonObject offline = setOnlineRequest(false);
    QCOMPARE(offline.size(), 3);
    QCOMPARE(offline.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setOption"));
    QCOMPARE(offline.value(QStringLiteral("name")).toString(),
             QStringLiteral("online"));
    QCOMPARE(offline.value(QStringLiteral("value"))
                 .toObject()
                 .value(QStringLiteral("@type"))
                 .toString(),
             QStringLiteral("optionValueBoolean"));
    QCOMPARE(offline.value(QStringLiteral("value"))
                 .toObject()
                 .value(QStringLiteral("value"))
                 .toBool(),
             false);

    // The option name is a literal "online"; no account or chat id belongs here.
    QVERIFY(!online.contains(QStringLiteral("user_id")));
    QVERIFY(!online.contains(QStringLiteral("chat_id")));
}

void PresenceRequestsTest::setOnlineRequestSurvivesSerialization()
{
    const QJsonObject online = roundTrip(setOnlineRequest(true));
    QCOMPARE(online.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setOption"));
    QCOMPARE(online.value(QStringLiteral("name")).toString(),
             QStringLiteral("online"));
    QVERIFY(online.value(QStringLiteral("value"))
                .toObject()
                .value(QStringLiteral("value"))
                .isBool());
    QCOMPARE(online.value(QStringLiteral("value"))
                 .toObject()
                 .value(QStringLiteral("value"))
                 .toBool(),
             true);

    const QJsonObject offline = roundTrip(setOnlineRequest(false));
    QCOMPARE(offline.value(QStringLiteral("value"))
                 .toObject()
                 .value(QStringLiteral("value"))
                 .toBool(),
             false);
}

void PresenceRequestsTest::profileLookupRequestsMatchSchema()
{
    const QJsonObject user = getUserRequest(1234567890123LL);
    QCOMPARE(user.size(), 2);
    QCOMPARE(user.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getUser"));
    QCOMPARE(user.value(QStringLiteral("user_id")).toVariant().toLongLong(),
             qint64(1234567890123LL));
    QVERIFY(!user.contains(QStringLiteral("chat_id")));

    const QJsonObject full = getUserFullInfoRequest(1234567890123LL);
    QCOMPARE(full.size(), 2);
    QCOMPARE(full.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getUserFullInfo"));
    QCOMPARE(full.value(QStringLiteral("user_id")).toVariant().toLongLong(),
             qint64(1234567890123LL));
    QVERIFY(!full.contains(QStringLiteral("chat_id")));

    // int53 identifiers must not be truncated.
    QCOMPARE(getUserRequest(9007199254740991LL)
                 .value(QStringLiteral("user_id"))
                 .toVariant()
                 .toLongLong(),
             qint64(9007199254740991LL));
    QCOMPARE(getUserFullInfoRequest(9007199254740991LL)
                 .value(QStringLiteral("user_id"))
                 .toVariant()
                 .toLongLong(),
             qint64(9007199254740991LL));
}

void PresenceRequestsTest::parsesOnlineStatus()
{
    const QJsonObject user =
        userWithStatus(R"({"@type":"userStatusOnline","expires":1700000000})");
    const PresenceInfo info = presenceFromUser(user);

    QCOMPARE(info.status, QStringLiteral("userStatusOnline"));
    QVERIFY(info.online);
    // expires is the end of the online window, not a last-seen timestamp.
    QCOMPARE(info.lastSeen, qint64(0));
}

void PresenceRequestsTest::parsesOfflineStatus()
{
    const QJsonObject user =
        userWithStatus(R"({"@type":"userStatusOffline","was_online":1700000000})");
    const PresenceInfo info = presenceFromUser(user);

    QCOMPARE(info.status, QStringLiteral("userStatusOffline"));
    QCOMPARE(info.online, false);
    QCOMPARE(info.lastSeen, qint64(1700000000));
}

void PresenceRequestsTest::parsesDiscreteStatuses()
{
    const struct {
        const char* json;
        const char* type;
    } cases[] = {
        {R"({"@type":"userStatusRecently","by_my_privacy_settings":true})",
         "userStatusRecently"},
        {R"({"@type":"userStatusLastWeek","by_my_privacy_settings":false})",
         "userStatusLastWeek"},
        {R"({"@type":"userStatusLastMonth","by_my_privacy_settings":true})",
         "userStatusLastMonth"},
    };

    for (const auto& item : cases) {
        const PresenceInfo info = presenceFromUser(userWithStatus(item.json));
        QCOMPARE(info.status, QString::fromLatin1(item.type));
        QCOMPARE(info.online, false);
        QCOMPARE(info.lastSeen, qint64(0));
    }
}

void PresenceRequestsTest::parsesEmptyStatus()
{
    const PresenceInfo info =
        presenceFromUser(userWithStatus(R"({"@type":"userStatusEmpty"})"));

    // userStatusEmpty is a real schema variant, not the same as a missing
    // status object: the caller can tell "no data" from "empty status".
    QCOMPARE(info.status, QStringLiteral("userStatusEmpty"));
    QCOMPARE(info.online, false);
    QCOMPARE(info.lastSeen, qint64(0));
}

void PresenceRequestsTest::preservesUnknownStatus()
{
    const PresenceInfo info = presenceFromUser(
        userWithStatus(R"({"@type":"userStatusFutureVariant","was_online":1700000000})"));

    // A variant this build does not know is kept verbatim, so a newer TDLib
    // does not turn into "no status". Its time is not guessed at.
    QCOMPARE(info.status, QStringLiteral("userStatusFutureVariant"));
    QCOMPARE(info.online, false);
    QCOMPARE(info.lastSeen, qint64(0));
}

void PresenceRequestsTest::defaultsWhenStatusIsMissingOrMalformed()
{
    verifyNoPresence(parseObject(
        R"({"@type":"user","id":42,"first_name":"Ada","last_name":"Lovelace"})"));

    verifyNoPresence(userWithStatus(R"(null)"));
    verifyNoPresence(userWithStatus(R"("userStatusOnline")"));
    verifyNoPresence(userWithStatus(R"(42)"));
    verifyNoPresence(userWithStatus(R"([])"));
    verifyNoPresence(userWithStatus(R"({})"));
    verifyNoPresence(userWithStatus(R"({"@type":7})"));
    verifyNoPresence(userWithStatus(R"({"value":"userStatusOnline"})"));

    // A non-object "status" member must not be read as a status object.
    verifyNoPresence(parseObject(
        R"({"@type":"user","id":42,"status":"userStatusOffline","was_online":1700000000})"));
}

void PresenceRequestsTest::ignoresTimesThatAreNotLastSeen()
{
    // expires belongs to the online variant and must not become lastSeen.
    const PresenceInfo online = presenceFromUser(userWithStatus(
        R"({"@type":"userStatusOnline","expires":1700000000,"was_online":1699999999})"));
    QCOMPARE(online.status, QStringLiteral("userStatusOnline"));
    QVERIFY(online.online);
    QCOMPARE(online.lastSeen, qint64(0));

    // Only userStatusOffline carries was_online as a last-seen time.
    const PresenceInfo recently = presenceFromUser(userWithStatus(
        R"({"@type":"userStatusRecently","was_online":1700000000})"));
    QCOMPARE(recently.status, QStringLiteral("userStatusRecently"));
    QCOMPARE(recently.lastSeen, qint64(0));

    const PresenceInfo unknown = presenceFromUser(userWithStatus(
        R"({"@type":"userStatusFutureVariant","was_online":1700000000})"));
    QCOMPARE(unknown.lastSeen, qint64(0));
}

void PresenceRequestsTest::clampsOfflineLastSeen()
{
    QCOMPARE(presenceFromUser(
                 userWithStatus(R"({"@type":"userStatusOffline","was_online":-5})"))
                 .lastSeen,
             qint64(0));
    QCOMPARE(presenceFromUser(userWithStatus(
                 R"({"@type":"userStatusOffline","was_online":0})"))
                 .lastSeen,
             qint64(0));
    QCOMPARE(presenceFromUser(userWithStatus(R"({"@type":"userStatusOffline"})"))
                 .lastSeen,
             qint64(0));

    // int32 maximum, as reported by TDLib for a long-ago last-seen time.
    QCOMPARE(presenceFromUser(userWithStatus(
                 R"({"@type":"userStatusOffline","was_online":2147483647})"))
                 .lastSeen,
             qint64(2147483647));

    // The JSON interface can carry large integers as strings; jsonInt64 reads
    // both forms, and the value is still seconds rather than milliseconds.
    QCOMPARE(presenceFromUser(userWithStatus(
                 R"({"@type":"userStatusOffline","was_online":"1700000000"})"))
                 .lastSeen,
             qint64(1700000000));
}

void PresenceRequestsTest::presenceSurvivesSerialization()
{
    const QJsonObject user = roundTrip(userWithStatus(
        R"({"@type":"userStatusOffline","was_online":1700000000})"));
    const PresenceInfo info = presenceFromUser(user);
    QCOMPARE(info.status, QStringLiteral("userStatusOffline"));
    QCOMPARE(info.online, false);
    QCOMPARE(info.lastSeen, qint64(1700000000));

    const PresenceInfo online = presenceFromUser(roundTrip(
        userWithStatus(R"({"@type":"userStatusOnline","expires":1700000000})")));
    QCOMPARE(online.status, QStringLiteral("userStatusOnline"));
    QVERIFY(online.online);
    QCOMPARE(online.lastSeen, qint64(0));
}

QTEST_GUILESS_MAIN(PresenceRequestsTest)
#include "tst_presence_requests.moc"
