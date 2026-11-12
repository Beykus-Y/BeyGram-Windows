#include "UserStatusModel.h"

namespace beygram::telegram {
namespace {

// Only Offline has a last-seen timestamp; userStatusOnline.expires is when the
// online state lapses, not when the user was seen.
bool carriesLastSeen(PresenceKind kind)
{
    return kind == PresenceKind::Offline;
}

PresenceKind kindFromStatus(const QString& status)
{
    if (status == QLatin1String("userStatusOnline"))
        return PresenceKind::Online;
    if (status == QLatin1String("userStatusOffline"))
        return PresenceKind::Offline;
    if (status == QLatin1String("userStatusRecently"))
        return PresenceKind::Recently;
    if (status == QLatin1String("userStatusLastWeek"))
        return PresenceKind::LastWeek;
    if (status == QLatin1String("userStatusLastMonth"))
        return PresenceKind::LastMonth;
    if (status == QLatin1String("userStatusEmpty"))
        return PresenceKind::Hidden;
    return PresenceKind::Unknown;
}

} // namespace

UserStatusModel::UserStatusModel(QObject* parent)
    : QObject(parent)
{
}

void UserStatusModel::setStatus(PresenceKind kind, qint64 lastSeenUnix)
{
    const qint64 nextLastSeen = carriesLastSeen(kind) ? qMax<qint64>(0, lastSeenUnix) : 0;

    if (m_kind == kind && m_lastSeen == nextLastSeen)
        return;

    m_kind = kind;
    m_lastSeen = nextLastSeen;
    emit changed();
}

void UserStatusModel::setStatus(const PresenceInfo& presence)
{
    setStatus(kindFromStatus(presence.status), presence.lastSeen);
}

QString UserStatusModel::label() const
{
    switch (m_kind) {
    case PresenceKind::Online:
        return QStringLiteral("online");
    case PresenceKind::Recently:
        return QStringLiteral("last seen recently");
    case PresenceKind::LastWeek:
        return QStringLiteral("last seen within a week");
    case PresenceKind::LastMonth:
        return QStringLiteral("last seen within a month");
    case PresenceKind::Offline:
        return QStringLiteral("offline");
    case PresenceKind::Unknown:
    case PresenceKind::Hidden:
        break;
    }
    return QString();
}

void UserStatusModel::clear()
{
    if (m_kind == PresenceKind::Unknown && m_lastSeen == 0)
        return;

    m_kind = PresenceKind::Unknown;
    m_lastSeen = 0;
    emit changed();
}

} // namespace beygram::telegram
