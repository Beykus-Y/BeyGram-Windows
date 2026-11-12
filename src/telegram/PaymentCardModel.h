#pragma once

#include <QObject>
#include <QString>

namespace beygram::telegram {

/// The card a buyer types into a checkout form: the number, the expiry printed
/// on it, the security code and the cardholder name.
///
/// The model is display and input state only. It never talks to TDLib, never
/// asks a payment provider anything, never reads the clock and performs no I/O,
/// so a view binds its fields to the getters, calls the setters from its
/// controls and repaints on changed(). Logging and persistence are the caller's
/// job: the number and the security code are held in plain members and are
/// never printed by this class.
///
/// Every setter stores a canonical form, so the model never holds text a card
/// form could not send:
///
///   - setNumber() keeps digits only, capped at nineteen, so spaces, dashes
///     and a pasted "4242-4242-..." all store "4242424242...".
///   - setExpiry() stores the month as two digits in 1..12 ("1", "01" and
///     " 1 " all become "01") and the year as exactly two or four digits
///     ("26" or "2026"). A month outside 1..12 or a year of any other length
///     clears that field, because a partial date is not a date and valid()
///     could never accept it. The two fields are otherwise independent: a bad
///     month does not disturb a good year.
///   - setSecurityCode() keeps digits only, capped at four, so a view can grow
///     a three-digit code into a four-digit one; valid() is what insists on
///     three or four.
///   - setCardholderName() trims surrounding whitespace, so a name of only
///     spaces counts as empty and never satisfies valid().
///
/// maskedNumber() is the form safe to show: the last four digits behind four
/// bullets, "•••• 1234", and empty while fewer than four digits are stored.
/// brand() is derived from the leading digits of the number, never from the
/// stored length:
///
///   - "4"            -> "Visa"
///   - 51..55, 2221..2720 -> "Mastercard"
///   - 34, 37         -> "Amex"
///   - anything else, including a prefix too short to decide, -> "Unknown"
///
/// valid() requires 13..19 digits, a month and year in the forms above, a
/// three- or four-digit security code and a non-empty name. It is a format
/// check, not a liveness check: no Luhn digit is computed and no expiry is
/// compared against today.
///
/// changed() is emitted only when a stored value really differs, so a field
/// that reformats its own text and reports the same digits back does not
/// repaint or restart a binding loop.
class PaymentCardModel final : public QObject {
    Q_OBJECT

public:
    explicit PaymentCardModel(QObject* parent = nullptr);

    /// Records the card number: every non-digit is dropped and the result is
    /// capped at nineteen digits, so "4242 4242-4242 4242" stores
    /// "4242424242424242".
    void setNumber(const QString& number);

    /// The stored number digits, unformatted. Empty when none was typed.
    [[nodiscard]] QString number() const;

    /// The last four digits behind four bullets, "•••• 1234". Empty while fewer
    /// than four digits are stored, because a shorter tail would be most of the
    /// number rather than a hint of it.
    [[nodiscard]] QString maskedNumber() const;

    /// Records the expiry as the two pieces a form has: month and year. The
    /// month becomes two digits in 1..12; the year must have two or four
    /// digits. A value that does not fit clears that field (see the class
    /// comment), and the other field keeps whatever it already held.
    void setExpiry(const QString& month, const QString& year);

    /// The stored month as two digits, e.g. "01" or "12". Empty when the last
    /// setExpiry() month was missing or out of range.
    [[nodiscard]] QString expiryMonth() const;

    /// The stored year as two or four digits, e.g. "26" or "2026". Empty when
    /// the last setExpiry() year was missing or of another length.
    [[nodiscard]] QString expiryYear() const;

    /// Records the security code: digits only, capped at four.
    void setSecurityCode(const QString& code);

    /// The stored security-code digits, e.g. "123". May be shorter than three
    /// while the user is still typing; valid() is the gate.
    [[nodiscard]] QString securityCode() const;

    /// Records the name printed on the card, trimmed. Empty when only
    /// whitespace was passed.
    void setCardholderName(const QString& name);

    /// The stored cardholder name, trimmed. Empty when none was typed.
    [[nodiscard]] QString cardholderName() const;

    /// True when the number has 13 to 19 digits, the expiry holds a valid
    /// month and a two- or four-digit year, the security code has three or four
    /// digits and the cardholder name is not empty.
    [[nodiscard]] bool valid() const;

    /// The brand implied by the number's leading digits: "Visa",
    /// "Mastercard", "Amex", or "Unknown" when the prefix is not one of those
    /// or is still too short to tell. Computed on every call from the stored
    /// digits, so it follows setNumber() immediately.
    [[nodiscard]] QString brand() const;

    /// Forgets the number, the expiry, the security code and the name. Silent
    /// while the model already is empty, so closing an untouched form does not
    /// notify the view.
    void reset();

signals:
    /// Emitted once per real change to any of the stored fields, and once for a
    /// reset() that actually cleared something.
    void changed();

private:
    QString m_number;
    QString m_month;
    QString m_year;
    QString m_cvc;
    QString m_name;
};

} // namespace beygram::telegram
