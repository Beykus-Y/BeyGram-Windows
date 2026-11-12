#include "GroupCallModel.h"

#include <QSet>

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

bool sameParticipant(const GroupCallParticipant& left, const GroupCallParticipant& right)
{
    return left.userId == right.userId && left.name == right.name
        && left.muted == right.muted && left.speaking == right.speaking
        && left.video == right.video;
}

// The roster is compared field by field rather than with operator==, so a
// repeated delivery of the same participants stays silent while any visible
// change (a new name, a mute, a raised hand) is reported.
bool sameParticipants(const QVector<GroupCallParticipant>& left,
                      const QVector<GroupCallParticipant>& right)
{
    if (left.size() != right.size())
        return false;
    for (qsizetype index = 0; index < left.size(); ++index) {
        if (!sameParticipant(left.at(index), right.at(index)))
            return false;
    }
    return true;
}

} // namespace

GroupCallModel::GroupCallModel(QObject* parent)
    : QObject(parent)
{
}

void GroupCallModel::setCall(qint64 callId, int participantCount)
{
    // A call without a positive id is not a call. Refusing it here keeps
    // active() and callId() in agreement with the rest of the model.
    if (callId <= 0)
        return;

    const int count = std::max(participantCount, 0);
    const bool newCall = callId != m_callId;

    // Same call, same count: nothing a listener could react to.
    if (!newCall && count == m_count)
        return;

    // Rows collected for the previous call describe people in a call we are no
    // longer in, so they go with it.
    if (newCall)
        m_participants.clear();

    m_callId = callId;
    m_count = count;
    emit changed();
}

bool GroupCallModel::active() const
{
    return m_callId > 0;
}

qint64 GroupCallModel::callId() const
{
    return m_callId;
}

int GroupCallModel::participantCount() const
{
    return m_count;
}

void GroupCallModel::setParticipants(QVector<GroupCallParticipant> participants)
{
    QVector<GroupCallParticipant> prepared;
    prepared.reserve(participants.size());

    // Keep the input order: the call screen lists people as they joined, and
    // re-sorting would move rows under the cursor on every update.
    QSet<qint64> seen;
    for (const GroupCallParticipant& participant : std::as_const(participants)) {
        if (participant.userId <= 0)
            continue;
        if (seen.contains(participant.userId))
            continue;
        seen.insert(participant.userId);
        prepared.append(participant);
    }

    if (sameParticipants(prepared, m_participants))
        return;

    m_participants = std::move(prepared);
    emit changed();
}

QVector<GroupCallParticipant> GroupCallModel::participants() const
{
    return m_participants;
}

int GroupCallModel::speakingCount() const
{
    int count = 0;
    for (const GroupCallParticipant& participant : m_participants) {
        if (participant.speaking)
            ++count;
    }
    return count;
}

bool GroupCallModel::updateParticipant(const GroupCallParticipant& participant)
{
    if (participant.userId <= 0)
        return false;

    for (GroupCallParticipant& stored : m_participants) {
        if (stored.userId != participant.userId)
            continue;
        if (sameParticipant(stored, participant))
            return false;
        stored = participant;
        emit changed();
        return true;
    }

    // Unknown participant: an update cannot add a row.
    return false;
}

void GroupCallModel::leave()
{
    if (m_callId == 0 && m_count == 0 && m_participants.isEmpty())
        return;

    m_callId = 0;
    m_count = 0;
    m_participants.clear();
    emit changed();
}

void GroupCallModel::clear()
{
    if (m_callId == 0 && m_count == 0 && m_participants.isEmpty())
        return;

    m_callId = 0;
    m_count = 0;
    m_participants.clear();
    emit changed();
}

} // namespace beygram::telegram
