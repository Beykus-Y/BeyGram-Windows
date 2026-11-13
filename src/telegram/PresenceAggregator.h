#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// The presence a chat row can paint, aggregated over that chat's members.
///
/// The struct is a plain snapshot. The aggregator normalises it on the way in:
/// counts are never negative and lastSeenText is trimmed, so a stored record is
/// already display-ready.
///
/// `botOnline` is the online flag of a chat that is a bot (a private chat with
/// a bot has no member list to count). It only matters while no human member is
/// online, i.e. it is an alternative source of "online", not an extra member.
struct ChatPresence {
    int onlineMembers = 0;
    int typingMembers = 0;
    int recordingMembers = 0;
    bool botOnline = false;
    QString lastSeenText;
};

/// Holds the presence of every chat, keyed by chat id, and turns it into the
/// line shown under the chat title.
///
/// The transport aggregates TDLib's per-user updates into one ChatPresence per
/// chat and feeds it here; a view repaints on changed(). The class keeps no
/// clock, sends nothing to TDLib and never logs, which keeps tests
/// deterministic.
///
/// Only chats with something to show are stored. A record whose counts are all
/// zero, whose bot is offline and whose last-seen text is blank is dropped, so
/// the hash never fills with empty entries. Reading a chat that is not stored
/// returns a default-constructed ChatPresence instead of failing.
///
/// Every mutator emits changed() only when the stored record really moved, so
/// repeated heartbeats and no-op clears stay silent. Calls with a chat id that
/// is not positive are ignored, because they cannot name a chat.
class PresenceAggregator final : public QObject {
    Q_OBJECT

public:
    explicit PresenceAggregator(QObject* parent = nullptr);

    /// Replaces the whole record of `chatId` with `presence`. Negative counts
    /// are clamped to zero and lastSeenText is trimmed. A record with nothing
    /// to show removes the chat. Ignored when the chat id is not positive.
    void setPresence(qint64 chatId, const ChatPresence& presence);

    /// The stored record of `chatId`, or a default-constructed ChatPresence
    /// when the chat has none, so callers can read a chat without checking for
    /// existence first.
    [[nodiscard]] ChatPresence presence(qint64 chatId) const;

    /// The line shown under the chat title, in this priority order:
    ///
    ///   recordingMembers > 0   "recording…"
    ///   typingMembers > 0      "typing…" or "typing… +N"
    ///   onlineMembers > 0      "N online"
    ///   botOnline              "online"
    ///   lastSeenText           the stored text verbatim
    ///   nothing                ""
    ///
    /// Recording wins over typing, and typing over online: a chat shows the
    /// strongest ongoing action, not a combination. "+N" names the other
    /// typists, so three typists show "typing… +2"; recording carries no count.
    [[nodiscard]] QString statusText(qint64 chatId) const;

    /// Forgets a chat entirely, e.g. after it was deleted or left. Silent when
    /// the chat has no record.
    void clearChat(qint64 chatId);

    /// Forgets every chat. Silent when the aggregator is already empty.
    void clear();

signals:
    /// Emitted whenever a stored record really changed, i.e. on additions,
    /// edits and removals, but never for an ignored or identical call.
    void changed();

private:
    QHash<qint64, ChatPresence> m_presences;
};

} // namespace beygram::telegram
