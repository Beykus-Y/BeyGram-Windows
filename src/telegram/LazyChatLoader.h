#pragma once

#include <QObject>
#include <QSet>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Bounded on-demand loader for chat histories.
///
/// A chat id is in one of three states: waiting for a slot, in flight (its
/// loadRequested() was emitted and the owner has not answered yet) or loaded.
/// enqueue() appends the chat to the waiting list and immediately starts as
/// many waiting chats as concurrency() allows, emitting loadRequested() once
/// per started chat in queue order. markLoaded() and markFailed() settle an
/// in-flight chat, free its slot and start the next waiting one, so no more
/// than concurrency() chats are ever in flight at the same time.
///
/// A failed chat is forgotten instead of being remembered: nothing retries it
/// on its own, but a later enqueue() accepts it again. A loaded chat is
/// remembered so a repeated enqueue() is ignored; clear() forgets it too.
///
/// Only positive chat ids are accepted. pending() and isPending() describe the
/// waiting chats only; in-flight chats are counted by inFlight() and are not
/// part of that list.
///
/// Every mutator is a no-op when it would not change the stored state, so
/// changed() is emitted once per real transition and views never reload for
/// nothing. The class performs no I/O and is not thread-safe; use it from the
/// thread that owns it.
class LazyChatLoader final : public QObject {
    Q_OBJECT

public:
    explicit LazyChatLoader(QObject* parent = nullptr);

    /// Caps how many chats may be loading at once, clamped to 1..16. Defaults
    /// to 6. Lowering the cap below the current in-flight count keeps those
    /// chats running and only blocks new starts; raising it starts waiting
    /// chats right away.
    void setConcurrency(int max);
    [[nodiscard]] int concurrency() const { return m_concurrency; }

    /// Queues a chat and starts it immediately when a slot is free. A
    /// non-positive id, a chat already waiting or in flight, and a chat
    /// already loaded are ignored.
    void enqueue(qint64 chatId);

    /// Settles an in-flight chat as loaded and frees its slot. A waiting chat
    /// is marked loaded too, but it never occupied a slot, so no new chat
    /// starts because of it. Unknown, already loaded and failed ids are
    /// ignored.
    void markLoaded(qint64 chatId);

    /// Drops an in-flight or waiting chat without remembering it and frees its
    /// slot. The chat is not retried automatically; a later enqueue() accepts
    /// it again. Unknown and already loaded ids are ignored.
    void markFailed(qint64 chatId);

    [[nodiscard]] bool isLoaded(qint64 chatId) const;

    /// True while the chat waits for a slot. An in-flight chat is not pending;
    /// see inFlight().
    [[nodiscard]] bool isPending(qint64 chatId) const;

    /// Waiting chats in the order they will start. In-flight chats are not
    /// included.
    [[nodiscard]] QVector<qint64> pending() const;

    /// Number of chats that were started and not settled yet.
    [[nodiscard]] int inFlight() const;

    /// Number of chats marked loaded since the last clear().
    [[nodiscard]] int loadedCount() const;

    /// Forgets every waiting, in-flight and loaded chat. The concurrency cap is
    /// kept. Clearing an already empty loader is a no-op.
    void clear();

signals:
    /// Emitted whenever the waiting list, the in-flight list, the loaded set or
    /// the concurrency cap really changed.
    void changed();

    /// A chat was started and the owner should request its history. Emitted
    /// once per start, in queue order.
    void loadRequested(qint64 chatId);

private:
    /// Moves as many waiting chats as the cap allows to the in-flight list and
    /// returns them in start order.
    [[nodiscard]] QVector<qint64> startReady();

    /// Emits loadRequested() for every started chat that is still in flight, so
    /// a slot that settled itself while changed() was delivered is not
    /// announced again.
    void announceStarted(const QVector<qint64>& started);

    int m_concurrency = 6;
    QVector<qint64> m_pending;
    QVector<qint64> m_inFlight;
    QSet<qint64> m_loaded;
};

} // namespace beygram::telegram
