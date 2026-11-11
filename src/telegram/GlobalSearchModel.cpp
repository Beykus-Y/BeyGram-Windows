#include "GlobalSearchModel.h"

#include <QSet>
#include <utility>

namespace beygram::telegram {

GlobalSearchModel::GlobalSearchModel(QObject* parent) : QAbstractListModel(parent)
{
}

int GlobalSearchModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_hits.size();
}

QVariant GlobalSearchModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_hits.size())
        return {};

    const SearchHit& hit = m_hits.at(index.row());
    switch (role) {
    case ChatIdRole: return hit.chatId;
    case MessageIdRole: return hit.messageId;
    case DateRole: return hit.date;
    case ChatTitleRole:
        if (!hit.chatTitle.isEmpty())
            return hit.chatTitle;
        return m_titles.value(hit.chatId);
    case SenderNameRole: return hit.senderName;
    case TextRole: return hit.text;
    case TextHtmlRole: return hit.textHtml;
    default: return {};
    }
}

QHash<int, QByteArray> GlobalSearchModel::roleNames() const
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

void GlobalSearchModel::setHits(QVector<SearchHit> hits, bool hasMore)
{
    beginResetModel();
    m_hits = std::move(hits);
    endResetModel();
    emit countChanged();

    if (m_hasMore != hasMore) {
        m_hasMore = hasMore;
        emit stateChanged();
    }
}

void GlobalSearchModel::appendHits(const QVector<SearchHit>& hits, bool hasMore)
{
    // Both the rows already on screen and the rows inside this batch must be
    // de-duplicated: a page boundary can repeat a hit.
    QHash<qint64, QSet<qint64>> seen;
    seen.reserve(m_hits.size() + hits.size());
    for (const SearchHit& hit : m_hits)
        seen[hit.chatId].insert(hit.messageId);

    QVector<SearchHit> fresh;
    fresh.reserve(hits.size());
    for (const SearchHit& hit : hits) {
        QSet<qint64>& messageIds = seen[hit.chatId];
        if (messageIds.contains(hit.messageId))
            continue;
        messageIds.insert(hit.messageId);
        fresh.append(hit);
    }

    if (!fresh.isEmpty()) {
        const int firstNew = m_hits.size();
        beginInsertRows({}, firstNew, firstNew + fresh.size() - 1);
        m_hits += fresh;
        endInsertRows();
        emit countChanged();
    }

    if (m_hasMore != hasMore) {
        m_hasMore = hasMore;
        emit stateChanged();
    }
}

void GlobalSearchModel::setChatTitles(const QHash<qint64, QString>& titles)
{
    // Only rows that fall back to the cache can change, and only when the
    // cache value for their chat actually differs.
    int firstChanged = -1;
    int lastChanged = -1;
    for (int row = 0; row < m_hits.size(); ++row) {
        const SearchHit& hit = m_hits.at(row);
        if (!hit.chatTitle.isEmpty())
            continue;
        if (m_titles.value(hit.chatId) == titles.value(hit.chatId))
            continue;
        if (firstChanged < 0)
            firstChanged = row;
        lastChanged = row;
    }

    m_titles = titles;

    if (firstChanged >= 0) {
        emit dataChanged(index(firstChanged), index(lastChanged), {ChatTitleRole});
    }
}

void GlobalSearchModel::clear()
{
    if (m_hits.isEmpty() && !m_hasMore)
        return;

    if (!m_hits.isEmpty()) {
        beginResetModel();
        m_hits.clear();
        endResetModel();
        emit countChanged();
    }

    if (m_hasMore) {
        m_hasMore = false;
        emit stateChanged();
    }
}

} // namespace beygram::telegram
