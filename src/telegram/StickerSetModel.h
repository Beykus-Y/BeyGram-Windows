#pragma once

#include "StickerInfo.h"

#include <QAbstractListModel>
#include <QVector>

namespace beygram::telegram {

/// Flat list of the stickers inside one sticker set, for the QML picker.
/// The model only holds already-parsed metadata; downloading the actual
/// images stays with the caller.
class StickerSetModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        StickerIdRole = Qt::UserRole + 1,
        EmojiRole,
        ThumbnailFileIdRole,
        FullFileIdRole,
        AnimatedRole,
        VideoRole,
    };
    Q_ENUM(Role)

    explicit StickerSetModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int count() const { return m_stickers.size(); }

    void setStickers(QVector<StickerInfo> stickers);
    void clear();

signals:
    void countChanged();

private:
    QVector<StickerInfo> m_stickers;
};

} // namespace beygram::telegram
