#pragma once

#include "telegram/PaymentCheckoutModel.h"
#include "telegram/PaymentMethodModel.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::ui {

/// The state a checkout sheet binds to: one invoice, the buyer's order fields
/// and the payment methods on offer, behind one QML-facing object.
///
/// The facade is a thin adapter, not a second model. It owns a
/// telegram::PaymentCheckoutModel (invoice + order form) and a
/// telegram::PaymentMethodModel (method list + selection) and adds only what a
/// view needs and neither model has:
///
///   - the ids arrive from TDLib as int53 but QML carries strings, so
///     setInvoice() takes strings and parses them; a value that is not an
///     integer is treated as 0, which the checkout model rejects by clearing
///     itself;
///   - the method list is given as two parallel lists (ids, titles) and exposed
///     back as titles plus a selected index, which is what a repeater and a
///     radio row can use directly;
///   - canPay() combines both models, and pay() turns the whole state into one
///     payRequested() signal.
///
/// No transport, no provider SDK, no persistence and no logging: the facade
/// never sends anything. A caller connects payRequested() to the payment
/// requests in telegram/PaymentRequests.h.
///
/// changed() is forwarded from the two models, so it fires only on a real
/// change and a view can bind to it without echoing its own edits. reset()
/// touches both models but emits changed() at most once.
class PaymentsFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString totalText READ totalText NOTIFY changed)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY changed)
    Q_PROPERTY(QString email READ email WRITE setEmail NOTIFY changed)
    Q_PROPERTY(QString phone READ phone WRITE setPhone NOTIFY changed)
    Q_PROPERTY(QStringList methodTitles READ methodTitles NOTIFY changed)
    Q_PROPERTY(int methodIndex READ methodIndex NOTIFY changed)
    Q_PROPERTY(bool canPay READ canPay NOTIFY changed)

public:
    explicit PaymentsFacade(QObject* parent = nullptr);

    /// Loads the invoice message (chatId, messageId) with its display title,
    /// total in minor units and ISO 4217 currency.
    ///
    /// The ids are decimal int53 strings. A value that does not parse is passed
    /// on as 0, so the checkout model clears the whole checkout rather than
    /// keeping a half-valid invoice. The buyer's name, email and phone are kept,
    /// matching the model: re-opening the sheet for a refresh, or paying another
    /// invoice, does not wipe what was already typed.
    Q_INVOKABLE void setInvoice(const QString& chatId, const QString& messageId,
                                const QString& title, qint64 totalAmount,
                                const QString& currency);

    /// True while an invoice is loaded, i.e. the checkout has positive ids and a
    /// non-negative amount. Not the same as canPay().
    [[nodiscard]] bool active() const;

    /// The invoice title, empty when the invoice had none.
    [[nodiscard]] QString title() const;

    /// The total as "1234.56 USD", formatted by the checkout model from minor
    /// units. Display only: the amount to charge is the invoice's.
    [[nodiscard]] QString totalText() const;

    [[nodiscard]] QString name() const;
    void setName(const QString& name);

    [[nodiscard]] QString email() const;
    void setEmail(const QString& email);

    [[nodiscard]] QString phone() const;
    void setPhone(const QString& phone);

    /// Replaces the offered methods. ids and titles are parallel lists; the
    /// shorter one ends the pairing, so a missing title becomes an empty one.
    /// The method model drops empty ids, keeps the first row of a duplicate id
    /// and preserves the given order, so the caller's order is the screen's
    /// order. A selection whose method disappears is cleared.
    Q_INVOKABLE void setMethods(const QStringList& ids, const QStringList& titles);

    /// The display text of each offered method, in methodIndex() order. A row
    /// whose title is empty falls back to its id, because a radio row with no
    /// text could not be tapped knowingly.
    [[nodiscard]] QStringList methodTitles() const;

    /// Index of the selected method in methodTitles(), or -1 when none is
    /// selected. The index is the row's position, not a stable key: it shifts
    /// when setMethods() replaces the list.
    [[nodiscard]] int methodIndex() const;

    /// Selects the method at index, matching the sibling facades' rule: -1
    /// clears the selection and any other out-of-range index is ignored, so a
    /// stale tap cannot select an invisible row.
    Q_INVOKABLE void selectMethod(int index);

    /// True when pay() may run: the order form is valid (invoice loaded, name
    /// and a usable contact filled in) and, when methods are offered, one of
    /// them is selected. A checkout with no method list can be paid with an
    /// empty methodId, which the caller may read as "the invoice's own method".
    [[nodiscard]] bool canPay() const;

    /// Emits payRequested() with the invoice ids, the three order fields and the
    /// selected method id, and returns true. Returns false, emitting nothing,
    /// when canPay() is false. The facade is not reset: the caller decides when
    /// the checkout is over and calls reset().
    Q_INVOKABLE bool pay();

    /// Clears the invoice, the order fields, the methods and the selection, e.g.
    /// when the sheet closes or the payment finished. Emits changed() once if
    /// anything was cleared, and nothing at all when the facade was empty.
    Q_INVOKABLE void reset();

signals:
    /// Emitted whenever any exposed value actually changed.
    void changed();

    /// A payable checkout: the ids are decimal int53 strings, the order fields
    /// are the trimmed values the buyer typed and methodId is the selected
    /// method's opaque id (empty when no methods were offered).
    void payRequested(const QString& chatId, const QString& messageId,
                      const QString& name, const QString& email,
                      const QString& phone, const QString& methodId);

private:
    void handleModelChanged();

    telegram::PaymentCheckoutModel m_checkout;
    telegram::PaymentMethodModel m_methods;
    // Coalesces the two model signals of reset() into one changed().
    bool m_silent = false;
    bool m_pending = false;
};

} // namespace beygram::ui
