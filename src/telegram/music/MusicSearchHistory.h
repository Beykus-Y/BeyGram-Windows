#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace beygram::telegram {

/// The recent queries of the music search, newest first.
///
/// Queries are stored as trimmed, non-empty text. An entry is unique ignoring
/// case, so "Rock" and "rock" never appear twice. The list is capped on every
/// add; the oldest entry falls off when the cap is reached. The history holds
/// no account state itself: the owner creates one instance per account.
///
/// changed() is emitted only when the stored list actually changes, so a
/// repeated add of the front entry, an ignored blank query, a remove that
/// matched nothing and clear() on an empty history all stay silent.
class MusicSearchHistory final : public QObject {
    Q_OBJECT

public:
    explicit MusicSearchHistory(QObject* parent = nullptr);

    /// Adds \a query at the front, keeping at most \a maxEntries entries.
    ///
    /// \a query is trimmed and ignored when nothing is left. An entry that
    /// equals the trimmed query ignoring case is dropped first, so the query
    /// moves to the front and takes the casing of this call. A non-positive
    /// \a maxEntries keeps no entries at all.
    void add(const QString& query, int maxEntries = 20);

    /// Returns a copy of the stored queries, newest first.
    [[nodiscard]] QStringList entries() const;

    /// Returns the entries that start with \a prefix, ignoring case, in stored
    /// order. A prefix that is blank after trimming matches every entry.
    [[nodiscard]] QStringList matching(const QString& prefix) const;

    /// Removes the entry that equals \a query ignoring case, after trimming
    /// \a query, and returns whether one was removed. Entries are unique
    /// ignoring case, so at most one entry is dropped.
    bool remove(const QString& query);

    /// Drops every entry, emitting changed() when there was anything to drop.
    void clear();

signals:
    void changed();

private:
    QStringList m_entries;
};

/// The player facts the mini player and the full-player sheet depend on.
///
/// hasTrack is true while the shared player holds any item, playing is the
/// actual transport state, sheetOpen marks an open full-player sheet and
/// fullScreen a full-screen player covering the app.
struct MiniPlayerState {
    bool hasTrack = false;
    bool playing = false;
    bool sheetOpen = false;
    bool fullScreen = false;
};

/// Whether the mini player is shown: the player has a track and neither the
/// full-player sheet nor a full-screen player covers it.
[[nodiscard]] bool miniPlayerVisible(const MiniPlayerState& state);

/// Whether the mini player shows its expanded controls: playback is running
/// and the full-player sheet is closed. Visibility is not part of the rule,
/// because the bar is only asked for its expansion while it is visible.
[[nodiscard]] bool miniPlayerExpanded(const MiniPlayerState& state);

} // namespace beygram::telegram
