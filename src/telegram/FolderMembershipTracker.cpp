#include "FolderMembershipTracker.h"

#include <QSet>

namespace beygram::telegram {
namespace {

// Folder 0 is the main list, positive ids are custom folders. Negative ids are
// not addressable chat lists, so they are dropped; duplicates keep the first
// occurrence and therefore the caller's order.
QVector<qint64> sanitizedFolders(const QVector<qint64>& folderIds)
{
    QVector<qint64> result;
    result.reserve(folderIds.size());
    QSet<qint64> seen;
    seen.reserve(folderIds.size());

    for (qint64 folderId : folderIds) {
        if (folderId < 0 || seen.contains(folderId))
            continue;

        seen.insert(folderId);
        result.append(folderId);
    }

    return result;
}

// Chat ids are TDLib's positive identifiers. Zero, negative values and repeats
// cannot name a chat, so they are skipped while the first occurrence keeps its
// position.
QVector<qint64> sanitizedChats(const QVector<qint64>& chatIds)
{
    QVector<qint64> result;
    result.reserve(chatIds.size());
    QSet<qint64> seen;
    seen.reserve(chatIds.size());

    for (qint64 chatId : chatIds) {
        if (chatId <= 0 || seen.contains(chatId))
            continue;

        seen.insert(chatId);
        result.append(chatId);
    }

    return result;
}

} // namespace

FolderMembershipTracker::FolderMembershipTracker(QObject* parent)
    : QObject(parent)
{
}

void FolderMembershipTracker::setFolders(const QVector<qint64>& folderIds)
{
    const QVector<qint64> folders = sanitizedFolders(folderIds);
    bool didChange = folders != m_folders;

    QHash<qint64, QVector<qint64>> members;
    members.reserve(m_members.size());
    for (auto it = m_members.cbegin(); it != m_members.cend(); ++it) {
        if (folders.contains(it.key())) {
            members.insert(it.key(), it.value());
        } else {
            // Membership in a folder that is no longer known is not observable
            // any more, so dropping it is a real change.
            didChange = true;
        }
    }

    m_folders = folders;
    m_members = members;

    if (didChange)
        emit changed();
}

QVector<qint64> FolderMembershipTracker::members(qint64 folderId) const
{
    if (!m_folders.contains(folderId))
        return {};

    return m_members.value(folderId);
}

void FolderMembershipTracker::setMembers(qint64 folderId, const QVector<qint64>& chatIds)
{
    if (!m_folders.contains(folderId))
        return;

    const QVector<qint64> chats = sanitizedChats(chatIds);
    if (m_members.value(folderId) == chats)
        return;

    if (chats.isEmpty())
        m_members.remove(folderId);
    else
        m_members.insert(folderId, chats);

    emit changed();
}

void FolderMembershipTracker::addChat(qint64 folderId, qint64 chatId)
{
    if (chatId <= 0 || !m_folders.contains(folderId))
        return;

    QVector<qint64>& chats = m_members[folderId];
    if (chats.contains(chatId))
        return;

    chats.append(chatId);
    emit changed();
}

void FolderMembershipTracker::removeChat(qint64 folderId, qint64 chatId)
{
    auto it = m_members.find(folderId);
    if (it == m_members.end())
        return;

    QVector<qint64>& chats = it.value();
    const qsizetype before = chats.size();
    chats.removeAll(chatId);
    if (chats.size() == before)
        return;

    if (chats.isEmpty())
        m_members.erase(it);

    emit changed();
}

void FolderMembershipTracker::removeChatEverywhere(qint64 chatId)
{
    if (chatId <= 0)
        return;

    bool didChange = false;
    for (auto it = m_members.begin(); it != m_members.end();) {
        QVector<qint64>& chats = it.value();
        const qsizetype before = chats.size();
        chats.removeAll(chatId);
        if (chats.size() == before) {
            ++it;
            continue;
        }

        didChange = true;
        if (chats.isEmpty())
            it = m_members.erase(it);
        else
            ++it;
    }

    if (didChange)
        emit changed();
}

QVector<qint64> FolderMembershipTracker::foldersForChat(qint64 chatId) const
{
    QVector<qint64> result;
    if (chatId <= 0)
        return result;

    result.reserve(m_folders.size());
    for (qint64 folderId : m_folders) {
        if (m_members.value(folderId).contains(chatId))
            result.append(folderId);
    }

    return result;
}

void FolderMembershipTracker::clear()
{
    if (m_folders.isEmpty() && m_members.isEmpty())
        return;

    m_folders.clear();
    m_members.clear();
    emit changed();
}

} // namespace beygram::telegram
