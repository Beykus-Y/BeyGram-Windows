#include "ui/GroupAdminFacade.h"

#include <QChar>
#include <QLatin1String>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <utility>

namespace beygram::ui {
namespace {

using telegram::AdminRights;

// Folds case, underscores, dashes and spaces so "can_change_info",
// "can-change-info" and "canChangeInfo" are one spelling. The same tolerance is
// applied to a status, where it also lets the TDLib @type name match.
QString folded(const QString& text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar character : text) {
        if (character == QLatin1Char('_') || character == QLatin1Char('-')
            || character.isSpace()) {
            continue;
        }
        out.append(character.toLower());
    }
    return out;
}

bool isCreatorStatus(const QString& status)
{
    const QString key = folded(status);
    return key == QLatin1String("creator") || key == QLatin1String("owner")
           || key == QLatin1String("chatmemberstatuscreator")
           || key == QLatin1String("chatmemberstatusowner");
}

// Maps a right name onto the draft field it edits. Null for anything this
// editor does not model, so a typo or a different AdminRights field can never
// grant a permission by accident.
bool* rightByName(AdminRights& rights, const QString& name)
{
    const QString key = folded(name);
    if (key.isEmpty())
        return nullptr;

    if (key == QLatin1String("canchangeinfo"))
        return &rights.canChangeInfo;
    if (key == QLatin1String("candeletemessages"))
        return &rights.canDeleteMessages;
    if (key == QLatin1String("canbanusers") || key == QLatin1String("canrestrictmembers"))
        return &rights.canBanUsers;
    if (key == QLatin1String("caninviteusers"))
        return &rights.canInviteUsers;
    if (key == QLatin1String("canpinmessages"))
        return &rights.canPinMessages;
    return nullptr;
}

// The enabled rights in the canonical order the panel and the request builder
// share, independent of the order the switches were flipped in.
QStringList enabledRights(const AdminRights& rights)
{
    QStringList names;
    if (rights.canChangeInfo)
        names.append(QStringLiteral("canChangeInfo"));
    if (rights.canDeleteMessages)
        names.append(QStringLiteral("canDeleteMessages"));
    if (rights.canBanUsers)
        names.append(QStringLiteral("canBanUsers"));
    if (rights.canInviteUsers)
        names.append(QStringLiteral("canInviteUsers"));
    if (rights.canPinMessages)
        names.append(QStringLiteral("canPinMessages"));
    return names;
}

} // namespace

GroupAdminFacade::GroupAdminFacade(QObject* parent)
    : QObject(parent)
{
}

QStringList GroupAdminFacade::memberNames() const
{
    QStringList names;
    names.reserve(m_members.size());
    for (const MemberRow& row : m_members)
        names.append(row.name);
    return names;
}

QStringList GroupAdminFacade::memberIds() const
{
    QStringList ids;
    ids.reserve(m_members.size());
    for (const MemberRow& row : m_members)
        ids.append(row.id);
    return ids;
}

QStringList GroupAdminFacade::memberStatuses() const
{
    QStringList statuses;
    statuses.reserve(m_members.size());
    for (const MemberRow& row : m_members) {
        statuses.append(row.creator ? QStringLiteral("creator")
                                    : QStringLiteral("administrator"));
    }
    return statuses;
}

int GroupAdminFacade::selectedIndex() const
{
    return m_selectedIndex;
}

bool GroupAdminFacade::selectedEditable() const
{
    return m_selectedIndex >= 0 && m_selectedIndex < m_members.size()
           && !m_members.at(m_selectedIndex).creator;
}

bool GroupAdminFacade::canChangeInfo() const
{
    return selectedRights().canChangeInfo;
}

bool GroupAdminFacade::canDeleteMessages() const
{
    return selectedRights().canDeleteMessages;
}

bool GroupAdminFacade::canBanUsers() const
{
    return selectedRights().canBanUsers;
}

bool GroupAdminFacade::canInviteUsers() const
{
    return selectedRights().canInviteUsers;
}

bool GroupAdminFacade::canPinMessages() const
{
    return selectedRights().canPinMessages;
}

void GroupAdminFacade::setMembers(const QStringList& ids, const QStringList& names,
                                  const QStringList& statuses)
{
    QVector<MemberRow> rows;
    rows.reserve(ids.size());

    for (qsizetype index = 0; index < ids.size(); ++index) {
        // Telegram user ids are positive; chat ids may be negative, but this
        // list is users only. A malformed id would make a save target nobody,
        // so the row is dropped instead of shown.
        bool numeric = false;
        const qint64 userId = ids.at(index).trimmed().toLongLong(&numeric);
        if (!numeric || userId <= 0)
            continue;

        // The canonical decimal spelling makes "42" and " 0042 " one row, so
        // the duplicate check below cannot let two lines target one user.
        const QString id = QString::number(userId);
        const bool duplicate = std::any_of(rows.cbegin(), rows.cend(),
                                           [&id](const MemberRow& row) {
                                               return row.id == id;
                                           });
        if (duplicate)
            continue;

        MemberRow row;
        row.id = id;
        row.name = names.value(index).trimmed();
        row.creator = isCreatorStatus(statuses.value(index));
        rows.append(std::move(row));
    }

    const bool hadSelection = m_selectedIndex >= 0;
    const bool rowsChanged = rows != m_members;

    m_members = std::move(rows);
    m_selectedIndex = -1;
    m_rights = {};

    // Without a selection the rights getters read all-denied either way, so a
    // new list announces only when its rows moved or a selection went away.
    if (rowsChanged || hadSelection)
        emit changed();
}

void GroupAdminFacade::selectMember(int index)
{
    if (index < -1 || index >= m_members.size())
        return;
    if (index == m_selectedIndex)
        return;

    m_selectedIndex = index;
    // The draft belongs to the member that was selected. The payload carries no
    // confirmed rights, so there is nothing to restore for the new member and
    // carrying the old grants over would be a permission leak.
    m_rights = {};
    emit changed();
}

void GroupAdminFacade::setRight(const QString& name, bool value)
{
    if (!selectedEditable())
        return;

    bool* target = rightByName(m_rights, name);
    if (!target || *target == value)
        return;

    *target = value;
    emit changed();
}

void GroupAdminFacade::save()
{
    if (!selectedEditable())
        return;

    // Snapshot both payload parts: a handler may re-enter the facade (for
    // example by reloading the list) before it returns.
    const QString userId = m_members.at(m_selectedIndex).id;
    const QStringList rights = enabledRights(m_rights);
    emit rightsSaveRequested(userId, rights);
}

telegram::AdminRights GroupAdminFacade::selectedRights() const
{
    if (m_selectedIndex < 0 || m_selectedIndex >= m_members.size())
        return {};
    return m_rights;
}

} // namespace beygram::ui
