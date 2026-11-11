#include "ChatFolderListModel.h"

#include <QSet>

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

constexpr qint64 kMainFolderId = 0;

QString mainFolderTitle()
{
    return QStringLiteral("All chats");
}

QString fallbackTitle()
{
    return QStringLiteral("Folder");
}

// Builds the display order: main list first, custom folders in parser order,
// sorted by position only when the parser order is not already ascending.
QVector<ChatFolderInfo> normalizedFolders(const ChatFoldersState& state)
{
    ChatFolderInfo main;
    bool mainFound = false;
    QVector<ChatFolderInfo> customFolders;
    customFolders.reserve(state.folders.size());
    QSet<qint64> seenIds;

    for (const ChatFolderInfo& folder : state.folders) {
        if (folder.isMain || folder.id == kMainFolderId) {
            if (!mainFound) {
                main = folder;
                mainFound = true;
            }
            continue;
        }
        if (seenIds.contains(folder.id))
            continue;
        seenIds.insert(folder.id);
        customFolders.append(folder);
    }

    if (!mainFound) {
        main.id = kMainFolderId;
        main.isMain = true;
    }
    main.id = kMainFolderId;
    main.isMain = true;
    if (main.title.isEmpty())
        main.title = mainFolderTitle();

    bool needsSort = false;
    for (int row = 1; row < customFolders.size(); ++row) {
        if (customFolders.at(row).position < customFolders.at(row - 1).position) {
            needsSort = true;
            break;
        }
    }
    if (needsSort) {
        std::stable_sort(customFolders.begin(), customFolders.end(),
                         [](const ChatFolderInfo& left, const ChatFolderInfo& right) {
                             return left.position < right.position;
                         });
    }

    QVector<ChatFolderInfo> folders;
    folders.reserve(customFolders.size() + 1);
    folders.append(main);
    folders += customFolders;
    return folders;
}

bool sameFields(const ChatFolderInfo& left, const ChatFolderInfo& right)
{
    return left.id == right.id
        && left.title == right.title
        && left.icon == right.icon
        && left.unreadCount == right.unreadCount
        && left.isMain == right.isMain;
}

} // namespace

ChatFolderListModel::ChatFolderListModel(QObject* parent) : QAbstractListModel(parent)
{
}

int ChatFolderListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_folders.size();
}

QVariant ChatFolderListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_folders.size())
        return {};

    const ChatFolderInfo& folder = m_folders.at(index.row());
    switch (role) {
    case FolderIdRole: return folder.id;
    case TitleRole:
        if (!folder.title.isEmpty())
            return folder.title;
        return folder.id == kMainFolderId ? mainFolderTitle() : fallbackTitle();
    case IconRole: return folder.icon;
    case UnreadCountRole: return folder.unreadCount;
    case IsMainRole: return folder.isMain || folder.id == kMainFolderId;
    default: return {};
    }
}

QHash<int, QByteArray> ChatFolderListModel::roleNames() const
{
    return {
        {FolderIdRole, "folderId"},
        {TitleRole, "title"},
        {IconRole, "icon"},
        {UnreadCountRole, "unreadCount"},
        {IsMainRole, "isMain"},
    };
}

void ChatFolderListModel::setState(const ChatFoldersState& state)
{
    QVector<ChatFolderInfo> folders = normalizedFolders(state);

    if (folders.size() == m_folders.size()) {
        bool same = true;
        for (int row = 0; row < folders.size(); ++row) {
            if (!sameFields(m_folders.at(row), folders.at(row))) {
                same = false;
                break;
            }
        }
        if (same)
            return;
    }

    const int previousCount = m_folders.size();
    beginResetModel();
    m_folders = std::move(folders);
    endResetModel();
    if (m_folders.size() != previousCount)
        emit countChanged();
}

void ChatFolderListModel::clear()
{
    if (m_folders.isEmpty())
        return;
    beginResetModel();
    m_folders.clear();
    endResetModel();
    emit countChanged();
}

qint64 ChatFolderListModel::folderIdAt(int row) const
{
    if (row < 0 || row >= m_folders.size())
        return 0;
    return m_folders.at(row).id;
}

} // namespace beygram::telegram
