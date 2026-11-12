#include "RecentStickerModel.h"

#include <QtGlobal>

#include <utility>

namespace beygram::telegram {
namespace {

// The picker cannot usefully show more than this many recents, and a cap below
// one would forget the sticker the user just sent.
constexpr int kMinMaxEntries = 1;
constexpr int kMaxMaxEntries = 100;

int clampedMaxEntries(int maxEntries)
{
    return qBound(kMinMaxEntries, maxEntries, kMaxMaxEntries);
}

// Drops non-positive ids, keeps the first occurrence of every id and stops at
// the cap, so a too-long list loses its tail (the oldest entries).
QVector<qint64> normalized(const QVector<qint64>& stickerIds, int maxEntries)
{
    QVector<qint64> next;
    next.reserve(qMin<qsizetype>(stickerIds.size(),
                                 static_cast<qsizetype>(maxEntries)));

    for (qint64 stickerId : stickerIds) {
        if (stickerId <= 0)
            continue;
        if (next.contains(stickerId))
            continue;
        next.append(stickerId);
        if (next.size() >= maxEntries)
            break;
    }

    return next;
}

int indexOf(const QVector<qint64>& stickerIds, qint64 stickerId)
{
    for (int i = 0; i < stickerIds.size(); ++i) {
        if (stickerIds.at(i) == stickerId)
            return i;
    }
    return -1;
}

} // namespace

RecentStickerModel::RecentStickerModel(QObject* parent)
    : QObject(parent)
{
}

void RecentStickerModel::setStickers(QVector<qint64> stickerIds, int maxEntries)
{
    const int nextMax = clampedMaxEntries(maxEntries);
    QVector<qint64> next = normalized(stickerIds, nextMax);

    // A response that normalizes to the stored list is not a change; TDLib can
    // repeat the same recents after every send.
    if (next == m_ids && nextMax == m_max)
        return;

    m_ids = std::move(next);
    m_max = nextMax;
    emit changed();
}

QVector<qint64> RecentStickerModel::stickers() const
{
    return m_ids;
}

int RecentStickerModel::count() const
{
    return int(m_ids.size());
}

void RecentStickerModel::add(qint64 stickerId)
{
    if (stickerId <= 0)
        return;

    // The sticker is already the newest entry, so the list is unchanged.
    if (!m_ids.isEmpty() && m_ids.first() == stickerId)
        return;

    QVector<qint64> next;
    next.reserve(qMin<qsizetype>(m_ids.size() + 1,
                                 static_cast<qsizetype>(m_max)));
    next.append(stickerId);

    for (qint64 existing : std::as_const(m_ids)) {
        // The sticker moved to the front instead of being listed twice.
        if (existing == stickerId)
            continue;
        if (next.size() >= m_max)
            break;
        next.append(existing);
    }

    m_ids = std::move(next);
    emit changed();
}

bool RecentStickerModel::remove(qint64 stickerId)
{
    const int index = indexOf(m_ids, stickerId);
    if (index < 0)
        return false;

    m_ids.removeAt(index);
    emit changed();
    return true;
}

int RecentStickerModel::maxEntries() const
{
    return m_max;
}

void RecentStickerModel::setMaxEntries(int maxEntries)
{
    const int nextMax = clampedMaxEntries(maxEntries);

    const bool trims = m_ids.size() > nextMax;
    if (!trims && nextMax == m_max)
        return;

    m_max = nextMax;
    while (m_ids.size() > m_max)
        m_ids.removeLast();

    emit changed();
}

void RecentStickerModel::clear()
{
    if (m_ids.isEmpty())
        return;

    m_ids.clear();
    emit changed();
}

} // namespace beygram::telegram
