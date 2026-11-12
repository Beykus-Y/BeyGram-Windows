#pragma once

#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One entry of a gift history list.
///
/// The row is a flattened gift: a history view needs the gift identity, both
/// peers, the send date and the price at once. GiftRequests.h has no value type
/// for that. GiftInfo there describes only the getAvailableGifts catalog (id,
/// title, sticker, starCount) and carries no sender, receiver, date or anonymity
/// field, so it cannot be reused here. giftId and starCount correspond to
/// GiftInfo::id and GiftInfo::starCount; starCount is widened to qint64 because
/// the schema types star_count as int53.
struct GiftHistoryRow {
    qint64 giftId = 0;         ///< Identity of the gift, used to de-duplicate.
    qint64 senderUserId = 0;   ///< Sending user; 0 when the row records none.
    qint64 receiverUserId = 0; ///< Receiving user; 0 when the row records none.
    qint64 date = 0;           ///< Send time as a Unix timestamp in seconds.
    qint64 starCount = 0;      ///< Price in stars, clamped to >= 0 by the model.
    bool anonymous = false;    ///< The sender is hidden from other users.
};

/// Holds a gift history in the order a gift list renders it.
///
/// setRows() and append() are the only ways to fill the model. Both drop rows
/// without a positive giftId and keep the first row of each giftId, so a repeated
/// page never duplicates a gift. setRows() replaces the whole list, while
/// append() only adds ids that are not stored yet and leaves an already stored
/// gift untouched even when the incoming copy differs. Both clamp a negative
/// starCount to 0 and sort the result newest first by date, then by giftId
/// descending so two gifts sharing a date keep a stable order.
///
/// sent() and received() split the stored list by direction and keep that order:
/// a row belongs to sent() when senderUserId is not 0 and to received() when
/// receiverUserId is not 0, so 0 means "no such peer is recorded". The anonymous
/// flag is carried for the view and does not affect either split. totalStars()
/// sums the stored prices and saturates at the qint64 maximum instead of
/// overflowing. The model is a plain value holder: it never touches TDLib, the
/// network or the UI.
class GiftHistoryModel final : public QObject {
    Q_OBJECT

public:
    explicit GiftHistoryModel(QObject* parent = nullptr);

    /// Replaces the list with the prepared form of `rows`: invalid and duplicate
    /// rows are dropped, star counts are clamped and the list is sorted newest
    /// first. Emits changed() only when the stored list differs.
    void setRows(QVector<GiftHistoryRow> rows);

    /// Returns a copy of the stored rows, newest first.
    [[nodiscard]] QVector<GiftHistoryRow> rows() const;

    /// Number of stored rows.
    [[nodiscard]] int count() const;

    /// The rows with a non-zero senderUserId, keeping the stored order.
    [[nodiscard]] QVector<GiftHistoryRow> sent() const;

    /// The rows with a non-zero receiverUserId, keeping the stored order.
    [[nodiscard]] QVector<GiftHistoryRow> received() const;

    /// Sum of the stored star counts, saturated at the qint64 maximum.
    [[nodiscard]] qint64 totalStars() const;

    /// Merges the not yet stored rows of `rows` into the list, for paging. Stored
    /// rows keep their copy and the merged list is re-sorted. Emits changed() only
    /// when at least one row was added.
    void append(QVector<GiftHistoryRow> rows);

    /// Drops every row, emitting changed() once when there was anything to drop.
    /// Clearing an already empty model is a no-op.
    void clear();

signals:
    void changed();

private:
    QVector<GiftHistoryRow> m_rows;
};

} // namespace beygram::telegram
