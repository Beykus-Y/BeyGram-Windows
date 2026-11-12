#pragma once

#include "ChatMembers.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// Name used by the member-list screens for one row of this model.
///
/// The row is the same value type ChatMembers.h already parses out of TDLib
/// (userId, displayName, status, isAdmin, isOwner, isBot), so a member the
/// caller fetched once can be shown, filtered and updated here without a second
/// struct that would drift from the parser. The alias exists only so call sites
/// can talk about "a member row" without repeating the wire-level type name.
using MemberRow = ChatMember;

/// The member list of one chat, as a plain value holder for the members screen.
///
/// The model never talks to TDLib, never logs and never reads the clock: the
/// caller fetches the pages, hands the merged answer to setMembers(), and sends
/// any status change itself when updateStatus() reports one. Because nothing
/// here performs I/O, the tests stay deterministic.
///
/// Invariants:
///
///  - `userId` is unique and positive. setMembers() drops members whose userId
///    is 0 or negative (a chat member that is not a user, or an unparsed entry)
///    because such a row can be neither looked up nor acted on, and keeps the
///    first member of each userId so a caller merging several pages cannot end
///    up with two rows fighting over one user.
///  - Order is the caller's order. setMembers() only removes entries; it never
///    sorts, so the screen shows members in the order the server returned them.
///  - changed() is emitted only when the visible list really differs. Reloading
///    the same members, rejecting an unknown userId, writing a status a member
///    already has, removing an absent member or clearing an empty model are all
///    silent, so a refresh that brings the same data does not repaint the list
///    or re-trigger a binding loop.
class ChatMemberListModel final : public QObject {
    Q_OBJECT

public:
    explicit ChatMemberListModel(QObject* parent = nullptr);

    /// Replaces every member with `members`, in the given order. Members without
    /// a positive userId and later duplicates of a userId (the first occurrence
    /// wins) are dropped. changed() is emitted only when the resulting list
    /// differs from the current one.
    void setMembers(QVector<ChatMember> members);

    /// A copy of the members, in display order.
    [[nodiscard]] QVector<ChatMember> members() const;

    /// Number of members currently held. Always equal to members().size().
    [[nodiscard]] int count() const;

    /// The members whose displayName or status contains `query`, ignoring case,
    /// in display order. An empty query returns every member rather than none,
    /// which is what the search box shows before the user types. The query is
    /// matched as written: it is not trimmed or split into words.
    [[nodiscard]] QVector<ChatMember> filtered(const QString& query) const;

    /// The members this client may administer, in display order.
    ///
    /// Only members flagged isAdmin are returned. The chat owner carries
    /// isOwner instead of isAdmin (see ChatMembers.h), so a caller that wants
    /// every member who "may administer" unions this list with the owner; the
    /// two flags are kept apart here because the screen labels the roles
    /// differently.
    [[nodiscard]] QVector<ChatMember> admins() const;

    /// Stores `status` for the member with `userId`. Returns true only when an
    /// existing member's status actually changed, which is also the only case
    /// that emits changed(). An unknown userId and a write of the status the
    /// member already has both return false and leave the model untouched; a
    /// member is never created for an unknown id, because a userId the server
    /// did not report is not one this client may invent.
    bool updateStatus(qint64 userId, const QString& status);

    /// Forgets the member with `userId`, e.g. when TDLib reports that the user
    /// left the chat. Returns true only when a member was removed, which is also
    /// the only case that emits changed(). An absent userId is a silent no-op,
    /// so a "left" update for someone already gone does not repaint the list.
    bool remove(qint64 userId);

    /// Forgets every member, e.g. when the chat is closed or the account is
    /// signed out. Emits changed() only if there was something to forget.
    void clear();

signals:
    /// The visible member list changed. Not emitted for a rejected or no-op
    /// call.
    void changed();

private:
    QVector<ChatMember> m_members;
};

} // namespace beygram::telegram
