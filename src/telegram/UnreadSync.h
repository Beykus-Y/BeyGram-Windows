#pragma once

#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One chat's unread state as the chat list reports it.
struct UnreadEntry {
    qint64 chatId = 0;  // non-positive ids are dropped by normalizeUnread
    int unread = 0;     // negative counts are clamped to zero
    bool muted = false; // muted chats still count, just not as unmuted
};

/// The result of comparing two unread snapshots: the chats that actually
/// changed, plus the totals of the new snapshot.
struct UnreadDiff {
    /// Entries whose (unread, muted) pair changed, including chats that are new
    /// in `next` and chats that disappeared from it. A removed chat is
    /// reported as unread 0 and muted false, which is how an absent chat counts.
    QVector<UnreadEntry> changed;

    /// Sum of `unread` over the normalized `next`, saturating at INT_MAX.
    int totalUnread = 0;

    /// The same sum over chats that are not muted, also saturating at INT_MAX.
    int unmutedUnread = 0;
};

/// Cleans a snapshot so the rest of the module can rely on a few invariants:
/// the chat id is positive, `unread` is never negative and each chat appears
/// once.
///
/// Entries with a non-positive chat id are dropped. A chat that appears more
/// than once keeps the values of its last occurrence but the position of its
/// first, so the surviving chats stay in input order.
[[nodiscard]] QVector<UnreadEntry> normalizeUnread(QVector<UnreadEntry> entries);

/// Compares two snapshots after normalizing both, so unsorted, duplicated or
/// negative input cannot fake a change.
///
/// `changed` holds the entries of `next` whose (unread, muted) pair differs
/// from `previous`, in `next` order. A chat missing from `previous` counts as
/// unread 0 and unmuted, so a new chat is reported as soon as it carries unread
/// messages or is muted. Chats that were in `previous` but not in `next` follow,
/// in `previous` order, as unread 0 and unmuted; a chat that was already quiet
/// and unmuted is no change and stays out.
///
/// The totals describe `next` only. They saturate at INT_MAX instead of
/// overflowing.
[[nodiscard]] UnreadDiff diffUnread(const QVector<UnreadEntry>& previous,
                                    const QVector<UnreadEntry>& next);

} // namespace beygram::telegram
