#include "MediaImageProvider.h"

#include "telegram/MediaFileStore.h"

#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>

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
    if (image.isNull()) {
        if (size)
            *size = QSize();
        return image;
    }

    if (id.contains(QStringLiteral("round=1"))) {
        const int side = qMin(image.width(), image.height());
        if (side <= 0) {
            if (size)
                *size = QSize();
            return {};
        }
        image = image.copy((image.width() - side) / 2, (image.height() - side) / 2,
                           side, side);
        if (image.width() > 512 || image.height() > 512) {
            image = image.scaled(512, 512, Qt::KeepAspectRatioByExpanding,
                                 Qt::SmoothTransformation);
        }
        QImage rounded(image.size(), QImage::Format_ARGB32_Premultiplied);
        rounded.fill(Qt::transparent);
        QPainter painter(&rounded);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath path;
        path.addEllipse(QRectF(0, 0, image.width(), image.height()));
        painter.setClipPath(path);
        painter.drawImage(0, 0, image);
        painter.end();
        image = rounded;
    }

    if (size)
        *size = image.size();
    return image;
}

} // namespace beygram::ui
