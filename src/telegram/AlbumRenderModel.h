#pragma once

#include "TelegramTypes.h"

#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One rendered line of the message list: either a media album collapsed into
/// a single row or one plain message.
///
/// `anchorMessageId` is the id of the row's first message and the id the view
/// uses to locate the row; `messageIds` lists every message the row shows in
/// display order. `isAlbum` is true when the row was built from a non-zero
/// grouped_id, even if only one of its messages is currently visible; an
/// ungrouped message is never an album.
struct AlbumRenderRow {
    qint64 anchorMessageId = 0;
    QVector<qint64> messageIds;
    bool isAlbum = false;
};

/// Collapses consecutive messages that TDLib reported with the same non-zero
/// grouped_id into one album row.
///
/// Messages are taken in the order given and never sorted, a message without a
/// positive id is dropped, and only messages that are adjacent after that drop
/// share an album: a grouped_id that reappears after an unrelated message
/// opens a new row instead of joining its earlier one. An ungrouped message is
/// always a row of its own. `changed()` is emitted only when the resulting
/// rows differ from the current ones.
class AlbumRenderModel final : public QObject {
    Q_OBJECT

public:
    explicit AlbumRenderModel(QObject* parent = nullptr);

    /// Rebuilds the rows from \a messages, which are taken in display order.
    void setMessages(QVector<MessageData> messages);

    [[nodiscard]] QVector<AlbumRenderRow> rows() const;
    [[nodiscard]] int rowCount() const;

    /// Number of rows for which isAlbum is true.
    [[nodiscard]] int albumCount() const;

    /// The row showing \a messageId, or a default row when no row carries it.
    [[nodiscard]] AlbumRenderRow rowForMessage(qint64 messageId) const;

    /// Drops every row. Emits changed() only when there was something to drop.
    void clear();

signals:
    /// Emitted whenever the rows actually changed.
    void changed();

private:
    QVector<AlbumRenderRow> m_rows;
};

} // namespace beygram::telegram
