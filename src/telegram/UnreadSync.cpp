#include "UnreadSync.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <limits>

namespace beygram::telegram {
namespace {

int saturatedAdd(int total, int unread)
{
    // Both operands are non-negative and at most INT_MAX, so the qint64 sum
    // cannot wrap and only the clamp is left to do.
    constexpr qint64 kMaxTotal = std::numeric_limits<int>::max();
    const qint64 sum = static_cast<qint64>(total) + static_cast<qint64>(unread);
    return static_cast<int>(sum > kMaxTotal ? kMaxTotal : sum);
}

} // namespace

QVector<UnreadEntry> normalizeUnread(QVector<UnreadEntry> entries)
{
    QVector<UnreadEntry> normalized;
    normalized.reserve(entries.size());

    QHash<qint64, qsizetype> indexByChatId;
    indexByChatId.reserve(entries.size());

    for (UnreadEntry& entry : entries) {
        if (entry.chatId <= 0)
            continue;
        entry.unread = std::max(entry.unread, 0);

        const auto existing = indexByChatId.constFind(entry.chatId);
        if (existing != indexByChatId.cend()) {
            // Last occurrence wins, but it takes over the slot of the first one
            // so the surviving chats keep their original relative order.
            normalized[*existing] = entry;
            continue;
        }

        indexByChatId.insert(entry.chatId, normalized.size());
        normalized.append(entry);
    }

    return normalized;
}

UnreadDiff diffUnread(const QVector<UnreadEntry>& previous, const QVector<UnreadEntry>& next)
{
    const QVector<UnreadEntry> before = normalizeUnread(previous);
    const QVector<UnreadEntry> after = normalizeUnread(next);

    QHash<qint64, UnreadEntry> beforeByChatId;
    beforeByChatId.reserve(before.size());
    for (const UnreadEntry& entry : before)
        beforeByChatId.insert(entry.chatId, entry);

    UnreadDiff diff;
    diff.changed.reserve(after.size());

    QSet<qint64> seenInNext;
    seenInNext.reserve(after.size());

    for (const UnreadEntry& entry : after) {
        seenInNext.insert(entry.chatId);

        // A chat absent from `previous` reads as unread 0 and unmuted, which is
        // exactly how it should compare against a newly appearing chat.
        const UnreadEntry prior = beforeByChatId.value(entry.chatId);
        if (prior.unread != entry.unread || prior.muted != entry.muted)
            diff.changed.append(entry);

        diff.totalUnread = saturatedAdd(diff.totalUnread, entry.unread);
        if (!entry.muted)
            diff.unmutedUnread = saturatedAdd(diff.unmutedUnread, entry.unread);
    }

    for (const UnreadEntry& entry : before) {
        if (seenInNext.contains(entry.chatId))
            continue;
        if (entry.unread != 0 || entry.muted)
            diff.changed.append(UnreadEntry{entry.chatId, 0, false});
    }

    return diff;
}

} // namespace beygram::telegram
