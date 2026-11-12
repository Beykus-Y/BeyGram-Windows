#include "StarsFacade.h"

#include <QDateTime>
#include <QVector>

#include <utility>

namespace beygram::ui {

namespace {

/// The Telegram star glyph (U+2B50), written as a code point so the source
/// stays ASCII and the glyph does not depend on the compiler's charset.
constexpr char16_t kStarGlyph = 0x2B50;

/// The separator between the segments of a label or a transaction row: a
/// middle dot (U+00B7) with one space on each side.
QString separator()
{
    return QStringLiteral(" ") + QChar(0x00B7) + QStringLiteral(" ");
}

/// Groups the digits of a non-negative amount by spaces, three at a time from
/// the right: "999", "1 234", "1 234 567". Mirrors StarBalanceModel's grouping
/// so a package row and the balance header read the same way.
QString groupedDigits(qint64 value)
{
    const QString digits = QString::number(value);

    QString grouped;
    grouped.reserve(digits.size() + digits.size() / 3 + 2);

    for (qsizetype i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0)
            grouped += QLatin1Char(' ');
        grouped += digits.at(i);
    }

    return grouped;
}

/// "100 ⭐" / "1 000 ⭐", the star amount as the UI prints it.
QString starText(qint64 stars)
{
    return groupedDigits(stars) + QLatin1Char(' ') + QChar(kStarGlyph);
}

/// A price from integer minor units with exactly two decimals, plus the
/// currency code when there is one: "1.99 USD", "2.00 USD", "1.99".
/// Built from the integers, never from a double, so it cannot round.
QString priceTextFor(qint64 priceMinor, const QString& currency)
{
    QString text = QString::number(priceMinor / 100);
    text += QLatin1Char('.');
    text += QString::number(priceMinor % 100).rightJustified(2, QLatin1Char('0'));

    if (!currency.isEmpty())
        text += QLatin1Char(' ') + currency;

    return text;
}

/// A decimal integer from the string lists, or 0 when the text is not a
/// number. Zero is the models' neutral value: a package or transaction with no
/// stars or no id is dropped, a 0 price is free and a 0 date means "unset", so
/// a malformed entry is dropped or rendered as absent instead of guessed at.
qint64 toInt64(const QString& text)
{
    bool ok = false;
    const qint64 value = text.toLongLong(&ok);
    return ok ? value : 0;
}

/// Whether a payload flag says "credit". TDLib-shaped payloads carry booleans
/// as JSON text, so both the JSON spelling and the obvious alternatives are
/// accepted; anything else is a debit.
bool isIncoming(const QString& text)
{
    const QString value = text.trimmed().toLower();
    return value == QStringLiteral("true") || value == QStringLiteral("1")
        || value == QStringLiteral("yes");
}

/// One history row: "+100 ⭐ · Stars purchase · 2024-05-01". The sign is the
/// direction flag, never the sign of the stored amount, because the model keeps
/// amounts as magnitudes.
QString transactionText(const telegram::StarTransaction& transaction)
{
    QStringList parts;
    parts.reserve(3);

    parts.append((transaction.incoming ? QStringLiteral("+") : QStringLiteral("-"))
                 + starText(transaction.amount));

    if (!transaction.title.isEmpty())
        parts.append(transaction.title);

    if (transaction.date > 0) {
        // UTC and a fixed pattern: the same row must read the same in every
        // locale and time zone, and the date is display data, not an instant
        // the user interacts with. An out-of-range timestamp formats to an
        // empty string, which is omitted like a missing date.
        const QString dateText = QDateTime::fromSecsSinceEpoch(transaction.date)
                                     .toUTC()
                                     .toString(QStringLiteral("yyyy-MM-dd"));
        if (!dateText.isEmpty())
            parts.append(dateText);
    }

    return parts.join(separator());
}

} // namespace

StarsFacade::StarsFacade(QObject* parent) : QObject(parent)
{
    // Each model already suppresses no-op changes, so forwarding all three is
    // enough: a redundant update emits nothing, a real one repaints every
    // binding on this object.
    connect(&m_balance, &telegram::StarBalanceModel::changed, this, &StarsFacade::changed);
    connect(&m_purchases, &telegram::StarPurchaseModel::changed, this, &StarsFacade::changed);
    connect(&m_history, &telegram::StarTransactionModel::changed, this, &StarsFacade::changed);
}

QString StarsFacade::balanceText() const
{
    return m_balance.displayText();
}

void StarsFacade::setBalance(qint64 stars)
{
    m_balance.setBalance(stars);
}

void StarsFacade::setPackages(const QStringList& stars, const QStringList& priceMinor,
                              const QStringList& currency)
{
    QVector<telegram::StarPackage> packages;
    packages.reserve(stars.size());

    for (qsizetype i = 0; i < stars.size(); ++i) {
        telegram::StarPackage package;
        package.stars = toInt64(stars.at(i));
        package.priceMinor = i < priceMinor.size() ? toInt64(priceMinor.at(i)) : 0;
        package.currency = i < currency.size() ? currency.at(i) : QString();
        packages.append(package);
    }

    // The model owns the normalization rules (drop, de-duplicate, clamp,
    // upper-case), so the facade does not repeat them.
    m_purchases.setPackages(std::move(packages));
}

QStringList StarsFacade::packageLabels() const
{
    const QVector<telegram::StarPackage> packages = m_purchases.packages();

    QStringList labels;
    labels.reserve(packages.size());

    for (const telegram::StarPackage& package : packages) {
        labels.append(starText(package.stars) + separator()
                      + priceTextFor(package.priceMinor, package.currency));
    }

    return labels;
}

int StarsFacade::selectedIndex() const
{
    return m_purchases.selectedIndex();
}

void StarsFacade::select(int index)
{
    m_purchases.select(index);
}

QString StarsFacade::priceText() const
{
    return m_purchases.priceText();
}

bool StarsFacade::canBuy() const
{
    return m_purchases.canPurchase();
}

void StarsFacade::buy()
{
    if (!m_purchases.canPurchase())
        return;

    const telegram::StarPackage package = m_purchases.selectedPackage();
    emit purchaseRequested(package.stars, package.currency, package.priceMinor);
}

QStringList StarsFacade::transactions() const
{
    const QVector<telegram::StarTransaction> items = m_history.transactions();

    QStringList rows;
    rows.reserve(items.size());

    for (const telegram::StarTransaction& item : items)
        rows.append(transactionText(item));

    return rows;
}

void StarsFacade::setTransactions(const QStringList& ids, const QStringList& dates,
                                  const QStringList& amounts, const QStringList& titles,
                                  const QStringList& incoming)
{
    QVector<telegram::StarTransaction> items;
    items.reserve(ids.size());

    for (qsizetype i = 0; i < ids.size(); ++i) {
        telegram::StarTransaction item;
        item.id = toInt64(ids.at(i));
        item.date = i < dates.size() ? toInt64(dates.at(i)) : 0;
        item.amount = i < amounts.size() ? toInt64(amounts.at(i)) : 0;
        item.title = i < titles.size() ? titles.at(i) : QString();
        item.incoming = i < incoming.size() && isIncoming(incoming.at(i));
        items.append(item);
    }

    m_history.setTransactions(std::move(items));
}

} // namespace beygram::ui
