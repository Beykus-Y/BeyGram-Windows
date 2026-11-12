#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One row of the gift picker catalog.
///
/// giftId identifies the catalog gift, title is its (often empty) resale title,
/// starCount the price in Telegram stars, stickerFileId the sticker file that
/// stands in for the gift's visual and limited whether the gift is a limited
/// edition. All fields are already-parsed values; nothing here touches TDLib.
struct GiftRow {
    qint64 giftId = 0;
    QString title;
    qint64 starCount = 0;
    int stickerFileId = 0;
    bool limited = false;
};

/// Holds the gift picker catalog.
///
/// The model is a plain value holder: it stores already-parsed GiftRow values
/// and answers price and limited-edition queries without touching TDLib or the
/// UI. Rows without a positive giftId and rows with a negative starCount are
/// dropped on assignment because neither can be shown or sent, and later rows
/// repeating an already stored giftId are dropped so every gift appears once;
/// the first row wins and the incoming order is otherwise kept.
///
/// The catalog order is the server order and is preserved as-is;
/// sortedByPrice() returns a stable copy instead of reordering the stored rows.
class GiftCatalogModel final : public QObject {
    Q_OBJECT

public:
    explicit GiftCatalogModel(QObject* parent = nullptr);

    /// Replaces the stored catalog, dropping rows without a positive giftId,
    /// rows with a negative starCount and later rows whose giftId is already
    /// stored. Emits changed() only when the resulting catalog differs from the
    /// stored one.
    void setGifts(QVector<GiftRow> gifts);

    /// Returns a copy of the stored catalog in stored order.
    [[nodiscard]] QVector<GiftRow> gifts() const;

    /// Returns the number of stored rows.
    [[nodiscard]] int count() const;

    /// Returns the stored rows ordered by starCount, cheapest first when
    /// ascending is true and most expensive first otherwise. The sort is
    /// stable, so rows with the same price keep their stored order. The stored
    /// catalog is not reordered.
    [[nodiscard]] QVector<GiftRow> sortedByPrice(bool ascending = true) const;

    /// Returns the limited-edition rows in stored order.
    [[nodiscard]] QVector<GiftRow> limited() const;

    /// Returns the stored row with giftId, or a default-constructed GiftRow
    /// when no such gift is stored.
    [[nodiscard]] GiftRow gift(qint64 giftId) const;

    /// Drops the catalog, emitting changed() when there was anything to drop.
    void clear();

signals:
    void changed();

private:
    QVector<GiftRow> m_gifts;
};

} // namespace beygram::telegram
