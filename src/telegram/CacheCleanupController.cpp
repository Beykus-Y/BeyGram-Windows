#include "CacheCleanupController.h"

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

/// Drops blank kinds and clamps negative sizes so the stored map is always a
/// usable candidate list.
QHash<QString, qint64> normalizedCandidates(const QHash<QString, qint64>& bytesByKind)
{
    QHash<QString, qint64> normalized;
    normalized.reserve(bytesByKind.size());
    for (auto it = bytesByKind.cbegin(); it != bytesByKind.cend(); ++it) {
        if (it.key().trimmed().isEmpty())
            continue;
        normalized.insert(it.key(), std::max<qint64>(it.value(), 0));
    }
    return normalized;
}

/// Keeps the selected kinds that are still candidates, preserving their order.
QStringList keptSelection(const QStringList& selected, const QHash<QString, qint64>& known)
{
    QStringList kept;
    kept.reserve(selected.size());
    for (const QString& kind : selected) {
        if (known.contains(kind))
            kept.append(kind);
    }
    return kept;
}

} // namespace

CacheCleanupController::CacheCleanupController(QObject* parent)
    : QObject(parent)
{
}

void CacheCleanupController::setCandidates(QHash<QString, qint64> bytesByKind)
{
    QHash<QString, qint64> normalized = normalizedCandidates(bytesByKind);
    QStringList kept = keptSelection(m_selected, normalized);

    if (normalized == m_bytes && kept == m_selected)
        return;

    m_bytes = std::move(normalized);
    m_selected = std::move(kept);
    emit changed();
}

QHash<QString, qint64> CacheCleanupController::candidates() const
{
    return m_bytes;
}

void CacheCleanupController::setSelected(const QStringList& kinds)
{
    QStringList kept;
    kept.reserve(kinds.size());
    for (const QString& kind : kinds) {
        // Unknown kinds cannot be cleaned, and a repeated tick is still one kind.
        if (!m_bytes.contains(kind) || kept.contains(kind))
            continue;
        kept.append(kind);
    }
    std::sort(kept.begin(), kept.end());

    if (kept == m_selected)
        return;

    m_selected = std::move(kept);
    emit changed();
}

QStringList CacheCleanupController::selected() const
{
    return m_selected;
}

qint64 CacheCleanupController::selectedBytes() const
{
    qint64 total = 0;
    for (const QString& kind : m_selected)
        total += m_bytes.value(kind);
    return total;
}

bool CacheCleanupController::canClean() const
{
    return !m_running && !m_selected.isEmpty() && selectedBytes() > 0;
}

void CacheCleanupController::setRunning(bool running)
{
    if (m_running == running)
        return;
    m_running = running;
    emit changed();
}

bool CacheCleanupController::running() const
{
    return m_running;
}

void CacheCleanupController::clear()
{
    if (m_bytes.isEmpty() && m_selected.isEmpty() && !m_running)
        return;

    m_bytes.clear();
    m_selected.clear();
    m_running = false;
    emit changed();
}

} // namespace beygram::telegram
