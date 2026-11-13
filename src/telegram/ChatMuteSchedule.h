#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Per-chat mute expiry, stored as the Unix second at which a mute ends.
///
/// The model is a pure state holder: it never reads the clock, never talks to
/// TDLib and performs no I/O, so the caller passes "now" to the queries and the
/// tests stay deterministic.
///
/// A stored value is one of:
///
///  - -1: muted forever;
///  - a positive Unix second: muted until that second, exclusive, i.e. the mute
///    is already over at exactly that second;
///  - absent (reported as 0): not muted.
///
/// Invariants:
///
///  - Only positive chat ids are accepted. A call with a chat id that cannot
///    name a chat is ignored by the mutators and reports unmuted from the
///    queries.
///  - Only -1 and non-negative values are valid expiries. Values below -1 are
///    rejected instead of being read as "forever", so a malformed value cannot
///    silence a chat.
///  - An entry whose expiry has passed reports unmuted. Queries that receive
///    `nowUnix` drop such an entry when they meet it, so an expired mute does
///    not stay in the hash forever. That pruning is silent: the chat already
///    reported unmuted, so nothing observable changed.
///  - mutedUntil() has no clock of its own and therefore returns the stored
///    value verbatim: 0 for a chat without a record, -1 for a forever mute and
///    the positive expiry otherwise. It may return an expiry that is already in
///    the past until a query with `nowUnix` prunes it; isMuted() and label()
///    are the calls that decide whether a mute is currently in force.
///  - changed() is emitted only when the stored value really moves: writing the
///    value already stored, clearing an absent chat, or an ignored call is
///    silent.
class ChatMuteSchedule final : public QObject {
    Q_OBJECT

public:
    explicit ChatMuteSchedule(QObject* parent = nullptr);

    /// Stores the mute expiry of `chatId`: -1 mutes forever, 0 unmutes, and a
    /// positive value mutes until that Unix second (exclusive). A value below
    /// -1 is ignored. Ignored when the chat id is not positive. Emits changed()
    /// once when the stored value really changes.
    void setMutedUntil(qint64 chatId, qint64 untilUnix);

    /// The stored expiry of `chatId`: -1 for a forever mute, a positive Unix
    /// second for a timed mute, and 0 when the chat has no record (including an
    /// invalid chat id). The value is returned verbatim, without consulting a
    /// clock, so an expiry that has already passed is still reported until a
    /// query with `nowUnix` prunes it.
    [[nodiscard]] qint64 mutedUntil(qint64 chatId) const;

    /// Whether `chatId` is muted at `nowUnix`. A forever mute is always muted;
    /// a timed mute is muted while `nowUnix` is before its expiry and unmuted
    /// at exactly the expiry second. An expired entry is dropped when it is
    /// met. A chat id that is not positive reports false.
    [[nodiscard]] bool isMuted(qint64 chatId, qint64 nowUnix) const;

    /// The mute state for a chat row: "Muted forever", "Muted for 2h",
    /// "Muted for 3d" or "Unmuted". The duration is the remaining time in the
    /// largest whole unit (minutes, hours or days), rounded down, with a
    /// minimum of "1m" while the mute is still active, so an active mute never
    /// reads as "0m". An expired or unknown chat reports "Unmuted". Like
    /// isMuted(), this drops an entry whose expiry has passed.
    [[nodiscard]] QString label(qint64 chatId, qint64 nowUnix) const;

    /// Forgets the mute of `chatId`. Emits changed() once when the chat had a
    /// record, even an expired one, and stays silent otherwise. Ignored when
    /// the chat id is not positive.
    void clearChat(qint64 chatId);

    /// Forgets every chat's mute. Emits changed() once when anything was
    /// stored, and stays silent when the schedule is already empty.
    void clear();

signals:
    /// Emitted once per real change to the stored expiries. Query-time pruning
    /// of an expired entry is silent.
    void changed();

private:
    // Mutable so a query can drop an expired entry without a separate sweep.
    mutable QHash<qint64, qint64> m_mutedUntil;
};

} // namespace beygram::telegram
