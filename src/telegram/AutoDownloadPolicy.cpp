#include "AutoDownloadPolicy.h"

#include <algorithm>
#include <initializer_list>
#include <utility>

namespace beygram::telegram {
namespace {

constexpr int MiB = 1024 * 1024;

// The same conservative buckets the Android client ships with: WiFi fetches
// photos, videos and files up to a generous size, mobile data keeps only small
// previews, and roaming never downloads anything without a tap.
DownloadLimits defaultLimits(NetworkKind network)
{
    switch (network) {
    case NetworkKind::WiFi:
        return DownloadLimits{10 * MiB, 50 * MiB, 20 * MiB};
    case NetworkKind::Mobile:
        return DownloadLimits{1 * MiB, 5 * MiB, 2 * MiB};
    case NetworkKind::Roaming:
        return DownloadLimits{};
    }
    return DownloadLimits{};
}

// A negative ceiling is not "unlimited": the user asked for a budget, and a
// value that cannot be a budget must fall back to the safe "never".
DownloadLimits sanitized(DownloadLimits limits)
{
    limits.photoBytes = std::max(limits.photoBytes, 0);
    limits.videoBytes = std::max(limits.videoBytes, 0);
    limits.fileBytes = std::max(limits.fileBytes, 0);
    return limits;
}

// allows() carries no media kind, so the widest ceiling is the only choice that
// never refuses a size a per-kind rule would have accepted.
int widestLimit(const DownloadLimits& limits)
{
    return std::max({limits.photoBytes, limits.videoBytes, limits.fileBytes});
}

int keyFor(NetworkKind network)
{
    return static_cast<int>(network);
}

} // namespace

AutoDownloadPolicy::AutoDownloadPolicy(QObject* parent)
    : QObject(parent)
{
    m_limits.reserve(3);
    for (const NetworkKind network : {NetworkKind::WiFi, NetworkKind::Mobile, NetworkKind::Roaming})
        m_limits.insert(keyFor(network), defaultLimits(network));
}

void AutoDownloadPolicy::setLimits(NetworkKind network, DownloadLimits limits)
{
    const DownloadLimits clean = sanitized(limits);
    const int key = keyFor(network);

    const auto existing = m_limits.constFind(key);
    if (existing != m_limits.constEnd() && *existing == clean)
        return;

    m_limits.insert(key, clean);
    emit changed();
}

DownloadLimits AutoDownloadPolicy::limits(NetworkKind network) const
{
    const auto it = m_limits.constFind(keyFor(network));
    if (it == m_limits.constEnd())
        return defaultLimits(network);
    return *it;
}

void AutoDownloadPolicy::setAutoDownload(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit changed();
}

bool AutoDownloadPolicy::autoDownload() const
{
    return m_enabled;
}

bool AutoDownloadPolicy::allows(NetworkKind network, qint64 sizeBytes) const
{
    if (!m_enabled)
        return false;
    // A negative size is not a known size; it cannot be compared to a budget.
    if (sizeBytes < 0)
        return false;

    const int limit = widestLimit(limits(network));
    if (limit <= 0)
        return false;
    return sizeBytes <= limit;
}

void AutoDownloadPolicy::setPreloadStories(bool enabled)
{
    if (m_stories == enabled)
        return;
    m_stories = enabled;
    emit changed();
}

bool AutoDownloadPolicy::preloadStories() const
{
    return m_stories;
}

void AutoDownloadPolicy::resetToDefaults()
{
    QHash<int, DownloadLimits> defaults;
    defaults.reserve(3);
    for (const NetworkKind network : {NetworkKind::WiFi, NetworkKind::Mobile, NetworkKind::Roaming})
        defaults.insert(keyFor(network), defaultLimits(network));

    const bool differs = defaults != m_limits || !m_enabled || m_stories;
    if (!differs)
        return;

    m_limits = std::move(defaults);
    m_enabled = true;
    m_stories = false;
    emit changed();
}

} // namespace beygram::telegram
