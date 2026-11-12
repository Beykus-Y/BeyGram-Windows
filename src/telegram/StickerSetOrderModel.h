#pragma once

#include "StickerCatalog.h"

#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// The user's sticker-set order: which set the picker strip shows first.
///
/// The model stores the catalog entries themselves, so the strip can be built
/// from ordered() without going back to the catalog. Only positive ids are
/// sets: id 0 and negative ids are dropped everywhere, and an id appears at
/// most once, keeping its first position.
///
/// A set can additionally be pinned. Pinned ids keep their own order, and
/// ordered() lists them first, followed by the remaining sets in stored order.
/// Pinning never duplicates and never removes: pin() is not a toggle. Nothing
/// is persisted; the owner decides whether the order survives a restart.
///
/// setSets() replaces the entries but keeps the pinned order for ids that are
/// still present, so a catalog refresh does not lose the user's pins. changed()
/// is emitted only when the model's state really changes.
class StickerSetOrderModel final : public QObject {
    Q_OBJECT

public:
    explicit StickerSetOrderModel(QObject* parent = nullptr);

    /// Replaces the sets with `sets`.
    ///
    /// Non-positive ids are dropped and a repeated id keeps only its first
    /// position, so the stored order is the incoming order without duplicates.
    /// Pinned ids that are no longer in `sets` are forgotten; the surviving
    /// pins keep their pinned order. Emits changed() only when the normalized
    /// sets or the pinned list differ from what is stored.
    void setSets(const QVector<StickerSetInfo>& sets);

    /// The stored sets in stored order, pinned or not. A copy, so a caller
    /// cannot reorder the model through the returned vector.
    [[nodiscard]] QVector<StickerSetInfo> sets() const;

    [[nodiscard]] int count() const;

    /// Moves one set to `newIndex` in the stored order.
    ///
    /// `newIndex` is the position in the resulting list and must be within
    /// 0..count()-1. An unknown id, an out-of-range index, or a set that is
    /// already at `newIndex` is a silent no-op. Returns true when the stored
    /// order changed, i.e. when changed() was emitted. Moving a pinned set
    /// reorders the stored list but not ordered(), which keeps pinned sets
    /// first.
    bool move(qint64 setId, int newIndex);

    /// Pins one set, moving it to the front of the pinned list.
    ///
    /// An unknown id is a silent no-op. Pinning is idempotent: the id is never
    /// added twice and is never unpinned, so pinning the set that is already
    /// the first pin changes nothing. Pinning an id that is pinned further back
    /// moves it to the front, which is a real reorder. Returns true when the
    /// pinned order changed, i.e. when changed() was emitted.
    bool pin(qint64 setId);

    /// The pinned ids in pinned order. A copy, like sets().
    [[nodiscard]] QVector<qint64> pinned() const;

    /// The sets as the strip should show them: pinned sets first in pinned
    /// order, then the remaining sets in stored order.
    [[nodiscard]] QVector<StickerSetInfo> ordered() const;

    /// Forgets the sets and the pinned ids. A no-op on an already empty model.
    void clear();

signals:
    void changed();

private:
    QVector<StickerSetInfo> m_sets;
    QVector<qint64> m_pinned;
};

} // namespace beygram::telegram
