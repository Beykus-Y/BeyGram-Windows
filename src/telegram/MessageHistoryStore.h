#pragma once

#include "TelegramTypes.h"

#include <QObject>
#include <QVector>

namespace beygram::telegram {

/// One chat's loaded history window, kept in display order.
///
/// The store owns the merge rules of the Android client: rows are ordered by
/// date and then id, one row per message id, a temporary (pending) id is
/// replaced by the confirmed server id, and removed ids are tombstoned so a
/// late history page cannot resurrect a deleted message. Every mutation that
/// changes the window emits changed().
class MessageHistoryStore final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY changed)

public:
    explicit MessageHistoryStore(QObject* parent = nullptr);

    [[nodiscard]] int count() const;
    [[nodiscard]] QVector<MessageData> messages() const;
    [[nodiscard]] qint64 firstMessageId() const;
    [[nodiscard]] qint64 lastMessageId() const;

    /// Drops every row and forgets all tombstones.
    void clear();
    /// Replaces the window. Rows are de-duplicated (the later copy wins),
    /// tombstoned ids are dropped and the result is sorted.
    void setMessages(QVector<MessageData> messages);
    /// Inserts a row or replaces the loaded copy with the same id.
    void addOrUpdate(const MessageData& message);
    /// Replaces a temporary (pending) id with the confirmed message. The old
    /// id is tombstoned so a late page cannot bring the temporary row back.
    void replaceId(qint64 oldId, const MessageData& message);
    /// Removes rows and tombstones their ids.
    void removeIds(const QVector<qint64>& ids);
    /// Merges an older page: new ids are added, ids already loaded keep their
    /// live copy, tombstoned ids are ignored.
    void mergeOlder(QVector<MessageData> messages);
    [[nodiscard]] bool contains(qint64 id) const;

signals:
    void changed();

private:
    [[nodiscard]] int indexForId(qint64 id) const;
    [[nodiscard]] bool isTombstoned(qint64 id) const;
    /// Remembers a removed id; returns false when it was already known.
    bool retire(qint64 id);
    static void sortMessages(QVector<MessageData>& messages);
    static QVector<MessageData> dedupe(QVector<MessageData> messages);

    QVector<MessageData> m_messages;
    QVector<qint64> m_tombstones;
};

} // namespace beygram::telegram
