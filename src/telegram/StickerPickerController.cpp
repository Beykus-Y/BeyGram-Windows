#include "StickerPickerController.h"

#include <QSet>
#include <utility>

namespace beygram::telegram {
namespace {

// StickerSetInfo has no operator==, and the controller needs "did the strip
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

QVector<qint64> sanitizedStickerIds(const QVector<qint64>& stickerIds)
{
    QVector<qint64> ids;
    ids.reserve(stickerIds.size());
    QSet<qint64> seen;
    seen.reserve(stickerIds.size());

    for (const qint64 id : stickerIds) {
        if (id <= 0 || seen.contains(id))
            continue;
        seen.insert(id);
        ids.append(id);
    }

    return ids;
}

} // namespace

StickerPickerController::StickerPickerController(QObject* parent) : QObject(parent)
{
}

void StickerPickerController::setSets(QVector<StickerSetRow> sets)
{
    QVector<StickerSetInfo> rows = sanitizedSets(sets);

    if (sameSets(rows, m_sets))
        return;

    m_sets = std::move(rows);
    emit changed();
}

QVector<StickerSetRow> StickerPickerController::sets() const
{
    return m_sets;
}

int StickerPickerController::setCount() const
{
    return static_cast<int>(m_sets.size());
}

void StickerPickerController::selectSet(qint64 setId)
{
    // 0 is the recent/favourites tab, which exists without a catalog entry.
    if (setId != 0) {
        bool known = false;
        for (const StickerSetInfo& set : m_sets) {
            if (set.id == setId) {
                known = true;
                break;
            }
        }

        if (!known)
            return;
    }

    if (m_currentSetId == setId)
        return;

    m_currentSetId = setId;
    m_stickers.clear();
    m_loading = false;
    emit changed();
}

qint64 StickerPickerController::currentSetId() const
{
    return m_currentSetId;
}

void StickerPickerController::setStickers(QVector<qint64> stickerIds)
{
    QVector<qint64> ids = sanitizedStickerIds(stickerIds);

    if (ids == m_stickers)
        return;

    m_stickers = std::move(ids);
    emit changed();
}

QVector<qint64> StickerPickerController::stickers() const
{
    return m_stickers;
}

void StickerPickerController::setLoading(bool loading)
{
    if (m_loading == loading)
        return;

    m_loading = loading;
    emit changed();
}

bool StickerPickerController::loading() const
{
    return m_loading;
}

void StickerPickerController::clear()
{
    if (m_sets.isEmpty() && m_currentSetId == 0 && m_stickers.isEmpty() && !m_loading)
        return;

    m_sets.clear();
    m_currentSetId = 0;
    m_stickers.clear();
    m_loading = false;
    emit changed();
}

} // namespace beygram::telegram
