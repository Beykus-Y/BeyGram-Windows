#include "BoostModel.h"

#include <QSet>

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

/// A user id identifies a booster, so a row without a positive id is dropped
/// (it could be neither shown nor acted on) and a repeated id keeps its first
/// copy, matching how the session and member lists treat their ids.
QVector<BoostRow> sanitizeBoosters(const QVector<BoostRow>& boosters)
{
    QVector<BoostRow> kept;
    kept.reserve(boosters.size());

    QSet<qint64> seenUserIds;
    seenUserIds.reserve(boosters.size());

    for (BoostRow row : boosters) {
        if (row.userId <= 0)
            continue;
        if (seenUserIds.contains(row.userId))
            continue;
        seenUserIds.insert(row.userId);
        row.boostCount = std::max(row.boostCount, 0);
        kept.append(row);
    }

    return kept;
}

bool sameRow(const BoostRow& left, const BoostRow& right)
{
    return left.userId == right.userId && left.name == right.name
        && left.boostCount == right.boostCount
        && left.expirationDate == right.expirationDate;
}

bool sameRows(const QVector<BoostRow>& left, const QVector<BoostRow>& right)
{
    if (left.size() != right.size())
        return false;

    for (qsizetype index = 0; index < left.size(); ++index) {
        if (!sameRow(left.at(index), right.at(index)))
            return false;
    }
    return true;
}

} // namespace

BoostModel::BoostModel(QObject* parent) : QObject(parent)
{
}

void BoostModel::setStatus(int level, int boostCount, int nextLevelBoostCount)
{
    const int nextLevel = std::max(level, 0);
    const int nextBoosts = std::max(boostCount, 0);
    const int nextTarget = std::max(nextLevelBoostCount, 0);

    if (nextLevel == m_level && nextBoosts == m_boosts && nextTarget == m_next)
        return;

    m_level = nextLevel;
    m_boosts = nextBoosts;
    m_next = nextTarget;

    emit changed();
}

int BoostModel::level() const
{
    return m_level;
}

int BoostModel::boostCount() const
{
    return m_boosts;
}

int BoostModel::nextLevelBoostCount() const
{
    return m_next;
}

double BoostModel::progressToNextLevel() const
{
    // Without a target there is nothing to fill, and a chat with no boosts sits
    // at the start of the bar rather than at a division by zero.
    if (m_next <= 0 || m_boosts <= 0)
        return 0.0;

    const double ratio = static_cast<double>(m_boosts) / static_cast<double>(m_next);
    return std::clamp(ratio, 0.0, 1.0);
}

void BoostModel::setBoosters(QVector<BoostRow> boosters)
{
    QVector<BoostRow> next = sanitizeBoosters(boosters);
    if (sameRows(next, m_boosters))
        return;

    m_boosters = std::move(next);
    emit changed();
}

QVector<BoostRow> BoostModel::boosters() const
{
    return m_boosters;
}

int BoostModel::boosterCount() const
{
    return static_cast<int>(m_boosters.size());
}

void BoostModel::clear()
{
    if (m_boosters.isEmpty() && m_level == 0 && m_boosts == 0 && m_next == 0)
        return;

    m_boosters.clear();
    m_level = 0;
    m_boosts = 0;
    m_next = 0;

    emit changed();
}

} // namespace beygram::telegram
