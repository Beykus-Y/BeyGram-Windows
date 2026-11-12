#include "ForwardTargetModel.h"

#include <QSet>

#include <utility>

namespace beygram::telegram {

namespace {

/// Positive and unique, first occurrence wins, input order preserved.
QVector<qint64> usableIds(const QVector<qint64>& ids)
{
    QVector<qint64> result;
    result.reserve(ids.size());
    QSet<qint64> seen;
    seen.reserve(ids.size());
    for (const qint64 id : ids) {
        if (id <= 0 || seen.contains(id))
            continue;
        seen.insert(id);
        result.append(id);
    }
    return result;
}

bool containsChatId(const QVector<ChatData>& chats, qint64 chatId)
{
    for (const ChatData& chat : chats) {
        if (chat.id == chatId)
            return true;
    }
    return false;
}

bool sameChats(const QVector<ChatData>& lhs, const QVector<ChatData>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;
    for (int i = 0; i < lhs.size(); ++i) {
        const ChatData& a = lhs.at(i);
        const ChatData& b = rhs.at(i);
        if (a.id != b.id || a.title != b.title || a.type != b.type
            || a.preview != b.preview || a.order != b.order
            || a.lastMessageDate != b.lastMessageDate
            || a.lastMessageId != b.lastMessageId
            || a.unreadCount != b.unreadCount || a.pinned != b.pinned
            || a.muted != b.muted || a.inMainList != b.inMainList
            || a.archived != b.archived || a.archiveOrder != b.archiveOrder
            || a.isChannel != b.isChannel || a.photoFileId != b.photoFileId
            || a.photoReady != b.photoReady || a.photoRevision != b.photoRevision
            || a.initials != b.initials || a.accentIndex != b.accentIndex) {
            return false;
        }
    }
    return true;
}

} // namespace

ForwardTargetModel::ForwardTargetModel(QObject* parent)
    : QObject(parent)
{
}

void ForwardTargetModel::open(QVector<qint64> messageIds)
{
    const QVector<qint64> ids = usableIds(messageIds);
    if (ids.isEmpty())
        return;

    const bool idsChanged = ids != m_ids;
    const bool selectionChanged = m_selected != 0;
    if (!idsChanged && !selectionChanged)
        return;

    m_ids = ids;
    m_selected = 0;
    emit changed();
}

void ForwardTargetModel::close()
{
    if (!active())
        return;

    m_ids.clear();
    m_selected = 0;
    emit changed();
}

bool ForwardTargetModel::active() const
{
    return !m_ids.isEmpty();
}

QVector<qint64> ForwardTargetModel::messageIds() const
{
    return m_ids;
}

void ForwardTargetModel::setChats(QVector<ChatData> chats)
{
    QVector<ChatData> usable;
    usable.reserve(chats.size());
    QSet<qint64> seen;
    seen.reserve(chats.size());
    for (const ChatData& chat : chats) {
        if (chat.id <= 0 || seen.contains(chat.id))
            continue;
        seen.insert(chat.id);
        usable.append(chat);
    }

    const bool selectionChanged =
        m_selected != 0 && !containsChatId(usable, m_selected);
    if (sameChats(usable, m_chats) && !selectionChanged)
        return;

    m_chats = std::move(usable);
    if (selectionChanged)
        m_selected = 0;
    emit changed();
}

QVector<ChatData> ForwardTargetModel::chats() const
{
    return m_chats;
}

void ForwardTargetModel::setFilter(const QString& filter)
{
    if (filter == m_filter)
        return;

    m_filter = filter;
    emit changed();
}

QString ForwardTargetModel::filter() const
{
    return m_filter;
}

QVector<ChatData> ForwardTargetModel::filteredChats() const
{
    if (m_filter.isEmpty())
        return m_chats;

    QVector<ChatData> result;
    result.reserve(m_chats.size());
    for (const ChatData& chat : m_chats) {
        if (chat.title.contains(m_filter, Qt::CaseInsensitive))
            result.append(chat);
    }
    return result;
}

void ForwardTargetModel::setSelectedChat(qint64 chatId)
{
    if (chatId <= 0) {
        if (m_selected == 0)
            return;
        m_selected = 0;
        emit changed();
        return;
    }

    if (m_selected == chatId || !containsChatId(m_chats, chatId))
        return;

    m_selected = chatId;
    emit changed();
}

qint64 ForwardTargetModel::selectedChatId() const
{
    return m_selected;
}

bool ForwardTargetModel::canSend() const
{
    return active() && m_selected > 0 && !messageIds().isEmpty();
}

void ForwardTargetModel::reset()
{
    if (m_ids.isEmpty() && m_chats.isEmpty() && m_filter.isEmpty()
        && m_selected == 0) {
        return;
    }

    m_ids.clear();
    m_chats.clear();
    m_filter.clear();
    m_selected = 0;
    emit changed();
}

} // namespace beygram::telegram
