#include "GiftCatalogModel.h"

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

bool storesGift(const QVector<GiftRow>& gifts, qint64 giftId)
{
    return std::any_of(gifts.cbegin(), gifts.cend(),
                       [giftId](const GiftRow& row) {
                           return row.giftId == giftId;
                       });
}

// GiftRow carries no operator==, so the change check compares the five fields
// explicitly. Keeps setGifts() from notifying on a no-op assignment.
bool sameGifts(const QVector<GiftRow>& lhs, const QVector<GiftRow>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    return std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(),
                      [](const GiftRow& left, const GiftRow& right) {
                          return left.giftId == right.giftId
                              && left.title == right.title
                              && left.starCount == right.starCount
                              && left.stickerFileId == right.stickerFileId
                              && left.limited == right.limited;
                      });
}

} // namespace

GiftCatalogModel::GiftCatalogModel(QObject* parent)
    : QObject(parent)
{
}

void GiftCatalogModel::setGifts(QVector<GiftRow> gifts)
{
    QVector<GiftRow> next;
    next.reserve(gifts.size());

    for (GiftRow& row : gifts) {
        // A gift without an id cannot be addressed by sendGift, and a negative
        // price cannot be charged.
        if (row.giftId <= 0 || row.starCount < 0)
            continue;
        // Telegram gift ids are unique; the first row and its title, price and
        // sticker win over later repeats.
        if (storesGift(next, row.giftId))
            continue;
        next.append(std::move(row));
    }

    if (sameGifts(next, m_gifts))
        return;

    m_gifts = std::move(next);
    emit changed();
}

QVector<GiftRow> GiftCatalogModel::gifts() const
{
    return m_gifts;
}

int GiftCatalogModel::count() const
{
    return static_cast<int>(m_gifts.size());
}

QVector<GiftRow> GiftCatalogModel::sortedByPrice(bool ascending) const
{
    QVector<GiftRow> rows = m_gifts;

    // stable_sort keeps equal prices in stored order in both directions.
    std::stable_sort(rows.begin(), rows.end(),
                     [ascending](const GiftRow& left, const GiftRow& right) {
                         return ascending ? left.starCount < right.starCount
                                          : left.starCount > right.starCount;
                     });

    return rows;
}

QVector<GiftRow> GiftCatalogModel::limited() const
{
    QVector<GiftRow> rows;
    rows.reserve(m_gifts.size());

    for (const GiftRow& row : m_gifts) {
        if (row.limited)
            rows.append(row);
    }

    return rows;
}

GiftRow GiftCatalogModel::gift(qint64 giftId) const
{
    const auto it = std::find_if(m_gifts.cbegin(), m_gifts.cend(),
                                 [giftId](const GiftRow& row) {
                                     return row.giftId == giftId;
                                 });

    if (it == m_gifts.cend())
        return GiftRow{};

    return *it;
}

void GiftCatalogModel::clear()
{
    if (m_gifts.isEmpty())
        return;

    m_gifts.clear();
    emit changed();
}

} // namespace beygram::telegram
