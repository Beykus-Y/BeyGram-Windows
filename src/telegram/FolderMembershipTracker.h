#pragma once

#include <QHash>
#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Tracks which chats belong to which chat lists without owning the lists.
///
/// Folder id 0 is the main chat list and positive ids are custom folders; chat
/// ids are always positive. Every mutator is idempotent and emits changed() only
/// when the stored folders or members really differ, so a view can rebuild from
/// one signal instead of diffing. Folders keep the order they were set in, and
/// foldersForChat() reports matches in that order.
class FolderMembershipTracker final : public QObject {
    Q_OBJECT

public:
    explicit FolderMembershipTracker(QObject* parent = nullptr);

    /// Replaces the known folders. Id 0 (main) and positive ids are accepted;
    /// duplicates and negative ids are dropped. Members of folders that are no
    /// longer known are discarded.
    void setFolders(const QVector<qint64>& folderIds);
    [[nodiscard]] QVector<qint64> folders() const { return m_folders; }

    /// Replaces the members of one known folder, keeping the given order.
    /// Non-positive and duplicate chat ids are dropped. A folder that is not
    /// known is ignored, so its members stay empty.
    void setMembers(qint64 folderId, const QVector<qint64>& chatIds);
    [[nodiscard]] QVector<qint64> members(qint64 folderId) const;

    /// Appends a chat to one known folder when it is not a member yet. Unknown
    /// folders and non-positive chat ids are ignored.
    void addChat(qint64 folderId, qint64 chatId);

    /// Removes a chat from one folder. Removing a chat that is not a member is
    /// harmless.
    void removeChat(qint64 folderId, qint64 chatId);

    /// Removes a chat from every folder that contains it, with a single change
    /// notification when anything was removed.
    void removeChatEverywhere(qint64 chatId);

    /// The folders that contain the chat, in folders() order.
    [[nodiscard]] QVector<qint64> foldersForChat(qint64 chatId) const;

    /// Forgets every folder and every member.
    void clear();

signals:
    /// Emitted once for each real change to the folders or their members.
    void changed();

private:
    QVector<qint64> m_folders;
    QHash<qint64, QVector<qint64>> m_members;
};

} // namespace beygram::telegram
