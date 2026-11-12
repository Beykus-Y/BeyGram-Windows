#include "StarPurchaseModel.h"

#include <utility>

namespace beygram::telegram {
namespace {

// StarPackage carries no operator== (it is a plain view value), so the model
// compares the fields it holds. Every field is compared: a reload that only
// rewrites the price or the currency still changes what the row renders.
bool samePackage(const StarPackage& left, const StarPackage& right)
{
    return left.stars == right.stars
        && left.priceMinor == right.priceMinor
        && left.currency == right.currency;
}

bool samePackages(const QVector<StarPackage>& left, const QVector<StarPackage>& right)
{
    if (left.size() != right.size())
        return false;
    for (qsizetype i = 0; i < left.size(); ++i) {
        if (!samePackage(left.at(i), right.at(i)))
            return false;
    }
    return true;
}

// Whether an amount of stars is already offered. The list is one screen's
// packages, so a linear scan beats keeping a second index in sync.
bool hasStars(const QVector<StarPackage>& packages, qint64 stars)
{
    for (const StarPackage& package : packages) {
        if (package.stars == stars)
            return true;
    }
    return false;
}

} // namespace

StarPurchaseModel::StarPurchaseModel(QObject* parent)
    : QObject(parent)
{
}

void StarPurchaseModel::setPackages(QVector<StarPackage> packages)
{
    // Keep the first package of each star amount: packages arrive in the order
    // the caller merged its answers, and the first answer is the one already on
    // screen, so it wins over a later duplicate instead of making the visible
    // row depend on merge order. A package without stars could never be bought,
    // so it is dropped.
    QVector<StarPackage> unique;
    unique.reserve(packages.size());
    for (StarPackage& package : packages) {
        if (package.stars <= 0)
            continue;
        if (hasStars(unique, package.stars))
            continue;
        // A negative price is not a price: it would render as a refund. The
        // currency code is display data, so the server's casing is normalized.
        if (package.priceMinor < 0)
            package.priceMinor = 0;
        package.currency = package.currency.toUpper();
        unique.append(std::move(package));
    }

    // The selection is an index, so it survives only while the new list still
    // has that row: keeping it past the end would leave canPurchase() true for a
    // package the screen cannot show.
    const bool selectionDropped = m_index >= static_cast<int>(unique.size());

    // A reload that brings the same packages is not a change: repainting the
    // list for it would also restart every animation bound to changed().
    if (!selectionDropped && samePackages(unique, m_packages))
        return;

    m_packages = std::move(unique);
    if (selectionDropped)
        m_index = -1;
    emit changed();
}

QVector<StarPackage> StarPurchaseModel::packages() const
{
    return m_packages;
}

int StarPurchaseModel::count() const
{
    return static_cast<int>(m_packages.size());
}

void StarPurchaseModel::select(int index)
{
    // -1 is "no choice", not a package: it clears the selection so the caller
    // does not have to know whether one was set.
    if (index < 0) {
        if (m_index < 0)
            return;
        m_index = -1;
        emit changed();
        return;
    }

    // A tap on a row the last reload removed must not select an invisible
    // package; the current selection stays as it was.
    if (index >= count())
        return;

    if (m_index == index)
        return;

    m_index = index;
    emit changed();
}

int StarPurchaseModel::selectedIndex() const
{
    return m_index;
}

StarPackage StarPurchaseModel::selectedPackage() const
{
    if (m_index < 0 || m_index >= count())
        return {};
    return m_packages.at(m_index);
}

QString StarPurchaseModel::priceText() const
{
    if (m_index < 0 || m_index >= count())
        return {};

    const StarPackage& package = m_packages.at(m_index);

    // The price is built from the integer minor units instead of dividing by
    // 100.0, so the text cannot pick up a rounding error and does not depend on
    // the system locale. priceMinor is clamped at 0, so the remainder is never
    // negative and the whole part never needs a sign.
    QString text = QString::number(package.priceMinor / 100);
    text += QLatin1Char('.');
    text += QString::number(package.priceMinor % 100).rightJustified(2, QLatin1Char('0'));

    // An empty code means the server named no currency; the amount alone reads
    // better than a dangling space.
    if (!package.currency.isEmpty())
        text += QLatin1Char(' ') + package.currency;

    return text;
}

bool StarPurchaseModel::canPurchase() const
{
    return m_index >= 0 && m_index < count();
}

void StarPurchaseModel::reset()
{
    if (m_packages.isEmpty() && m_index < 0)
        return;

    m_packages.clear();
    m_index = -1;
    emit changed();
}

} // namespace beygram::telegram
