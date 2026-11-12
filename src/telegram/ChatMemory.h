#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// What the chat list last knew about one chat.
///
/// `lastMessageDate` is the message date TDLib reports, in seconds. `updatedAt`
/// is the moment this memory was told about the chat, in milliseconds, and is
/// what recency is measured by; remember() stamps it, so a caller-supplied value
/// is only a placeholder.
struct ChatMemoryEntry {
    qint64 chatId = 0;          // always > 0 in a stored entry
    QString title;
    QString lastPreview;
    int unreadCount = 0;
    qint64 lastMessageDate = 0;
    qint64 updatedAt = 0;
};

/// The chats the window has seen, kept so the chat list can be drawn before
/// TDLib answers and can still show a chat that a short response left out.
///
/// Entries are identified by chatId: remembering a chat that is already here
/// replaces its fields instead of adding a second row, and moves it to the
/// front of recent() because its updatedAt becomes the new `nowMs`.
///
/// The store is bounded. capacity() is clamped to 16..5000, and once a
/// remember() or a smaller capacity would exceed it, the least recently updated
/// entries go first. Equal updatedAt values are evicted by ascending chatId, so
/// the outcome never depends on the order chats happened to arrive in.
///
/// changed() is emitted only when the stored set really differs: an entry with
/// a non-positive chatId is ignored, re-remembering an entry with the same
/// fields and the same `nowMs` is a no-op, and so is forgetting an absent chat
/// or clearing an empty store. A newcomer that is older than everything already
/// stored is evicted immediately, which also leaves the store unchanged.
///
/// Nothing here is persisted and the class is not thread-safe; the owner keeps
/// it on the thread that draws the chat list.
class ChatMemory final : public QObject {
    Q_OBJECT

public:
    explicit ChatMemory(QObject* parent = nullptr);

    /// Sets how many chats are kept, clamped to 16..5000.
    ///
    /// Lowering the capacity evicts the least recently updated entries at once.
    /// Setting the value it already has does nothing and does not signal.
    void setCapacity(int capacity);

    [[nodiscard]] int capacity() const;

    /// Records the chat as it looks at `nowMs` (milliseconds).
    ///
    /// The entry is stored with `updatedAt = nowMs`; every other field is kept
    /// as given. An entry whose chatId is zero or negative is ignored. A chat
    /// already present is replaced and becomes the most recent one.
    void remember(const ChatMemoryEntry& entry, qint64 nowMs);

    /// The stored chat, or a default-constructed entry (chatId 0) when the chat
    /// is not remembered. Use contains() to tell the two apart.
    [[nodiscard]] ChatMemoryEntry entry(qint64 chatId) const;

    [[nodiscard]] bool contains(qint64 chatId) const;

    /// Up to `limit` entries, most recently updated first. `limit` is clamped
    /// to 1..capacity(), so asking for zero still yields the newest chat.
    [[nodiscard]] QVector<ChatMemoryEntry> recent(int limit) const;

    /// Drops one chat. Forgetting a chat that is not remembered does nothing.
    void forget(qint64 chatId);

    /// Drops every chat. Clearing an empty store does nothing.
    void clear();

    [[nodiscard]] int count() const;

signals:
    void changed();

private:
    static constexpr int kMinimumCapacity = 16;
    static constexpr int kMaximumCapacity = 5000;
    static constexpr int kDefaultCapacity = 200;

    /// Index of `chatId` in m_entries, or -1.
    [[nodiscard]] int indexOf(qint64 chatId) const;

    /// Removes the least recently updated entries until count() fits capacity().
    void evictOverflow();

    QVector<ChatMemoryEntry> m_entries;
    int m_capacity = kDefaultCapacity;
};

} // namespace beygram::telegram
