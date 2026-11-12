#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One entry of a star transaction list.
///
/// `amount` is a magnitude: the model stores the absolute value and leaves the
/// direction of the transfer to `incoming`, so a refund and the payment it
/// reverses share the same amount and differ only in that flag.
struct StarTransaction {
    qint64 id = 0;
    qint64 date = 0;
    qint64 amount = 0;
    QString title;
    bool incoming = false;
};

/// Holds an account's star transactions in the order a history view renders
/// them.
///
/// setTransactions() and append() are the only ways to fill the model. Both drop
/// transactions without a positive id and keep the first copy of each id, so a
/// repeated page never duplicates an entry. setTransactions() replaces the whole
/// list, while append() only adds ids that are not stored yet and leaves an
/// already stored transaction untouched even when the incoming copy differs.
/// Both store `amount` as its absolute value and sort the result newest first by
/// date, then by id descending so two transactions sharing a date keep a stable
/// order.
///
/// The totals are computed from the stored amounts: totalIncoming() sums the
/// incoming ones, totalOutgoing() the outgoing ones and net() is their
/// difference. The two sums saturate at the qint64 maximum instead of
/// overflowing, so net() is always exact.
/// The model is a plain value holder: it never touches TDLib, the network or the
/// UI.
class StarTransactionModel final : public QObject {
    Q_OBJECT

public:
    explicit StarTransactionModel(QObject* parent = nullptr);

    /// Replaces the list with the prepared form of `transactions`: invalid and
    /// duplicate entries are dropped, amounts become absolute and the list is
    /// sorted newest first. Emits changed() only when the stored list differs.
    void setTransactions(QVector<StarTransaction> transactions);

    /// Returns a copy of the stored transactions, newest first.
    [[nodiscard]] QVector<StarTransaction> transactions() const;

    /// Number of stored transactions.
    [[nodiscard]] int count() const;

    /// Sum of the incoming amounts, saturated at the qint64 maximum.
    [[nodiscard]] qint64 totalIncoming() const;

    /// Sum of the outgoing amounts, saturated at the qint64 maximum.
    [[nodiscard]] qint64 totalOutgoing() const;

    /// totalIncoming() minus totalOutgoing(), so a spending account is negative.
    [[nodiscard]] qint64 net() const;

    /// Merges the not yet stored entries of `transactions` into the list, for
    /// paging. Stored entries keep their copy and the merged list is re-sorted.
    /// Emits changed() only when at least one entry was added.
    void append(QVector<StarTransaction> transactions);

    /// Drops every transaction, emitting changed() once when there was anything
    /// to drop. Clearing an already empty model is a no-op.
    void clear();

signals:
    void changed();

private:
    QVector<StarTransaction> m_items;
};

} // namespace beygram::telegram
