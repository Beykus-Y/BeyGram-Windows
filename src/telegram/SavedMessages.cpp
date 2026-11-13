#include "SavedMessages.h"

#include "ForwardMessages.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <utility>

namespace beygram::telegram {

namespace {

bool sameItem(const SavedMessage& lhs, const SavedMessage& rhs)
{
    return lhs.chatId == rhs.chatId && lhs.messageId == rhs.messageId
        && lhs.savedAt == rhs.savedAt && lhs.preview == rhs.preview;
}

bool sameItems(const QVector<SavedMessage>& lhs, const QVector<SavedMessage>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    for (qsizetype index = 0; index < lhs.size(); ++index) {
        if (!sameItem(lhs.at(index), rhs.at(index)))
            return false;
    }

    return true;
}

/// Newest first by savedAt, then by messageId. Entries that tie on both are
/// left in their arrival order, which stable_sort preserves.
bool newerFirst(const SavedMessage& lhs, const SavedMessage& rhs)
{
    if (lhs.savedAt != rhs.savedAt)
        return lhs.savedAt > rhs.savedAt;
    return lhs.messageId > rhs.messageId;
}

/// Drops entries TDLib cannot address, keeps the first copy of every
/// (chatId, messageId) pair and sorts the rest newest first.
QVector<SavedMessage> normalised(QVector<SavedMessage> items)
{
    QVector<SavedMessage> result;
    result.reserve(items.size());

    QHash<qint64, QSet<qint64>> seen;
    seen.reserve(items.size());

    for (const SavedMessage& item : items) {
        if (item.chatId <= 0 || item.messageId <= 0)
            continue;

        QSet<qint64>& messageIds = seen[item.chatId];
        if (messageIds.contains(item.messageId))
            continue;

        messageIds.insert(item.messageId);
        result.append(item);
    }

    std::stable_sort(result.begin(), result.end(), newerFirst);
    return result;
}

} // namespace

QJsonObject forwardToSavedMessagesRequest(qint64 fromChatId,
                                         const QVector<qint64>& messageIds)
{
    // Saved Messages is a chat like any other, so the request is a plain
    // forwardMessages. The target id is not part of this signature: 0 is the
    // placeholder the caller replaces with the current user id before sending.
    return forwardMessagesRequest(0, fromChatId, messageIds);
}

QJsonObject getSavedMessagesChatRequest()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getOption")},
        {QStringLiteral("name"), QStringLiteral("my_id")},
    };
}

SavedMessagesModel::SavedMessagesModel(QObject* parent)
    : QObject(parent)
{
}

void SavedMessagesModel::setItems(QVector<SavedMessage> items)
{
    QVector<SavedMessage> next = normalised(std::move(items));
    if (sameItems(next, m_items))
        return;

    m_items = std::move(next);
    emit changed();
}

void SavedMessagesModel::append(QVector<SavedMessage> items)
{
    const QVector<SavedMessage> incoming = normalised(std::move(items));
    if (incoming.isEmpty())
        return;

    QHash<qint64, QSet<qint64>> stored;
    stored.reserve(m_items.size() + incoming.size());
    for (const SavedMessage& item : m_items)
        stored[item.chatId].insert(item.messageId);

    bool added = false;
    for (const SavedMessage& item : incoming) {
        QSet<qint64>& messageIds = stored[item.chatId];
        if (messageIds.contains(item.messageId))
            continue;

        messageIds.insert(item.messageId);
        m_items.append(item);
        added = true;
    }

    if (!added)
        return;

    std::stable_sort(m_items.begin(), m_items.end(), newerFirst);
    emit changed();
}

QVector<SavedMessage> SavedMessagesModel::items() const
{
    return m_items;
}

int SavedMessagesModel::count() const
{
    return static_cast<int>(m_items.size());
}

bool SavedMessagesModel::remove(qint64 chatId, qint64 messageId)
{
    for (qsizetype index = 0; index < m_items.size(); ++index) {
        const SavedMessage& item = m_items.at(index);
        if (item.chatId != chatId || item.messageId != messageId)
            continue;

        m_items.removeAt(index);
        emit changed();
        return true;
    }

    return false;
}

void SavedMessagesModel::clear()
{
    if (m_items.isEmpty())
        return;

    m_items.clear();
    emit changed();
}

} // namespace beygram::telegram
