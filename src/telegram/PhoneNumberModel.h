#pragma once

#include <QObject>
#include <QString>

namespace beygram::telegram {

/// Holds the three pieces of a phone number a login screen edits: the country
/// the user picked, that country's dial code and the national part they typed.
///
/// The setters are the only entry points and each one stores a canonical form,
/// so the model never holds text a request could not carry:
///
///   - setCountryCode() keeps the ISO code as given and reduces the dial code to
///     its digits without the leading '+' ("+7", " 7 " and "+7 " all become
///     "7"), capped at four digits because no dial code is longer.
///   - setNationalNumber() keeps digits only, capped at fifteen, so spaces,
///     dashes, brackets and a pasted "+7 999 ..." prefix cannot leak into it.
///
/// fullNumber() is what a request sends and is empty until both a dial code and
/// a national number are present. displayText() is what the field shows, with
/// the national digits grouped 3-3-2-2 ("+7 999 123-45-67"). valid() applies
/// the bounds Telegram itself accepts: 1 to 4 dial digits and 4 to 15 national
/// digits.
///
/// changed() is emitted only when a stored value really changed, so a view
/// bound to it does not repaint while the user retypes the same digits.
class PhoneNumberModel final : public QObject {
    Q_OBJECT

public:
    explicit PhoneNumberModel(QObject* parent = nullptr);

    /// Records the picked country. The ISO code is stored verbatim; the dial
    /// code loses a leading '+' and every non-digit, and is capped at four
    /// digits. An empty dial code keeps the ISO code and clears the dial part.
    void setCountryCode(const QString& iso, const QString& dialCode);

    /// The ISO code exactly as it was last stored, e.g. "RU". Empty when no
    /// country was picked.
    [[nodiscard]] QString countryIso() const;

    /// The dial code's digits only, without '+', e.g. "7". Empty when none.
    [[nodiscard]] QString dialCode() const;

    /// Records the national part the user typed: every non-digit is dropped and
    /// the result is capped at fifteen digits, so "999 123-45-67" and
    /// "+79991234567" both store "9991234567".
    void setNationalNumber(const QString& number);

    /// The stored national digits, unformatted.
    [[nodiscard]] QString nationalNumber() const;

    /// The number to send, "+<dial><digits>", e.g. "+79991234567". Empty when
    /// either the dial code or the national number is missing, because neither
    /// half is a usable number on its own.
    [[nodiscard]] QString fullNumber() const;

    /// True when the dial code has 1 to 4 digits and the national number has 4
    /// to 15 digits.
    [[nodiscard]] bool valid() const;

    /// The number as shown to the user, with the national digits grouped
    /// 3-3-2-2: "+7 999 123-45-67". Only the groups that exist are printed, so
    /// a short number shows fewer blocks ("+7 999 12"). A dial code without a
    /// national number shows just "+7", and a national number without a country
    /// shows just the groups, with no leading '+'.
    [[nodiscard]] QString displayText() const;

    /// Drops the country and the number. Silent when everything is empty.
    void clear();

signals:
    /// Emitted whenever the country or the national number actually changed.
    void changed();

private:
    QString m_iso;
    QString m_dial;
    QString m_national;
};

} // namespace beygram::telegram
