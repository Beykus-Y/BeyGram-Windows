#include "StarTransactionModel.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace beygram::telegram {

namespace {

bool sameTransaction(const StarTransaction& lhs, const StarTransaction& rhs)
{
    return lhs.id == rhs.id && lhs.date == rhs.date && lhs.amount == rhs.amount
        && lhs.title == rhs.title && lhs.incoming == rhs.incoming;
}

bool sameTransactions(const QVector<StarTransaction>& lhs, const QVector<StarTransaction>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    for (qsizetype index = 0; index < lhs.size(); ++index) {
        if (!sameTransaction(lhs.at(index), rhs.at(index)))
            return false;
    }

    return true;
}

/// Returns the magnitude of `amount`. The smallest qint64 has no positive
/// counterpart, so it saturates at the maximum instead of overflowing.
qint64 magnitude(qint64 amount)
{
    if (amount >= 0)
        return amount;
    if (amount == std::numeric_limits<qint64>::min())
        return std::numeric_limits<qint64>::max();
    return -amount;
}

qint64 saturatedAdd(qint64 lhs, qint64 rhs)
{
    constexpr qint64 maximum = std::numeric_limits<qint64>::max();
    if (lhs > maximum - rhs)
        return maximum;
    return lhs + rhs;
}

void sortNewestFirst(QVector<StarTransaction>& transactions)
{
    std::stable_sort(
        transactions.begin(), transactions.end(),
        [](const StarTransaction& lhs, const StarTransaction& rhs) {
            if (lhs.date != rhs.date)
                return lhs.date > rhs.date;
            return lhs.id > rhs.id;
        });
}

/// Prepares `transactions` for storage: invalid and duplicate entries are
/// dropped, amounts become absolute and the list is sorted newest first.
QVector<StarTransaction> prepared(QVector<StarTransaction> transactions)
{
    QVector<StarTransaction> result;
    result.reserve(transactions.size());

    for (StarTransaction transaction : transactions) {
        if (transaction.id <= 0)
            continue;

        const bool duplicate = std::any_of(
            result.cbegin(), result.cend(), [&transaction](const StarTransaction& existing) {
                return existing.id == transaction.id;
            });
        if (duplicate)
            continue;

        transaction.amount = magnitude(transaction.amount);
        result.append(transaction);
    }

    sortNewestFirst(result);

    return result;
}

} // namespace

StarTransactionModel::StarTransactionModel(QObject* parent)
    : QObject(parent)
{
}

void StarTransactionModel::setTransactions(QVector<StarTransaction> transactions)
{
    QVector<StarTransaction> next = prepared(std::move(transactions));

    if (sameTransactions(next, m_items))
        return;

    m_items = std::move(next);
    emit changed();
}

QVector<StarTransaction> StarTransactionModel::transactions() const
{
    return m_items;
}

int StarTransactionModel::count() const
{
    return static_cast<int>(m_items.size());
}

qint64 StarTransactionModel::totalIncoming() const
{
    qint64 total = 0;
    for (const StarTransaction& transaction : m_items) {
        if (transaction.incoming)
            total = saturatedAdd(total, transaction.amount);
    }
    return total;
}

qint64 StarTransactionModel::totalOutgoing() const
{
    qint64 total = 0;
    for (const StarTransaction& transaction : m_items) {
        if (!transaction.incoming)
            total = saturatedAdd(total, transaction.amount);
    }
    return total;
}

qint64 StarTransactionModel::net() const
{
    // Both totals are non-negative and never exceed the qint64 maximum, so
    // their difference cannot overflow.
    return totalIncoming() - totalOutgoing();
}

void StarTransactionModel::append(QVector<StarTransaction> transactions)
{
    QVector<StarTransaction> added;
    added.reserve(transactions.size());

    for (StarTransaction transaction : transactions) {
        if (transaction.id <= 0)
            continue;

        const bool stored = std::any_of(
            m_items.cbegin(), m_items.cend(), [&transaction](const StarTransaction& existing) {
                return existing.id == transaction.id;
            });
        const bool pending = std::any_of(
            added.cbegin(), added.cend(), [&transaction](const StarTransaction& existing) {
                return existing.id == transaction.id;
            });
        if (stored || pending)
            continue;

        transaction.amount = magnitude(transaction.amount);
        added.append(transaction);
    }

    if (added.isEmpty())
        return;

    m_items.append(added);
    sortNewestFirst(m_items);

    emit changed();
}

void StarTransactionModel::clear()
{
    if (m_items.isEmpty())
        return;

    m_items.clear();
    emit changed();
}

} // namespace beygram::telegram
