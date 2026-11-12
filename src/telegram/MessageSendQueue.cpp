#include "MessageSendQueue.h"

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int MinMaxAttempts = 1;
constexpr int MaxMaxAttempts = 10;
constexpr int RetryBaseDelayMs = 500;
constexpr int RetryMaxDelayMs = 30000;

/// 500 ms * 2^attempt, capped. `attempt` stays below MaxMaxAttempts (10), so
/// the shift cannot overflow even before the cap is applied.
int retryDelayMs(int attempt)
{
    const qint64 delay = qint64(RetryBaseDelayMs) * (qint64(1) << attempt);
    return int(std::min<qint64>(delay, RetryMaxDelayMs));
}

} // namespace

MessageSendQueue::MessageSendQueue(QObject* parent)
    : QObject(parent)
{
}

bool MessageSendQueue::enqueue(const QueuedMessage& message)
{
    if (message.tempId <= 0 || message.chatId <= 0
        || message.text.trimmed().isEmpty())
        return false;
    if (indexOf(message.tempId) >= 0 || m_inFlight.contains(message.tempId))
        return false;

    QueuedMessage stored = message;
    // A message cannot have been attempted a negative number of times; a
    // positive count is kept so a caller can re-enqueue a partially retried
    // message without losing its history.
    stored.attempts = std::max(0, message.attempts);
    m_queue.append(stored);
    emit changed();
    return true;
}

QVector<QueuedMessage> MessageSendQueue::pending() const
{
    return m_queue;
}

int MessageSendQueue::count() const
{
    return int(m_queue.size());
}

QueuedMessage MessageSendQueue::takeNext()
{
    if (m_queue.isEmpty())
        return QueuedMessage();

    QueuedMessage next = m_queue.takeFirst();
    m_inFlight.insert(next.tempId, next);
    emit changed();
    return next;
}

void MessageSendQueue::markFailed(qint64 tempId)
{
    QueuedMessage message;
    if (!takeKnown(tempId, &message))
        return;

    ++message.attempts;

    // maxAttempts() counts the first send too, so the failure that reaches the
    // limit is the last one: the message is dropped rather than retried.
    if (message.attempts >= m_maxAttempts) {
        emit changed();
        return;
    }

    const int delayMs = retryDelayMs(message.attempts);
    m_queue.append(message);
    emit retryScheduled(tempId, message.attempts, delayMs);
    emit changed();
}

void MessageSendQueue::markSent(qint64 tempId)
{
    QueuedMessage message;
    if (!takeKnown(tempId, &message))
        return;

    emit changed();
}

void MessageSendQueue::clear()
{
    if (m_queue.isEmpty() && m_inFlight.isEmpty())
        return;

    m_queue.clear();
    m_inFlight.clear();
    emit changed();
}

int MessageSendQueue::maxAttempts() const
{
    return m_maxAttempts;
}

void MessageSendQueue::setMaxAttempts(int maxAttempts)
{
    const int clamped = std::clamp(maxAttempts, MinMaxAttempts, MaxMaxAttempts);
    if (clamped == m_maxAttempts)
        return;

    m_maxAttempts = clamped;
    emit changed();
}

int MessageSendQueue::indexOf(qint64 tempId) const
{
    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue.at(i).tempId == tempId)
            return i;
    }
    return -1;
}

bool MessageSendQueue::takeKnown(qint64 tempId, QueuedMessage* message)
{
    auto inFlight = m_inFlight.find(tempId);
    if (inFlight != m_inFlight.end()) {
        *message = inFlight.value();
        m_inFlight.erase(inFlight);
        return true;
    }

    const int index = indexOf(tempId);
    if (index < 0)
        return false;

    *message = m_queue.takeAt(index);
    return true;
}

} // namespace beygram::telegram
