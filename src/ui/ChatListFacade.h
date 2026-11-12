#pragma once

#include "telegram/ChatArchiveController.h"
#include "telegram/ChatListSortController.h"
#include "telegram/NotificationBadgeController.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::ui {

/// QML-facing view of the chat list controls.
///
/// It owns the three Telegram controllers that describe how the list is shown
/// and aggregates them behind one property set, so a view binds to a single
/// object instead of wiring three controllers itself:
///
/// - ChatListSortController supplies the mode, the direction and the pin rule.
///   The mode is exposed as a string ("position", "date", "unread", "name");
///   an unknown string is ignored instead of resetting the selection.
/// - ChatArchiveController supplies the archive folder state. Ids arrive from
///   QML as strings and are handed over as numbers; the controller drops
///   non-positive ids and duplicates.
/// - NotificationBadgeController supplies the unread totals and the badge label.
///
/// Every mutator is delegated to a controller that is already a no-op for a
/// value it holds, so changed() fires exactly once per real transition and
/// bindings are never invalidated for nothing. archiveOpenChanged() is emitted
/// only when the open flag itself flips; the other properties notify through
/// changed(), which the archive and badge controllers also drive.
class ChatListFacade final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString sortMode READ sortMode WRITE setSortMode NOTIFY changed)
    Q_PROPERTY(bool ascending READ ascending WRITE setAscending NOTIFY changed)
    Q_PROPERTY(bool pinnedFirst READ pinnedFirst WRITE setPinnedFirst NOTIFY changed)
    Q_PROPERTY(bool archiveOpen READ archiveOpen NOTIFY changed)
    Q_PROPERTY(int archivedCount READ archivedCount NOTIFY changed)
    Q_PROPERTY(QString badgeText READ badgeText NOTIFY changed)
    Q_PROPERTY(int totalUnread READ totalUnread NOTIFY changed)

public:
    explicit ChatListFacade(QObject* parent = nullptr);

    /// The current order as one of "position", "date", "unread" or "name".
    [[nodiscard]] QString sortMode() const;

    /// Selects the order. Case and surrounding whitespace are ignored, and a
    /// string that names no mode leaves the current one untouched.
    void setSortMode(const QString& mode);

    /// Direction applied to the current mode. False means descending, which is
    /// Telegram's newest-first default.
    [[nodiscard]] bool ascending() const;
    void setAscending(bool ascending);

    /// Whether pinned chats are hoisted above the rest, whatever the mode is.
    [[nodiscard]] bool pinnedFirst() const;
    void setPinnedFirst(bool pinnedFirst);

    /// Whether the Archive folder is the list currently shown.
    [[nodiscard]] bool archiveOpen() const;

    /// Number of chats the archive currently lists.
    [[nodiscard]] int archivedCount() const;

    /// Badge label: empty at zero, the exact number up to 99, "99+" above it.
    [[nodiscard]] QString badgeText() const;

    /// Unread messages across every tracked chat, muted or not.
    [[nodiscard]] int totalUnread() const;

    /// Replaces the archived ids. Values are parsed as 64-bit chat ids; text
    /// that is not a number becomes 0, and the archive controller discards it.
    Q_INVOKABLE void setArchivedIds(const QStringList& chatIds);

    /// Shows the Archive folder. The archived ids are kept while it is open.
    Q_INVOKABLE void openArchive();

    /// Returns to the main chat list. The archived ids are remembered.
    Q_INVOKABLE void closeArchive();

    /// Records one chat's unread count and muted flag for the badge. A negative
    /// count is clamped to zero and an invalid chat id is ignored.
    Q_INVOKABLE void setChatUnread(const QString& chatId, int unread, bool muted);

    /// Forgets one chat, dropping its unread count from the totals.
    Q_INVOKABLE void removeChat(const QString& chatId);

signals:
    /// Emitted whenever any exposed value actually changed.
    void changed();

    /// Emitted only when the archive folder was opened or closed.
    void archiveOpenChanged(bool open);

private:
    /// Forwards an archive controller change: always as changed(), plus
    /// archiveOpenChanged() when the open flag really flipped.
    void syncArchiveState();

    telegram::ChatListSortController m_sort;
    telegram::ChatArchiveController m_archive;
    telegram::NotificationBadgeController m_badge;
    bool m_archiveOpen = false;
};

} // namespace beygram::ui
