#include "AlbumModel.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

/// A message id identifies an item, so a repeated id keeps its first copy and
/// an id that cannot address a message (non-positive) is dropped, matching
/// what AlbumGrouper does with the same input.
QVector<AlbumMediaItem> dedupeItems(const QVector<AlbumMediaItem>& items)
{
    QVector<AlbumMediaItem> kept;
    kept.reserve(items.size());

    QSet<qint64> seenMessageIds;
    seenMessageIds.reserve(items.size());

    for (const AlbumMediaItem& item : items) {
        if (item.messageId <= 0)
            continue;
        if (seenMessageIds.contains(item.messageId))
            continue;
        seenMessageIds.insert(item.messageId);
        kept.append(item);
    }

    return kept;
}

/// One row per non-zero grouped_id, one row per ungrouped message. Items stay
/// in the order they were supplied; a grouped_id that reappears after
/// unrelated items still joins its earlier row instead of opening a second one.
QVector<AlbumEntry> groupIntoAlbums(const QVector<AlbumMediaItem>& items)
{
    QVector<AlbumEntry> entries;
    entries.reserve(items.size());

    QHash<qint64, int> entryIndexByGroupedId;
    entryIndexByGroupedId.reserve(items.size());

    for (const AlbumMediaItem& item : items) {
        if (item.groupedId <= 0) {
            AlbumEntry singleton;
            singleton.items.append(item);
            entries.append(singleton);
            continue;
        }

        const auto existing = entryIndexByGroupedId.constFind(item.groupedId);
        if (existing != entryIndexByGroupedId.cend()) {
            entries[*existing].items.append(item);
            continue;
        }

        AlbumEntry entry;
        entry.groupedId = item.groupedId;
        entry.items.append(item);
        entries.append(entry);
        entryIndexByGroupedId.insert(item.groupedId, static_cast<int>(entries.size()) - 1);
    }

    // Albums read in message order, keyed by the id of the album's first item.
    std::stable_sort(entries.begin(), entries.end(),
                     [](const AlbumEntry& left, const AlbumEntry& right) {
                         return left.items.constFirst().messageId
                             < right.items.constFirst().messageId;
                     });

    return entries;
}

bool sameItem(const AlbumMediaItem& left, const AlbumMediaItem& right)
{
    return left.messageId == right.messageId && left.groupedId == right.groupedId
        && left.caption == right.caption && left.thumbnailFileId == right.thumbnailFileId
        && left.fullFileId == right.fullFileId && left.video == right.video;
}

bool sameItems(const QVector<AlbumMediaItem>& left, const QVector<AlbumMediaItem>& right)
{
    if (left.size() != right.size())
        return false;

    for (qsizetype index = 0; index < left.size(); ++index) {
        if (!sameItem(left.at(index), right.at(index)))
            return false;
    }
    return true;
}

} // namespace

AlbumModel::AlbumModel(QObject* parent) : QObject(parent)
{
}

void AlbumModel::setItems(QVector<AlbumMediaItem> items)
{
    QVector<AlbumMediaItem> next = dedupeItems(items);
    if (sameItems(next, m_items))
        return;

    m_items = std::move(next);
    emit changed();
}

QVector<AlbumEntry> AlbumModel::albums() const
{
    return groupIntoAlbums(m_items);
}

int AlbumModel::albumCount() const
{
    return static_cast<int>(groupIntoAlbums(m_items).size());
}

void AlbumModel::clear()
{
    if (m_items.isEmpty())
        return;

    m_items.clear();
    emit changed();
}

} // namespace beygram::telegram
