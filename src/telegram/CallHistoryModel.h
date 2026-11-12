#pragma once

#include "CallHistory.h"

#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Holds an account's call history in the order a call list renders it.
///
/// The model reuses CallRecord, the value type that callFromMessage() produces,
/// so parsed calls can be handed over without an intermediate copy. Two of the
/// fields carry a different name than a call list might use, and the model
/// reads them as follows:
///
///   CallRecord::messageId  the identity of the call, used to de-duplicate
///   CallRecord::chatId     the peer, matched by withUser()
///   CallRecord::duration   the call length in seconds
///   CallRecord::status     "missed" marks a missed incoming call
///
/// setRecords() is the only way to fill the model. It drops records without a
/// positive id, keeps the first record for each id, clamps a negative duration
/// to 0 and sorts the result newest first by date, then by id descending so two
/// calls sharing a date still have a stable order. The model is a plain value
/// holder: it never touches TDLib, the network or the UI.
class CallHistoryModel final : public QObject {
    Q_OBJECT

public:
    explicit CallHistoryModel(QObject* parent = nullptr);

    /// Replaces the history with the prepared form of `records`: invalid and
    /// duplicate records are dropped, durations are clamped and the list is
    /// sorted newest first. Emits changed() only when the stored list differs.
    void setRecords(QVector<CallRecord> records);

    /// Returns a copy of the stored history, newest first.
    [[nodiscard]] QVector<CallRecord> records() const;

    /// Number of stored records.
    [[nodiscard]] int count() const;

    /// The missed calls, keeping the stored order.
    [[nodiscard]] QVector<CallRecord> missed() const;

    /// The calls with `userId`, keeping the stored order. A non-positive
    /// `userId` matches nothing, so it always yields an empty list.
    [[nodiscard]] QVector<CallRecord> withUser(qint64 userId) const;

    /// Number of stored records whose status is "missed".
    [[nodiscard]] int missedCount() const;

    /// Drops every record, emitting changed() once when there was anything to
    /// drop. Clearing an already empty model is a no-op.
    void clear();

signals:
    void changed();

private:
    QVector<CallRecord> m_records;
};

} // namespace beygram::telegram
