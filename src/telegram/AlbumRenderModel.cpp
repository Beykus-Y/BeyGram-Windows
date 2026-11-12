#include "AlbumRenderModel.h"

#include <utility>

namespace beygram::telegram {
namespace {

[[nodiscard]] bool sameRow(const AlbumRenderRow& left, const AlbumRenderRow& right)
{
    return left.anchorMessageId == right.anchorMessageId && left.isAlbum == right.isAlbum
        && left.messageIds == right.messageIds;
}

[[nodiscard]] bool sameRows(const QVector<AlbumRenderRow>& left,
                            const QVector<AlbumRenderRow>& right)
{
    if (left.size() != right.size())
        return false;
    for (qsizetype index = 0; index < left.size(); ++index) {
        if (!sameRow(left.at(index), right.at(index)))
            return false;
    }
    return true;
}

/// Consecutive messages with the same non-zero grouped_id collapse into one
/// row; everything else keeps a row of its own. A row built from a non-zero
/// grouped_id is an album even when only one of its messages is currently
/// visible, while an ungrouped message is never an album. Ids that cannot
/// address a message are dropped before adjacency is judged, so they never
/// split an album.
[[nodiscard]] QVector<AlbumRenderRow> buildRows(const QVector<MessageData>& messages)
{
    QVector<AlbumRenderRow> rows;
    rows.reserve(messages.size());

    qint64 previousGroupedId = 0;

    for (const MessageData& message : messages) {
        if (message.id <= 0)
            continue;

        const bool continuesAlbum = !rows.isEmpty() && message.groupedId != 0
            && message.groupedId == previousGroupedId;

        if (continuesAlbum) {
            rows.last().messageIds.append(message.id);
        } else {
            AlbumRenderRow row;
            row.anchorMessageId = message.id;
            row.messageIds.append(message.id);
            row.isAlbum = message.groupedId != 0;
            rows.append(row);
        }

        previousGroupedId = message.groupedId;
    }

    return rows;
}

} // namespace

AlbumRenderModel::AlbumRenderModel(QObject* parent)
    : QObject(parent)
{
}

void AlbumRenderModel::setMessages(QVector<MessageData> messages)
{
    QVector<AlbumRenderRow> next = buildRows(messages);
    if (sameRows(next, m_rows))
        return;

    m_rows = std::move(next);
    emit changed();
}

QVector<AlbumRenderRow> AlbumRenderModel::rows() const
{
    return m_rows;
}

int AlbumRenderModel::rowCount() const
{
    return static_cast<int>(m_rows.size());
}

int AlbumRenderModel::albumCount() const
{
    int count = 0;
    for (const AlbumRenderRow& row : m_rows) {
        if (row.isAlbum)
            ++count;
    }
    return count;
}

AlbumRenderRow AlbumRenderModel::rowForMessage(qint64 messageId) const
{
    if (messageId <= 0)
        return {};

    for (const AlbumRenderRow& row : m_rows) {
        if (row.messageIds.contains(messageId))
            return row;
    }
    return {};
}

void AlbumRenderModel::clear()
{
    if (m_rows.isEmpty())
        return;

    m_rows.clear();
    emit changed();
}

} // namespace beygram::telegram
