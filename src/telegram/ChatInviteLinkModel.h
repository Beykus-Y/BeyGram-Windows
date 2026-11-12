#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One invite link of a group or channel, as shown on the invite-link screen.
///
/// The struct is a plain value type: no pointers, no session state and no
/// secrets beyond the link itself, so a row can be cached per account and
/// copied across threads.
///
/// Fields follow the parts of TDLib's chatInviteLink this screen renders:
///
///   chatInviteLink invite_link:string name:string creator_user_id:int53
///       date:int32 edit_date:int32 expiration_date:int32 member_limit:int32
///       member_count:int32 is_primary:Bool is_revoked:Bool ... = ChatInviteLink;
///
/// The model never parses that object itself; the caller hands it rows that are
/// already parsed. This build carries no chatInviteLink request builder (neither
/// TelegramRequests.h nor ChatMembers.h has export/edit/revoke helpers), and
/// DeepLinks.h's joinChatRequest() only builds a joinChatByInviteLink request
/// from a link, which is a different operation from listing or revoking one, so
/// nothing here is reused or duplicated.
struct InviteLinkRow {
    /// The full invite link, exactly as Telegram reported it. Empty rows are
    /// dropped by the model because a link without text cannot be shown,
    /// copied or revoked. The value is never trimmed or normalised: revoke()
    /// matches it byte for byte, so rewriting it would make that match fail.
    QString link;

    /// Human label the creator gave the link ("name" in TDLib). Empty for an
    /// unnamed link, which the UI shows as the default invite link.
    QString title;

    /// Maximum number of members the link may admit, 0 when unlimited. TDLib
    /// uses 0 for "no limit"; a negative value is not meaningful and is folded
    /// to 0 when the row is stored.
    qint64 memberLimit = 0;

    /// Members who joined through this link. Negative values are folded to 0
    /// when the row is stored, since the screen shows a counter.
    int memberCount = 0;

    /// True once the link was revoked and can no longer be used. A revoked link
    /// stays in links() (so the screen can still show it, greyed out) but is
    /// excluded from active() and primary().
    bool revoked = false;

    /// Expiry as a Unix timestamp in seconds, 0 when the link never expires.
    /// The model does not read the clock: whether a link is "expired" is the
    /// caller's decision, so the tests stay deterministic.
    qint64 expiresAt = 0;
};

/// The invite links of one chat, as a plain value holder for the invite screen.
///
/// The model never talks to TDLib, never logs and never reads the clock: the
/// caller fetches the links, hands them to setLinks(), and sends the matching
/// revokeChatInviteLink request itself when revoke() reports a change. Because
/// nothing here performs I/O, the tests stay deterministic.
///
/// Invariants:
///
///  - Every stored row has a non-empty link, and links are unique. setLinks()
///    drops rows with an empty link and keeps the first row of each link, so a
///    caller merging several pages cannot end up with two rows fighting over
///    one link.
///  - Order is the caller's order. setLinks() only removes rows; it never
///    sorts, so the screen shows links in the order the server returned them
///    and the primary link stays first.
///  - memberCount and memberLimit are never negative; see InviteLinkRow.
///  - changed() is emitted only when the visible list really differs. Reloading
///    the same links, revoking an unknown or already revoked link, or clearing
///    an empty model are all silent, so a refresh that brings the same data
///    does not repaint the list or re-trigger a binding loop.
class ChatInviteLinkModel final : public QObject {
    Q_OBJECT

public:
    explicit ChatInviteLinkModel(QObject* parent = nullptr);

    /// Replaces every link with `links`, in the given order. Rows whose link is
    /// empty and later duplicates of a link (the first occurrence wins) are
    /// dropped; memberCount and memberLimit are clamped to at least 0.
    /// changed() is emitted only when the resulting list differs from the
    /// current one.
    void setLinks(QVector<InviteLinkRow> links);

    /// A copy of the links, in display order, revoked ones included.
    [[nodiscard]] QVector<InviteLinkRow> links() const;

    /// Number of links currently held. Always equal to links().size().
    [[nodiscard]] int count() const;

    /// The links that can still be used, i.e. those not revoked, in display
    /// order. Expired links are still returned: expiry is judged by the caller,
    /// which is the only party that knows the current time.
    [[nodiscard]] QVector<InviteLinkRow> active() const;

    /// The chat's main invite link: the first active link in display order.
    ///
    /// Returns a default-constructed InviteLinkRow (empty link, zero counters,
    /// not revoked, no expiry) when no link is active, so callers can test
    /// primary().link.isEmpty() instead of handling an optional.
    [[nodiscard]] InviteLinkRow primary() const;

    /// Marks the link equal to `link` as revoked. Returns true only when an
    /// existing, still active link was marked, which is also the only case that
    /// emits changed(). The comparison is exact and case-sensitive, because two
    /// invite links that differ in case are different links. An unknown link and
    /// a link that is already revoked both return false and leave the model
    /// untouched; the row is never removed or reordered, it only stops being
    /// active.
    bool revoke(const QString& link);

    /// Forgets every link, e.g. when the chat is closed or the account is
    /// signed out. Emits changed() only if there was something to forget.
    void clear();

signals:
    /// The visible invite-link list changed. Not emitted for a rejected or
    /// no-op call.
    void changed();

private:
    QVector<InviteLinkRow> m_links;
};

} // namespace beygram::telegram
