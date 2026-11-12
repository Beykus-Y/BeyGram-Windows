#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// The account's Telegram Stars balance, as the UI needs it.
///
/// This is display and arithmetic state only: no transport, no request builder,
/// no persistence and no logging. The balance itself comes from the server (the
/// star amounts TDLib reports, e.g. starTransaction.amount in
/// getStarTransactions from PaymentRequests.h); this model never asks for it and
/// never decides what a star is worth.
///
/// Naming follows the rest of the star code: GiftRequests.h and GiftCatalogModel
/// call a star price starCount, and PaymentRequests.h groups the same currency
/// under getStarTransactions. TDLib types those amounts as int53, so the balance
/// is a qint64 rather than an int: a large account must not truncate.
///
/// The unit is never pluralized or localized. displayText() follows the same
/// "amount + separator + unit" shape as PaymentCheckoutModel::totalText()
/// ("1234.56 USD"), with a space-grouped integer and the star glyph as the unit,
/// so a balance reads "1 234 ⭐" regardless of the system locale.
///
/// The balance is never negative: setBalance() and add() clamp at 0, because a
/// negative star count is not a state the server reports and would render as a
/// debt the account cannot have. Every mutator is a no-op when the stored value
/// would not change, and changed() is emitted only for a real change, so a view
/// can bind to it without repainting on redundant updates.
class StarBalanceModel final : public QObject {
    Q_OBJECT

public:
    explicit StarBalanceModel(QObject* parent = nullptr);

    /// Replaces the balance with the server's value. A negative value is not a
    /// balance, so it is stored as 0. Emits changed() only when the stored value
    /// actually differs.
    void setBalance(qint64 stars);

    /// The current balance, always >= 0.
    [[nodiscard]] qint64 balance() const;

    /// Whether a price of amount stars is covered by the balance.
    ///
    /// A negative amount is not a price, so it is never affordable; this keeps
    /// a malformed value from reading as "free". An amount equal to the balance
    /// is affordable, i.e. spending the whole balance is allowed.
    [[nodiscard]] bool canAfford(qint64 amount) const;

    /// Credits (positive amount) or debits (negative amount) the balance.
    ///
    /// The result saturates at 0 instead of going negative, and at the qint64
    /// maximum instead of wrapping, because both wrap-arounds would display as a
    /// nonsense balance. An amount of 0 changes nothing and emits nothing.
    void add(qint64 amount);

    /// The balance as text, thousands grouped by a space and the star glyph
    /// appended: "0 ⭐", "999 ⭐", "1 234 ⭐", "1 234 567 ⭐".
    ///
    /// The separator is a plain space and the grouping is fixed, not
    /// locale-dependent, so the string is stable in tests and identical in every
    /// language. The star is U+2B50 (WHITE MEDIUM STAR), the glyph Telegram uses
    /// for the currency.
    [[nodiscard]] QString displayText() const;

    /// Returns the balance to 0, e.g. when the account changes or the Stars
    /// screen closes. A no-op, with no signal, when the balance is already 0.
    void clear();

signals:
    /// Emitted whenever balance() actually changed.
    void changed();

private:
    qint64 m_stars = 0;
};

} // namespace beygram::telegram
