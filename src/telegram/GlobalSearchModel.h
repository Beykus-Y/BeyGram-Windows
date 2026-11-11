#pragma once

#include "MessageSearch.h"

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// Flat list of global-search hits for the QML search panel.
///
/// Unlike SearchResultModel this model is paginated: the first page arrives
/// through setHits(), later pages through appendHits(), and hasMore() reports
/// whether another page exists. Chat titles that the response itself does not
/// carry are joined in from a caller-supplied chat cache with
/// setChatTitles(); they are never derived from identifiers.
class GlobalSearchModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY stateChanged)

public:
    enum Role {
        ChatIdRole = Qt::UserRole + 1,
        MessageIdRole,
        DateRole,
        ChatTitleRole,
        SenderNameRole,
        TextRole,
        TextHtmlRole,
    };
    Q_ENUM(Role)

    explicit GlobalSearchModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int count() const { return m_hits.size(); }
    [[nodiscard]] bool hasMore() const { return m_hasMore; }

    /// Replaces the whole list with the first page and sets the hasMore flag.
    void setHits(QVector<SearchHit> hits, bool hasMore);

    /// Appends the next page, skipping hits whose (chatId, messageId) pair is
    /// already present, and sets the hasMore flag.
    void appendHits(const QVector<SearchHit>& hits, bool hasMore);

    /// Replaces the chatId -> title cache used as a fallback for hits that do
    /// not carry their own chatTitle. Rows whose displayed title changes are
    /// announced with dataChanged(ChatTitleRole).
    void setChatTitles(const QHash<qint64, QString>& titles);

    void clear();

signals:
    void countChanged();
    void stateChanged();

private:
    QVector<SearchHit> m_hits;
    QHash<qint64, QString> m_titles;
    bool m_hasMore = false;
};

} // namespace beygram::telegram
