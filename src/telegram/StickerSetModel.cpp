#include "StickerSetModel.h"

#include <utility>

namespace beygram::telegram {

StickerSetModel::StickerSetModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int StickerSetModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_stickers.size();
}

QVariant StickerSetModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_stickers.size())
        return {};

    const StickerInfo& sticker = m_stickers.at(index.row());
    switch (role) {
    case StickerIdRole: return sticker.stickerId;
    case EmojiRole: return sticker.emoji;
    case ThumbnailFileIdRole: return sticker.thumbnailFileId;
    case FullFileIdRole: return sticker.fullFileId;
    case AnimatedRole: return sticker.isAnimated;
    case VideoRole: return sticker.isVideo;
    default: return {};
    }
}

QHash<int, QByteArray> StickerSetModel::roleNames() const
{
    return {
        {StickerIdRole, "stickerId"},
        {EmojiRole, "emoji"},
        {ThumbnailFileIdRole, "thumbnailFileId"},
        {FullFileIdRole, "fullFileId"},
        {AnimatedRole, "isAnimated"},
        {VideoRole, "isVideo"},
    };
}

void StickerSetModel::setStickers(QVector<StickerInfo> stickers)
{
    beginResetModel();
    m_stickers = std::move(stickers);
    endResetModel();
    emit countChanged();
}

void StickerSetModel::clear()
{
    if (m_stickers.isEmpty())
        return;
    beginResetModel();
    m_stickers.clear();
    endResetModel();
    emit countChanged();
}

} // namespace beygram::telegram
