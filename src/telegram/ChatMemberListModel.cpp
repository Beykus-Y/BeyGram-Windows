#include "ChatMemberListModel.h"

#include <utility>

namespace beygram::telegram {
namespace {

// ChatMember carries no operator== (it is a wire-shaped value type), so the
// model compares the fields it actually shows. Every field is compared: a
// reload that only flips isBot or isOwner is still a real change for a row that
// renders a badge from those flags.
bool sameMember(const ChatMember& left, const ChatMember& right)
{
    return left.userId == right.userId
        && left.displayName == right.displayName
        && left.status == right.status
        && left.isAdmin == right.isAdmin
        && left.isOwner == right.isOwner
        && left.isBot == right.isBot;
}

bool sameMembers(const QVector<ChatMember>& left, const QVector<ChatMember>& right)
{
    if (left.size() != right.size())
        return false;
    for (qsizetype i = 0; i < left.size(); ++i) {
        if (!sameMember(left.at(i), right.at(i)))
            return false;
    }
    return true;
}

// -1 when no member has that userId. The list is small enough (one chat's
// members) that a linear scan beats keeping a second index in sync.
qsizetype indexOfUser(const QVector<ChatMember>& members, qint64 userId)
{
    for (qsizetype i = 0; i < members.size(); ++i) {
        if (members.at(i).userId == userId)
            return i;
    }
    return -1;
}

// A member without a positive userId cannot be looked up, updated or removed,
// so it is dropped when the list is set.
bool hasUser(const ChatMember& member)
{
    return member.userId > 0;
}

} // namespace

ChatMemberListModel::ChatMemberListModel(QObject* parent)
    : QObject(parent)
{
}

void ChatMemberListModel::setMembers(QVector<ChatMember> members)
{
    // Keep the first member of each userId: members arrive in the order the
    // caller merged its pages, and the first answer is the one already on
    // screen, so it wins over a later duplicate instead of making the visible
    // row depend on merge order.
    QVector<ChatMember> unique;
    unique.reserve(members.size());
    for (ChatMember& member : members) {
        if (!hasUser(member))
            continue;
        if (indexOfUser(unique, member.userId) >= 0)
            continue;
        unique.append(std::move(member));
    }

    // A reload that brings the same data is not a change: repainting the list
    // for it would also restart every animation bound to changed().
    if (sameMembers(unique, m_members))
        return;

    m_members = std::move(unique);
    emit changed();
}

QVector<ChatMember> ChatMemberListModel::members() const
{
    return m_members;
}

int ChatMemberListModel::count() const
{
    return static_cast<int>(m_members.size());
}

QVector<ChatMember> ChatMemberListModel::filtered(const QString& query) const
{
    // An empty search box means "everything", not "nothing".
    if (query.isEmpty())
        return m_members;

    QVector<ChatMember> matches;
    matches.reserve(m_members.size());
    for (const ChatMember& member : m_members) {
        // Status is searched too, so typing "admin" finds the administrators
        // without the caller having to know the role names up front.
        if (member.displayName.contains(query, Qt::CaseInsensitive)
            || member.status.contains(query, Qt::CaseInsensitive)) {
            matches.append(member);
        }
    }
    return matches;
}

QVector<ChatMember> ChatMemberListModel::admins() const
{
    QVector<ChatMember> admins;
    admins.reserve(m_members.size());
    for (const ChatMember& member : m_members) {
        if (member.isAdmin)
            admins.append(member);
    }
    return admins;
}

bool ChatMemberListModel::updateStatus(qint64 userId, const QString& status)
{
    const qsizetype index = indexOfUser(m_members, userId);
    if (index < 0)
        return false;

    ChatMember& member = m_members[index];
    if (member.status == status)
        return false;

    member.status = status;
    emit changed();
    return true;
}

bool ChatMemberListModel::remove(qint64 userId)
{
    const qsizetype index = indexOfUser(m_members, userId);
    if (index < 0)
        return false;

    m_members.removeAt(index);
    emit changed();
    return true;
}

void ChatMemberListModel::clear()
{
    if (m_members.isEmpty())
        return;

    m_members.clear();
    emit changed();
}

} // namespace beygram::telegram
