#include "EditHistoryModel.h"

#include <algorithm>

namespace beygram::telegram {

EditHistoryModel::EditHistoryModel(QObject* parent)
    : QObject(parent)
{
}

void EditHistoryModel::record(qint64 messageId, const QString& text, qint64 editedAt, int maxVersions)
{
    // The id names the message and the timestamp places the edit; without
    // either the version could never be read back or ordered, so drop it.
    if (messageId <= 0 || editedAt <= 0)
        return;

    const auto existing = m_history.find(messageId);
    if (existing != m_history.end() && !existing->isEmpty()
        && existing->constLast().text == text) {
        // TDLib repeats the current text on some updates; it is not a new edit.
        return;
    }

    QVector<EditVersion>& history =
        existing != m_history.end()
            ? *existing
            : *m_history.insert(messageId, QVector<EditVersion>());
    history.append(EditVersion{text, editedAt});

    // Keep the newest versions: history is read from its ends, and the oldest
    // text is the least useful once the cap is reached.
    const int limit = std::max(1, maxVersions);
    if (history.size() > limit)
        history.remove(0, history.size() - limit);

    emit changed();
}

QVector<EditVersion> EditHistoryModel::versions(qint64 messageId) const
{
    if (messageId <= 0)
        return {};
    return m_history.value(messageId);
}

QString EditHistoryModel::originalText(qint64 messageId) const
{
    const auto it = m_history.constFind(messageId);
    return it != m_history.cend() && !it->isEmpty() ? it->constFirst().text : QString();
}

QString EditHistoryModel::latestText(qint64 messageId) const
{
    const auto it = m_history.constFind(messageId);
    return it != m_history.cend() && !it->isEmpty() ? it->constLast().text : QString();
}

int EditHistoryModel::versionCount(qint64 messageId) const
{
    const auto it = m_history.constFind(messageId);
    return it != m_history.cend() ? static_cast<int>(it->size()) : 0;
}

void EditHistoryModel::clearMessage(qint64 messageId)
{
    const auto it = m_history.find(messageId);
    if (it == m_history.end())
        return;

    m_history.erase(it);
    emit changed();
}

void EditHistoryModel::clear()
{
    if (m_history.isEmpty())
        return;

    m_history.clear();
    emit changed();
}

} // namespace beygram::telegram
