#include "ChatListFacade.h"

#include <QString>
#include <QVector>

namespace beygram::ui {
namespace {

using telegram::ChatSortMode;

// Canonical, lower-case name of a mode. The same strings are the accepted
// input of setSortMode() and the values the QML combo writes back.
QString modeToString(ChatSortMode mode)
{
    switch (mode) {
    case ChatSortMode::Position:
        return QStringLiteral("position");
    case ChatSortMode::Date:
        return QStringLiteral("date");
    case ChatSortMode::Unread:
        return QStringLiteral("unread");
    case ChatSortMode::Name:
        return QStringLiteral("name");
    }
    return QStringLiteral("position");
}

// Parses a mode name, ignoring case and surrounding whitespace. Returns false
// for anything else so the caller can leave the current mode alone instead of
// silently falling back to position.
bool modeFromString(const QString& text, ChatSortMode* mode)
{
    if (mode == nullptr)
        return false;

    const QString normalised = text.trimmed().toLower();

    if (normalised == QLatin1String("position"))
        *mode = ChatSortMode::Position;
    else if (normalised == QLatin1String("date"))
        *mode = ChatSortMode::Date;
    else if (normalised == QLatin1String("unread"))
        *mode = ChatSortMode::Unread;
    else if (normalised == QLatin1String("name"))
        *mode = ChatSortMode::Name;
    else
        return false;

    return true;
}

// Chat ids reach QML as strings because Telegram ids exceed the 32-bit range a
// QML number would truncate. Text that is not a number maps to 0, which both
// controllers already reject.
qint64 parseChatId(const QString& chatId)
{
    bool ok = false;
    const qint64 id = chatId.trimmed().toLongLong(&ok);
    return ok ? id : 0;
}

} // namespace

ChatListFacade::ChatListFacade(QObject* parent)
    : QObject(parent)
    , m_archiveOpen(m_archive.isArchiveOpen())
{
    // The sort and badge controllers have nothing but changed() to report, so
    // their signal is the facade signal.
    connect(&m_sort, &telegram::ChatListSortController::changed,
            this, &ChatListFacade::changed);
    connect(&m_badge, &telegram::NotificationBadgeController::changed,
            this, &ChatListFacade::changed);

    // The archive controller reports both the open flag and the id list through
    // one signal; the facade keeps the two cases apart.
    connect(&m_archive, &telegram::ChatArchiveController::changed,
            this, &ChatListFacade::syncArchiveState);
}

QString ChatListFacade::sortMode() const
{
    return modeToString(m_sort.mode());
}

void ChatListFacade::setSortMode(const QString& mode)
{
    ChatSortMode parsed = ChatSortMode::Position;
    if (!modeFromString(mode, &parsed))
        return;

    // setMode() is already a no-op for the value it holds, so an equivalent
    // name ("Date", " date ") does not re-emit changed().
    m_sort.setMode(parsed);
}

bool ChatListFacade::ascending() const
{
    return m_sort.ascending();
}

void ChatListFacade::setAscending(bool ascending)
{
    m_sort.setAscending(ascending);
}

bool ChatListFacade::pinnedFirst() const
{
    return m_sort.pinnedFirst();
}

void ChatListFacade::setPinnedFirst(bool pinnedFirst)
{
    m_sort.setPinnedFirst(pinnedFirst);
}

bool ChatListFacade::archiveOpen() const
{
    return m_archiveOpen;
}

int ChatListFacade::archivedCount() const
{
    return static_cast<int>(m_archive.archivedIds().size());
}

QString ChatListFacade::badgeText() const
{
    return m_badge.badgeText();
}

int ChatListFacade::totalUnread() const
{
    return m_badge.totalUnread();
}

void ChatListFacade::setArchivedIds(const QStringList& chatIds)
{
    QVector<qint64> ids;
    ids.reserve(chatIds.size());

    for (const QString& chatId : chatIds)
        ids.append(parseChatId(chatId));

    // The controller drops the 0s produced by unparsable text, negative ids and
    // duplicates, and keeps the incoming order for the rest.
    m_archive.setArchivedIds(ids);
}

void ChatListFacade::openArchive()
{
    m_archive.setArchiveOpen(true);
}

void ChatListFacade::closeArchive()
{
    m_archive.setArchiveOpen(false);
}

void ChatListFacade::setChatUnread(const QString& chatId, int unread, bool muted)
{
    m_badge.setChatUnread(parseChatId(chatId), unread, muted);
}

void ChatListFacade::removeChat(const QString& chatId)
{
    m_badge.removeChat(parseChatId(chatId));
}

void ChatListFacade::syncArchiveState()
{
    const bool open = m_archive.isArchiveOpen();
    const bool flipped = open != m_archiveOpen;
    m_archiveOpen = open;

    emit changed();

    if (flipped)
        emit archiveOpenChanged(open);
}

} // namespace beygram::ui
