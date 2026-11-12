#pragma once

#include <QJsonObject>
#include <QtGlobal>

namespace beygram::telegram {

/// Request builders for the group-call (voice chat) surface.
///
/// These builders are pure: they validate and clamp their arguments, add no
/// @extra and do no logging. The transport owns correlation and retries.
///
/// Every builder returns an empty object when a required identifier is not
/// positive, so a caller can check isEmpty() and skip the round trip:
///   - groupCallId must be > 0 for every method;
///   - userId must be > 0 for the two per-participant methods;
///   - audioSourceId may be 0, which lets TDLib pick the default source, but a
///     negative value is rejected.

/// Builds getGroupCall for one call.
///
/// The answer is a groupCall object; live participant and state updates arrive
/// separately as updateGroupCall / updateGroupCallParticipants.
[[nodiscard]] QJsonObject getGroupCallRequest(qint64 groupCallId);

/// Builds joinGroupCall for the current user.
///
/// audioSourceId 0 selects the default audio source. muted and video are sent
/// as is_muted and is_video_enabled; the caller owns the microphone and camera
/// state, so both flags are written exactly as passed.
[[nodiscard]] QJsonObject joinGroupCallRequest(qint64 groupCallId, qint64 audioSourceId,
                                               bool muted, bool video);

/// Builds leaveGroupCall for one call.
///
/// Only meaningful while the call is joined or being joined.
[[nodiscard]] QJsonObject leaveGroupCallRequest(qint64 groupCallId);

/// Builds loadGroupCallParticipants for one call.
///
/// limit is clamped to 1..1000. It bounds how many participants this request
/// loads; it is not the call's total participant count.
[[nodiscard]] QJsonObject loadGroupCallParticipantsRequest(qint64 groupCallId, int limit);

/// Builds toggleGroupCallParticipantIsMuted for one participant.
///
/// userId is the participant's user id; muted is written as is_muted. A call
/// owner or administrator may mute another participant this way.
[[nodiscard]] QJsonObject toggleGroupCallParticipantIsMutedRequest(qint64 groupCallId,
                                                                   qint64 userId, bool muted);

/// Builds setGroupCallParticipantVolumeLevel for one participant.
///
/// volume is clamped to 0..20000, TDLib's linear scale where 10000 is the
/// unchanged level (100%), 0 is silent and 20000 is the maximum boost.
[[nodiscard]] QJsonObject setGroupCallParticipantVolumeLevelRequest(qint64 groupCallId,
                                                                    qint64 userId, int volume);

/// Builds startGroupCallScreenSharing for one call.
///
/// The call must already be joined and the caller must be allowed to share.
[[nodiscard]] QJsonObject startGroupCallScreenSharingRequest(qint64 groupCallId);

/// Builds endGroupCallScreenSharing for one call.
///
/// Only meaningful while a screen share started by this client is active.
[[nodiscard]] QJsonObject endGroupCallScreenSharingRequest(qint64 groupCallId);

} // namespace beygram::telegram
