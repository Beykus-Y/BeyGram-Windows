#include "ChatMemberPermissionsModel.h"

namespace beygram::telegram {
namespace {

/// Compares every right, isAdmin included. isAdmin cannot be edited through
/// setRight(), but a begin() that reloads a member whose status changed must
/// still count as a difference.
bool sameRights(const MemberRights& left, const MemberRights& right)
{
    return left.canSendMessages == right.canSendMessages
        && left.canSendMedia == right.canSendMedia
        && left.canSendPolls == right.canSendPolls
        && left.canAddMembers == right.canAddMembers
        && left.canPinMessages == right.canPinMessages
        && left.canManageTopics == right.canManageTopics
        && left.isAdmin == right.isAdmin;
}

} // namespace

MemberRights memberRightsFromChatPermissions(const ChatPermissions& permissions)
{
    MemberRights rights;
    rights.canSendMessages = permissions.canSendMessages;
    rights.canSendMedia = permissions.canSendMedia;
    rights.canSendPolls = permissions.canSendPolls;
    rights.canAddMembers = permissions.canInviteUsers;
    rights.canPinMessages = permissions.canPinMessages;
    rights.canManageTopics = permissions.canManageTopics;
    // isAdmin keeps its default: chatPermissions never describes an admin.
    return rights;
}

ChatMemberPermissionsModel::ChatMemberPermissionsModel(QObject* parent)
    : QObject(parent)
{
}

void ChatMemberPermissionsModel::begin(qint64 chatId, qint64 userId, const MemberRights& rights)
{
    if (chatId == 0 || userId <= 0)
        return;

    const bool wasDirty = dirty();
    const bool sameSession = m_active && m_chatId == chatId && m_userId == userId;
    const bool sameLoadedRights = sameRights(m_rights, rights);

    m_chatId = chatId;
    m_userId = userId;
    m_rights = rights;
    // What was loaded is the baseline: opening the dialog never looks edited,
    // and a reload discards a pending edit instead of merging with it.
    m_original = rights;
    m_active = true;

    if (!sameSession || !sameLoadedRights || wasDirty)
        emit changed();
}

void ChatMemberPermissionsModel::cancel()
{
    if (!m_active)
        return;

    m_active = false;
    m_chatId = 0;
    m_userId = 0;
    m_rights = MemberRights{};
    m_original = MemberRights{};

    emit changed();
}

bool ChatMemberPermissionsModel::active() const
{
    return m_active;
}

qint64 ChatMemberPermissionsModel::chatId() const
{
    return m_chatId;
}

qint64 ChatMemberPermissionsModel::userId() const
{
    return m_userId;
}

MemberRights ChatMemberPermissionsModel::rights() const
{
    return m_rights;
}

bool ChatMemberPermissionsModel::setRight(const QString& name, bool value)
{
    // Admin status is part of the member's status, not of the rights editor, so
    // it is read-only here even though it travels in the same struct.
    if (!m_active || name == QLatin1String("isAdmin"))
        return false;

    bool* target = nullptr;
    if (name == QLatin1String("canSendMessages"))
        target = &m_rights.canSendMessages;
    else if (name == QLatin1String("canSendMedia"))
        target = &m_rights.canSendMedia;
    else if (name == QLatin1String("canSendPolls"))
        target = &m_rights.canSendPolls;
    else if (name == QLatin1String("canAddMembers"))
        target = &m_rights.canAddMembers;
    else if (name == QLatin1String("canPinMessages"))
        target = &m_rights.canPinMessages;
    else if (name == QLatin1String("canManageTopics"))
        target = &m_rights.canManageTopics;

    if (target == nullptr)
        return false;

    // The name is accepted even when the value is the one already stored, but
    // only a real change is worth telling a view about.
    if (*target != value) {
        *target = value;
        emit changed();
    }
    return true;
}

bool ChatMemberPermissionsModel::dirty() const
{
    if (!m_active)
        return false;
    return !sameRights(m_rights, m_original);
}

void ChatMemberPermissionsModel::markSaved()
{
    if (!m_active)
        return;

    const bool wasDirty = dirty();

    m_original = m_rights;

    // Only the dirty flag moved, but that is exactly what the Save button shows.
    if (wasDirty)
        emit changed();
}

bool ChatMemberPermissionsModel::canSave() const
{
    return m_active && dirty();
}

} // namespace beygram::telegram
