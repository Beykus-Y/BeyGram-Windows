#include "telegram/GroupCallRequests.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

constexpr qint64 kCallId = 1234567890123LL;
constexpr qint64 kUserId = 987654321LL;

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

QString typeOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("@type")).toString();
}

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

} // namespace

class GroupCallRequestsTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsCallLifecycleRequests();
    void buildsJoinRequest();
    void clampsParticipantLimit();
    void buildsParticipantMuteRequest();
    void clampsParticipantVolume();
    void buildsScreenSharingRequests();
    void rejectsInvalidIdentifiers();
    void omitsExtraFromEveryRequest();
};

void GroupCallRequestsTest::buildsCallLifecycleRequests()
{
    const QJsonObject get = serialized(getGroupCallRequest(kCallId));
    QCOMPARE(typeOf(get), QStringLiteral("getGroupCall"));
    QCOMPARE(numberAt(get, QStringLiteral("group_call_id")), kCallId);
    QCOMPARE(int(get.size()), 2);

    const QJsonObject leave = serialized(leaveGroupCallRequest(kCallId));
    QCOMPARE(typeOf(leave), QStringLiteral("leaveGroupCall"));
    QCOMPARE(numberAt(leave, QStringLiteral("group_call_id")), kCallId);
    QCOMPARE(int(leave.size()), 2);
}

void GroupCallRequestsTest::buildsJoinRequest()
{
    // audio_source_id 0 is the "let TDLib pick a source" value, not an error.
    const QJsonObject autoSource =
        serialized(joinGroupCallRequest(kCallId, 0, false, false));
    QCOMPARE(typeOf(autoSource), QStringLiteral("joinGroupCall"));
    QCOMPARE(numberAt(autoSource, QStringLiteral("group_call_id")), kCallId);
    QCOMPARE(numberAt(autoSource, QStringLiteral("audio_source_id")), qint64(0));
    QVERIFY(!autoSource.value(QStringLiteral("is_muted")).toBool());
    QVERIFY(!autoSource.value(QStringLiteral("is_video_enabled")).toBool());
    QCOMPARE(int(autoSource.size()), 5);

    const QJsonObject active =
        serialized(joinGroupCallRequest(kCallId, 42, true, true));
    QCOMPARE(numberAt(active, QStringLiteral("audio_source_id")), qint64(42));
    QVERIFY(active.value(QStringLiteral("is_muted")).toBool());
    QVERIFY(active.value(QStringLiteral("is_video_enabled")).toBool());
}

void GroupCallRequestsTest::clampsParticipantLimit()
{
    struct Case {
        int limit;
        int expected;
    };

    const Case cases[] = {
        {-5, 1},
        {0, 1},
        {1, 1},
        {250, 250},
        {1000, 1000},
        {1001, 1000},
        {2147483647, 1000},
    };

    for (const Case& entry : cases) {
        const QJsonObject request =
            serialized(loadGroupCallParticipantsRequest(kCallId, entry.limit));
        QCOMPARE(typeOf(request), QStringLiteral("loadGroupCallParticipants"));
        QCOMPARE(numberAt(request, QStringLiteral("group_call_id")), kCallId);
        QCOMPARE(numberAt(request, QStringLiteral("limit")), qint64(entry.expected));
        QCOMPARE(int(request.size()), 3);
    }
}

void GroupCallRequestsTest::buildsParticipantMuteRequest()
{
    const QJsonObject muted = serialized(
        toggleGroupCallParticipantIsMutedRequest(kCallId, kUserId, true));
    QCOMPARE(typeOf(muted), QStringLiteral("toggleGroupCallParticipantIsMuted"));
    QCOMPARE(numberAt(muted, QStringLiteral("group_call_id")), kCallId);
    QCOMPARE(numberAt(muted, QStringLiteral("user_id")), kUserId);
    QVERIFY(muted.value(QStringLiteral("is_muted")).toBool());
    QCOMPARE(int(muted.size()), 4);

    const QJsonObject unmuted = serialized(
        toggleGroupCallParticipantIsMutedRequest(kCallId, kUserId, false));
    QVERIFY(!unmuted.value(QStringLiteral("is_muted")).toBool());
}

