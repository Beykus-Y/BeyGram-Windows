#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::telegram {

/// Remembers the queries a user typed into search, newest first.
///
/// The list is ordered by recency: the entry at index 0 is what the search bar
/// offers as "last search". Repeating a query does not create a second row, it
/// moves the existing one back to the front, so the history reads like a
/// most-recently-used list rather than an append-only log.
///
/// Comparisons ignore case and surrounding whitespace, because "Alice" and
/// "alice " are the same search to the person typing. The stored text is the
/// trimmed query from the latest add(), so the casing the user just typed wins.
///
/// Nothing here is persisted; the owner decides whether the history survives a
/// restart. changed() is emitted only when the visible list really changes.
class SearchHistoryStore final : public QObject {
    Q_OBJECT

public:
    explicit SearchHistoryStore(QObject* parent = nullptr);

    /// Records one submitted query at the front of the history.
    ///
    /// The query is trimmed; an empty result is ignored and does not signal.
    /// A query already present (case-insensitively) is moved to the front with
    /// its new text instead of being duplicated. `maxEntries` also becomes the
    /// cap for this call and later ones; it is clamped to at least 1, so the
    /// newest query always survives.
    void add(const QString& query, int maxEntries = 20);

    /// The history, newest first. A copy, so callers cannot reorder the store.
    [[nodiscard]] QStringList entries() const;

    [[nodiscard]] int count() const;

    /// Forgets one query, matching case-insensitively. Returns true when the
    /// entry was there, i.e. when changed() was emitted.
    bool remove(const QString& query);

    /// Entries that start with `prefix`, case-insensitively, in history order.
    /// An empty prefix returns everything, which is what an untouched search
    /// field wants to show.
    [[nodiscard]] QStringList matching(const QString& prefix) const;

    /// Forgets the whole history. A no-op on an already empty store.
    void clear();

signals:
    void changed();

private:
    /// Index of `query` in m_entries, compared case-insensitively, or -1.
    [[nodiscard]] int indexOf(const QString& query) const;

    QStringList m_entries;
    int m_max = 20;
};

} // namespace beygram::telegram
