#include "MediaImageProvider.h"

#include "telegram/MediaFileStore.h"

#include <QFileInfo>
#include <QImageReader>

namespace beygram::ui {

MediaImageProvider::MediaImageProvider(telegram::MediaFileStore* store)
    : QQuickImageProvider(QQuickImageProvider::Image), m_store(store)
{
}

QImage MediaImageProvider::requestImage(const QString& id, QSize* size,
                                        const QSize& requestedSize)
{
    if (!m_store) {
        if (size)
            *size = QSize();
        return {};
    }

    const QString fileIdText = id.section(QLatin1Char('?'), 0, 0);
    bool ok = false;
    const int fileId = fileIdText.toInt(&ok);
    if (!ok || fileId == 0) {
        if (size)
            *size = QSize();
        return {};
    }

    const QString path = m_store->path(fileId);
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        if (size)
            *size = QSize();
        return {};
    }

    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (requestedSize.isValid() && requestedSize.width() > 0) {
        QSize target = requestedSize;
        const QSize original = reader.size();
        if (original.isValid() && original.width() > 0) {
            target.setHeight(qMax(1, requestedSize.width() * original.height()
                                         / original.width()));
        }
        reader.setScaledSize(target);
    }

    QImage image = reader.read();
    if (size)
        *size = image.size();
    return image;
}

} // namespace beygram::ui
