#pragma once

#include "StickerCatalog.h"

#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Tracks which sticker sets the account has installed and which set currently
/// has an install/uninstall request in flight.
///
/// The controller owns no requests and no sticker metadata. Set ids are
/// StickerSetInfo::id values and the install flag is StickerSetInfo::installed,
/// so a caller can read the same rows the catalog parsed and hand the ids to
/// changeStickerSetRequest() without a second lookup. changed() is emitted only
/// when a stored value really changes, so a repaint bound to it stays cheap.
class StickerSetInstallController final : public QObject {
    Q_OBJECT

public:
    explicit StickerSetInstallController(QObject* parent = nullptr);

    /// Replaces the tracked sets. Rows without a positive id are dropped and a
    /// repeated id keeps only its first row, so a set cannot be listed twice;
    /// the caller's order is preserved otherwise. The pending id is not
    /// revalidated here: a set that disappears from the list keeps its id until
    /// markPending() or clearPending() moves it.
    void setSets(QVector<StickerSetInfo> sets);

    [[nodiscard]] QVector<StickerSetInfo> sets() const;

    /// Number of tracked sets whose installed flag is set.
    [[nodiscard]] int installedCount() const;

    /// Whether the set is tracked and installed. An unknown id is never
    /// installed.
    [[nodiscard]] bool isInstalled(qint64 setId) const;

    /// Sets the installed flag of a tracked set. An unknown id is ignored, and
    /// so is a value that matches the current flag; only a real change emits
    /// changed().
    bool setInstalled(qint64 setId, bool installed);

    /// The set whose install/uninstall request is in flight, or 0 when none is.
    [[nodiscard]] qint64 pendingSetId() const;

    /// Marks a set as pending. Only tracked ids are accepted; 0 clears the
    /// pending set, which is also how a request completes. Marking the pending
    /// id again is a no-op.
    void markPending(qint64 setId);

    /// Clears the pending set. A no-op when nothing is pending.
    void clearPending();

    /// Drops the sets and the pending id.
    void clear();

signals:
    void changed();

private:
    QVector<StickerSetInfo> m_sets;
    qint64 m_pending = 0;
};

} // namespace beygram::telegram
