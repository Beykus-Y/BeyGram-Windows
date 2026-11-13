#include "MediaWarmup.h"

#include <QPair>

#include <algorithm>

namespace beygram::telegram {

namespace {
constexpr int kMinBudget = 1;
constexpr int kMaxBudget = 64;
} // namespace

MediaWarmup::MediaWarmup(QObject* parent)
    : QObject(parent)
{
}

void MediaWarmup::setBudget(int maxPerBatch)
{
    const int clamped = qBound(kMinBudget, maxPerBatch, kMaxBudget);
    if (clamped == m_budget)
        return;

    m_budget = clamped;
    emit changed();
}

void MediaWarmup::requestAvatar(int fileId, int priority)
{
    request(fileId, priority, true);
}

void MediaWarmup::requestThumbnail(int fileId, int priority)
{
    request(fileId, priority, false);
}

void MediaWarmup::request(int fileId, int priority, bool avatar)
{
    // A file that is already downloading or warm needs no second transfer, and
    // a non-positive id is not a TDLib file at all.
    if (fileId <= 0 || m_ready.contains(fileId) || m_inFlight.contains(fileId))
        return;

    const auto it = m_pending.find(fileId);
    if (it == m_pending.end()) {
        Entry entry;
        entry.priority = priority;
        entry.avatar = avatar;
        entry.sequence = m_nextSequence++;
        m_pending.insert(fileId, entry);
        emit changed();
        emit downloadRequested(fileId, entry.priority);
        return;
    }

    Entry& entry = it.value();
    const bool becomesAvatar = avatar && !entry.avatar;
    const bool raisesPriority = priority > entry.priority;
    if (!becomesAvatar && !raisesPriority)
        return;

    // The sequence is deliberately untouched: an upgrade changes the rank, not
    // the arrival order, so the id keeps its place among equal entries.
    entry.avatar = entry.avatar || avatar;
    entry.priority = qMax(entry.priority, priority);
    emit changed();
    emit downloadRequested(fileId, entry.priority);
}

QVector<int> MediaWarmup::nextBatch()
{
    QVector<int> batch;
    if (m_pending.isEmpty())
        return batch;

    QVector<QPair<int, Entry>> ranked;
    ranked.reserve(m_pending.size());
    for (auto it = m_pending.cbegin(); it != m_pending.cend(); ++it)
        ranked.append(qMakePair(it.key(), it.value()));

    std::sort(ranked.begin(), ranked.end(),
              [](const QPair<int, Entry>& left, const QPair<int, Entry>& right) {
                  const Entry& a = left.second;
                  const Entry& b = right.second;
                  if (a.priority != b.priority)
                      return a.priority > b.priority;
                  if (a.avatar != b.avatar)
                      return a.avatar;
                  return a.sequence < b.sequence;
              });

    const int take = qMin(m_budget, static_cast<int>(ranked.size()));
    batch.reserve(take);
    for (int i = 0; i < take; ++i) {
        const int fileId = ranked.at(i).first;
        m_pending.remove(fileId);
        m_inFlight.insert(fileId);
        batch.append(fileId);
    }

    if (!batch.isEmpty())
        emit changed();
    return batch;
}

void MediaWarmup::markReady(int fileId)
{
    if (fileId <= 0)
        return;

    // An id that is already ready is not removed from anywhere, so a repeated
    // markReady() is silent. The same is true for an id nobody requested.
    const bool wasPending = m_pending.remove(fileId) > 0;
    const bool wasInFlight = m_inFlight.remove(fileId);
    if (!wasPending && !wasInFlight)
        return;

    m_ready.insert(fileId);
    emit changed();
}

void MediaWarmup::markFailed(int fileId)
{
    if (fileId <= 0)
        return;

    // A failure also invalidates a previously ready id: the cached file cannot
    // be trusted, so the id becomes requestable again.
    const bool wasPending = m_pending.remove(fileId) > 0;
    const bool wasInFlight = m_inFlight.remove(fileId);
    const bool wasReady = m_ready.remove(fileId);
    if (!wasPending && !wasInFlight && !wasReady)
        return;

    emit changed();
}

bool MediaWarmup::isReady(int fileId) const
{
    return fileId > 0 && m_ready.contains(fileId);
}

bool MediaWarmup::isQueued(int fileId) const
{
    return fileId > 0 && m_pending.contains(fileId);
}

int MediaWarmup::queuedCount() const
{
    return static_cast<int>(m_pending.size());
}

void MediaWarmup::clear()
{
    if (m_pending.isEmpty() && m_inFlight.isEmpty() && m_ready.isEmpty())
        return;

    m_pending.clear();
    m_inFlight.clear();
    m_ready.clear();
    emit changed();
}

} // namespace beygram::telegram
