#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Drives one sticker-search session: the trimmed query, the sticker ids of the
/// current result page, and whether a request is in flight.
///
/// The controller owns no requests and no catalog data. Ids are
/// StickerInfo::stickerId values, the same ones StickerSetModel exposes through
/// StickerIdRole, so a result list can be fed straight into the existing sticker
/// views. changed() is emitted only when one of the stored values really
/// changes, so a repaint bound to it stays cheap.
class StickerSearchController final : public QObject {
    Q_OBJECT

public:
    explicit StickerSearchController(QObject* parent = nullptr);

    /// Starts a new search. The query is trimmed; a query that is empty after
    /// trimming clears the query, the results and the loading flag. A query
    /// that only differs in surrounding whitespace is a no-op and keeps the
    /// current results. A different query always drops the previous results,
    /// and a request still in flight belongs to the old query, so loading()
    /// falls back to false.
    void setQuery(const QString& query);

    [[nodiscard]] QString query() const;

    /// Replaces the result page. Ids that are not positive are dropped and
    /// repeated ids are kept only once, in first-seen order. Ignored while no
    /// query is active, so a late response cannot revive a cleared search.
    void setResults(QVector<qint64> stickerIds);

    [[nodiscard]] QVector<qint64> results() const;

    /// Marks whether a request is in flight.
    void setLoading(bool loading);

    [[nodiscard]] bool loading() const;

    /// Appends the next page. Invalid ids and ids already in the results are
    /// skipped, so a repeated or overlapping page adds nothing. Ignored while
    /// no query is active.
    void appendResults(QVector<qint64> stickerIds);

    /// Drops the query, the results and the loading flag.
    void clear();

signals:
    void changed();

private:
    QString m_query;
    QVector<qint64> m_ids;
    bool m_loading = false;
};

} // namespace beygram::telegram
