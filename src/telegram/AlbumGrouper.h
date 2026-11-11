#pragma once

#include <QPair>
#include <QVector>

namespace beygram::telegram {

/// One media album: every message that TDLib reported with the same non-zero
/// message.grouped_id, plus every ungrouped message as its own singleton.
struct AlbumGroup {
    qint64 groupedId = 0;
    QVector<qint64> messageIds; // chronological, positive ids only
};

/// Groups message ids by their non-zero grouped_id, preserving input order
/// inside each group and ordering groups by their first message id.
[[nodiscard]] QVector<AlbumGroup> groupAlbums(
    const QVector<QPair<qint64, qint64>>& messageIdAndGroupedId);

} // namespace beygram::telegram
