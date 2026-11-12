#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::telegram {
class GlobalSearchController;
class SearchHistoryStore;
}

namespace beygram::ui {

/// One QML-facing object for the global search bar.
///
/// It owns a SearchHistoryStore (recent queries) and a GlobalSearchController
/// (query, pagination cursor, in-flight flag) and re-exposes them as a single
/// set of properties, so QML needs one context property instead of two C++
/// objects. The facade adds only what neither child knows about: the result
/// counts behind summary(), the suggestion list derived from the query, and
/// the searchRequested() signal the request layer listens to. It never talks to
/// TDLib, never logs and never persists anything; the history lives in memory
/// for as long as this object does.
///
/// begin() is the submit path, not the per-keystroke path. It trims the query
/// exactly like the controller, records the trimmed text in the history, and
/// emits searchRequested() when something remains. A blank query clears the
/// search but is never remembered. Re-submitting the query that is already
/// active keeps the page on screen and only re-announces the request, so the
/// user can retry without losing the current results.
///
/// suggestions() are the history entries that start with the current query,
/// case-insensitively, newest first. Without a query there is nothing to
/// complete, so the list is empty and the recent list (history()) is what the
/// bar shows.
///
/// summary() is always "N chats, M messages, K files"; the nouns stay plural
/// for every count so the line keeps a fixed shape. The page owner reports
/// counts through setResultCounts(), and they drop to zero whenever the query
/// changes or clear() runs, so a summary can never describe an older search.
/// Negative counts are stored as zero.
///
/// changed() is emitted only when the visible state really differs. One begin()
/// or clear() that touches both children is still announced once, and setters
/// that receive the current value stay silent.
class SearchFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString query READ query NOTIFY changed)
    Q_PROPERTY(QStringList history READ history NOTIFY changed)
    Q_PROPERTY(QStringList suggestions READ suggestions NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)

public:
    explicit SearchFacade(QObject* parent = nullptr);

    /// The trimmed query of the active search, empty when none is running.
    [[nodiscard]] QString query() const;

    /// The recent queries, newest first, without duplicates. At most 20.
    [[nodiscard]] QStringList history() const;

    /// History entries that start with query(), newest first; empty without a
    /// query, because an untouched field shows history() instead.
    [[nodiscard]] QStringList suggestions() const;

    /// True while a request for the active query is in flight.
    [[nodiscard]] bool loading() const;

    /// True while another page exists for the active query.
    [[nodiscard]] bool hasMore() const;

    /// "N chats, M messages, K files" for the last reported result counts.
    [[nodiscard]] QString summary() const;

    /// Starts (or restarts) a search. The query is trimmed; a blank one clears
    /// the search and is not recorded. A non-empty query is remembered, then
    /// announced through searchRequested().
    Q_INVOKABLE void begin(const QString& query);

    /// Sets the in-flight flag. A repeat of the current value is silent.
    Q_INVOKABLE void setLoading(bool loading);

    /// Records the cursor of the last page. Ignored while no query is active,
    /// so a late response cannot revive a cleared search.
    Q_INVOKABLE void setPage(const QString& nextOffset, bool hasMore);

    /// Drops the query, the cursor, both flags and the result counts. The
    /// history is untouched; use clearHistory() for that.
    Q_INVOKABLE void clear();

    /// Records a query in the history without starting a search. The store
    /// trims it, ignores blanks and moves a repeat to the front.
    Q_INVOKABLE void remember(const QString& query);

    /// Forgets one history entry, matching case-insensitively.
    Q_INVOKABLE void removeHistory(const QString& query);

    /// Forgets the whole history. The active search is untouched.
    Q_INVOKABLE void clearHistory();

    /// Stores the counts behind summary(). Negative values are stored as zero.
    Q_INVOKABLE void setResultCounts(int chats, int messages, int files);

signals:
    /// Emitted whenever any observable state of the facade changed.
    void changed();

    /// Emitted by begin() for a non-empty query, for the owner to run the
    /// TDLib search. Emitted after the state has been updated.
    void searchRequested(const QString& query);

private:
    /// The state changed() is diffed against, so one action that touches both
    /// children still repaints the bar only once.
    struct VisibleState {
        QString query;
        QStringList history;
        QStringList suggestions;
        bool loading = false;
        bool hasMore = false;
        int chats = 0;
        int messages = 0;
        int files = 0;

        bool operator==(const VisibleState&) const = default;
    };

    [[nodiscard]] VisibleState visibleState() const;

    /// Emits changed() only when the visible state differs from the last one
    /// announced.
    void syncChanged();

    /// Zeroes the three counts without signalling.
    void forgetResultCounts();

    telegram::SearchHistoryStore* m_history = nullptr;
    telegram::GlobalSearchController* m_search = nullptr;
    int m_chatCount = 0;
    int m_messageCount = 0;
    int m_fileCount = 0;
    VisibleState m_visible;
};

} // namespace beygram::ui
