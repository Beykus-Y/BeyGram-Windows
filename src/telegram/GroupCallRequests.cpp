#include "GroupCallRequests.h"

#include <QString>

#include <algorithm>

namespace beygram::telegram {
namespace {

// loadGroupCallParticipants takes an int32 participant count per request.
constexpr int kMinParticipantsLimit = 1;
constexpr int kMaxParticipantsLimit = 1000;

// volume_level is a linear scale: 10000 is unchanged, 0 silent, 20000 maximum.
constexpr int kMinVolumeLevel = 0;
constexpr int kMaxVolumeLevel = 20000;

QJsonObject callRequest(const QString& type, qint64 groupCallId)
{
    return QJsonObject{
        {QStringLiteral("@type"), type},
        {QStringLiteral("group_call_id"), groupCallId},
    };
}

} // namespace

QJsonObject getGroupCallRequest(qint64 groupCallId)
{
    if (groupCallId <= 0)
        return {};
    return callRequest(QStringLiteral("getGroupCall"), groupCallId);
}

QJsonObject joinGroupCallRequest(qint64 groupCallId, qint64 audioSourceId, bool muted,
                                 bool video)
{
    // A negative source id is not a valid source and not the "pick one" value,
    // so the whole request is rejected rather than sent with a wrong source.
    if (groupCallId <= 0 || audioSourceId < 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("joinGroupCall")},
        {QStringLiteral("group_call_id"), groupCallId},
        {QStringLiteral("audio_source_id"), audioSourceId},
        {QStringLiteral("is_muted"), muted},
        {QStringLiteral("is_video_enabled"), video},
    };
}

QJsonObject leaveGroupCallRequest(qint64 groupCallId)
{
    if (groupCallId <= 0)
        return {};
    return callRequest(QStringLiteral("leaveGroupCall"), groupCallId);
}

QJsonObject loadGroupCallParticipantsRequest(qint64 groupCallId, int limit)
{
    if (groupCallId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("loadGroupCallParticipants")},
        {QStringLiteral("group_call_id"), groupCallId},
        {QStringLiteral("limit"),
         std::clamp(limit, kMinParticipantsLimit, kMaxParticipantsLimit)},
    };
}

QJsonObject toggleGroupCallParticipantIsMutedRequest(qint64 groupCallId, qint64 userId,
                                                     bool muted)
{
    if (groupCallId <= 0 || userId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("toggleGroupCallParticipantIsMuted")},
        {QStringLiteral("group_call_id"), groupCallId},
        {QStringLiteral("user_id"), userId},
        {QStringLiteral("is_muted"), muted},
    };
}

QJsonObject setGroupCallParticipantVolumeLevelRequest(qint64 groupCallId, qint64 userId,
                                                      int volume)
{
    if (groupCallId <= 0 || userId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setGroupCallParticipantVolumeLevel")},
        {QStringLiteral("group_call_id"), groupCallId},
        {QStringLiteral("user_id"), userId},
        {QStringLiteral("volume_level"),
         std::clamp(volume, kMinVolumeLevel, kMaxVolumeLevel)},
    };
}

QJsonObject startGroupCallScreenSharingRequest(qint64 groupCallId)
{
    if (groupCallId <= 0)
        return {};
    return callRequest(QStringLiteral("startGroupCallScreenSharing"), groupCallId);
}

QJsonObject endGroupCallScreenSharingRequest(qint64 groupCallId)
{
    if (groupCallId <= 0)
        return {};
    return callRequest(QStringLiteral("endGroupCallScreenSharing"), groupCallId);
}

} // namespace beygram::telegram
