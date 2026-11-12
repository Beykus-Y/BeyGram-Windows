#include "SpoilerRevealModel.h"

#include <algorithm>

namespace beygram::telegram {

namespace {

// A cap of zero would make every reveal a no-op, so the lower bound is one.
constexpr int kMinMaxRevealed = 1;
constexpr int kMaxMaxRevealed = 10000;

} // namespace

SpoilerRevealModel::SpoilerRevealModel(QObject* parent) : QObject(parent)
{
}

void SpoilerRevealModel::reveal(qint64 messageId)
{
    if (messageId <= 0 || m_ids.contains(messageId))
        return;

    m_ids.append(messageId);
    if (m_ids.size() > m_max)
        m_ids.removeFirst();
    emit changed();
}

bool SpoilerRevealModel::isRevealed(qint64 messageId) const
{
    return m_ids.contains(messageId);
}

void SpoilerRevealModel::hide(qint64 messageId)
{
    if (m_ids.removeOne(messageId))
        emit changed();
}

void SpoilerRevealModel::hideAll()
{
    if (m_ids.isEmpty())
        return;

    m_ids.clear();
    emit changed();
}

int SpoilerRevealModel::revealedCount() const
{
    return static_cast<int>(m_ids.size());
}

void SpoilerRevealModel::setMaxRevealed(int max)
{
    const int clamped = std::clamp(max, kMinMaxRevealed, kMaxMaxRevealed);
    if (clamped == m_max)
        return;

    m_max = clamped;

    const qsizetype excess = m_ids.size() - m_max;
    if (excess > 0)
        m_ids.remove(0, excess);

    emit changed();
}

int SpoilerRevealModel::maxRevealed() const
{
    return m_max;
}

} // namespace beygram::telegram
