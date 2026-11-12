#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Editable state of one payment checkout: which invoice is being paid, what
/// the buyer has typed into the order form, and whether the form is usable.
///
/// The model is the input side of the requests in PaymentRequests.h and holds
/// no transport, no provider SDK and no logging. Mapping:
///
///   - chatId/messageId identify the invoice message. They are exactly the pair
///     getPaymentFormRequest, validateOrderInfoRequest and sendPaymentFormRequest
///     wrap in inputInvoiceMessage, so a caller can hand them straight to those
///     builders.
///   - name, phone and email are the three values orderInfo carries (name,
///     phone_number, email_address); validateOrderInfoRequest normalizes them
///     into the schema's shape.
///   - saveCredentials is the allow_save consent flag of
///     validateOrderInfoRequest: whether the account may keep the order
///     information for reuse.
///   - title, totalAmount and currency are display data taken from
///     messageInvoice (title, total_amount, currency). No request carries them:
///     the pinned sendPaymentForm has no amount or currency field, and TDLib
///     itself reads them from the message. totalAmount is in minor units, the
///     unit TDLib reports total_amount in.
///
/// All text setters trim their input, because the values are typed by the user
/// and are shown back to them. Only currency is also uppercased: it is an
/// ISO 4217 code, not free text.
///
/// Every setter is a no-op when the stored value would not change, and
/// changed() is emitted only for a real change, so a view can bind to it
/// without echoing its own edits.
class PaymentCheckoutModel final : public QObject {
    Q_OBJECT

public:
    explicit PaymentCheckoutModel(QObject* parent = nullptr);

    /// Starts (or replaces) the checkout for the invoice message
    /// (chatId, messageId), priced totalAmount in minor units and labelled
    /// currency.
    ///
    /// The invoice fields are replaced and the buyer's name, email, phone and
    /// saveCredentials are kept: the buyer is the same person, so re-opening the
    /// same invoice (a refresh) or paying another one must not wipe what they
    /// already typed. reset() clears them.
    ///
    /// A non-positive chatId or messageId, or a negative totalAmount, is not an
    /// invoice this model can pay, so the call clears the whole checkout
    /// (equivalent to reset()) instead of leaving a half-valid state. title is
    /// trimmed and currency is trimmed and uppercased.
    void setInvoice(qint64 chatId, qint64 messageId, const QString& title,
                    qint64 totalAmount, const QString& currency);

    /// True while an invoice is loaded, i.e. both ids are positive and the
    /// amount is not negative. Nothing else is required: an active checkout is
    /// not necessarily a valid() one.
    [[nodiscard]] bool active() const;

    [[nodiscard]] qint64 chatId() const;
    [[nodiscard]] qint64 messageId() const;

    /// The invoice title as shown in the checkout header; empty when the
    /// invoice had none.
    [[nodiscard]] QString title() const;

    /// The invoice total in minor units (TDLib's total_amount), so 123456 is
    /// 1234.56 of currency.
    [[nodiscard]] qint64 totalAmount() const;

    /// The ISO 4217 code, uppercased, or empty when the invoice carried none.
    [[nodiscard]] QString currency() const;

    void setName(const QString& name);
    [[nodiscard]] QString name() const;

    void setEmail(const QString& email);
    [[nodiscard]] QString email() const;

    void setPhone(const QString& phone);
    [[nodiscard]] QString phone() const;

    /// Whether the account may keep the order information (allow_save).
    void setSaveCredentials(bool save);
    [[nodiscard]] bool saveCredentials() const;

    /// True when the form can be submitted: an invoice is active(), the name is
    /// non-empty, at least one contact is usable, and the amount is not
    /// negative.
    ///
    /// The contact rules are deliberately loose, matching what TDLib accepts:
    /// an email needs an '@' with a '.' somewhere after it, and a phone number
    /// only needs five digits, so a number typed with spaces, dashes,
    /// parentheses or a leading '+' still counts. Neither check can prove the
    /// address or number exists; validateOrderInfo's answer is what does.
    [[nodiscard]] bool valid() const;

    /// The total as "1234.56" plus " USD" when a currency is set, always two
    /// decimals and always a '.' decimal separator (QLocale::c()). Minor units
    /// are divided by 100, so the text is a display value only: the amount to
    /// charge is the invoice's, not this string.
    [[nodiscard]] QString totalText() const;

    /// Clears the invoice and the buyer's details, e.g. when the checkout is
    /// closed or the payment finished. A no-op, with no signal, when the model
    /// is already empty.
    void reset();

signals:
    /// Emitted whenever a stored value actually changed.
    void changed();

private:
    qint64 m_chatId = 0;
    qint64 m_messageId = 0;
    qint64 m_amount = 0;
    QString m_title;
    QString m_currency;
    QString m_name;
    QString m_email;
    QString m_phone;
    bool m_save = false;
};

} // namespace beygram::telegram
