#include "PhoneNumberModel.h"

namespace beygram::telegram {
namespace {

/// No dial code is longer than four digits, and fifteen is the longest national
/// part Telegram accepts.
constexpr int kMaxDialDigits = 4;
constexpr int kMaxNationalDigits = 15;
constexpr int kMinNationalDigits = 4;

/// The block sizes displayText() prints, in order: 3-3-2-2.
constexpr int kBlocks[] = {3, 3, 2, 2};
constexpr int kBlockCount = 4;

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

/// Splits digits into the 3-3-2-2 blocks a phone number is read in. Anything
/// past the fourth block stays together as one last block, so the whole number
/// is always shown. Blocks are joined by a space after the first one and by a
/// hyphen after the rest: "9991234567" becomes "999 123-45-67".
QString grouped(const QString& digits)
{
    QString text;
    int position = 0;
    int block = 0;
    while (position < digits.size()) {
        if (!text.isEmpty())
            text += block == 1 ? QChar(u' ') : QChar(u'-');

        const int left = digits.size() - position;
        const int take = block < kBlockCount ? qMin(kBlocks[block], left) : left;
        text += digits.mid(position, take);
        position += take;
        ++block;
    }
    return text;
}

} // namespace

PhoneNumberModel::PhoneNumberModel(QObject* parent) : QObject(parent)
{
}

void PhoneNumberModel::setCountryCode(const QString& iso, const QString& dialCode)
{
    const QString dial = digitsOnly(dialCode).left(kMaxDialDigits);

    // The same country reported again (a picker that re-emits the current row,
    // for instance) is not an edit, so the number is left alone.
    if (iso == m_iso && dial == m_dial)
        return;

    m_iso = iso;
    m_dial = dial;
    emit changed();
}

QString PhoneNumberModel::countryIso() const
{
    return m_iso;
}

QString PhoneNumberModel::dialCode() const
{
    return m_dial;
}

void PhoneNumberModel::setNationalNumber(const QString& number)
{
    const QString national = digitsOnly(number).left(kMaxNationalDigits);

    // A field that reformats its own text reports the same digits back; that is
    // not an edit and must not repaint or restart anything.
    if (national == m_national)
        return;

    m_national = national;
    emit changed();
}

QString PhoneNumberModel::nationalNumber() const
{
    return m_national;
}

QString PhoneNumberModel::fullNumber() const
{
    // Half a number is not a number: a dial code alone or a national part alone
    // would be sent as somebody else's number.
    if (m_dial.isEmpty() || m_national.isEmpty())
        return QString();

    return QStringLiteral("+") + m_dial + m_national;
}

bool PhoneNumberModel::valid() const
{
    if (m_dial.isEmpty() || m_dial.size() > kMaxDialDigits)
        return false;

    return m_national.size() >= kMinNationalDigits && m_national.size() <= kMaxNationalDigits;
}

QString PhoneNumberModel::displayText() const
{
    if (m_dial.isEmpty())
        return grouped(m_national);

    const QString prefix = QStringLiteral("+") + m_dial;
    if (m_national.isEmpty())
        return prefix;

    return prefix + QChar(u' ') + grouped(m_national);
}

void PhoneNumberModel::clear()
{
    if (m_iso.isEmpty() && m_dial.isEmpty() && m_national.isEmpty())
        return;

    m_iso.clear();
    m_dial.clear();
    m_national.clear();
    emit changed();
}

} // namespace beygram::telegram
