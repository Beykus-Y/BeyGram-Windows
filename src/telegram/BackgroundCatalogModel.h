#pragma once

#include "ChatBackgrounds.h"

#include <QObject>
#include <QVector>

namespace beygram::telegram {

/// Holds the background picker catalog.
///
/// The model is a plain value holder: it stores already-parsed BackgroundInfo
/// values and never talks to TDLib, the file store or the network. A view binds
/// its list to backgrounds(), asks count() for the list size and repaints on
/// changed().
///
/// setBackgrounds() keeps the incoming order and the first row of every id;
/// later rows repeating an id are dropped. Rows without a positive id are
/// malformed (backgroundFromJson() reports them as id 0) and are dropped as
/// well, so every stored background is addressable. Names are trimmed on the
/// way in, matching the rest of the background protocol.
///
/// defaultBackground() returns the first stored row with isDefault set, in
/// stored order. There is no separate preference: when a catalog contains
/// several defaults, the first one wins. An empty catalog, or one without a
/// default row, reports a default-constructed BackgroundInfo (id 0).
class BackgroundCatalogModel final : public QObject {
    Q_OBJECT

public:
    explicit BackgroundCatalogModel(QObject* parent = nullptr);

    /// Replaces the stored catalog with `backgrounds`, dropping rows without a
    /// positive id and later rows whose id is already stored, and trimming each
    /// kept name. Emits changed() only when the resulting catalog differs from
    /// the stored one.
    void setBackgrounds(QVector<BackgroundInfo> backgrounds);

    /// Returns a copy of the stored catalog in stored order.
    [[nodiscard]] QVector<BackgroundInfo> backgrounds() const;

    /// Returns the number of stored backgrounds.
    [[nodiscard]] int count() const;

    /// Returns the first stored background with isDefault set, or a
    /// default-constructed BackgroundInfo when there is none.
    [[nodiscard]] BackgroundInfo defaultBackground() const;

    /// Returns the stored background with `backgroundId`, or a
    /// default-constructed BackgroundInfo when no such background is stored.
    /// Non-positive ids are never stored, so they always report id 0.
    [[nodiscard]] BackgroundInfo find(qint64 backgroundId) const;

    /// Drops the catalog, emitting changed() only when there was anything to
    /// drop.
    void clear();

signals:
    void changed();

private:
    QVector<BackgroundInfo> m_backgrounds;
};

} // namespace beygram::telegram
