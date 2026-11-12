#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One row of a group call's participant list.
///
/// TDLib delivers a group call as a call object plus per-participant updates.
/// This struct is the flattened form the UI renders: the user behind the row,
/// the name to show, and the three flags that decorate it. It is a plain value
/// so it can be copied into and out of the model without sharing state.
struct GroupCallParticipant {
    qint64 userId = 0;
    QString name;
    bool muted = false;
    bool speaking = false;
    bool video = false;
};

/// Holds the group call this client is currently in.
///
/// The model stores two things that arrive from different places: the call
/// itself (its id and how many people are in it) and the participant roster.
/// participantCount() is the number the server reports for the call, which can
/// exceed the number of rows we have been told about, so it is kept apart from
/// participants().size() instead of being derived from it.
///
/// A call is active exactly when it has a positive id. setCall() refuses a
/// non-positive id, so there is no way to enter a half-open state, and leave()
/// is the only way out. When setCall() moves to a different id the stored
/// roster is dropped, because those rows described the previous call.
///
/// The model is a plain value holder: it never touches TDLib, the network or
/// the UI, and it does no logging. Every mutator emits changed() at most once
/// and only when the stored state really differs, so a repeated update from
/// TDLib does not repaint the call screen.
class GroupCallModel final : public QObject {
    Q_OBJECT

public:
    explicit GroupCallModel(QObject* parent = nullptr);

    /// Records the call this client joined. A non-positive `callId` names no
    /// call and is ignored, leaving the model untouched. A negative
    /// `participantCount` is clamped to 0. Moving to a different call id
    /// discards the participants collected for the previous one; repeating the
    /// same id and count changes nothing and emits nothing.
    void setCall(qint64 callId, int participantCount);

    /// True once setCall() has accepted a call and before leave().
    [[nodiscard]] bool active() const;

    /// The call id, or 0 when no call is active.
    [[nodiscard]] qint64 callId() const;

    /// The participant count reported for the call, clamped to at least 0. It
    /// is independent of how many participants have been delivered.
    [[nodiscard]] int participantCount() const;

    /// Replaces the roster with the prepared form of `participants`: entries
    /// with a non-positive userId name nobody and are dropped, and a repeated
    /// userId keeps its first entry, so a later duplicate never overwrites the
    /// data we already showed. The surviving entries keep their input order,
    /// which is the order the call screen lists them in. Emits changed() only
    /// when the stored roster differs.
    void setParticipants(QVector<GroupCallParticipant> participants);

    /// Returns a copy of the stored roster in stored order.
    [[nodiscard]] QVector<GroupCallParticipant> participants() const;

    /// Number of stored participants whose speaking flag is set. The flag is
    /// read as delivered: a muted entry that still claims to speak counts,
    /// because the server is the authority on who is audible.
    [[nodiscard]] int speakingCount() const;

    /// Applies a single participant update. Only an existing userId is
    /// accepted; an unknown or non-positive id is refused, since adding a row
    /// is setParticipants()'s job and a stray update must not invent one.
    /// Returns true, and emits changed(), only when a stored field really
    /// changed, so a heartbeat that repeats the current state is silent.
    bool updateParticipant(const GroupCallParticipant& participant);

    /// Ends the call: the id, the reported count and the roster are all
    /// dropped, leaving the same empty state as a fresh model. Emits changed()
    /// once when there was anything to drop; leaving twice is a no-op.
    void leave();

    /// Resets the model to its empty state, for teardown paths that are not a
    /// leave (an account switch, a closed window). Clearing an already empty
    /// model is a no-op and emits nothing.
    void clear();

signals:
    void changed();

private:
    qint64 m_callId = 0;
    int m_count = 0;
    QVector<GroupCallParticipant> m_participants;
};

} // namespace beygram::telegram
