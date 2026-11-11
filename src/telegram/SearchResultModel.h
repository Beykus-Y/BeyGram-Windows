#pragma once

#include "MessageSearch.h"

#include <QAbstractListModel>
#include <QVector>

namespace beygram::telegram {

/// Flat list of message-search hits for the QML search panel.
class SearchResultModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

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

    explicit SearchResultModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int count() const { return m_hits.size(); }
    [[nodiscard]] SearchHit hitAt(int row) const;

    void setHits(QVector<SearchHit> hits);
    void clear();

signals:
    void countChanged();

private:
    QVector<SearchHit> m_hits;
};

} // namespace beygram::telegram
