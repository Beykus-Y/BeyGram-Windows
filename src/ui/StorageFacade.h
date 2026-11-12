#pragma once

#include "telegram/AutoDownloadPolicy.h"
#include "telegram/CacheCleanupController.h"
#include "telegram/DataUsageModel.h"
#include "telegram/StorageStatsModel.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::ui {

/// The storage and data-usage settings screen as one QML-facing object.
///
/// A thin adapter over four telegram models, each of which keeps its own rules:
///
///   - telegram::StorageStatsModel holds the cache breakdown (kind, file count,
///     bytes). setStorage() takes the parallel string lists a TDLib-shaped
///     payload arrives as and lets the model drop blank kinds, keep the first
///     copy of a duplicate and clamp negative counts.
///   - telegram::CacheCleanupController owns the ticked kinds and whether a
///     cleanup may run. Its candidates are always the model's cleaned rows, so
///     a kind the user can tick is exactly a kind the panel shows.
///   - telegram::DataUsageModel holds the per-category traffic and does the
///     byte formatting, so every size on screen has one voice.
///   - telegram::AutoDownloadPolicy stores the auto-download switch.
///
/// The facade adds only what QML cannot: parsing the positional string lists,
/// pairing a kind with its formatted size, and turning the selection into one
/// cleanRequested() signal. It never scans, measures or deletes anything and
/// never talks to TDLib; a caller connects cleanRequested() to the real
/// cleanup.
///
/// Formatting is DataUsageModel::formatBytes() throughout: sizes() is one
/// "1.5 MB" per row, totalText() the same for the whole breakdown,
/// selectedText() for the ticked rows, and usage() one
/// "<category> · <sent> sent · <received> received" line per category.
///
/// changed() is forwarded from the models, so it fires only on a real change.
/// setStorage() touches two models but emits changed() at most once.
class StorageFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList kinds READ kinds NOTIFY changed)
    Q_PROPERTY(QStringList sizes READ sizes NOTIFY changed)
    Q_PROPERTY(QString totalText READ totalText NOTIFY changed)
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY changed)
    Q_PROPERTY(bool canClean READ canClean NOTIFY changed)
    Q_PROPERTY(bool autoDownload READ autoDownload WRITE setAutoDownload NOTIFY changed)
    Q_PROPERTY(QStringList usage READ usage NOTIFY changed)

public:
    explicit StorageFacade(QObject* parent = nullptr);

    /// The cache kinds in display order, one per row; empty before any update.
    [[nodiscard]] QStringList kinds() const;

    /// The formatted size of each row, parallel to kinds().
    [[nodiscard]] QStringList sizes() const;

    /// Replaces the breakdown. kinds, fileCounts and bytes are parallel lists
    /// and are read positionally; a shorter list ends the pairing, so a missing
    /// count or size reads as 0, and a value that is not a number reads as 0.
    ///
    /// A negative size is stored as 0: a cache row cannot free less than
    /// nothing. The model applies the rest of the normalization (blank kinds
    /// dropped, first duplicate kept, negative counts clamped), and the ticked
    /// kinds are pruned to the rows that remain, so the selection is always a
    /// subset of what the panel shows.
    Q_INVOKABLE void setStorage(const QStringList& kinds, const QStringList& fileCounts,
                                const QStringList& bytes);

    /// The size of every row together as "1.5 MB"; "0 B" when nothing is
    /// stored.
    [[nodiscard]] QString totalText() const;

    /// Ticks the kinds to clean. Unknown kinds are ignored, duplicates are
    /// dropped and the result is sorted, so the same set in any order is the
    /// same selection.
    Q_INVOKABLE void setSelection(const QStringList& kinds);

    /// The ticked kinds, sorted; empty when nothing is ticked.
    [[nodiscard]] QStringList selectedKinds() const;

    /// The size of the ticked kinds as "1.5 MB"; "0 B" when nothing is ticked.
    [[nodiscard]] QString selectedText() const;

    /// Whether clean() would do anything now: at least one kind is ticked, that
    /// selection is worth more than 0 bytes, and no cleanup is in flight.
    [[nodiscard]] bool canClean() const;

    /// Emits cleanRequested() with the sorted ticked kinds, marks the cleanup
    /// as running and returns true. Returns false, emitting nothing, when
    /// canClean() is false. The caller owns the cleanup and must call
    /// setRunning(false) when it finishes; the facade does not time it out.
    Q_INVOKABLE bool clean();

    /// Marks a cleanup as in flight, which disables clean() until it is
    /// released. Setting the current value is a no-op and stays silent.
    Q_INVOKABLE void setRunning(bool running);

    /// Whether automatic downloading is enabled; on by default.
    [[nodiscard]] bool autoDownload() const;
    void setAutoDownload(bool on);

    /// Replaces the per-category traffic. categories, sent and received are
    /// parallel lists read positionally; a missing byte count reads as 0. The
    /// model drops blank categories, keeps the first row of a duplicate and
    /// clamps negative counts to 0.
    Q_INVOKABLE void setUsage(const QStringList& categories, const QStringList& sent,
                              const QStringList& received);

    /// One row per category, in display order:
    /// "Photos · 1.0 KB sent · 512 B received".
    [[nodiscard]] QStringList usage() const;

signals:
    /// Emitted whenever any exposed value actually changed.
    void changed();

    /// A cleanup was requested for the sorted ticked kinds, which are exactly
    /// the kinds selectedText() covers. The facade has already marked itself
    /// as running.
    void cleanRequested(const QStringList& kinds);

private:
    void handleModelChanged();

    telegram::StorageStatsModel m_stats;
    telegram::CacheCleanupController m_cleanup;
    telegram::DataUsageModel m_dataUsage;
    telegram::AutoDownloadPolicy m_policy;
    // Coalesces the two model signals of setStorage() into one changed().
    bool m_silent = false;
    bool m_pending = false;
};

} // namespace beygram::ui
