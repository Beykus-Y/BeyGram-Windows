#include "ChatFilterModel.h"

#include "ChatListModel.h"

namespace beygram::telegram {

ChatFilterModel::ChatFilterModel(QObject* parent) : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);

    const auto notifyCount = [this] { emit countChanged(); };
    connect(this, &QAbstractItemModel::rowsInserted, this, notifyCount);
    connect(this, &QAbstractItemModel::rowsRemoved, this, notifyCount);
    connect(this, &QAbstractItemModel::modelReset, this, notifyCount);
    connect(this, &QAbstractItemModel::layoutChanged, this, notifyCount);
}

void ChatFilterModel::setFilterText(const QString& text)
{
    if (m_filterText == text)
        return;
    m_filterText = text.trimmed();
    invalidateFilter();
    emit filterTextChanged();
    emit countChanged();
}

bool ChatFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (m_filterText.isEmpty())
        return true;
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    const QString title =
        index.data(ChatListModel::TitleRole).toString().toLower();
    const QString preview =
        index.data(ChatListModel::PreviewRole).toString().toLower();
    const QString needle = m_filterText.toLower();
    return title.contains(needle) || preview.contains(needle);
}

} // namespace beygram::telegram
