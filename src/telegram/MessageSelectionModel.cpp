#include "MessageSelectionModel.h"

#include <algorithm>
#include <utility>

namespace beygram::telegram {

MessageSelectionModel::MessageSelectionModel(QObject* parent) : QObject(parent)
{
}

int MessageSelectionModel::count() const
{
    return m_selected.size();
}

bool MessageSelectionModel::isActive() const
{
    return !m_selected.isEmpty();
}

QVector<qint64> MessageSelectionModel::selectedIds() const
{
    QVector<qint64> ids;
    ids.reserve(m_selected.size());
    for (qint64 id : m_selected)
        ids.append(id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool MessageSelectionModel::contains(qint64 messageId) const
{
    return m_selected.contains(messageId);
}

void MessageSelectionModel::toggle(qint64 messageId)
{
    if (messageId <= 0)
        return;

    if (m_selected.remove(messageId)) {
        emit selectionChanged();
        return;
    }

    m_selected.insert(messageId);
    emit selectionChanged();
}

void MessageSelectionModel::select(qint64 messageId)
{
    if (messageId <= 0 || m_selected.contains(messageId))
        return;

    m_selected.insert(messageId);
    emit selectionChanged();
}

void MessageSelectionModel::clear()
{
    if (m_selected.isEmpty())
        return;

    m_selected.clear();
    emit selectionChanged();
}

void MessageSelectionModel::selectAll(const QVector<qint64>& messageIds)
{
    QSet<qint64> selected;
    selected.reserve(messageIds.size());
    for (qint64 id : messageIds) {
        if (id > 0)
            selected.insert(id);
    }

    if (selected == m_selected)
        return;

    m_selected = std::move(selected);
    emit selectionChanged();
}

} // namespace beygram::telegram
