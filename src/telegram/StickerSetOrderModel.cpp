#include "StickerSetOrderModel.h"

#include <utility>

namespace beygram::telegram {
namespace {

bool sameSet(const StickerSetInfo& left, const StickerSetInfo& right)
{
    return left.id == right.id && left.title == right.title && left.name == right.name
        && left.stickerCount == right.stickerCount
        && left.thumbnailFileId == right.thumbnailFileId
        && left.installed == right.installed && left.archived == right.archived;
}

bool sameSets(const QVector<StickerSetInfo>& left, const QVector<StickerSetInfo>& right)
{
    if (left.size() != right.size())
        return false;

    for (qsizetype i = 0; i < left.size(); ++i) {
        if (!sameSet(left.at(i), right.at(i)))
            return false;
    }

    return true;
}

int indexOfSet(const QVector<StickerSetInfo>& sets, qint64 setId)
{
    for (int i = 0; i < sets.size(); ++i) {
        if (sets.at(i).id == setId)
            return i;
    }
    return -1;
}

int indexOfId(const QVector<qint64>& ids, qint64 id)
{
    for (int i = 0; i < ids.size(); ++i) {
        if (ids.at(i) == id)
            return i;
    }
    return -1;
}

// Drops non-positive ids and keeps the first occurrence of every id, so the
// stored order is the incoming order without duplicates.
QVector<StickerSetInfo> normalized(const QVector<StickerSetInfo>& sets)
{
    QVector<StickerSetInfo> next;
    next.reserve(sets.size());

    for (const StickerSetInfo& set : sets) {
        if (set.id <= 0)
            continue;
        if (indexOfSet(next, set.id) >= 0)
            continue;
        next.append(set);
    }

    return next;
}

// Keeps the pinned order but forgets ids that are no longer in the sets, so a
// catalog refresh cannot leave a pin pointing at nothing.
QVector<qint64> survivingPins(const QVector<qint64>& pinned,
                              const QVector<StickerSetInfo>& sets)
{
    QVector<qint64> next;
    next.reserve(pinned.size());

    for (qint64 setId : pinned) {
        if (indexOfSet(sets, setId) >= 0)
            next.append(setId);
    }

    return next;
}

} // namespace

StickerSetOrderModel::StickerSetOrderModel(QObject* parent)
    : QObject(parent)
{
}

void StickerSetOrderModel::setSets(const QVector<StickerSetInfo>& sets)
{
    QVector<StickerSetInfo> next = normalized(sets);
    QVector<qint64> nextPinned = survivingPins(m_pinned, next);

    // A refresh that normalizes to the stored state is not a change; TDLib can
    // repeat the same sets after every request.
    if (sameSets(next, m_sets) && nextPinned == m_pinned)
        return;

    m_sets = std::move(next);
    m_pinned = std::move(nextPinned);
    emit changed();
}

QVector<StickerSetInfo> StickerSetOrderModel::sets() const
{
    return m_sets;
}

int StickerSetOrderModel::count() const
{
    return int(m_sets.size());
}

bool StickerSetOrderModel::move(qint64 setId, int newIndex)
{
    const int index = indexOfSet(m_sets, setId);
    if (index < 0)
        return false;
    if (newIndex < 0 || newIndex >= m_sets.size())
        return false;
    if (index == newIndex)
        return false;

    const StickerSetInfo moved = m_sets.at(index);
    m_sets.removeAt(index);
    m_sets.insert(newIndex, moved);

    emit changed();
    return true;
}

bool StickerSetOrderModel::pin(qint64 setId)
{
    if (indexOfSet(m_sets, setId) < 0)
        return false;

    const int index = indexOfId(m_pinned, setId);
    // The set is already the first pin, so the pinned order is unchanged.
    if (index == 0)
        return false;

    // Pinning an id that is pinned further back moves it to the front instead
    // of listing it twice; it is never unpinned.
    if (index > 0)
        m_pinned.removeAt(index);

    m_pinned.prepend(setId);
    emit changed();
    return true;
}

QVector<qint64> StickerSetOrderModel::pinned() const
{
    return m_pinned;
}

QVector<StickerSetInfo> StickerSetOrderModel::ordered() const
{
    QVector<StickerSetInfo> next;
    next.reserve(m_sets.size());

    for (qint64 setId : m_pinned) {
        const int index = indexOfSet(m_sets, setId);
        if (index >= 0)
            next.append(m_sets.at(index));
    }

    for (const StickerSetInfo& set : m_sets) {
        if (m_pinned.contains(set.id))
            continue;
        next.append(set);
    }

    return next;
}

void StickerSetOrderModel::clear()
{
    if (m_sets.isEmpty() && m_pinned.isEmpty())
        return;

    m_sets.clear();
    m_pinned.clear();
    emit changed();
}

} // namespace beygram::telegram
