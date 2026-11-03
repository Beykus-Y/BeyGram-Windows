#include "ChatListModel.h"

namespace beygram::telegram {
namespace {

bool sameChatFields(const ChatData& left, const ChatData& right)
{
    return left.id == right.id
        && left.title == right.title
        && left.type == right.type
        && left.preview == right.preview
        && left.order == right.order
        && left.lastMessageDate == right.lastMessageDate
        && left.lastMessageId == right.lastMessageId
        && left.unreadCount == right.unreadCount
        && left.pinned == right.pinned
        && left.muted == right.muted
        && left.inMainList == right.inMainList
        && left.isChannel == right.isChannel
        && left.photoFileId == right.photoFileId
        && left.photoReady == right.photoReady
        && left.photoRevision == right.photoRevision
        && left.initials == right.initials
        && left.accentIndex == right.accentIndex;
}

} // namespace

ChatListModel::ChatListModel(QObject* parent) : QAbstractListModel(parent)
{
}

int ChatListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_chats.size();
}

QVariant ChatListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_chats.size())
        return {};

    const ChatData& chat = m_chats.at(index.row());
    switch (role) {
    case ChatIdRole: return chat.id;
    case TitleRole: return chat.title;
    case PreviewRole: return chat.preview;
    case UnreadCountRole: return chat.unreadCount;
    case PinnedRole: return chat.pinned;
    case MutedRole: return chat.muted;
    case LastDateRole: return chat.lastMessageDate;
    case InitialsRole: return chat.initials;
    case AccentIndexRole: return chat.accentIndex;
    case IsChannelRole: return chat.isChannel;
    case ChatTypeRole: return chat.type;
    case PhotoFileIdRole: return chat.photoFileId;
    case PhotoReadyRole: return chat.photoReady;
    case PhotoRevisionRole: return chat.photoRevision;
    default: return {};
    }
}

QHash<int, QByteArray> ChatListModel::roleNames() const
{
    return {
        {ChatIdRole, "chatId"},
        {TitleRole, "title"},
        {PreviewRole, "preview"},
        {UnreadCountRole, "unreadCount"},
        {PinnedRole, "pinned"},
        {MutedRole, "muted"},
        {LastDateRole, "lastDate"},
        {InitialsRole, "initials"},
        {AccentIndexRole, "accentIndex"},
        {IsChannelRole, "isChannel"},
        {ChatTypeRole, "chatType"},
        {PhotoFileIdRole, "photoFileId"},
        {PhotoReadyRole, "photoReady"},
        {PhotoRevisionRole, "photoRevision"},
    };
}

void ChatListModel::setChats(QVector<ChatData> chats)
{
    // Most updates only change a preview or an unread counter. When the id
    // order is unchanged, update those rows in place instead of resetting the
    // whole ListView (which destroys and recreates every delegate).
    if (!m_chats.isEmpty() && m_chats.size() == chats.size()) {
        bool sameOrder = true;
        for (int row = 0; row < chats.size(); ++row) {
            if (m_chats.at(row).id != chats.at(row).id) {
                sameOrder = false;
                break;
            }
        }
        if (sameOrder) {
            int firstChanged = -1;
            int lastChanged = -1;
            for (int row = 0; row < chats.size(); ++row) {
                if (sameChatFields(m_chats.at(row), chats.at(row)))
                    continue;
                m_chats[row] = chats.at(row);
                if (firstChanged < 0)
                    firstChanged = row;
                lastChanged = row;
            }
            if (firstChanged >= 0) {
                const QModelIndex first = index(firstChanged);
                const QModelIndex last = index(lastChanged);
                emit dataChanged(first, last);
            }
            return;
        }
    }

    beginResetModel();
    m_chats = std::move(chats);
    endResetModel();
    emit countChanged();
}

void ChatListModel::clear()
{
    if (m_chats.isEmpty())
        return;
    beginResetModel();
    m_chats.clear();
    endResetModel();
    emit countChanged();
}

qint64 ChatListModel::chatIdAt(int row) const
{
    if (row < 0 || row >= m_chats.size())
        return 0;
    return m_chats.at(row).id;
}

void ChatListModel::markPhotoReady(int fileId)
{
    if (fileId == 0)
        return;
    for (int row = 0; row < m_chats.size(); ++row) {
        if (m_chats[row].photoFileId != fileId)
            continue;
        if (m_chats[row].photoReady)
            continue;
        m_chats[row].photoReady = true;
        ++m_chats[row].photoRevision;
        const QModelIndex changed = index(row);
        emit dataChanged(changed, changed, {PhotoReadyRole, PhotoRevisionRole});
    }
}

} // namespace beygram::telegram
