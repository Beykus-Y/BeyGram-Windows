#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace beygram::telegram {

/// The selection state of the "clear cache" dialog.
///
/// Candidates are the on-disk media sizes grouped by media kind, using the
/// lower-case names of MediaKind from MediaCachePolicy.h ("photo", "video",
/// "document", "audio", "other"). The controller only stores and validates that
/// map plus the kinds the user ticked; it never measures or deletes a file and
/// never talks to TDLib. A view feeds setCandidates() from its scan, binds its
/// checkboxes to setSelected(), and calls clean() itself once canClean() is
/// true.
///
/// Invariants: a candidate with a blank kind is dropped and a negative size is
/// clamped to 0, so a stored size is never negative. The selection is always a
/// deduplicated, sorted subset of the candidate kinds, so selectedBytes() is the
/// exact total of what a cleanup would free.
class CacheCleanupController final : public QObject {
    Q_OBJECT

public:
    explicit CacheCleanupController(QObject* parent = nullptr);

    /// Replaces the known candidates. Blank kinds are dropped and negative sizes
    /// become 0. Kinds that disappeared from the new map are also removed from
    /// the selection, which keeps the selection a subset of the candidates.
    /// Emits changed() once if the candidates or the pruned selection moved.
    void setCandidates(QHash<QString, qint64> bytesByKind);
    [[nodiscard]] QHash<QString, qint64> candidates() const;

    /// Selects the kinds to clean. Unknown kinds are ignored, duplicates are
    /// dropped and the result is sorted, so the same set in any order is the same
    /// selection. Emits changed() only when the stored selection really changed.
    void setSelected(const QStringList& kinds);
    [[nodiscard]] QStringList selected() const;

    /// Total size of the selected kinds. 0 when nothing is selected.
    [[nodiscard]] qint64 selectedBytes() const;

    /// True when a cleanup could run now: not already running, at least one kind
    /// selected, and that selection is worth more than 0 bytes.
    [[nodiscard]] bool canClean() const;

    /// Marks a cleanup as in flight so the view can disable its button. Setting
    /// the current value is a no-op and stays silent.
    void setRunning(bool running);
    [[nodiscard]] bool running() const;

    /// Drops the candidates, the selection and the running flag. Emits changed()
    /// once if anything was actually set.
    void clear();

signals:
    /// Emitted whenever the candidates, the selection or the running flag really
    /// changed. Rejected kinds, no-op setters and no-op clears stay silent.
    void changed();

private:
    QHash<QString, qint64> m_bytes;
    QStringList m_selected;
    bool m_running = false;
};

} // namespace beygram::telegram
