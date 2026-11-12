#pragma once

#include "TelegramTypes.h"

#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// The loaded comments of one channel post's discussion thread.
///
/// The model mirrors MessageThread.kt: comments are unique by id and always
/// oldest-first, so a thread view can append a newer page or prepend an older
/// one without reordering. The thread identity is kept separately from the
/// comments, because the discussion chat and its thread id are not the channel
/// post that opened the thread.
///
/// Every mutation emits changed() only when it really moves the stored state.
class ThreadMessageModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(int replyCount READ replyCount NOTIFY changed)
    Q_PROPERTY(bool hasThread READ hasThread NOTIFY changed)
    Q_PROPERTY(qint64 chatId READ chatId NOTIFY changed)
    Q_PROPERTY(qint64 messageId READ messageId NOTIFY changed)

public:
    explicit ThreadMessageModel(QObject* parent = nullptr);

    /// Selects the thread. Both ids must be positive; anything else is ignored,
    /// so a malformed call cannot destroy a valid open thread. replyCount is
    /// clamped to 0. A different id pair drops the loaded comments, because
    /// they belong to the previous thread; the same pair keeps them and only
    /// refreshes the reply count.
    void setThread(qint64 chatId, qint64 messageId, int replyCount);

    /// Replaces the loaded comments. Rows are de-duplicated by id (the later
    /// copy wins), non-positive ids are dropped and the result is sorted
    /// oldest-first. Emits changed() only when the resulting page differs from
    /// the loaded one.
    void setMessages(QVector<MessageData> messages);

    /// Merges an older page. Only ids that are not loaded yet are added; an
    /// already loaded row keeps its live copy so an edit cannot be rolled back
    /// by an older snapshot. Emits changed() only when at least one row was
    /// added.
    void appendOlder(QVector<MessageData> messages);

    /// A snapshot of the comments, oldest-first. Mutating it does not change
    /// the model.
    [[nodiscard]] QVector<MessageData> messages() const;
    [[nodiscard]] int count() const;
    [[nodiscard]] int replyCount() const;
    [[nodiscard]] bool hasThread() const;
    [[nodiscard]] qint64 chatId() const;
    [[nodiscard]] qint64 messageId() const;

    /// Drops the thread, its reply count and every loaded comment. A no-op
    /// while the model is already empty.
    void clear();

signals:
    /// Emitted whenever the thread identity, reply count or comments changed.
    void changed();

private:
    static void sortMessages(QVector<MessageData>& messages);
    static QVector<MessageData> dedupe(QVector<MessageData> messages);

    qint64 m_chatId = 0;
    qint64 m_messageId = 0;
    int m_replyCount = 0;
    QVector<MessageData> m_messages;
};

} // namespace beygram::telegram
