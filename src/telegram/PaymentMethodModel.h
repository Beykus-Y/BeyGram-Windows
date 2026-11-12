#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One row of the payment-method list: the opaque id the caller selects by, the
/// two display lines, the kind label that groups the row (card, Stars, a saved
/// provider, ...), and saved, which marks a method the account may reuse.
///
/// The type is a view value, not a wire object. PaymentRequests.h only builds the
/// getPaymentForm/validateOrderInfo/sendPaymentForm payloads, and the pinned
/// schema names credentials rather than a method list, so it carries no
/// payment-method concept to reuse; PaymentCheckoutModel holds the buyer's order
/// fields and has no method either. PaymentMethodRow is therefore the model's
/// own type, and nothing here is sent to TDLib: a caller that pays maps the
/// selected row onto the credentials of sendPaymentFormRequest itself.
///
/// id is opaque and compared exactly (case-sensitive, untrimmed): it is the
/// caller's key, so " " is a real id while "" is not.
struct PaymentMethodRow {
    QString id;
    QString title;
    QString subtitle;
    QString kind;
    bool saved = false;
};

/// The payment methods offered at checkout, plus which one the buyer picked.
///
/// The model is display state only: no transport, no provider SDK and no
/// logging. Its rules:
///
///   - setMethods drops rows with an empty id (nothing could select them) and
///     keeps only the first row of each id, because the caller's order is the
///     screen's order and a later duplicate would make the visible row depend on
///     merge order. Surviving rows keep their arrival order; nothing is sorted.
///   - setMethods keeps the current selection while that id is still offered and
///     clears it when the method disappears, so selectedId() never names a row
///     the list no longer has.
///   - select ignores an id the list does not contain, so a stale tap cannot
///     select an invisible row; an empty id clears the selection.
///   - selectedMethod returns a default-constructed row when nothing is
///     selected, so a caller can read title/subtitle without a has-selection
///     branch.
///   - clear() forgets the methods and the selection.
///
/// changed() is emitted only for a real change: a reload that brings the same
/// rows, a tap on the already-selected row or a repeated clear stays silent, so a
/// view can bind to it without echoing its own edits.
class PaymentMethodModel final : public QObject {
    Q_OBJECT

public:
    explicit PaymentMethodModel(QObject* parent = nullptr);

    /// Replaces the offered methods with methods after normalization (empty ids
    /// dropped, first row of each id kept, order preserved).
    ///
    /// A selection whose method is not in the new list is cleared. The call is a
    /// no-op, with no signal, when the normalized list equals the stored one and
    /// the selection survives; a dropped selection alone is a real change.
    void setMethods(QVector<PaymentMethodRow> methods);

    /// The stored rows, in display order, as a copy.
    [[nodiscard]] QVector<PaymentMethodRow> methods() const;

    /// Number of stored rows, i.e. methods().size().
    [[nodiscard]] int count() const;

    /// The rows with saved set, in the same order as methods().
    [[nodiscard]] QVector<PaymentMethodRow> savedMethods() const;

    /// Selects the method with id. An id the list does not contain is ignored
    /// (the current selection stays); an empty id clears the selection. Both are
    /// silent when the selection would not change.
    void select(const QString& id);

    /// The selected id, or an empty string when nothing is selected.
    [[nodiscard]] QString selectedId() const;

    /// The selected row, or a default-constructed PaymentMethodRow (empty id and
    /// text, saved false) when nothing is selected.
    [[nodiscard]] PaymentMethodRow selectedMethod() const;

    /// Forgets the methods and the selection, e.g. when the checkout closes. A
    /// no-op, with no signal, when the model is already empty.
    void clear();

signals:
    /// Emitted whenever the stored rows or the selection actually changed.
    void changed();

private:
    QVector<PaymentMethodRow> m_methods;
    QString m_selected;
};

} // namespace beygram::telegram
