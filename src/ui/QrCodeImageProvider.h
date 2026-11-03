#pragma once

#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>
#include <QString>

namespace beygram::ui {

/// Renders the current Telegram QR login link as a QR image.
///
/// The link is a secret: it is never exposed as a QML string property, never
/// written to disk and never logged. AppController pushes a snapshot into this
/// provider when TDLib updates the link; the QML side only sees an image URL.
class QrCodeImageProvider final : public QQuickImageProvider {
public:
    QrCodeImageProvider();

    void setLink(const QString& link);

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

private:
    mutable QMutex m_mutex;
    QString m_link;
};

} // namespace beygram::ui
