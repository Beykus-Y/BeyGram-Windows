#include "QrCodeImageProvider.h"

#include "qrcodegen.hpp"

#include <QPainter>

#include <optional>

namespace beygram::ui {

QrCodeImageProvider::QrCodeImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

void QrCodeImageProvider::setLink(const QString& link)
{
    QMutexLocker locker(&m_mutex);
    m_link = link;
}

QImage QrCodeImageProvider::requestImage(const QString&, QSize* size, const QSize& requestedSize)
{
    QString link;
    {
        QMutexLocker locker(&m_mutex);
        link = m_link;
    }

    if (link.isEmpty()) {
        if (size)
            *size = QSize();
        return {};
    }

    std::optional<qrcodegen::QrCode> encoded;
    try {
        encoded = qrcodegen::QrCode::encodeText(link.toUtf8().constData(),
                                                qrcodegen::QrCode::Ecc::MEDIUM);
    } catch (...) {
        encoded.reset();
    }
    if (!encoded.has_value()) {
        qWarning("Could not encode the Telegram QR login link");
        if (size)
            *size = QSize();
        return {};
    }
    const qrcodegen::QrCode& code = *encoded;

    constexpr int borderModules = 4;
    const int moduleCount = code.getSize() + borderModules * 2;
    int scale = 8;
    if (requestedSize.isValid() && requestedSize.width() > 0)
        scale = qMax(2, requestedSize.width() / moduleCount);

    const int pixelSize = moduleCount * scale;
    QImage image(pixelSize, pixelSize, QImage::Format_RGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    for (int y = 0; y < code.getSize(); ++y) {
        for (int x = 0; x < code.getSize(); ++x) {
            if (code.getModule(x, y)) {
                painter.drawRect((x + borderModules) * scale, (y + borderModules) * scale,
                                 scale, scale);
            }
        }
    }
    painter.end();

    if (size)
        *size = image.size();
    return image;
}

} // namespace beygram::ui
