#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One message of a media album as the album strip needs it: the ids of the
/// thumbnail and full-size files, the caption shown under the album and
/// whether the item is a video.
struct AlbumMediaItem {
    qint64 messageId = 0;
    qint64 groupedId = 0;
    QString caption;
    int thumbnailFileId = 0;
    int fullFileId = 0;
    bool video = false;
};

/// One album row: every message that TDLib reported with the same non-zero
/// grouped_id, or a single ungrouped message as its own entry with groupedId 0.
struct AlbumEntry {
    qint64 groupedId = 0;
    QVector<AlbumMediaItem> items;
};

/// Groups media items into the album rows of the message list.
///
/// Items keep the order they were supplied in inside their album, and albums
/// are ordered by the message id of their first item. A message id identifies
/// an item: a repeated id keeps its first copy and later copies are dropped,
/// and an item without a positive message id is ignored the same way
/// AlbumGrouper ignores it.
/// Every mutation that changes the stored items emits changed().
class AlbumModel final : public QObject {
    Q_OBJECT

public:
    explicit AlbumModel(QObject* parent = nullptr);

    /// Replaces the stored items with a de-duplicated copy of \a items.
    void setItems(QVector<AlbumMediaItem> items);
    [[nodiscard]] QVector<AlbumEntry> albums() const;
    [[nodiscard]] int albumCount() const;
    void clear();

signals:
    void changed();

private:
    QVector<AlbumMediaItem> m_items;
};

} // namespace beygram::telegram
