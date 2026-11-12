#include "ProfilePhotoModel.h"

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

bool storesId(const QVector<ProfilePhotoRow>& rows, qint64 id)
{
    return std::any_of(rows.cbegin(), rows.cend(),
                       [id](const ProfilePhotoRow& row) { return row.id == id; });
}

// QVector equality uses ProfilePhotoRow::operator==, so the normalized list can
// be compared as a whole. Keeps setPhotos() from notifying on a no-op assignment.
bool samePhotos(const QVector<ProfilePhotoRow>& lhs,
                const QVector<ProfilePhotoRow>& rhs)
{
    return lhs == rhs;
}

} // namespace

ProfilePhotoModel::ProfilePhotoModel(QObject* parent)
    : QObject(parent)
{
}

void ProfilePhotoModel::setPhotos(QVector<ProfilePhotoRow> photos)
{
    QVector<ProfilePhotoRow> next;
    next.reserve(photos.size());

    for (const ProfilePhotoRow& row : photos) {
        // A row without a positive id cannot be selected or deleted.
        if (row.id <= 0)
            continue;
        // The first row for an id wins; later copies add nothing.
        if (storesId(next, row.id))
            continue;
        next.append(row);
    }

    if (samePhotos(next, m_photos))
        return;

    m_photos = std::move(next);
    emit changed();
}

QVector<ProfilePhotoRow> ProfilePhotoModel::photos() const
{
    return m_photos;
}

int ProfilePhotoModel::count() const
{
    return static_cast<int>(m_photos.size());
}

bool ProfilePhotoModel::remove(qint64 photoId)
{
    for (qsizetype index = 0; index < m_photos.size(); ++index) {
        if (m_photos.at(index).id != photoId)
            continue;

        m_photos.removeAt(index);
        emit changed();
        return true;
    }

    return false;
}

void ProfilePhotoModel::append(QVector<ProfilePhotoRow> photos)
{
    QVector<ProfilePhotoRow> additions;
    additions.reserve(photos.size());

    for (const ProfilePhotoRow& row : photos) {
        if (row.id <= 0)
            continue;
        // Existing rows are never rewritten; an id already on screen wins over
        // the incoming copy, and the first new copy wins over later repeats.
        if (storesId(m_photos, row.id) || storesId(additions, row.id))
            continue;
        additions.append(row);
    }

    if (additions.isEmpty())
        return;

    m_photos.reserve(m_photos.size() + additions.size());
    for (ProfilePhotoRow& row : additions)
        m_photos.append(std::move(row));
    emit changed();
}

void ProfilePhotoModel::clear()
{
    if (m_photos.isEmpty())
        return;

    m_photos.clear();
    emit changed();
}

} // namespace beygram::telegram
