#include "SearchResultModel.h"

#include <utility>

namespace beygram::telegram {

SearchResultModel::SearchResultModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int SearchResultModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_hits.size();
}

QVariant SearchResultModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_hits.size())
        return {};
    const SearchHit& hit = m_hits.at(index.row());
    switch (role) {
    case ChatIdRole: return hit.chatId;
    case MessageIdRole: return hit.messageId;
    case DateRole: return hit.date;
    case ChatTitleRole: return hit.chatTitle;
    case SenderNameRole: return hit.senderName;
    case TextRole: return hit.text;
    case TextHtmlRole: return hit.textHtml;
    default: return {};
    }
}

QHash<int, QByteArray> SearchResultModel::roleNames() const
{
    return {
        {ChatIdRole, "chatId"},
        {MessageIdRole, "messageId"},
        {DateRole, "date"},
        {ChatTitleRole, "chatTitle"},
        {SenderNameRole, "senderName"},
        {TextRole, "text"},
        {TextHtmlRole, "textHtml"},
    };
}

SearchHit SearchResultModel::hitAt(int row) const
{
    if (row < 0 || row >= m_hits.size())
        return {};
    return m_hits.at(row);
}

void SearchResultModel::setHits(QVector<SearchHit> hits)
{
    beginResetModel();
    m_hits = std::move(hits);
    endResetModel();
    emit countChanged();
}

void SearchResultModel::clear()
{
    if (m_hits.isEmpty())
        return;
    beginResetModel();
    m_hits.clear();
    endResetModel();
    emit countChanged();
}

} // namespace beygram::telegram
