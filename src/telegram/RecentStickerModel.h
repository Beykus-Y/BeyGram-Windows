#pragma once

#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// The sticker picker's "Recent" strip, ordered newest first.
///
/// Only sticker ids are stored; the picker resolves them through the sticker
/// catalog and the file store, so this model never touches TDLib. Id 0 and
/// negative ids are not stickers and are ignored everywhere, and an id appears
/// at most once: the list is a most-recently-used order, not a log.
///
/// The list is capped at maxEntries(), which is always within 1..100. A cap
/// that is too small always drops from the tail, i.e. the oldest entry, so the
/// newest sticker survives. Nothing is persisted; the owner decides whether the
/// list survives a restart. changed() is emitted only when the list or the cap
/// really changes.
class RecentStickerModel final : public QObject {
    Q_OBJECT

public:
    explicit RecentStickerModel(QObject* parent = nullptr);

    /// Replaces the list with `stickerIds` and installs `maxEntries` as the cap.
    ///
    /// Non-positive ids are dropped, a repeated id keeps only its first
    /// position, and a list longer than the cap loses its tail. `maxEntries` is
    /// clamped exactly like setMaxEntries(). Emits changed() only when the
    /// resulting list or the cap differs from what is stored.
    void setStickers(QVector<qint64> stickerIds, int maxEntries = 30);

    /// The stored sticker ids, newest first. A copy, so a caller cannot reorder
    /// the model through the returned vector.
    [[nodiscard]] QVector<qint64> stickers() const;

    [[nodiscard]] int count() const;

    /// Records one sent sticker as the newest entry.
    ///
    /// A sticker that is already in the list moves to the front instead of
    /// being duplicated, so it is never listed twice. A non-positive id is
    /// ignored, and the tail is dropped when the list is full. Emits changed()
    /// only when the order actually changed, so re-sending the newest sticker
    /// is silent.
    void add(qint64 stickerId);

    /// Forgets one sticker. Returns true when it was there, i.e. when changed()
    /// was emitted. An id that is not in the list is a silent no-op.
    bool remove(qint64 stickerId);

    /// The cap on stored entries, always within 1..100.
    [[nodiscard]] int maxEntries() const;

    /// Changes the cap, clamping it to 1..100.
    ///
    /// Lowering it drops the oldest entries beyond the new cap; raising it keeps
    /// the stored list as it is. Emits changed() only when the cap or the list
    /// changed.
    void setMaxEntries(int maxEntries);

    /// Forgets the whole list. A no-op on an already empty list.
    void clear();

signals:
    void changed();

private:
    QVector<qint64> m_ids;
    int m_max = 30;
};

} // namespace beygram::telegram
