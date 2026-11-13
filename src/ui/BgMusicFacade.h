#pragma once

#include "telegram/music/BgMusic.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace beygram::ui {

/// The background-music playlist panel as one QML-facing object.
///
/// The facade owns the ordered playlist strip and the entries imported for it.
/// setPlaylists() is how the caller publishes what a server or a local library
/// reported: ids and titles only, because that is all a list needs. The content
/// of a playlist is owned here and arrives through importFromText(), which
/// parses pasted JSON or M3U text with the same rules as telegram::BgMusic and
/// telegram::planBgMusicImport(). exportJson() and exportM3u() serialize the
/// selected playlist back out; saving the string is the caller's job.
///
/// Invariants:
///  - playlistIds() and playlistTitles() are parallel, have the same length and
///    keep the caller's order. An empty id is dropped because nothing could
///    select it, and only the first row of a repeated id is kept.
///  - selectedId() is empty or names one of playlistIds(). It is never left
///    pointing at a playlist that setPlaylists() removed.
///  - entryCount() is the number of entries in the selected playlist, not a
///    total: a list with no selection reports 0. A playlist published by
///    setPlaylists() has no known entries until something is imported for it.
///  - exportJson() and exportM3u() return an empty string while nothing is
///    selected. A selected playlist without entries still exports a valid,
///    empty document.
///
/// setPlaylists() preserves the entries and updatedAt of a playlist whose id is
/// still listed, so a metadata refresh does not throw away imported content; a
/// playlist that is no longer listed is dropped with its entries.
///
/// importFromText() validates with planBgMusicImport(), so the rejection reasons
/// are the documented tags `empty`, `too-large`, `unsupported-extension` and
/// `no-entries`, and a rejection changes no state and emits importFailed() only.
/// An accepted playlist replaces the playlist with the same id, or is appended
/// when the id is new, and becomes selected so the panel immediately shows it.
/// A file without a playlist id (every M3U) gets a generated local id, and a
/// playlist without a title gets the file's base name (or the generated id when
/// even that is empty), so a row is never blank.
///
/// changed() is emitted at most once per call and only when a property QML can
/// read really moved. A refresh with the same ids and titles, a tap on the
/// already-selected row or a repeated clear stays silent. Nothing here reads or
/// writes a file, touches TDLib or the network, reads the clock or logs.
class BgMusicFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList playlistTitles READ playlistTitles NOTIFY changed)
    Q_PROPERTY(QStringList playlistIds READ playlistIds NOTIFY changed)
    Q_PROPERTY(int entryCount READ entryCount NOTIFY changed)
    Q_PROPERTY(QString selectedId READ selectedId NOTIFY changed)

public:
    explicit BgMusicFacade(QObject* parent = nullptr);

    /// Display titles of the playlist strip, parallel to playlistIds().
    [[nodiscard]] QStringList playlistTitles() const;

    /// Opaque ids of the playlist strip, in display order.
    [[nodiscard]] QStringList playlistIds() const;

    /// Entries of the selected playlist, or 0 when nothing is selected.
    [[nodiscard]] int entryCount() const;

    /// The selected playlist id, or an empty string when nothing is selected.
    [[nodiscard]] QString selectedId() const;

    /// Publishes the playlist strip from the parallel lists a library payload
    /// arrives as. The lists are paired by index and cut to the shorter one, so
    /// a title with no id is ignored rather than inventing a playlist.
    ///
    /// An empty id is dropped, a repeated id keeps its first row, and titles are
    /// taken as given (an empty title stays empty). Entries and updatedAt of a
    /// playlist that keeps its id survive the refresh. The selection survives
    /// while its id is still listed and becomes empty otherwise. Silent when the
    /// visible strip and the selection did not move.
    Q_INVOKABLE void setPlaylists(const QStringList& ids, const QStringList& titles);

    /// Selects the playlist with `id`. An empty id clears the selection; an id
    /// the strip does not contain is ignored, so a stale tap cannot select an
    /// invisible playlist. Both are silent when the selection would not change.
    Q_INVOKABLE void select(const QString& id);

    /// Imports pasted playlist text. `fileName` only selects the parser by its
    /// extension, exactly like planBgMusicImport(): `.json` uses
    /// bgMusicFromJson(), `.m3u` and `.m3u8` use bgMusicFromM3u().
    ///
    /// Returns true when the text was accepted and the playlist strip now
    /// contains it. Returns false and emits importFailed(reason) when the
    /// extension, size, emptiness or entry count is unusable; a failure leaves
    /// the strip and the selection untouched.
    Q_INVOKABLE bool importFromText(const QString& fileName, const QString& content);

    /// The selected playlist as versioned JSON, or an empty string when nothing
    /// is selected.
    Q_INVOKABLE QString exportJson() const;

    /// The selected playlist as extended M3U, or an empty string when nothing is
    /// selected.
    Q_INVOKABLE QString exportM3u() const;

    /// Forgets every playlist and the selection. Silent when already empty.
    Q_INVOKABLE void clear();

signals:
    /// Any observable state of this facade moved: the strip, the entry count or
    /// the selection.
    void changed();

    /// importFromText() rejected the content. `reason` is the machine-readable
    /// tag from telegram::ImportDecision.
    void importFailed(const QString& reason);

private:
    /// Everything QML can read. Mutators snapshot it, run, and emit changed()
    /// only when the snapshot moved, so a no-op call stays silent.
    struct State {
        QStringList ids;
        QStringList titles;
        int entryCount = 0;
        QString selectedId;

        bool operator==(const State&) const = default;
    };

    [[nodiscard]] State state() const;
    void emitIfChanged(const State& before);

    /// Index of the playlist with `id`, or -1. Ids are compared exactly; the
    /// strip is short, so a linear scan beats a second index to keep in sync.
    [[nodiscard]] qsizetype indexOf(const QString& id) const;

    /// Index of the selected playlist, or -1 when nothing is selected.
    [[nodiscard]] qsizetype selectedIndex() const;

    /// A local id that no playlist in the strip uses yet: `import-1`, then
    /// `import-2`, and so on. Deterministic, so a test and a reload agree.
    [[nodiscard]] QString nextGeneratedId() const;

    QVector<telegram::BgMusicPlaylist> m_playlists;
    QString m_selectedId;
};

} // namespace beygram::ui
