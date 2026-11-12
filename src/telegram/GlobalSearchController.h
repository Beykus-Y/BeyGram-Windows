#pragma once

#include <QObject>
#include <QString>

namespace beygram::telegram {

/// Drives one global-search session: the current query plus the pagination
/// cursor of its page list.
///
/// The controller owns no requests and no results. It only holds the state the
/// search panel and the request layer share: the trimmed query, the opaque
/// `next_offset` token of the last page, whether another page exists, and
/// whether a request is in flight. changed() is emitted only when one of those
/// values really changes, so a repaint bound to it stays cheap.
class GlobalSearchController final : public QObject {
    Q_OBJECT

public:
    explicit GlobalSearchController(QObject* parent = nullptr);

    /// Starts a new search. The query is trimmed; a query that is empty after
    /// trimming clears the controller instead. A new query always restarts
    /// pagination, and a request still in flight belongs to the previous query,
    /// so loading() drops back to false.
    void begin(const QString& query);

    [[nodiscard]] QString query() const;

    /// The `next_offset` token of the last page; empty for the first page and
    /// once the last page has been reached. TDLib's token is opaque, so it is
    /// stored exactly as given and never trimmed.
    [[nodiscard]] QString nextOffset() const;

    [[nodiscard]] bool hasMore() const;
    [[nodiscard]] bool loading() const;

    /// Records the cursor returned by one page. Ignored while no query is
    /// active, so a late response cannot revive a cleared search. Ending the
    /// in-flight request stays with the caller through setLoading(false).
    void setPage(QString nextOffset, bool hasMore);

    /// Marks whether a request is in flight.
    void setLoading(bool loading);

    /// Drops the query, the cursor and both flags.
    void clear();

signals:
    void changed();

private:
    QString m_query;
    QString m_offset;
    bool m_more = false;
    bool m_loading = false;
};

} // namespace beygram::telegram
