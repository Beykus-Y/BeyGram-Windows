#include "PremiumStatusModel.h"

#include <QSet>

#include <algorithm>
#include <limits>
#include <utility>

namespace beygram::telegram {
namespace {

/// Seconds in a day, the divisor daysLeft() rounds up with.
constexpr qint64 kSecondsPerDay = 24 * 60 * 60;

/// Trims every key, drops the empty ones and keeps the first spelling of each
/// key, comparing case-insensitively to match hasFeature(). Order is preserved
/// so the screen lists features in the order the caller sent them.
QStringList sanitizeFeatures(const QStringList& features)
{
    QStringList kept;
    kept.reserve(features.size());

    QSet<QString> seen;
    seen.reserve(features.size());

    for (const QString& feature : features) {
        const QString trimmed = feature.trimmed();
        if (trimmed.isEmpty())
            continue;

        const QString folded = trimmed.toCaseFolded();
        if (seen.contains(folded))
            continue;

        seen.insert(folded);
        kept.append(trimmed);
    }

    return kept;
}

} // namespace

PremiumStatusModel::PremiumStatusModel(QObject* parent) : QObject(parent)
{
}

void PremiumStatusModel::setPremium(bool premium, qint64 expiresAt)
{
    // An expiry belongs to a Premium account only, and a negative timestamp is
    // an unparsed value rather than a date before the epoch.
    const qint64 nextExpires = premium ? std::max<qint64>(expiresAt, 0) : 0;

    if (m_premium == premium && m_expires == nextExpires)
        return;

    m_premium = premium;
    m_expires = nextExpires;

    emit changed();
}

bool PremiumStatusModel::premium() const
{
    return m_premium;
}

qint64 PremiumStatusModel::expiresAt() const
{
    return m_expires;
}

void PremiumStatusModel::setFeatures(QStringList features)
{
    QStringList next = sanitizeFeatures(features);
    if (next == m_features)
        return;

    m_features = std::move(next);
    emit changed();
}

QStringList PremiumStatusModel::features() const
{
    return m_features;
}

bool PremiumStatusModel::hasFeature(const QString& feature) const
{
    const QString query = feature.trimmed();
    if (query.isEmpty())
        return false;

    for (const QString& stored : m_features) {
        if (stored.compare(query, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

int PremiumStatusModel::daysLeft(qint64 nowUnix) const
{
    // Not Premium, or no expiry known: there is no countdown to show.
    if (!m_premium || m_expires <= 0)
        return 0;

    const qint64 remaining = m_expires - nowUnix;
    if (remaining <= 0)
        return 0;

    // Round up, so any part of a day still counts as a whole day left. Written
    // as a division plus a remainder rather than (remaining + 86399) / 86400 so
    // a timestamp near the end of the range cannot overflow the addition.
    const qint64 wholeDays = remaining / kSecondsPerDay;
    const qint64 days = wholeDays + (remaining % kSecondsPerDay != 0 ? 1 : 0);

    // A countdown never exceeds an int on real timestamps; the clamp only keeps
    // the narrowing conversion defined.
    const qint64 capped = std::min<qint64>(days, std::numeric_limits<int>::max());
    return static_cast<int>(capped);
}

QString PremiumStatusModel::badgeText() const
{
    return m_premium ? QStringLiteral("Premium") : QString();
}

void PremiumStatusModel::clear()
{
    if (!m_premium && m_expires == 0 && m_features.isEmpty())
        return;

    m_premium = false;
    m_expires = 0;
    m_features.clear();

    emit changed();
}

} // namespace beygram::telegram
