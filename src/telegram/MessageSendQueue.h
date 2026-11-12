#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One text message waiting to be sent.
///
/// `tempId` identifies the message locally and is the key for markSent() and
/// markFailed(); `chatId` and `replyTo` are TDLib ids. A `replyTo` of 0 means
/// the message is not a reply. `attempts` counts failed send attempts and is
/// advanced by the queue itself.
struct QueuedMessage {
    qint64 tempId = 0;
    qint64 chatId = 0;
    QString text;
    qint64 replyTo = 0;
    int attempts = 0;
};

/// A FIFO queue of text messages that still have to be handed to TDLib.
///
/// enqueue() accepts a message only when its ids are positive, its text is not
/// blank and its tempId is not already known; it returns whether the message
/// was accepted. Messages keep their send order, so pending() lists the oldest
/// first and takeNext() removes and returns that oldest one.
///
/// A message returned by takeNext() stays in flight until markSent() or
/// markFailed() settles it. While it is in flight its tempId still counts as a
/// duplicate, and markFailed() puts it back at the end of the queue. pending()
/// and count() describe the waiting messages only; the in-flight set is
/// internal.
///
/// markFailed() counts one failed attempt. While attempts remain,
/// retryScheduled() reports the new attempt number and the backoff delay
/// (500 ms * 2^attempt, capped at 30 s); once the attempt count reaches
/// maxAttempts() the message is dropped instead. markSent() forgets a delivered
/// message. Neither call does anything for an unknown id.
///
/// changed() is emitted only when the queue, the in-flight set or the retry
/// limit really changes, so a rejected enqueue() or an unknown id stays silent.
/// The queue is not thread-safe; use it from the thread that owns it.
class MessageSendQueue final : public QObject {
    Q_OBJECT

public:
    explicit MessageSendQueue(QObject* parent = nullptr);

    /// Appends `message` at the back of the queue.
    ///
    /// Rejected, with no signal, when tempId or chatId is not positive, when
    /// the text is empty or only whitespace, or when tempId is already waiting
    /// or in flight. The text is stored verbatim; only the validation ignores
    /// whitespace. A negative attempts count is stored as 0.
    bool enqueue(const QueuedMessage& message);

    /// A copy of the waiting messages, oldest first. Messages handed out by
    /// takeNext() are in flight and not included.
    [[nodiscard]] QVector<QueuedMessage> pending() const;

    /// Number of waiting messages; in-flight messages are not counted.
    [[nodiscard]] int count() const;

    /// Removes and returns the oldest waiting message, keeping it in flight
    /// until markSent() or markFailed() settles it. Returns a default
    /// QueuedMessage when the queue is empty.
    [[nodiscard]] QueuedMessage takeNext();

    /// Counts one failed attempt for tempId, waiting or in flight.
    ///
    /// Below maxAttempts() the message is re-queued at the back with its
    /// attempt count incremented and retryScheduled() is emitted; at the limit
    /// it is dropped. An unknown id is ignored.
    void markFailed(qint64 tempId);

    /// Settles the delivered message, removing it from the in-flight set or
    /// the waiting queue. An unknown id is ignored.
    void markSent(qint64 tempId);

    /// Drops every waiting and in-flight message. A no-op when both are empty.
    void clear();

    /// Maximum number of send attempts per message, including the first one.
    /// Defaults to 3.
    [[nodiscard]] int maxAttempts() const;

    /// Sets the retry limit, clamped to 1..10.
    void setMaxAttempts(int maxAttempts);

signals:
    /// The waiting messages, the in-flight set or the retry limit changed.
    void changed();

    /// A failed message was re-queued for another attempt.
    /// `attempt` is the new attempt number (starting at 1) and `delayMs` is
    /// 500 * 2^attempt milliseconds, capped at 30000.
    void retryScheduled(qint64 tempId, int attempt, int delayMs);

private:
    /// Index of tempId among the waiting messages, or -1.
    [[nodiscard]] int indexOf(qint64 tempId) const;

    /// Removes tempId from the in-flight set or the waiting queue and stores
    /// the message in *message. Returns false for an unknown id.
    bool takeKnown(qint64 tempId, QueuedMessage* message);

    QVector<QueuedMessage> m_queue;
    QHash<qint64, QueuedMessage> m_inFlight;
    int m_maxAttempts = 3;
};

} // namespace beygram::telegram
