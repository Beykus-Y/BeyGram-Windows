#include "GalleryFacade.h"

#include "telegram/AlbumModel.h"
#include "telegram/MediaGalleryController.h"

#include <QVector>

#include <utility>

namespace beygram::ui {
namespace {

/// QML passes ids as strings. A value that is not a number cannot address a
/// message, so it is read as 0 and then dropped by the controller or the album
/// model like any other non-positive id.
qint64 parseId(const QString& id)
{
    bool ok = false;
    const qint64 value = id.trimmed().toLongLong(&ok);
    return ok ? value : 0;
}

} // namespace

GalleryFacade::GalleryFacade(QObject* parent)
    : QObject(parent),
      m_gallery(new telegram::MediaGalleryController(this)),
      m_albums(new telegram::AlbumModel(this))
{
    connect(m_gallery, &telegram::MediaGalleryController::changed, this,
            &GalleryFacade::handleGalleryChanged);
    connect(m_albums, &telegram::AlbumModel::changed, this,
            &GalleryFacade::handleAlbumsChanged);
}

bool GalleryFacade::open() const
{
    return m_gallery->active();
}

QString GalleryFacade::currentId() const
{
    const qint64 id = m_gallery->currentMessageId();
    return id > 0 ? QString::number(id) : QString();
}

int GalleryFacade::index() const
{
    return m_gallery->currentIndex();
}

int GalleryFacade::count() const
{
    return static_cast<int>(m_gallery->messageIds().size());
}

double GalleryFacade::zoom() const
{
    return m_gallery->zoom();
}

void GalleryFacade::openGallery(const QStringList& messageIds, const QString& currentId)
{
    QVector<qint64> ids;
    ids.reserve(messageIds.size());
    for (const QString& id : messageIds)
        ids.append(parseId(id));

    m_gallery->open(std::move(ids), parseId(currentId));
}

void GalleryFacade::close()
{
    m_gallery->close();
}

void GalleryFacade::next()
{
    m_gallery->next();
}

void GalleryFacade::previous()
{
    m_gallery->previous();
}

void GalleryFacade::setZoom(double zoom)
{
    // The controller owns the range and ignores non-finite factors, so the
    // facade reads the clamped value back instead of duplicating the clamp.
    m_gallery->setZoom(zoom);
}

void GalleryFacade::resetZoom()
{
    m_gallery->resetZoom();
}

void GalleryFacade::setAlbums(const QStringList& groupedIds, const QStringList& messageIds)
{
    // The lists are parallel, so a different length describes inconsistent
    // data. Rejecting it keeps the previous grouping instead of half of it.
    if (groupedIds.size() != messageIds.size())
        return;

    QVector<telegram::AlbumMediaItem> items;
    items.reserve(messageIds.size());
    for (qsizetype index = 0; index < messageIds.size(); ++index) {
        telegram::AlbumMediaItem item;
        item.messageId = parseId(messageIds.at(index));
        item.groupedId = parseId(groupedIds.at(index));
        items.append(item);
    }

    m_albums->setItems(std::move(items));
}

QStringList GalleryFacade::albumMessageIds(const QString& messageId) const
{
    const qint64 target = parseId(messageId);
    if (target <= 0)
        return {};

    const QVector<telegram::AlbumEntry> albums = m_albums->albums();
    for (const telegram::AlbumEntry& album : albums) {
        QStringList ids;
        ids.reserve(album.items.size());

        bool found = false;
        for (const telegram::AlbumMediaItem& item : album.items) {
            ids.append(QString::number(item.messageId));
            if (item.messageId == target)
                found = true;
        }

        if (found)
            return ids;
    }

    return {};
}

void GalleryFacade::handleGalleryChanged()
{
    const QString current = currentId();
    const bool moved = current != m_lastCurrentId;
    m_lastCurrentId = current;

    // The item on screen is the one thing the viewer animates, so it is
    // announced on its own before the broad notification.
    if (moved)
        emit currentChanged(current);
    emit changed();
}

void GalleryFacade::handleAlbumsChanged()
{
    emit changed();
}

} // namespace beygram::ui
