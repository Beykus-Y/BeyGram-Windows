#pragma once

#include <QHash>
#include <QObject>
#include <QPair>
#include <QtGlobal>

namespace beygram::telegram {

/// Stops one message from being announced twice.
///
/// TDLib can report the same message more than once: an updateNewMessage
/// replayed after a reconnect, a push that races the chat-list refresh, or a
/// caller that sees both an update and a re-read of the chat. A second banner
/// for the same message is the noise this class prevents.
///
/// The state is one record per chat: the last message id that notified and the
/// second it did. A repeat of that id while it is still inside the window is
/// dropped. Any other message notifies, because dedupe is per message rather
/// than per chat: two different messages in a row both notify, and the sequence
/// A, B, A notifies all three, since only the most recent id is remembered.
///
/// The class never reads the clock. Callers pass `nowUnix` in seconds, which
/// keeps tests deterministic, and stale records are pruned lazily: a record is
/// only examined when its chat is seen again, or dropped by forget()/clear().
class NotificationDeduper final : public QObject {
    Q_OBJECT

public:
    explicit NotificationDeduper(QObject* parent = nullptr);

    /// Length of the dedupe window in seconds, clamped to 0..3600. A window of
    /// 0 disables deduplication, so every valid message notifies. Shrinking the
    /// window does not sweep the existing records; they expire the next time
    /// their chat is seen. Emits changed() only when the stored value moves.
    void setWindowSeconds(int seconds);
    [[nodiscard]] int windowSeconds() const;

    /// True when `messageId` in `chatId` should be announced at `nowUnix`.
    ///
    /// A non-positive chat or message id returns false and records nothing. A
    /// repeat of the chat's last notified id while it is inside the window
    /// returns false and leaves the record untouched, so a flood of duplicate
    /// events cannot keep extending the window. Any other valid message returns
    /// true and becomes the chat's new record.
    ///
    /// A record whose window has passed, or whose timestamp is later than
    /// `nowUnix` (the clock moved backwards), is stale: it is replaced and the
    /// message notifies.
    bool shouldNotify(qint64 chatId, qint64 messageId, qint64 nowUnix);

    /// Drops the record for one chat, e.g. when it is deleted or its history is
    /// cleared. Unknown and non-positive ids are ignored. Emits changed() only
    /// when a record was removed.
    void forget(qint64 chatId);

    /// Drops every record. Emits changed() only when something was tracked.
    void clear();

    /// Number of chat records currently held. A record whose window has passed
    /// still counts until shouldNotify() for that chat prunes it.
    [[nodiscard]] int trackedCount() const;

signals:
    /// Emitted once whenever an observable value changed: windowSeconds() or
    /// trackedCount(). A message that only refreshes an existing chat's record
    /// changes neither, so a busy chat does not repaint its observers.
    void changed();

private:
    int m_window = 5;
    QHash<qint64, QPair<qint64, qint64>> m_last; // chat -> (messageId, time)
};

} // namespace beygram::telegram