void GroupCallRequestsTest::clampsParticipantVolume()
{
    struct Case {
        int volume;
        int expected;
    };

    const Case cases[] = {
        {-1, 0},
        {0, 0},
        {1, 1},
        {10000, 10000},
        {20000, 20000},
        {20001, 20000},
        {2147483647, 20000},
    };

    for (const Case& entry : cases) {
        const QJsonObject request = serialized(
            setGroupCallParticipantVolumeLevelRequest(kCallId, kUserId, entry.volume));
        QCOMPARE(typeOf(request), QStringLiteral("setGroupCallParticipantVolumeLevel"));
        QCOMPARE(numberAt(request, QStringLiteral("group_call_id")), kCallId);
        QCOMPARE(numberAt(request, QStringLiteral("user_id")), kUserId);
        QCOMPARE(numberAt(request, QStringLiteral("volume_level")),
                 qint64(entry.expected));
        QCOMPARE(int(request.size()), 4);
    }
}

void GroupCallRequestsTest::buildsScreenSharingRequests()
{
    const QJsonObject start = serialized(startGroupCallScreenSharingRequest(kCallId));
    QCOMPARE(typeOf(start), QStringLiteral("startGroupCallScreenSharing"));
    QCOMPARE(numberAt(start, QStringLiteral("group_call_id")), kCallId);
    QCOMPARE(int(start.size()), 2);

    const QJsonObject end = serialized(endGroupCallScreenSharingRequest(kCallId));
    QCOMPARE(typeOf(end), QStringLiteral("endGroupCallScreenSharing"));
    QCOMPARE(numberAt(end, QStringLiteral("group_call_id")), kCallId);
    QCOMPARE(int(end.size()), 2);
}

void GroupCallRequestsTest::rejectsInvalidIdentifiers()
{
    // The call id is required by every builder.
    QVERIFY(getGroupCallRequest(0).isEmpty());
    QVERIFY(getGroupCallRequest(-1).isEmpty());
    QVERIFY(leaveGroupCallRequest(0).isEmpty());
    QVERIFY(leaveGroupCallRequest(-1).isEmpty());
    QVERIFY(loadGroupCallParticipantsRequest(0, 50).isEmpty());
    QVERIFY(loadGroupCallParticipantsRequest(-1, 50).isEmpty());
    QVERIFY(startGroupCallScreenSharingRequest(0).isEmpty());
    QVERIFY(startGroupCallScreenSharingRequest(-1).isEmpty());
    QVERIFY(endGroupCallScreenSharingRequest(0).isEmpty());
    QVERIFY(endGroupCallScreenSharingRequest(-1).isEmpty());

    // audio_source_id may be 0, but a negative source is invalid.
    QVERIFY(joinGroupCallRequest(0, 0, false, false).isEmpty());
    QVERIFY(joinGroupCallRequest(-1, 0, false, false).isEmpty());
    QVERIFY(joinGroupCallRequest(kCallId, -1, false, false).isEmpty());
    QVERIFY(!joinGroupCallRequest(kCallId, 0, false, false).isEmpty());

    // The per-participant methods need both ids.
    QVERIFY(toggleGroupCallParticipantIsMutedRequest(0, kUserId, true).isEmpty());
    QVERIFY(toggleGroupCallParticipantIsMutedRequest(kCallId, 0, true).isEmpty());
    QVERIFY(toggleGroupCallParticipantIsMutedRequest(kCallId, -1, true).isEmpty());
    QVERIFY(setGroupCallParticipantVolumeLevelRequest(0, kUserId, 10000).isEmpty());
    QVERIFY(setGroupCallParticipantVolumeLevelRequest(kCallId, 0, 10000).isEmpty());
    QVERIFY(setGroupCallParticipantVolumeLevelRequest(kCallId, -1, 10000).isEmpty());

    // The smallest valid participant id is accepted.
    QVERIFY(!toggleGroupCallParticipantIsMutedRequest(kCallId, 1, false).isEmpty());
    QVERIFY(!setGroupCallParticipantVolumeLevelRequest(kCallId, 1, 0).isEmpty());
}

void GroupCallRequestsTest::omitsExtraFromEveryRequest()
{
    const QVector<QJsonObject> requests{
        getGroupCallRequest(kCallId),
        joinGroupCallRequest(kCallId, 0, false, false),
        leaveGroupCallRequest(kCallId),
        loadGroupCallParticipantsRequest(kCallId, 50),
        toggleGroupCallParticipantIsMutedRequest(kCallId, kUserId, true),
        setGroupCallParticipantVolumeLevelRequest(kCallId, kUserId, 10000),
        startGroupCallScreenSharingRequest(kCallId),
        endGroupCallScreenSharingRequest(kCallId),
    };

    for (const QJsonObject& request : requests) {
        QVERIFY(!request.isEmpty());
        QVERIFY(!typeOf(request).isEmpty());
        QVERIFY(!request.contains(QStringLiteral("@extra")));
    }
}

QTEST_GUILESS_MAIN(GroupCallRequestsTest)
#include "tst_group_call_requests.moc"
