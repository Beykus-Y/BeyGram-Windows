#include "StickerSetInstallController.h"

#include <QSet>
#include <utility>

namespace beygram::telegram {
namespace {

// StickerSetInfo has no operator==, and the controller needs "did the list
// really change?" to decide whether to signal. Every field is compared because
// a set can stay in the list while its title, preview or install state changes.
bool sameRow(const StickerSetInfo& lhs, const StickerSetInfo& rhs)
{
    return lhs.id == rhs.id
        && lhs.title == rhs.title
        && lhs.name == rhs.name
        && lhs.stickerCount == rhs.stickerCount
        && lhs.thumbnailFileId == rhs.thumbnailFileId
        && lhs.installed == rhs.installed
        && lhs.archived == rhs.archived;
}

QVector<StickerSetInfo> sanitizedSets(const QVector<StickerSetInfo>& sets)
{
    QVector<StickerSetInfo> rows;
    rows.reserve(sets.size());
    QSet<qint64> seen;
    seen.reserve(sets.size());

    for (const StickerSetInfo& set : sets) {
        if (set.id <= 0 || seen.contains(set.id))
            continue;
        seen.insert(set.id);
        rows.append(set);
    }

    return rows;
}

bool sameSets(const QVector<StickerSetInfo>& lhs, const QVector<StickerSetInfo>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    for (qsizetype i = 0; i < lhs.size(); ++i) {
        if (!sameRow(lhs.at(i), rhs.at(i)))
            return false;
    }

    return true;
}

} // namespace

StickerSetInstallController::StickerSetInstallController(QObject* parent) : QObject(parent)
{
}

void StickerSetInstallController::setSets(QVector<StickerSetInfo> sets)
{
    QVector<StickerSetInfo> rows = sanitizedSets(sets);

    if (sameSets(rows, m_sets))
        return;

    m_sets = std::move(rows);
    emit changed();
}

QVector<StickerSetInfo> StickerSetInstallController::sets() const
{
    return m_sets;
}

int StickerSetInstallController::installedCount() const
{
    int count = 0;
    for (const StickerSetInfo& set : m_sets) {
        if (set.installed)
            ++count;
    }

    return count;
}

bool StickerSetInstallController::isInstalled(qint64 setId) const
{
    if (setId <= 0)
        return false;

    for (const StickerSetInfo& set : m_sets) {
        if (set.id == setId)
            return set.installed;
    }

    return false;
}

bool StickerSetInstallController::setInstalled(qint64 setId, bool installed)
{
    if (setId <= 0)
        return false;

    for (StickerSetInfo& set : m_sets) {
        if (set.id != setId)
            continue;

        if (set.installed == installed)
            return false;

        set.installed = installed;
        emit changed();
        return true;
    }

    return false;
}

qint64 StickerSetInstallController::pendingSetId() const
{
    return m_pending;
}

void StickerSetInstallController::markPending(qint64 setId)
{
    if (setId == 0) {
        clearPending();
        return;
    }

    if (setId < 0)
        return;

    bool known = false;
    for (const StickerSetInfo& set : m_sets) {
        if (set.id == setId) {
            known = true;
            break;
        }
    }

    if (!known || m_pending == setId)
        return;

    m_pending = setId;
    emit changed();
}

void StickerSetInstallController::clearPending()
{
    if (m_pending == 0)
        return;

    m_pending = 0;
    emit changed();
}

void StickerSetInstallController::clear()
{
    if (m_sets.isEmpty() && m_pending == 0)
        return;

    m_sets.clear();
    m_pending = 0;
    emit changed();
}

} // namespace beygram::telegram
