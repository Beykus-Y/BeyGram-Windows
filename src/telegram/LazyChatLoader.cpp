#include "LazyChatLoader.h"

namespace beygram::telegram {
namespace {

constexpr int kMinConcurrency = 1;
constexpr int kMaxConcurrency = 16;

} // namespace

LazyChatLoader::LazyChatLoader(QObject* parent)
    : QObject(parent)
{
}

void LazyChatLoader::setConcurrency(int max)
{
    const int clamped = qBound(kMinConcurrency, max, kMaxConcurrency);
    if (clamped == m_concurrency)
        return;

    m_concurrency = clamped;

    // Raising the cap is permission to start more work, so waiting chats go
    // out right away. Lowering it only stops new starts; chats already in
    // flight are left alone.
    const QVector<qint64> started = startReady();
    emit changed();
    announceStarted(started);
}

void LazyChatLoader::enqueue(qint64 chatId)
{
    if (chatId <= 0 || m_loaded.contains(chatId)
        || m_pending.contains(chatId) || m_inFlight.contains(chatId))
        return;

    m_pending.append(chatId);
    const QVector<qint64> started = startReady();
    emit changed();
    announceStarted(started);
}

void LazyChatLoader::markLoaded(qint64 chatId)
{
    if (chatId <= 0)
        return;

    // A waiting chat can be settled too: it simply never occupied a slot.
    if (!m_inFlight.removeOne(chatId) && !m_pending.removeOne(chatId))
        return;

    m_loaded.insert(chatId);
    const QVector<qint64> started = startReady();
    emit changed();
    announceStarted(started);
}

void LazyChatLoader::markFailed(qint64 chatId)
{
    if (chatId <= 0)
        return;

    if (!m_inFlight.removeOne(chatId) && !m_pending.removeOne(chatId))
        return;

    // A failure is deliberately not recorded: the chat may be enqueued again,
    // but nothing here retries it on its own.
    const QVector<qint64> started = startReady();
    emit changed();
    announceStarted(started);
}

bool LazyChatLoader::isLoaded(qint64 chatId) const
{
    return chatId > 0 && m_loaded.contains(chatId);
}

bool LazyChatLoader::isPending(qint64 chatId) const
{
    return chatId > 0 && m_pending.contains(chatId);
}

QVector<qint64> LazyChatLoader::pending() const
{
    return m_pending;
}

int LazyChatLoader::inFlight() const
{
    return static_cast<int>(m_inFlight.size());
}

int LazyChatLoader::loadedCount() const
{
    return static_cast<int>(m_loaded.size());
}

void LazyChatLoader::clear()
{
    if (m_pending.isEmpty() && m_inFlight.isEmpty() && m_loaded.isEmpty())
        return;

    m_pending.clear();
    m_inFlight.clear();
    m_loaded.clear();
    emit changed();
}

QVector<qint64> LazyChatLoader::startReady()
{
    QVector<qint64> started;
    while (!m_pending.isEmpty()
           && static_cast<int>(m_inFlight.size()) < m_concurrency) {
        const qint64 chatId = m_pending.takeFirst();
        m_inFlight.append(chatId);
        started.append(chatId);
    }
    return started;
}

void LazyChatLoader::announceStarted(const QVector<qint64>& started)
{
    for (const qint64 chatId : started) {
        // changed() listeners run before this loop and may have cleared the
        // loader, so only announce a chat that is still in flight.
        if (m_inFlight.contains(chatId))
            emit loadRequested(chatId);
    }
}

} // namespace beygram::telegram
