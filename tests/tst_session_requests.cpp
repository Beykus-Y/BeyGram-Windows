#include "telegram/SessionRequests.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace beygram::telegram;

class SessionRequestsTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsGetActiveSessionsRequest();
    void buildsTerminateSessionRequest();
    void buildsTerminateAllOtherSessionsRequest();
    void parsesSessionsInOrder();
    void fallsBackToPlatformForDevice();
    void acceptsStringEncodedInt64();
    void rejectsNonSessionsResponse();
    void skipsMalformedEntries();
    void handlesEmptySessionList();
};

void SessionRequestsTest::buildsGetActiveSessionsRequest()
{
    const QJsonObject request = getActiveSessionsRequest();
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getActiveSessions"));
    QCOMPARE(request.size(), 1);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void SessionRequestsTest::buildsTerminateSessionRequest()
{
    const QJsonObject request = terminateSessionRequest(1234567890123LL);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("terminateSession"));
    QCOMPARE(request.value(QStringLiteral("session_id")).toVariant().toLongLong(),
             1234567890123LL);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void SessionRequestsTest::buildsTerminateAllOtherSessionsRequest()
{
    const QJsonObject request = terminateAllOtherSessionsRequest();
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("terminateAllOtherSessions"));
    QCOMPARE(request.size(), 1);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void SessionRequestsTest::parsesSessionsInOrder()
{
    const QJsonObject response = QJsonDocument::fromJson(R"JSON(
        {
            "@type": "sessions",
            "sessions": [
                {
                    "@type": "session",
                    "id": 111,
                    "is_current": true,
                    "device_model": "Pixel 8",
                    "platform": "Android",
                    "location": "Berlin, Germany",
                    "ip_address": "203.0.113.5",
                    "last_active_date": 1760000000
                },
                {
                    "@type": "session",
                    "id": 222,
                    "is_current": false,
                    "device_model": "Desktop",
                    "platform": "Windows",
                    "location": "London, United Kingdom",
                    "ip_address": "198.51.100.9",
                    "last_active_date": 1759000000
                }
            ],
            "inactive_session_ttl_days": 180
        })JSON")
                                       .object();

    const QVector<SessionInfo> sessions = sessionsFromResponse(response);
    QCOMPARE(sessions.size(), 2);

    QCOMPARE(sessions.at(0).id, qint64(111));
    QCOMPARE(sessions.at(0).device, QStringLiteral("Pixel 8"));
    QCOMPARE(sessions.at(0).location, QStringLiteral("Berlin, Germany"));
    QCOMPARE(sessions.at(0).ip, QStringLiteral("203.0.113.5"));
    QCOMPARE(sessions.at(0).lastActive, qint64(1760000000));
    QVERIFY(sessions.at(0).current);

    QCOMPARE(sessions.at(1).id, qint64(222));
    QCOMPARE(sessions.at(1).device, QStringLiteral("Desktop"));
    QCOMPARE(sessions.at(1).lastActive, qint64(1759000000));
    QVERIFY(!sessions.at(1).current);
}

void SessionRequestsTest::fallsBackToPlatformForDevice()
{
    const QJsonObject response = QJsonDocument::fromJson(R"JSON(
        {
            "@type": "sessions",
            "sessions": [
                {
                    "@type": "session",
                    "id": 1,
                    "device_model": "",
                    "platform": "iOS",
                    "is_current": false
                },
                {
                    "@type": "session",
                    "id": 2,
                    "platform": "macOS",
                    "is_current": false
                },
                {
                    "@type": "session",
                    "id": 3,
                    "is_current": false
                }
            ]
        })JSON")
                                       .object();

    const QVector<SessionInfo> sessions = sessionsFromResponse(response);
    QCOMPARE(sessions.size(), 3);
    QCOMPARE(sessions.at(0).device, QStringLiteral("iOS"));
    QCOMPARE(sessions.at(1).device, QStringLiteral("macOS"));
    QVERIFY(sessions.at(2).device.isEmpty());
    QCOMPARE(sessions.at(2).lastActive, qint64(0));
    QVERIFY(!sessions.at(2).current);
}

void SessionRequestsTest::acceptsStringEncodedInt64()
{
    // The JSON interface may carry int64 values as strings; both id and the
    // timestamp must survive without a numeric conversion losing them.
    const QJsonObject response = QJsonDocument::fromJson(R"JSON(
        {
            "@type": "sessions",
            "sessions": [
                {
                    "@type": "session",
                    "id": "9007199254740991",
                    "last_active_date": "1760000123",
                    "is_current": false
                }
            ]
        })JSON")
                                       .object();

    const QVector<SessionInfo> sessions = sessionsFromResponse(response);
    QCOMPARE(sessions.size(), 1);
    QCOMPARE(sessions.at(0).id, qint64(9007199254740991LL));
    QCOMPARE(sessions.at(0).lastActive, qint64(1760000123));
}

void SessionRequestsTest::rejectsNonSessionsResponse()
{
    QVERIFY(sessionsFromResponse(QJsonObject{
                                    {QStringLiteral("@type"), QStringLiteral("error")},
                                    {QStringLiteral("code"), 401},
                                    {QStringLiteral("message"), QStringLiteral("Unauthorized")},
                                })
                .isEmpty());

    // The @type identifies the answer; a bare sessions field from some other
    // object is not a sessions response.
    QVERIFY(sessionsFromResponse(QJsonObject{
                                    {QStringLiteral("sessions"),
                                     QJsonArray{QJsonObject{{QStringLiteral("id"), 1}}}},
                                })
                .isEmpty());

    QVERIFY(sessionsFromResponse(QJsonObject{}).isEmpty());
}

void SessionRequestsTest::skipsMalformedEntries()
{
    const QJsonObject response = QJsonDocument::fromJson(R"JSON(
        {
            "@type": "sessions",
            "sessions": [
                42,
                null,
                {},
                {"id": 0},
                {"id": -5},
                {"id": 7, "device_model": "Kept", "is_current": false}
            ]
        })JSON")
                                       .object();

    const QVector<SessionInfo> sessions = sessionsFromResponse(response);
    QCOMPARE(sessions.size(), 1);
    QCOMPARE(sessions.at(0).id, qint64(7));
    QCOMPARE(sessions.at(0).device, QStringLiteral("Kept"));
}

void SessionRequestsTest::handlesEmptySessionList()
{
    QVERIFY(sessionsFromResponse(QJsonObject{
                                    {QStringLiteral("@type"), QStringLiteral("sessions")},
                                    {QStringLiteral("sessions"), QJsonArray()},
                                })
                .isEmpty());

    QVERIFY(sessionsFromResponse(QJsonObject{
                                    {QStringLiteral("@type"), QStringLiteral("sessions")},
                                })
                .isEmpty());
}

QTEST_GUILESS_MAIN(SessionRequestsTest)
#include "tst_session_requests.moc"
