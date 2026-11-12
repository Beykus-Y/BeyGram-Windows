#include "ChatFilterEngine.h"

#include <QSet>

namespace beygram::telegram {
namespace {

bool sameQuery(const ChatFilterQuery& left, const ChatFilterQuery& right)
{
    return left.text == right.text && left.unreadOnly == right.unreadOnly
        && left.mutedOnly == right.mutedOnly && left.pinnedOnly == right.pinnedOnly
        && left.folderId == right.folderId;
}

// Membership is a set, but keeping the first occurrence makes the stored order
// deterministic and a repeated call with the same chats a no-op.
QVector<qint64> uniqueChatIds(const QVector<qint64>& chatIds)
{
    QVector<qint64> result;
    result.reserve(chatIds.size());
    QSet<qint64> seen;
    seen.reserve(chatIds.size());

    for (qint64 chatId : chatIds) {
        if (seen.contains(chatId))
            continue;

        seen.insert(chatId);
        result.append(chatId);
    }

    return result;
}

bool matchesQuery(const ChatData& chat, const ChatFilterQuery& query,
                  const QHash<qint64, QVector<qint64>>& members)
{
    if (!query.text.isEmpty()) {
        const bool inTitle = chat.title.contains(query.text, Qt::CaseInsensitive);
        const bool inPreview = chat.preview.contains(query.text, Qt::CaseInsensitive);
        if (!inTitle && !inPreview)
            return false;
    }

    if (query.unreadOnly && chat.unreadCount <= 0)
        return false;
    if (query.mutedOnly && !chat.muted)
        return false;
    if (query.pinnedOnly && !chat.pinned)
        return false;

    // Folder 0 is the whole list. An unknown folder has no members, so it
    // matches nothing rather than falling back to every chat.
    if (query.folderId != 0 && !members.value(query.folderId).contains(chat.id))
        return false;

    return true;
}

} // namespace

ChatFilterEngine::ChatFilterEngine(QObject* parent)
    : QObject(parent)
{
}

void ChatFilterEngine::setQuery(const ChatFilterQuery& query)
{
    if (sameQuery(m_query, query))
        return;

    m_query = query;
    emit changed();
}

ChatFilterQuery ChatFilterEngine::query() const
{
    return m_query;
}

void ChatFilterEngine::setFolderMembers(qint64 folderId, const QVector<qint64>& chatIds)
{
    const QVector<qint64> ids = uniqueChatIds(chatIds);
    if (m_members.value(folderId) == ids)
        return;

    if (ids.isEmpty())
        m_members.remove(folderId);
    else
        m_members.insert(folderId, ids);

    emit changed();
}

QVector<ChatData> ChatFilterEngine::apply(const QVector<ChatData>& chats) const
{
    QVector<ChatData> result;
    result.reserve(chats.size());

    for (const ChatData& chat : chats) {
        if (matchesQuery(chat, m_query, m_members))
            result.append(chat);
    }

    return result;
}

int ChatFilterEngine::matchCount(const QVector<ChatData>& chats) const
{
    int count = 0;
    for (const ChatData& chat : chats) {
        if (matchesQuery(chat, m_query, m_members))
            ++count;
    }

    return count;
}

void ChatFilterEngine::clear()
{
    const bool queryWasSet = !sameQuery(m_query, ChatFilterQuery{});
    if (!queryWasSet && m_members.isEmpty())
        return;

    m_query = ChatFilterQuery{};
    m_members.clear();
    emit changed();
}

} // namespace beygram::telegram
