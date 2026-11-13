#pragma once

#include "telegram/music/MusicModels.h"
#include "telegram/music/MusicProviders.h"
#include "telegram/music/MusicSearchHistory.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace beygram::ui {

/// QML-facing facade for the music search page.
///
/// The facade owns the library snapshot, the recent-query history and the
/// provider links the page has accepted. Searching is local and synchronous:
/// search() runs MusicSearchEngine over the snapshot handed to setLibrary() and
/// never talks to TDLib, the network or a player. A result row is a display
/// string in results(); resultIdAt() returns the track id behind the same row,
/// so a row click can announce playRequested() without the page parsing text.
///
/// setLibrary() pairs the five lists by index. An id is trimmed; an empty id
/// and a repeated id are dropped (the first entry wins), matching the library
/// store. A missing title, artist, album or duration becomes empty/zero, and a
/// duration is read as whole seconds clamped to 0..INT_MAX. Replacing the
/// library re-runs the active query, so results never describe old tracks.
///
/// search() trims the query. A non-empty query is recorded in the history
/// (newest first, case-insensitively unique, capped at 20) and searched; a
/// blank query clears the query and results but leaves the history alone.
/// clear() does the same and also drops the loading flag, which makes it the
/// page's cancel action. Neither call forgets history, waves or provider links.
///
/// addProviderLink() parses one link with parseMusicProviderLink(). Only a
/// supported, well-formed link is stored, once per provider/kind/id; an
/// invalid or repeated link is ignored silently. providerLinks() returns the
/// accepted links as display strings, for example
/// "Spotify · Track · 4uLU6hMCjMI75M1A2tKUQC".
///
/// setWaves() pairs ids and titles by index, trims both, drops an empty id and
/// keeps the first entry of a repeated id. waves() returns one display label
/// per wave: its title, or its id when the title is empty.
///
/// loading() is owner-driven through setLoading(): the facade never invents a
/// request, because searching the local library is instantaneous. changed() is
/// emitted only when query, results, history, loading, waves or provider links
/// actually differ.
class MusicSearchFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString query READ query NOTIFY changed)
    Q_PROPERTY(QStringList results READ results NOTIFY changed)
    Q_PROPERTY(QStringList history READ history NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)

public:
    explicit MusicSearchFacade(QObject* parent = nullptr);

    /// The trimmed query of the current search, empty when none is active.
    [[nodiscard]] QString query() const;

    /// Display rows for the current search, in library order. Each row is
    /// "title — artist · m:ss" with the artist and duration omitted when they
    /// are unknown; an untitled track falls back to its id.
    [[nodiscard]] QStringList results() const;

    /// The recent queries, newest first, without duplicates. At most 20.
    [[nodiscard]] QStringList history() const;

    /// True while the owner reports an external request in flight.
    [[nodiscard]] bool loading() const;

    /// Replaces the searchable library. The lists are paired by index; see the
    /// class comment for the normalization rules. An active query is re-run.
    Q_INVOKABLE void setLibrary(const QStringList& ids, const QStringList& titles,
                                const QStringList& artistIds, const QStringList& albumIds,
                                const QStringList& durations);

    /// Records and runs a query. A blank query clears the current search
    /// without recording anything.
    Q_INVOKABLE void search(const QString& query);

    /// Drops the query, results and loading flag. History, waves and provider
    /// links survive.
    Q_INVOKABLE void clear();

    /// Stores a supported music-service link once. Invalid and duplicate links
    /// are ignored.
    Q_INVOKABLE void addProviderLink(const QString& url);

    /// Display labels of the stored waves, in insertion order.
    Q_INVOKABLE QStringList waves() const;

    /// Replaces the waves. Ids and titles are paired by index; see the class
    /// comment for the normalization rules.
    Q_INVOKABLE void setWaves(const QStringList& ids, const QStringList& titles);

    /// The track id of the result row at \a index, or an empty string when the
    /// index is out of range. The row order is exactly results()'s order.
    Q_INVOKABLE QString resultIdAt(int index) const;

    /// Announces the track of the result row at \a index through
    /// playRequested(). An out-of-range index is ignored.
    Q_INVOKABLE void playResult(int index);

    /// Display strings of the accepted provider links, in insertion order.
    Q_INVOKABLE QStringList providerLinks() const;

    /// Sets the external-request flag. A repeat of the current value is silent.
    Q_INVOKABLE void setLoading(bool loading);

signals:
    /// Emitted whenever any observable state of the facade changed.
    void changed();

    /// Emitted with the track id of a result the user asked to play.
    void playRequested(const QString& trackId);

private:
    /// Every observable field, compared as a whole so one call emits changed()
    /// at most once and only when something visible really differs.
    struct Snapshot {
        QString query;
        QStringList results;
        QVector<QString> resultIds;
        QStringList history;
        bool loading = false;
        QStringList waves;
        QStringList providerLinks;

        bool operator==(const Snapshot&) const = default;
    };

    [[nodiscard]] Snapshot snapshot() const;

    /// Rebuilds results()/result ids for the active query from the current
    /// library snapshot. A blank query yields no rows.
    void rebuildResults();

    /// One display label per stored wave, with the id as a title fallback.
    [[nodiscard]] QStringList waveLabels() const;

    /// One display label per accepted provider link.
    [[nodiscard]] QStringList providerLinkLabels() const;

    /// Emits changed() when the current snapshot differs from \a before.
    void commit(const Snapshot& before);

    QVector<telegram::MusicTrackModel> m_tracks;
    QVector<QString> m_resultIds;
    QStringList m_results;
    QString m_query;
    bool m_loading = false;
    telegram::MusicSearchHistory m_history;
    QStringList m_waveIds;
    QStringList m_waveTitles;
    QVector<telegram::ProviderLink> m_providerLinks;
};

} // namespace beygram::ui
