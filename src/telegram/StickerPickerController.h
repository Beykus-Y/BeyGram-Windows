#pragma once

#include "StickerCatalog.h"

#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One row of the picker's set strip.
///
/// This is deliberately an alias and not a new struct: the strip needs the same
/// data the catalog already parses (id, title, preview file, install state), so
/// the existing StickerSetInfo is reused instead of duplicating its fields.
/// The row id is StickerSetInfo::id (a TDLib sticker set id) and the label is
/// StickerSetInfo::title; the remaining fields are carried along so a row can
/// drive the install/archive affordances without a second lookup.
using StickerSetRow = StickerSetInfo;

/// Drives the sticker picker's tab strip: which set is selected and which
/// stickers that set currently shows.
///
/// The controller owns no requests and no sticker metadata. Set ids are
/// StickerSetInfo::id values; sticker ids are StickerInfo::stickerId values, the
/// same ones StickerSetModel exposes through StickerIdRole, so stickers() can be
/// handed straight to the existing sticker views. changed() is emitted only when
/// a stored value really changes, so a repaint bound to it stays cheap.
class StickerPickerController final : public QObject {
    Q_OBJECT

public:
    explicit StickerPickerController(QObject* parent = nullptr);

    /// Replaces the set strip. Rows without a positive id are dropped and a
    /// repeated id keeps only its first row, so the strip cannot show the same
    /// set twice; the caller's order is preserved otherwise. The current
    /// selection is not revalidated here: a set that disappears from the list
    /// keeps its id until selectSet() moves the selection.
    void setSets(QVector<StickerSetRow> sets);

    [[nodiscard]] QVector<StickerSetRow> sets() const;

    [[nodiscard]] int setCount() const;

    /// Selects a tab. 0 is the recent/favourites tab; any other value must be
    /// an id present in sets(), otherwise the call is ignored. Selecting the
    /// already selected id is a no-op. A real change drops the stickers and the
    /// loading flag, because both belonged to the previous set.
    void selectSet(qint64 setId);

    [[nodiscard]] qint64 currentSetId() const;

    /// Replaces the stickers of the current set. Ids that are not positive are
    /// dropped and repeated ids are kept only once, in first-seen order. A
    /// response is accepted regardless of loading(), since the flag only tells
    /// the UI that a request is in flight.
    void setStickers(QVector<qint64> stickerIds);

    [[nodiscard]] QVector<qint64> stickers() const;

    [[nodiscard]] bool loading() const;

    /// Marks whether the current set's stickers are being fetched.
    void setLoading(bool loading);

    /// Drops the sets, the selection, the stickers and the loading flag.
    void clear();

signals:
    void changed();

private:
    QVector<StickerSetRow> m_sets;
    qint64 m_currentSetId = 0;
    QVector<qint64> m_stickers;
    bool m_loading = false;
};

} // namespace beygram::telegram
