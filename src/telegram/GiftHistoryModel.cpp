#include "GiftHistoryModel.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace beygram::telegram {

namespace {

bool sameRow(const GiftHistoryRow& lhs, const GiftHistoryRow& rhs)
{
    return lhs.giftId == rhs.giftId && lhs.senderUserId == rhs.senderUserId
        && lhs.receiverUserId == rhs.receiverUserId && lhs.date == rhs.date
        && lhs.starCount == rhs.starCount && lhs.anonymous == rhs.anonymous;
}

bool sameRows(const QVector<GiftHistoryRow>& lhs, const QVector<GiftHistoryRow>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    for (qsizetype index = 0; index < lhs.size(); ++index) {
        if (!sameRow(lhs.at(index), rhs.at(index)))
            return false;
    }

    return true;
}

bool containsGift(const QVector<GiftHistoryRow>& rows, qint64 giftId)
{
    return std::any_of(rows.cbegin(), rows.cend(), [giftId](const GiftHistoryRow& row) {
        return row.giftId == giftId;
    });
}

/// Clamps the price of `row` so a negative star count cannot reach the model,
/// and therefore cannot make totalStars() count backwards.
GiftHistoryRow clampedRow(GiftHistoryRow row)
{
    row.starCount = std::max<qint64>(0, row.starCount);
    return row;
}

void sortNewestFirst(QVector<GiftHistoryRow>& rows)
{
    std::stable_sort(
        rows.begin(), rows.end(), [](const GiftHistoryRow& lhs, const GiftHistoryRow& rhs) {
            if (lhs.date != rhs.date)
                return lhs.date > rhs.date;
            return lhs.giftId > rhs.giftId;
        });
}

qint64 saturatedAdd(qint64 lhs, qint64 rhs)
{
    constexpr qint64 maximum = std::numeric_limits<qint64>::max();
    if (lhs > maximum - rhs)
        return maximum;
    return lhs + rhs;
}

} // namespace

GiftHistoryModel::GiftHistoryModel(QObject* parent)
    : QObject(parent)
{
}

void GiftHistoryModel::setRows(QVector<GiftHistoryRow> rows)
{
    QVector<GiftHistoryRow> prepared;
    prepared.reserve(rows.size());

    for (GiftHistoryRow row : rows) {
        if (row.giftId <= 0)
            continue;

        // De-duplicate on the input order, so the first copy of a gift wins even
        // when a later copy carries a newer date or a different price.
        if (containsGift(prepared, row.giftId))
            continue;

        prepared.append(clampedRow(row));
    }

    sortNewestFirst(prepared);

    if (sameRows(prepared, m_rows))
        return;

    m_rows = std::move(prepared);
    emit changed();
}

QVector<GiftHistoryRow> GiftHistoryModel::rows() const
{
    return m_rows;
}

int GiftHistoryModel::count() const
{
    return static_cast<int>(m_rows.size());
}

QVector<GiftHistoryRow> GiftHistoryModel::sent() const
{
    QVector<GiftHistoryRow> result;
    result.reserve(m_rows.size());

    for (const GiftHistoryRow& row : m_rows) {
        if (row.senderUserId != 0)
            result.append(row);
    }

    return result;
}

QVector<GiftHistoryRow> GiftHistoryModel::received() const
{
    QVector<GiftHistoryRow> result;
    result.reserve(m_rows.size());

    for (const GiftHistoryRow& row : m_rows) {
        if (row.receiverUserId != 0)
            result.append(row);
    }

    return result;
}

qint64 GiftHistoryModel::totalStars() const
{
    qint64 total = 0;
    for (const GiftHistoryRow& row : m_rows)
        total = saturatedAdd(total, row.starCount);
    return total;
}

void GiftHistoryModel::append(QVector<GiftHistoryRow> rows)
{
    QVector<GiftHistoryRow> added;
    added.reserve(rows.size());

    for (GiftHistoryRow row : rows) {
        if (row.giftId <= 0)
            continue;

        // A stored gift keeps its copy, and a gift repeated inside this batch is
        // added once, so paging cannot duplicate or rewrite an entry.
        if (containsGift(m_rows, row.giftId) || containsGift(added, row.giftId))
            continue;

        added.append(clampedRow(row));
    }

    if (added.isEmpty())
        return;

    m_rows.append(added);
    sortNewestFirst(m_rows);

    emit changed();
}

void GiftHistoryModel::clear()
{
    if (m_rows.isEmpty())
        return;

    m_rows.clear();
    emit changed();
}

} // namespace beygram::telegram
