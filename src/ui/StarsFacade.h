#pragma once

#include "telegram/StarBalanceModel.h"
#include "telegram/StarPurchaseModel.h"
#include "telegram/StarTransactionModel.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace beygram::ui {

/// The Stars screen as one QML-facing object.
///
/// This is a thin presentation facade, not a second source of truth: the
/// balance, the packages and the selection, and the transaction history all
/// live in the telegram models this class owns, and every property is read
/// straight from them. The facade only does what QML cannot: it accepts the
/// string lists TDLib-shaped payloads arrive as, turns them into the models'
/// value types, and formats the rows the panel renders.
///
/// The three models' changed() signals are forwarded as this object's
/// changed(), so a QML binding on any property repaints on a real update and
/// stays quiet on a redundant one (each model already suppresses no-op
/// changes).
///
/// Formatting follows the models so the screen has one voice:
///   - balanceText() is StarBalanceModel::displayText(), e.g. "1 234 ⭐".
///   - packageLabels() is one row per package, "<stars> · <price>", e.g.
///     "1 000 ⭐ · 19.99 USD". The price is built from the integer minor units
///     with exactly two decimals, like StarPurchaseModel::priceText().
///   - priceText() is StarPurchaseModel::priceText() for the selected row and
///     is empty when nothing is selected.
///   - transactions() is one row per stored transaction, newest first, as
///     "<sign><amount> ⭐ · <title> · <date>", e.g.
///     "+100 ⭐ · Stars purchase · 2024-05-01". The date is UTC and fixed
///     "yyyy-MM-dd", so the text does not depend on the system locale or time
///     zone. Empty title or date segments are omitted rather than left as a
///     dangling separator.
///
/// The facade never touches TDLib, the network or storage, and never decides
/// what a star is worth. buy() only reports the selected package; starting the
/// actual payment is the caller's job.
class StarsFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString balanceText READ balanceText NOTIFY changed)
    Q_PROPERTY(QStringList packageLabels READ packageLabels NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY changed)
    Q_PROPERTY(QString priceText READ priceText NOTIFY changed)
    Q_PROPERTY(bool canBuy READ canBuy NOTIFY changed)
    Q_PROPERTY(QStringList transactions READ transactions NOTIFY changed)

public:
    explicit StarsFacade(QObject* parent = nullptr);

    /// The balance as "1 234 ⭐"; "0 ⭐" before any update.
    [[nodiscard]] QString balanceText() const;

    /// Replaces the balance. A negative value is stored as 0 by the model.
    Q_INVOKABLE void setBalance(qint64 stars);

    /// Replaces the offered packages from the string lists TDLib payloads
    /// arrive as: stars and priceMinor are decimal integers, currency is an
    /// ISO-style code. The three lists are read positionally and may be
    /// shorter than stars; a missing price reads as 0 and a missing currency
    /// as empty. A value that is not a number reads as 0, so a malformed star
    /// amount is dropped by the model instead of being guessed at.
    ///
    /// The model's normalization applies: non-positive star amounts are
    /// dropped, only the first package of each amount is kept, negative prices
    /// clamp to 0 and currency is upper-cased.
    Q_INVOKABLE void setPackages(const QStringList& stars, const QStringList& priceMinor,
                                 const QStringList& currency);

    /// One "<stars> · <price>" row per package, in display order.
    [[nodiscard]] QStringList packageLabels() const;

    /// The selected package index, or -1 when nothing is selected.
    [[nodiscard]] int selectedIndex() const;

    /// Selects a package; -1 clears the selection and an out-of-range index is
    /// ignored, as StarPurchaseModel::select() specifies.
    Q_INVOKABLE void select(int index);

    /// The selected package's price as "1.99 USD", or empty when nothing is
    /// selected.
    [[nodiscard]] QString priceText() const;

    /// Whether a package is selected, i.e. whether buy() would do anything.
    [[nodiscard]] bool canBuy() const;

    /// Emits purchaseRequested() with the selected package when canBuy() is
    /// true; otherwise does nothing. It does not clear the selection, because
    /// a failed payment should leave the buyer's choice in place.
    Q_INVOKABLE void buy();

    /// The formatted transaction rows, newest first.
    [[nodiscard]] QStringList transactions() const;

    /// Replaces the history from parallel string lists: id and date are decimal
    /// integers (date is seconds since the Unix epoch), amount is a signed
    /// decimal integer whose sign is taken from incoming, title is free text
    /// and incoming is "true"/"1"/"yes" for a credit. Lists are read
    /// positionally and may be shorter than ids; a missing field reads as its
    /// zero value. The model drops non-positive ids, de-duplicates and sorts
    /// newest first.
    Q_INVOKABLE void setTransactions(const QStringList& ids, const QStringList& dates,
                                     const QStringList& amounts, const QStringList& titles,
                                     const QStringList& incoming);

signals:
    /// Emitted whenever any bound model actually changed.
    void changed();

    /// Emitted by buy() with the selected package's star amount, currency and
    /// price in minor units, for the caller to start a payment.
    void purchaseRequested(qint64 stars, const QString& currency, qint64 priceMinor);

private:
    telegram::StarBalanceModel m_balance;
    telegram::StarPurchaseModel m_purchases;
    telegram::StarTransactionModel m_history;
};

} // namespace beygram::ui
