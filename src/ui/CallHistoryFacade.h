#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace beygram::ui {

/// QML-facing, transport-free view of one account's call history.
///
/// The owner hands the facade the five fields a call row needs through
/// setRecords() and the facade prepares them for a ListView or a Repeater: it
/// validates and de-duplicates the ids, clamps the numbers, orders the calls
/// newest first and renders each one as a compact JSON object. The facade never
/// touches TDLib, the network or the clock, so a view can be driven from a test
/// with plain strings.
///
/// The five input lists are parallel and are read by index. `ids` decides how
/// many calls there are; a shorter `names`, `dates`, `durations` or `missed`
/// list leaves the tail at its default (an empty name, date 0, duration 0, a
/// completed incoming call). An id must be a positive decimal number: a blank,
/// zero, negative or non-numeric id drops the whole call, because a row without
/// an id could not call back. When an id appears twice the first record wins,
/// matching CallHistoryModel.
///
/// Each entry in entries() is a compact JSON object, not a display line, so a
/// delegate can read named fields without splitting a delimiter out of a name:
///
///   id             the call id, trimmed, as callBackRequested() reports it
///   name           the display name, "Unknown" when the supplied name is blank
///   outgoing       true for a call the user placed
///   missed         true for a missed incoming call
///   date           the start of the call in epoch seconds, 0 when unknown
///   duration       the answered length in seconds, never negative
///   durationLabel  that length as "mm:ss"
///
/// A prepared entry looks like:
///   {"date":1763030400,"duration":75,"durationLabel":"01:15",
///    "id":"42","missed":false,"name":"Ada","outgoing":true}
///
/// The fifth input list, `missed`, carries each call's outcome. It accepts both
/// a flag and a CallRecord::status token, so the owner can pass either what its
/// model stores or a plain boolean:
///
///   "missed", "true", "1", "yes"        a missed incoming call
///   "cancelled"/"canceled", "outgoing",
///   "noanswer"/"no-answer"/"no_answer"  a non-missed outgoing call
///   "incoming", "declined", ...         a non-missed incoming call
///   anything else, including ""         a non-missed incoming call
///
/// Dates are compared as numbers, newest first; two calls that share a date are
/// ordered by id descending, so the prepared list is stable and matches
/// CallHistoryModel's order. A date or duration that does not parse falls back
/// to 0 instead of rejecting the call.
///
/// changed() is the single NOTIFY for all three properties and is emitted only
/// when the prepared list really differs, so re-reporting the same page (for
/// example after a refresh, or in another input order) repaints nothing.
///
/// callBackRequested() leaves the facade when the user presses a row's
/// call-back button: QML emits it with the entry's id, and the owner connects it
/// to the call controller. The facade itself never starts a call.
class CallHistoryFacade final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QStringList entries READ entries NOTIFY changed)
    Q_PROPERTY(int missedCount READ missedCount NOTIFY changed)
    Q_PROPERTY(bool empty READ empty NOTIFY changed)

public:
    explicit CallHistoryFacade(QObject* parent = nullptr);

    Q_DISABLE_COPY_MOVE(CallHistoryFacade)

    /// The prepared calls, newest first, one compact JSON object per call.
    [[nodiscard]] QStringList entries() const;

    /// Number of entries whose "missed" field is true.
    [[nodiscard]] int missedCount() const;

    /// True while entries() is empty.
    [[nodiscard]] bool empty() const;

    /// Replaces the history with the prepared form of the five parallel lists.
    /// Emits changed() only when the prepared list differs from the stored one.
    Q_INVOKABLE void setRecords(const QStringList& ids, const QStringList& names,
                                const QStringList& dates, const QStringList& durations,
                                const QStringList& missed);

    /// Drops every call, emitting changed() once when there was anything to
    /// drop. Clearing an already empty facade is a no-op.
    Q_INVOKABLE void clear();

signals:
    void changed();

    /// The user asked to call `callId` back.
    void callBackRequested(const QString& callId);

private:
    struct Entry {
        QString id;
        QString name;
        qint64 idValue = 0;
        qint64 date = 0;
        int duration = 0;
        bool outgoing = false;
        bool missed = false;

        bool operator==(const Entry& other) const;
    };

    /// Serializes one prepared call as a compact JSON object.
    static QString entryJson(const Entry& entry);

    QVector<Entry> m_entries;
    /// The JSON form of m_entries, kept in step so entries() never rebuilds it.
    QStringList m_lines;
};

} // namespace beygram::ui
