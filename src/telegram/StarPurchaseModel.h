#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One row of the "buy Stars" screen: the amount of stars the package grants and
/// its price in the smallest unit of the store's currency (cents for USD, so
/// 199 is "1.99").
///
/// The type is a view value, not a wire object. PaymentRequests.h builds the
/// getPaymentForm/sendPaymentForm payloads for Stars and carries no package
/// list, and GiftCatalogModel prices gifts rather than top-ups, so there is no
/// existing type to reuse.
///
/// priceMinor is the currency's minor unit rather than a double: a price is
/// money and must not pick up a rounding error on its way to the screen.
/// currency is an ISO-style code ("USD", "EUR") and is stored upper-cased by the
/// model, because the server's casing is not display data.
struct StarPackage {
    qint64 stars = 0;
    qint64 priceMinor = 0;
    QString currency;
};

/// The star packages offered for purchase, plus which one the buyer picked.
///
/// This is display and selection state only: no transport, no provider SDK, no
/// persistence and no logging. The packages come from the server (the star
/// amounts TDLib reports, e.g. starTransaction.amount in getStarTransactions
/// from PaymentRequests.h); the model never asks for them and never decides what
/// a star is worth. The balance it is bought against is StarBalanceModel's, and
/// the price formatting follows PaymentCheckoutModel::totalText()'s
/// "amount + space + currency" shape.
///
/// The model's rules:
///
///   - setPackages drops packages with stars <= 0 (nothing could be bought for
///     them) and keeps only the first package of each star amount, because a
///     later duplicate would make the visible row depend on merge order. The
///     surviving rows keep their arrival order; nothing is sorted.
///   - setPackages clamps a negative priceMinor to 0 (a negative price is not a
///     price and would render as a refund) and upper-cases currency.
///   - The selection is positional: setPackages keeps the current index while it
///     still names a row and clears it to -1 when the new list is shorter, so
///     selectedIndex()/selectedPackage() never name a row the list no longer
///     has.
///   - select() ignores an index outside the list (a stale tap must not select
///     an invisible row) and clears the selection on -1, so a caller does not
///     have to know whether one was set.
///   - selectedPackage() returns a default-constructed StarPackage when nothing
///     is selected, so a caller can read the fields without a has-selection
///     branch. canPurchase() is the explicit "is there a selection" test.
///   - priceText() is empty when nothing is selected, otherwise the price with
///     exactly two decimals and the currency code: "1.99 USD". The digits are
///     built from the integer minor units, so the text is locale-independent and
///     exact in every language.
///   - reset() forgets the packages and the selection, e.g. when the screen
///     closes.
///
/// changed() is emitted only for a real change: a reload that brings the same
/// packages, a tap on the already-selected row or a repeated reset stays silent,
/// so a view can bind to it without echoing its own edits.
class StarPurchaseModel final : public QObject {
    Q_OBJECT

public:
    explicit StarPurchaseModel(QObject* parent = nullptr);

    /// Replaces the offered packages with packages after normalization (stars
    /// <= 0 dropped, first package of each star amount kept, negative prices
    /// clamped to 0, currencies upper-cased, order preserved).
    ///
    /// A selection whose index is past the new list is cleared. The call is a
    /// no-op, with no signal, when the normalized list equals the stored one and
    /// the selection survives; a dropped selection alone is a real change.
    void setPackages(QVector<StarPackage> packages);

    /// The stored packages, in display order, as a copy.
    [[nodiscard]] QVector<StarPackage> packages() const;

    /// Number of stored packages, i.e. packages().size().
    [[nodiscard]] int count() const;

    /// Selects the package at index. An index outside the list is ignored (the
    /// current selection stays); -1 clears the selection. Both are silent when
    /// the selection would not change.
    void select(int index);

    /// The selected index, or -1 when nothing is selected.
    [[nodiscard]] int selectedIndex() const;

    /// The selected package, or a default-constructed StarPackage (0 stars, 0
    /// price, empty currency) when nothing is selected.
    [[nodiscard]] StarPackage selectedPackage() const;

    /// The selected package's price as "1.99 USD", or an empty string when
    /// nothing is selected. The decimals are always two, so a whole price reads
    /// "2.00 USD" and 5 minor units reads "0.05 USD".
    [[nodiscard]] QString priceText() const;

    /// Whether a purchase can be started, i.e. whether selectedIndex() names a
    /// stored package.
    [[nodiscard]] bool canPurchase() const;

    /// Forgets the packages and the selection, e.g. when the Stars screen
    /// closes. A no-op, with no signal, when the model is already empty.
    void reset();

signals:
    /// Emitted whenever the stored packages or the selection actually changed.
    void changed();

private:
    QVector<StarPackage> m_packages;
    int m_index = -1;
};

} // namespace beygram::telegram
