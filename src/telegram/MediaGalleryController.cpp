#include "MediaGalleryController.h"

#include <QSet>

#include <algorithm>
#include <cmath>
#include <utility>

namespace beygram::telegram {
namespace {

constexpr double MinZoom = 1.0;
constexpr double MaxZoom = 5.0;
constexpr double MaxPan = 1.0;

/// A message id identifies a gallery item, so a repeated id keeps its first
/// copy and an id that cannot address a message is dropped.
QVector<qint64> cleanIds(const QVector<qint64>& ids)
{
    QVector<qint64> kept;
    kept.reserve(ids.size());

    QSet<qint64> seen;
    seen.reserve(ids.size());

    for (const qint64 id : ids) {
        if (id <= 0)
            continue;
        if (seen.contains(id))
            continue;
        seen.insert(id);
        kept.append(id);
    }

    return kept;
}

} // namespace

MediaGalleryController::MediaGalleryController(QObject* parent) : QObject(parent)
{
}

void MediaGalleryController::open(QVector<qint64> messageIds, qint64 currentMessageId)
{
    QVector<qint64> next = cleanIds(messageIds);
    // An empty gallery has nothing to show, so it must not replace the one that
    // is already open.
    if (next.isEmpty())
        return;

    int index = static_cast<int>(next.indexOf(currentMessageId));
    if (index < 0)
        index = 0;

    // Reopening the same gallery with the same current item and an untouched
    // view changes nothing.
    if (m_index == index && m_ids == next && m_zoom == MinZoom && m_panX == 0.0
        && m_panY == 0.0) {
        return;
    }

    m_ids = std::move(next);
    m_index = index;
    m_zoom = MinZoom;
    m_panX = 0.0;
    m_panY = 0.0;
    emit changed();
}

void MediaGalleryController::close()
{
    if (!active())
        return;

    m_ids.clear();
    m_index = -1;
    m_zoom = MinZoom;
    m_panX = 0.0;
    m_panY = 0.0;
    emit changed();
}

bool MediaGalleryController::active() const
{
    return m_index >= 0 && m_index < static_cast<int>(m_ids.size());
}

QVector<qint64> MediaGalleryController::messageIds() const
{
    return m_ids;
}

int MediaGalleryController::currentIndex() const
{
    return m_index;
}

qint64 MediaGalleryController::currentMessageId() const
{
    if (!active())
        return 0;
    return m_ids.at(m_index);
}

bool MediaGalleryController::next()
{
    if (!active() || m_index + 1 >= static_cast<int>(m_ids.size()))
        return false;

    ++m_index;
    // The view state belongs to the item that was on screen.
    m_zoom = MinZoom;
    m_panX = 0.0;
    m_panY = 0.0;
    emit changed();
    return true;
}

bool MediaGalleryController::previous()
{
    if (!active() || m_index <= 0)
        return false;

    --m_index;
    m_zoom = MinZoom;
    m_panX = 0.0;
    m_panY = 0.0;
    emit changed();
    return true;
}

void MediaGalleryController::setZoom(double zoom)
{
    // A non-finite factor has no meaningful clamp, so it is ignored.
    if (!std::isfinite(zoom))
        return;

    const double next = std::clamp(zoom, MinZoom, MaxZoom);
    if (next == m_zoom)
        return;

    m_zoom = next;
    emit changed();
}

double MediaGalleryController::zoom() const
{
    return m_zoom;
}

void MediaGalleryController::setPan(double x, double y)
{
    // Both axes are applied together, so a non-finite axis rejects the call
    // instead of leaving the pan half updated.
    if (!std::isfinite(x) || !std::isfinite(y))
        return;

    const double nextX = std::clamp(x, -MaxPan, MaxPan);
    const double nextY = std::clamp(y, -MaxPan, MaxPan);
    if (nextX == m_panX && nextY == m_panY)
        return;

    m_panX = nextX;
    m_panY = nextY;
    emit changed();
}

double MediaGalleryController::panX() const
{
    return m_panX;
}

double MediaGalleryController::panY() const
{
    return m_panY;
}

void MediaGalleryController::resetZoom()
{
    if (m_zoom == MinZoom && m_panX == 0.0 && m_panY == 0.0)
        return;

    m_zoom = MinZoom;
    m_panX = 0.0;
    m_panY = 0.0;
    emit changed();
}

} // namespace beygram::telegram
