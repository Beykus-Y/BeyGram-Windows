#include "ChatListModel.h"

namespace beygram::telegram {

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
        m_chats[row].photoReady = true;
        ++m_chats[row].photoRevision;
        const QModelIndex changed = index(row);
        emit dataChanged(changed, changed, {PhotoReadyRole, PhotoRevisionRole});
    }
}

} // namespace beygram::telegram
