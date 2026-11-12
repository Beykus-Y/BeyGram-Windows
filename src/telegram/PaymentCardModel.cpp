#include "PaymentCardModel.h"

namespace beygram::telegram {

namespace {

/// Nineteen is the longest card number in use, so anything past it is a paste
/// accident rather than another digit of the same card.
constexpr int kMaxNumberDigits = 19;

/// Thirteen is the shortest card number the model accepts; shorter strings are
/// an account prefix, not a card.
constexpr int kMinValidNumberDigits = 13;

/// A security code is three digits on most cards and four on Amex. The setter
/// keeps up to four so a view can grow "123" into "1234", and valid() is what
/// insists on the full length.
constexpr int kMinSecurityCodeDigits = 3;
constexpr int kMaxSecurityCodeDigits = 4;

/// Four bullets plus the last four digits is the shortest mask that still
/// hides the number.
constexpr int kMaskBullets = 4;
constexpr int kMaskedSuffixDigits = 4;

constexpr int kMinMonth = 1;
constexpr int kMaxMonth = 12;

constexpr int kTwoDigitYear = 2;
constexpr int kFourDigitYear = 4;

/// Keeps the digits of text and drops everything else.
QString digitsOnly(const QString& text)
{
    QString digits;
    digits.reserve(text.size());
    for (const QChar character : text) {
        if (character.isDigit())
            digits += character;
    }
    return digits;
}

/// The four bullets maskedNumber() puts in front of the last four digits. Built
/// from the code point so the source stays plain ASCII.
QString maskPrefix()
{
    return QString(kMaskBullets, QChar(0x2022));
}

} // namespace

PaymentCardModel::PaymentCardModel(QObject* parent) : QObject(parent)
{
}

void PaymentCardModel::setNumber(const QString& number)
{
    const QString digits = digitsOnly(number).left(kMaxNumberDigits);

    // A field that regroups its own text reports the same digits back; that is
    // not an edit and must not repaint or restart anything.
    if (digits == m_number)
        return;

    m_number = digits;
    emit changed();
}

QString PaymentCardModel::number() const
{
    return m_number;
}

QString PaymentCardModel::maskedNumber() const
{
    // Half a number is not worth masking: with fewer than four digits the tail
    // would be the whole number, so the model shows nothing instead.
    if (m_number.size() < kMaskedSuffixDigits)
        return QString();

    return maskPrefix() + QLatin1Char(' ') + m_number.right(kMaskedSuffixDigits);
}

void PaymentCardModel::setExpiry(const QString& month, const QString& year)
{
    // The month is read as a number and printed back with a leading zero, so
    // "1", "01" and " 1 " all store the same two-digit value. Anything outside
    // 1..12 clears the month rather than keeping a date no card could have.
    const int monthValue = digitsOnly(month).left(2).toInt();
    const QString normalizedMonth = (monthValue >= kMinMonth && monthValue <= kMaxMonth)
        ? QStringLiteral("%1").arg(monthValue, 2, 10, QLatin1Char('0'))
        : QString();

    // The year is kept verbatim once it is two or four digits long. Other
    // lengths (a single digit, or three) are a half-typed year, and valid()
    // would reject them, so they are not stored at all.
    const QString yearDigits = digitsOnly(year).left(kFourDigitYear);
    const bool yearLengthOk =
        yearDigits.size() == kTwoDigitYear || yearDigits.size() == kFourDigitYear;
    const QString normalizedYear = yearLengthOk ? yearDigits : QString();

    if (normalizedMonth == m_month && normalizedYear == m_year)
        return;

    m_month = normalizedMonth;
    m_year = normalizedYear;
    emit changed();
}

QString PaymentCardModel::expiryMonth() const
{
    return m_month;
}

QString PaymentCardModel::expiryYear() const
{
    return m_year;
}

void PaymentCardModel::setSecurityCode(const QString& code)
{
    // Capped at four rather than rejected, so the field can grow from a
    // three-digit code to an Amex one without the model refusing the
    // intermediate value.
    const QString digits = digitsOnly(code).left(kMaxSecurityCodeDigits);
    if (digits == m_cvc)
        return;

    m_cvc = digits;
    emit changed();
}

QString PaymentCardModel::securityCode() const
{
    return m_cvc;
}

void PaymentCardModel::setCardholderName(const QString& name)
{
    // Stored trimmed, so a name of only spaces is the same as no name and
    // valid() does not have to trim again.
    const QString trimmed = name.trimmed();
    if (trimmed == m_name)
        return;

    m_name = trimmed;
    emit changed();
}

QString PaymentCardModel::cardholderName() const
{
    return m_name;
}

bool PaymentCardModel::valid() const
{
    if (m_number.size() < kMinValidNumberDigits || m_number.size() > kMaxNumberDigits)
        return false;

    // The setter already normalizes both halves, but the check is repeated here
    // so the answer does not depend on the setter having been used: a stored
    // "00" or a five-digit year is never a valid expiry.
    const int month = m_month.toInt();
    if (month < kMinMonth || month > kMaxMonth)
        return false;

    if (m_year.size() != kTwoDigitYear && m_year.size() != kFourDigitYear)
        return false;

    if (m_cvc.size() < kMinSecurityCodeDigits || m_cvc.size() > kMaxSecurityCodeDigits)
        return false;

    return !m_name.isEmpty();
}

QString PaymentCardModel::brand() const
{
    // Visa is a single leading 4, so it is decided before any two-digit range
    // and a one-digit number can already report it.
    if (m_number.startsWith(QLatin1Char('4')))
        return QStringLiteral("Visa");

    if (m_number.size() >= 2) {
        const int two = m_number.left(2).toInt();
        if (two >= 51 && two <= 55)
            return QStringLiteral("Mastercard");
    }

    // The 2-series Mastercard range needs four digits, so a shorter prefix of
    // it stays Unknown instead of guessing.
    if (m_number.size() >= 4) {
        const int four = m_number.left(4).toInt();
        if (four >= 2221 && four <= 2720)
            return QStringLiteral("Mastercard");
    }

    if (m_number.size() >= 2) {
        const QString two = m_number.left(2);
        if (two == QLatin1String("34") || two == QLatin1String("37"))
            return QStringLiteral("Amex");
    }

    return QStringLiteral("Unknown");
}

void PaymentCardModel::reset()
{
    // Nothing stored means nothing to forget: reset() then stays silent so
    // closing an untouched form does not notify the view.
    if (m_number.isEmpty() && m_month.isEmpty() && m_year.isEmpty() && m_cvc.isEmpty()
        && m_name.isEmpty())
        return;

    m_number.clear();
    m_month.clear();
    m_year.clear();
    m_cvc.clear();
    m_name.clear();
    emit changed();
}

} // namespace beygram::telegram
