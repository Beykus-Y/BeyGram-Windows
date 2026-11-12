#include "StarBalanceModel.h"

#include <limits>

namespace beygram::telegram {

namespace {

/// The Telegram star glyph (U+2B50). Written as a code point instead of a
/// literal so the source file stays ASCII and the glyph does not depend on the
/// compiler's execution charset.
constexpr char16_t kStarGlyph = 0x2B50;

constexpr int kGroupSize = 3;

} // namespace

StarBalanceModel::StarBalanceModel(QObject* parent) : QObject(parent)
{
}

void StarBalanceModel::setBalance(qint64 stars)
{
    const qint64 clamped = stars < 0 ? 0 : stars;
    if (clamped == m_stars)
        return;

    m_stars = clamped;
    emit changed();
}

qint64 StarBalanceModel::balance() const
{
    return m_stars;
}

bool StarBalanceModel::canAfford(qint64 amount) const
{
    return amount >= 0 && amount <= m_stars;
}

void StarBalanceModel::add(qint64 amount)
{
    if (amount == 0)
        return;

    // Saturating arithmetic: a wrapped balance would show as a negative number
    // (credit overflow) or as a huge one (debit underflow), so both ends clamp
    // to the nearest representable balance instead.
    qint64 next = m_stars;
    if (amount > 0) {
        const qint64 room = std::numeric_limits<qint64>::max() - m_stars;
        next = amount > room ? std::numeric_limits<qint64>::max() : m_stars + amount;
    } else {
        // amount < 0. Comparing against -m_stars avoids negating amount, which
        // is undefined for qint64's minimum. m_stars >= 0, so -m_stars is safe.
        next = amount < -m_stars ? 0 : m_stars + amount;
    }

    if (next == m_stars)
        return;

    m_stars = next;
    emit changed();
}

QString StarBalanceModel::displayText() const
{
    // The balance is never negative, so the decimal form has no sign to skip.
    const QString digits = QString::number(m_stars);

    QString grouped;
    grouped.reserve(digits.size() + digits.size() / kGroupSize + 2);

    for (qsizetype i = 0; i < digits.size(); ++i) {
        // A space goes before every group of three counted from the right, so
        // the first group is the only short one: "1 234", "12 345", "123 456".
        if (i > 0 && (digits.size() - i) % kGroupSize == 0)
            grouped += QLatin1Char(' ');
        grouped += digits.at(i);
    }

    return grouped + QLatin1Char(' ') + QChar(kStarGlyph);
}

void StarBalanceModel::clear()
{
    if (m_stars == 0)
        return;

    m_stars = 0;
    emit changed();
}

} // namespace beygram::telegram
