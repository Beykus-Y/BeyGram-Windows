#include "AlbumGrouper.h"

#include <QHash>
#include <QSet>

namespace beygram::telegram {

// TDLib sets message.grouped_id on every message of a media album; 0 means the
// message stands alone. Callers usually pass a page in chronological order, so
// first-seen order is also ascending-first-message-id order. A grouped_id that
// reappears after an unrelated message (e.g. a page boundary or an interleaved
// singleton) still joins its earlier group instead of opening a second one.
QVector<AlbumGroup> groupAlbums(const QVector<QPair<qint64, qint64>>& messageIdAndGroupedId)
{
    QVector<AlbumGroup> groups;
    groups.reserve(messageIdAndGroupedId.size());

    QSet<qint64> seenMessageIds;
    seenMessageIds.reserve(messageIdAndGroupedId.size());

    QHash<qint64, int> groupIndexByGroupedId;
    groupIndexByGroupedId.reserve(messageIdAndGroupedId.size());

    for (const QPair<qint64, qint64>& entry : messageIdAndGroupedId) {
        const qint64 messageId = entry.first;
        const qint64 groupedId = entry.second;

        if (messageId <= 0)
            continue;
        if (seenMessageIds.contains(messageId))
            continue;
        seenMessageIds.insert(messageId);

        if (groupedId <= 0) {
            AlbumGroup singleton;
            singleton.messageIds.append(messageId);
            groups.append(singleton);
            continue;
        }

        const auto existing = groupIndexByGroupedId.constFind(groupedId);
        if (existing != groupIndexByGroupedId.cend()) {
            groups[*existing].messageIds.append(messageId);
            continue;
        }

        AlbumGroup group;
        group.groupedId = groupedId;
        group.messageIds.append(messageId);
        groups.append(group);
        groupIndexByGroupedId.insert(groupedId, groups.size() - 1);
    }

    return groups;
}

} // namespace beygram::telegram
