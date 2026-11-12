#include "ShareTargets.h"

#include <QSet>
#include <QtGlobal>

#include <utility>

namespace beygram::telegram {

namespace {

/// Positive and unique, first occurrence wins, input order preserved.
QVector<ShareTarget> usableTargets(const QVector<ShareTarget>& targets)
{
    QVector<ShareTarget> result;
    result.reserve(targets.size());
    QSet<qint64> seen;
    seen.reserve(targets.size());
    for (const ShareTarget& target : targets) {
        if (target.chatId <= 0 || seen.contains(target.chatId))
            continue;
        seen.insert(target.chatId);
        result.append(target);
    }
    return result;
}

bool sameTarget(const ShareTarget& lhs, const ShareTarget& rhs)
{
    return lhs.chatId == rhs.chatId && lhs.title == rhs.title
        && lhs.subtitle == rhs.subtitle && lhs.kind == rhs.kind
        && lhs.unreadCount == rhs.unreadCount;
}

bool sameTargets(const QVector<ShareTarget>& lhs, const QVector<ShareTarget>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;
    for (int i = 0; i < lhs.size(); ++i) {
        if (!sameTarget(lhs.at(i), rhs.at(i)))
            return false;
    }
    return true;
}

/// A cap below one would forget the chat that was just used.
int clampedMaxRecent(int maxRecent)
{
    return qMax(1, maxRecent);
}

/// Positive and unique, first occurrence wins, stops at the cap so a too-long
/// list loses its tail (the oldest entries).
QVector<qint64> normalizedRecent(const QVector<qint64>& chatIds, int maxRecent)
{
    const int cap = clampedMaxRecent(maxRecent);

    QVector<qint64> result;
    result.reserve(qMin(chatIds.size(), static_cast<qsizetype>(cap)));
    for (const qint64 chatId : chatIds) {
        if (chatId <= 0 || result.contains(chatId))
            continue;
        result.append(chatId);
        if (result.size() >= cap)
            break;
    }
    return result;
}

} // namespace

ShareTargetModel::ShareTargetModel(QObject* parent)
    : QObject(parent)
{
}

void ShareTargetModel::setTargets(QVector<ShareTarget> targets)
{
    QVector<ShareTarget> usable = usableTargets(targets);
    if (sameTargets(usable, m_targets))
        return;

    m_targets = std::move(usable);
    emit changed();
}

QVector<ShareTarget> ShareTargetModel::targets() const
{
    return m_targets;
}

QVector<ShareTarget> ShareTargetModel::filtered(const QString& query) const
{
    const QString needle = query.trimmed();
    if (needle.isEmpty())
        return m_targets;

    QVector<ShareTarget> result;
    result.reserve(m_targets.size());
    for (const ShareTarget& target : m_targets) {
        if (target.title.contains(needle, Qt::CaseInsensitive)
            || target.subtitle.contains(needle, Qt::CaseInsensitive)) {
            result.append(target);
        }
    }
    return result;
}

int ShareTargetModel::count() const
{
    return int(m_targets.size());
}

void ShareTargetModel::setRecent(const QVector<qint64>& chatIds)
{
    QVector<qint64> next = normalizedRecent(chatIds, kDefaultMaxRecent);
    if (next == m_recent)
        return;

    m_recent = std::move(next);
    emit changed();
}

QVector<qint64> ShareTargetModel::recent() const
{
    return m_recent;
}

QVector<ShareTarget> ShareTargetModel::recentTargets() const
{
    QVector<ShareTarget> result;
    result.reserve(qMin(m_recent.size(), m_targets.size()));
    for (const qint64 chatId : m_recent) {
        for (const ShareTarget& target : m_targets) {
            if (target.chatId == chatId) {
                result.append(target);
                break;
            }
        }
    }
    return result;
}

void ShareTargetModel::addRecent(qint64 chatId, int maxRecent)
{
    if (chatId <= 0)
        return;

    const int cap = clampedMaxRecent(maxRecent);

    QVector<qint64> next;
    next.reserve(qMin(m_recent.size() + 1, static_cast<qsizetype>(cap)));
    next.append(chatId);
    for (const qint64 existing : std::as_const(m_recent)) {
        // The chat moved to the front instead of being listed twice.
        if (existing == chatId)
            continue;
        if (next.size() >= cap)
            break;
        next.append(existing);
    }

    // Re-sending to the newest chat, or an id that normalizes away, is silent.
    if (next == m_recent)
        return;

    m_recent = std::move(next);
    emit changed();
}

void ShareTargetModel::clear()
{
    if (m_targets.isEmpty() && m_recent.isEmpty())
        return;

    m_targets.clear();
    m_recent.clear();
    emit changed();
}

} // namespace beygram::telegram
